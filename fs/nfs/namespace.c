/*
 * linux/fs/nfs/namespace.c
 *
 * Copyright (C) 2005 Trond Myklebust <Trond.Myklebust@netapp.com>
 *
 * NFS namespace
 */

#include <linux/config.h>

#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/nfs_fs.h>
#include <linux/string.h>
#include <linux/sunrpc/clnt.h>
#include <linux/vfs.h>
#include "internal.h"

#define NFSDBG_FACILITY		NFSDBG_VFS

static void nfs_expire_automounts(void *list);

LIST_HEAD(nfs_automount_list);
static DECLARE_WORK(nfs_automount_task, nfs_expire_automounts, &nfs_automount_list);
int nfs_mountpoint_expiry_timeout = 500 * HZ;

/*
 * nfs_path - reconstruct the path given an arbitrary dentry
 * @base - arbitrary string to prepend to the path
 * @dentry - pointer to dentry
 * @buffer - result buffer
 * @buflen - length of buffer
 *
 * Helper function for constructing the path from the
 * root dentry to an arbitrary hashed dentry.
 *
 * This is mainly for use in figuring out the path on the
 * server side when automounting on top of an existing partition.
 */
char *nfs_path(const char *base, const struct dentry *dentry,
	       char *buffer, ssize_t buflen)
{
	char *end = buffer+buflen;
	int namelen;

	*--end = '\0';
	buflen--;
	spin_lock(&dcache_lock);
	while (!IS_ROOT(dentry)) {
		namelen = dentry->d_name.len;
		buflen -= namelen + 1;
		if (buflen < 0)
			goto Elong_unlock;
		end -= namelen;
		memcpy(end, dentry->d_name.name, namelen);
		*--end = '/';
		dentry = dentry->d_parent;
	}
	spin_unlock(&dcache_lock);
	namelen = strlen(base);
	/* Strip off excess slashes in base string */
	while (namelen > 0 && base[namelen - 1] == '/')
		namelen--;
	buflen -= namelen;
	if (buflen < 0)
		goto Elong;
	end -= namelen;
	memcpy(end, base, namelen);
	return end;
Elong_unlock:
	spin_unlock(&dcache_lock);
Elong:
	return ERR_PTR(-ENAMETOOLONG);
}

/*
 * nfs_follow_mountpoint - handle crossing a mountpoint on the server
 * @dentry - dentry of mountpoint
 * @nd - nameidata info
 *
 * When we encounter a mountpoint on the server, we want to set up
 * a mountpoint on the client too, to prevent inode numbers from
 * colliding, and to allow "df" to work properly.
 * On NFSv4, we also want to allow for the fact that different
 * filesystems may be migrated to different servers in a failover
 * situation, and that different filesystems may want to use
 * different security flavours.
 */
static void * nfs_follow_mountpoint(struct dentry *dentry, struct nameidata *nd)
{
	struct vfsmount *mnt;
	struct nfs_server *server = NFS_SERVER(dentry->d_inode);
	struct dentry *parent;
	struct nfs_fh fh;
	struct nfs_fattr fattr;
	int err;

	BUG_ON(IS_ROOT(dentry));
	dprintk("%s: enter\n", __FUNCTION__);
	dput(nd->dentry);
	nd->dentry = dget(dentry);
	if (d_mountpoint(nd->dentry))
		goto out_follow;
	/* Look it up again */
	parent = dget_parent(nd->dentry);
	err = server->rpc_ops->lookup(parent->d_inode, &nd->dentry->d_name, &fh, &fattr);
	dput(parent);
	if (err != 0)
		goto out_err;

	if (fattr.valid & NFS_ATTR_FATTR_V4_REFERRAL)
		mnt = nfs_do_refmount(nd->mnt, nd->dentry);
	else
		mnt = nfs_do_submount(nd->mnt, nd->dentry, &fh, &fattr);
	err = PTR_ERR(mnt);
	if (IS_ERR(mnt))
		goto out_err;

	mntget(mnt);
	err = do_add_mount(mnt, nd, nd->mnt->mnt_flags|MNT_SHRINKABLE, &nfs_automount_list);
	if (err < 0) {
		mntput(mnt);
		if (err == -EBUSY)
			goto out_follow;
		goto out_err;
	}
	mntput(nd->mnt);
	dput(nd->dentry);
	nd->mnt = mnt;
	nd->dentry = dget(mnt->mnt_root);
	schedule_delayed_work(&nfs_automount_task, nfs_mountpoint_expiry_timeout);
out:
	dprintk("%s: done, returned %d\n", __FUNCTION__, err);
	return ERR_PTR(err);
out_err:
	path_release(nd);
	goto out;
out_follow:
	while(d_mountpoint(nd->dentry) && follow_down(&nd->mnt, &nd->dentry))
		;
	err = 0;
	goto out;
}

struct inode_operations nfs_mountpoint_inode_operations = {
	.follow_link	= nfs_follow_mountpoint,
	.getattr	= nfs_getattr,
};

struct inode_operations nfs_referral_inode_operations = {
	.follow_link	= nfs_follow_mountpoint,
};

static void nfs_expire_automounts(void *data)
{
	struct list_head *list = (struct list_head *)data;

	mark_mounts_for_expiry(list);
	if (!list_empty(list))
		schedule_delayed_work(&nfs_automount_task, nfs_mountpoint_expiry_timeout);
}

void nfs_release_automount_timer(void)
{
	if (list_empty(&nfs_automount_list)) {
		cancel_delayed_work(&nfs_automount_task);
		flush_scheduled_work();
	}
}

/*
 * Clone a mountpoint of the appropriate type
 */
static struct vfsmount *nfs_do_clone_mount(struct nfs_server *server, char *devname,
					   struct nfs_clone_mount *mountdata)
{
#ifdef CONFIG_NFS_V4
	struct vfsmount *mnt = NULL;
	switch (server->rpc_ops->version) {
		case 2:
		case 3:
			mnt = vfs_kern_mount(&clone_nfs_fs_type, 0, devname, mountdata);
			break;
		case 4:
			mnt = vfs_kern_mount(&clone_nfs4_fs_type, 0, devname, mountdata);
	}
	return mnt;
#else
	return vfs_kern_mount(&clone_nfs_fs_type, 0, devname, mountdata);
#endif
}

/**
 * nfs_do_submount - set up mountpoint when crossing a filesystem boundary
 * @mnt_parent - mountpoint of parent directory
 * @dentry - parent directory
 * @fh - filehandle for new root dentry
 * @fattr - attributes for new root inode
 *
 */
struct vfsmount *nfs_do_submount(const struct vfsmount *mnt_parent,
		const struct dentry *dentry, struct nfs_fh *fh,
		struct nfs_fattr *fattr)
{
	struct nfs_clone_mount mountdata = {
		.sb = mnt_parent->mnt_sb,
		.dentry = dentry,
		.fh = fh,
		.fattr = fattr,
	};
	struct vfsmount *mnt = ERR_PTR(-ENOMEM);
	char *page = (char *) __get_free_page(GFP_USER);
	char *devname;

	dprintk("%s: submounting on %s/%s\n", __FUNCTION__,
			dentry->d_parent->d_name.name,
			dentry->d_name.name);
	if (page == NULL)
		goto out;
	devname = nfs_devname(mnt_parent, dentry, page, PAGE_SIZE);
	mnt = (struct vfsmount *)devname;
	if (IS_ERR(devname))
		goto free_page;
	mnt = nfs_do_clone_mount(NFS_SB(mnt_parent->mnt_sb), devname, &mountdata);
free_page:
	free_page((unsigned long)page);
out:
	dprintk("%s: done\n", __FUNCTION__);
	return mnt;
}
