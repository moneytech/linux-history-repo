/*
 * linux/fs/reiserfs/xattr.c
 *
 * Copyright (c) 2002 by Jeff Mahoney, <jeffm@suse.com>
 *
 */

/*
 * In order to implement EA/ACLs in a clean, backwards compatible manner,
 * they are implemented as files in a "private" directory.
 * Each EA is in it's own file, with the directory layout like so (/ is assumed
 * to be relative to fs root). Inside the /.reiserfs_priv/xattrs directory,
 * directories named using the capital-hex form of the objectid and
 * generation number are used. Inside each directory are individual files
 * named with the name of the extended attribute.
 *
 * So, for objectid 12648430, we could have:
 * /.reiserfs_priv/xattrs/C0FFEE.0/system.posix_acl_access
 * /.reiserfs_priv/xattrs/C0FFEE.0/system.posix_acl_default
 * /.reiserfs_priv/xattrs/C0FFEE.0/user.Content-Type
 * .. or similar.
 *
 * The file contents are the text of the EA. The size is known based on the
 * stat data describing the file.
 *
 * In the case of system.posix_acl_access and system.posix_acl_default, since
 * these are special cases for filesystem ACLs, they are interpreted by the
 * kernel, in addition, they are negatively and positively cached and attached
 * to the inode so that unnecessary lookups are avoided.
 *
 * Locking works like so:
 * Directory components (xattr root, xattr dir) are protectd by their i_mutex.
 * The xattrs themselves are protected by the xattr_sem.
 */

#include <linux/reiserfs_fs.h>
#include <linux/capability.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/pagemap.h>
#include <linux/xattr.h>
#include <linux/reiserfs_xattr.h>
#include <linux/reiserfs_acl.h>
#include <asm/uaccess.h>
#include <net/checksum.h>
#include <linux/smp_lock.h>
#include <linux/stat.h>
#include <linux/quotaops.h>

#define PRIVROOT_NAME ".reiserfs_priv"
#define XAROOT_NAME   "xattrs"

static struct reiserfs_xattr_handler *find_xattr_handler_prefix(const char *);

/* Helpers for inode ops. We do this so that we don't have all the VFS
 * overhead and also for proper i_mutex annotation.
 * dir->i_mutex must be held for all of them. */
static int xattr_create(struct inode *dir, struct dentry *dentry, int mode)
{
	BUG_ON(!mutex_is_locked(&dir->i_mutex));
	DQUOT_INIT(dir);
	return dir->i_op->create(dir, dentry, mode, NULL);
}

static int xattr_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	BUG_ON(!mutex_is_locked(&dir->i_mutex));
	DQUOT_INIT(dir);
	return dir->i_op->mkdir(dir, dentry, mode);
}

/* We use I_MUTEX_CHILD here to silence lockdep. It's safe because xattr
 * mutation ops aren't called during rename or splace, which are the
 * only other users of I_MUTEX_CHILD. It violates the ordering, but that's
 * better than allocating another subclass just for this code. */
static int xattr_unlink(struct inode *dir, struct dentry *dentry)
{
	int error;
	BUG_ON(!mutex_is_locked(&dir->i_mutex));
	DQUOT_INIT(dir);

	mutex_lock_nested(&dentry->d_inode->i_mutex, I_MUTEX_CHILD);
	error = dir->i_op->unlink(dir, dentry);
	mutex_unlock(&dentry->d_inode->i_mutex);

	if (!error)
		d_delete(dentry);
	return error;
}

static int xattr_rmdir(struct inode *dir, struct dentry *dentry)
{
	int error;
	BUG_ON(!mutex_is_locked(&dir->i_mutex));
	DQUOT_INIT(dir);

	mutex_lock_nested(&dentry->d_inode->i_mutex, I_MUTEX_CHILD);
	dentry_unhash(dentry);
	error = dir->i_op->rmdir(dir, dentry);
	if (!error)
		dentry->d_inode->i_flags |= S_DEAD;
	mutex_unlock(&dentry->d_inode->i_mutex);
	if (!error)
		d_delete(dentry);
	dput(dentry);

	return error;
}


#define xattr_may_create(flags)	(!flags || flags & XATTR_CREATE)

/* Returns and possibly creates the xattr dir. */
static struct dentry *lookup_or_create_dir(struct dentry *parent,
					    const char *name, int flags)
{
	struct dentry *dentry;
	BUG_ON(!parent);

	dentry = lookup_one_len(name, parent, strlen(name));
	if (IS_ERR(dentry))
		return dentry;
	else if (!dentry->d_inode) {
		int err = -ENODATA;

		if (xattr_may_create(flags)) {
			mutex_lock_nested(&parent->d_inode->i_mutex,
					  I_MUTEX_XATTR);
			err = xattr_mkdir(parent->d_inode, dentry, 0700);
			mutex_unlock(&parent->d_inode->i_mutex);
		}

		if (err) {
			dput(dentry);
			dentry = ERR_PTR(err);
		}
	}

	return dentry;
}

static struct dentry *open_xa_root(struct super_block *sb, int flags)
{
	struct dentry *privroot = REISERFS_SB(sb)->priv_root;
	if (!privroot)
		return ERR_PTR(-ENODATA);
	return lookup_or_create_dir(privroot, XAROOT_NAME, flags);
}

static struct dentry *open_xa_dir(const struct inode *inode, int flags)
{
	struct dentry *xaroot, *xadir;
	char namebuf[17];

	xaroot = open_xa_root(inode->i_sb, flags);
	if (IS_ERR(xaroot))
		return xaroot;

	snprintf(namebuf, sizeof(namebuf), "%X.%X",
		 le32_to_cpu(INODE_PKEY(inode)->k_objectid),
		 inode->i_generation);

	xadir = lookup_or_create_dir(xaroot, namebuf, flags);
	dput(xaroot);
	return xadir;

}

/*
 * this is very similar to fs/reiserfs/dir.c:reiserfs_readdir, but
 * we need to drop the path before calling the filldir struct.  That
 * would be a big performance hit to the non-xattr case, so I've copied
 * the whole thing for now. --clm
 *
 * the big difference is that I go backwards through the directory,
 * and don't mess with f->f_pos, but the idea is the same.  Do some
 * action on each and every entry in the directory.
 *
 * we're called with i_mutex held, so there are no worries about the directory
 * changing underneath us.
 */
static int __xattr_readdir(struct inode *inode, void *dirent, filldir_t filldir)
{
	struct cpu_key pos_key;	/* key of current position in the directory (key of directory entry) */
	INITIALIZE_PATH(path_to_entry);
	struct buffer_head *bh;
	int entry_num;
	struct item_head *ih, tmp_ih;
	int search_res;
	char *local_buf;
	loff_t next_pos;
	char small_buf[32];	/* avoid kmalloc if we can */
	struct reiserfs_de_head *deh;
	int d_reclen;
	char *d_name;
	off_t d_off;
	ino_t d_ino;
	struct reiserfs_dir_entry de;

	/* form key for search the next directory entry using f_pos field of
	   file structure */
	next_pos = max_reiserfs_offset(inode);

	while (1) {
	      research:
		if (next_pos <= DOT_DOT_OFFSET)
			break;
		make_cpu_key(&pos_key, inode, next_pos, TYPE_DIRENTRY, 3);

		search_res =
		    search_by_entry_key(inode->i_sb, &pos_key, &path_to_entry,
					&de);
		if (search_res == IO_ERROR) {
			// FIXME: we could just skip part of directory which could
			// not be read
			pathrelse(&path_to_entry);
			return -EIO;
		}

		if (search_res == NAME_NOT_FOUND)
			de.de_entry_num--;

		set_de_name_and_namelen(&de);
		entry_num = de.de_entry_num;
		deh = &(de.de_deh[entry_num]);

		bh = de.de_bh;
		ih = de.de_ih;

		if (!is_direntry_le_ih(ih)) {
			reiserfs_error(inode->i_sb, "jdm-20000",
				       "not direntry %h", ih);
			break;
		}
		copy_item_head(&tmp_ih, ih);

		/* we must have found item, that is item of this directory, */
		RFALSE(COMP_SHORT_KEYS(&(ih->ih_key), &pos_key),
		       "vs-9000: found item %h does not match to dir we readdir %K",
		       ih, &pos_key);

		if (deh_offset(deh) <= DOT_DOT_OFFSET) {
			break;
		}

		/* look for the previous entry in the directory */
		next_pos = deh_offset(deh) - 1;

		if (!de_visible(deh))
			/* it is hidden entry */
			continue;

		d_reclen = entry_length(bh, ih, entry_num);
		d_name = B_I_DEH_ENTRY_FILE_NAME(bh, ih, deh);
		d_off = deh_offset(deh);
		d_ino = deh_objectid(deh);

		if (!d_name[d_reclen - 1])
			d_reclen = strlen(d_name);

		if (d_reclen > REISERFS_MAX_NAME(inode->i_sb->s_blocksize)) {
			/* too big to send back to VFS */
			continue;
		}

		/* Ignore the .reiserfs_priv entry */
		if (reiserfs_xattrs(inode->i_sb) &&
		    !old_format_only(inode->i_sb) &&
		    deh_objectid(deh) ==
		    le32_to_cpu(INODE_PKEY
				(REISERFS_SB(inode->i_sb)->priv_root->d_inode)->
				k_objectid))
			continue;

		if (d_reclen <= 32) {
			local_buf = small_buf;
		} else {
			local_buf = kmalloc(d_reclen, GFP_NOFS);
			if (!local_buf) {
				pathrelse(&path_to_entry);
				return -ENOMEM;
			}
			if (item_moved(&tmp_ih, &path_to_entry)) {
				kfree(local_buf);

				/* sigh, must retry.  Do this same offset again */
				next_pos = d_off;
				goto research;
			}
		}

		// Note, that we copy name to user space via temporary
		// buffer (local_buf) because filldir will block if
		// user space buffer is swapped out. At that time
		// entry can move to somewhere else
		memcpy(local_buf, d_name, d_reclen);

		/* the filldir function might need to start transactions,
		 * or do who knows what.  Release the path now that we've
		 * copied all the important stuff out of the deh
		 */
		pathrelse(&path_to_entry);

		if (filldir(dirent, local_buf, d_reclen, d_off, d_ino,
			    DT_UNKNOWN) < 0) {
			if (local_buf != small_buf) {
				kfree(local_buf);
			}
			goto end;
		}
		if (local_buf != small_buf) {
			kfree(local_buf);
		}
	}			/* while */

      end:
	pathrelse(&path_to_entry);
	return 0;
}

/*
 * this could be done with dedicated readdir ops for the xattr files,
 * but I want to get something working asap
 * this is stolen from vfs_readdir
 *
 */
static
int xattr_readdir(struct inode *inode, filldir_t filler, void *buf)
{
	int res = -ENOENT;
	if (!IS_DEADDIR(inode)) {
		lock_kernel();
		res = __xattr_readdir(inode, buf, filler);
		unlock_kernel();
	}
	return res;
}

/* expects xadir->d_inode->i_mutex to be locked */
static int
__reiserfs_xattr_del(struct dentry *xadir, const char *name, int namelen)
{
	struct dentry *dentry;
	struct inode *dir = xadir->d_inode;
	int err = 0;
	struct reiserfs_xattr_handler *xah;

	dentry = lookup_one_len(name, xadir, namelen);
	if (IS_ERR(dentry)) {
		err = PTR_ERR(dentry);
		goto out;
	} else if (!dentry->d_inode) {
		err = -ENODATA;
		goto out_file;
	}

	/* Skip directories.. */
	if (S_ISDIR(dentry->d_inode->i_mode))
		goto out_file;

	if (!IS_PRIVATE(dentry->d_inode)) {
		reiserfs_error(dir->i_sb, "jdm-20003",
			       "OID %08x [%.*s/%.*s] doesn't have "
			       "priv flag set [parent is %sset].",
			       le32_to_cpu(INODE_PKEY(dentry->d_inode)->
					   k_objectid), xadir->d_name.len,
			       xadir->d_name.name, namelen, name,
			       IS_PRIVATE(xadir->d_inode) ? "" :
			       "not ");
		dput(dentry);
		return -EIO;
	}

	/* Deletion pre-operation */
	xah = find_xattr_handler_prefix(name);
	if (xah && xah->del) {
		err = xah->del(dentry->d_inode, name);
		if (err)
			goto out;
	}

	err = xattr_unlink(dir, dentry);

out_file:
	dput(dentry);

out:
	return err;
}

/* The following are side effects of other operations that aren't explicitly
 * modifying extended attributes. This includes operations such as permissions
 * or ownership changes, object deletions, etc. */

static int
reiserfs_delete_xattrs_filler(void *buf, const char *name, int namelen,
			      loff_t offset, u64 ino, unsigned int d_type)
{
	struct dentry *xadir = (struct dentry *)buf;

	return __reiserfs_xattr_del(xadir, name, namelen);

}

/* This is called w/ inode->i_mutex downed */
int reiserfs_delete_xattrs(struct inode *inode)
{
	int err = -ENODATA;
	struct dentry *dir, *root;
	struct reiserfs_transaction_handle th;
	int blocks = JOURNAL_PER_BALANCE_CNT * 2 + 2 +
		     4 * REISERFS_QUOTA_TRANS_BLOCKS(inode->i_sb);

	/* Skip out, an xattr has no xattrs associated with it */
	if (IS_PRIVATE(inode) || get_inode_sd_version(inode) == STAT_DATA_V1)
		return 0;

	dir = open_xa_dir(inode, XATTR_REPLACE);
	if (IS_ERR(dir)) {
		err = PTR_ERR(dir);
		goto out;
	} else if (!dir->d_inode) {
		dput(dir);
		goto out;
	}

	mutex_lock_nested(&dir->d_inode->i_mutex, I_MUTEX_XATTR);
	err = xattr_readdir(dir->d_inode, reiserfs_delete_xattrs_filler, dir);
	mutex_unlock(&dir->d_inode->i_mutex);
	if (err) {
		dput(dir);
		goto out;
	}

	root = dget(dir->d_parent);
	dput(dir);

	/* We start a transaction here to avoid a ABBA situation
	 * between the xattr root's i_mutex and the journal lock.
	 * Inode creation will inherit an ACL, which requires a
	 * lookup. The lookup locks the xattr root i_mutex with a
	 * transaction open.  Inode deletion takes teh xattr root
	 * i_mutex to delete the directory and then starts a
	 * transaction inside it. Boom. This doesn't incur much
	 * additional overhead since the reiserfs_rmdir transaction
	 * will just nest inside the outer transaction. */
	err = journal_begin(&th, inode->i_sb, blocks);
	if (!err) {
		int jerror;
		mutex_lock_nested(&root->d_inode->i_mutex, I_MUTEX_XATTR);
		err = xattr_rmdir(root->d_inode, dir);
		jerror = journal_end(&th, inode->i_sb, blocks);
		mutex_unlock(&root->d_inode->i_mutex);
		err = jerror ?: err;
	}

	dput(root);
out:
	if (!err)
		REISERFS_I(inode)->i_flags =
		    REISERFS_I(inode)->i_flags & ~i_has_xattr_dir;
	else
		reiserfs_warning(inode->i_sb, "jdm-20004",
				 "Couldn't remove all xattrs (%d)\n", err);
	return err;
}

struct reiserfs_chown_buf {
	struct inode *inode;
	struct dentry *xadir;
	struct iattr *attrs;
};

/* XXX: If there is a better way to do this, I'd love to hear about it */
static int
reiserfs_chown_xattrs_filler(void *buf, const char *name, int namelen,
			     loff_t offset, u64 ino, unsigned int d_type)
{
	struct reiserfs_chown_buf *chown_buf = (struct reiserfs_chown_buf *)buf;
	struct dentry *xafile, *xadir = chown_buf->xadir;
	struct iattr *attrs = chown_buf->attrs;
	int err = 0;

	xafile = lookup_one_len(name, xadir, namelen);
	if (IS_ERR(xafile))
		return PTR_ERR(xafile);
	else if (!xafile->d_inode) {
		dput(xafile);
		return -ENODATA;
	}

	if (!S_ISDIR(xafile->d_inode->i_mode)) {
		mutex_lock_nested(&xafile->d_inode->i_mutex, I_MUTEX_CHILD);
		err = reiserfs_setattr(xafile, attrs);
		mutex_unlock(&xafile->d_inode->i_mutex);
	}
	dput(xafile);

	return err;
}

int reiserfs_chown_xattrs(struct inode *inode, struct iattr *attrs)
{
	struct dentry *dir;
	int err = 0;
	struct reiserfs_chown_buf buf;
	unsigned int ia_valid = attrs->ia_valid;

	/* Skip out, an xattr has no xattrs associated with it */
	if (IS_PRIVATE(inode) || get_inode_sd_version(inode) == STAT_DATA_V1)
		return 0;

	dir = open_xa_dir(inode, XATTR_REPLACE);
	if (IS_ERR(dir)) {
		if (PTR_ERR(dir) != -ENODATA)
			err = PTR_ERR(dir);
		goto out;
	} else if (!dir->d_inode)
		goto out_dir;

	attrs->ia_valid &= (ATTR_UID | ATTR_GID | ATTR_CTIME);
	buf.xadir = dir;
	buf.attrs = attrs;
	buf.inode = inode;

	mutex_lock_nested(&dir->d_inode->i_mutex, I_MUTEX_XATTR);
	err = xattr_readdir(dir->d_inode, reiserfs_chown_xattrs_filler, &buf);

	if (!err)
		err = reiserfs_setattr(dir, attrs);
	mutex_unlock(&dir->d_inode->i_mutex);

	attrs->ia_valid = ia_valid;
out_dir:
	dput(dir);
out:
	if (err)
		reiserfs_warning(inode->i_sb, "jdm-20007",
				 "Couldn't chown all xattrs (%d)\n", err);
	return err;
}

#ifdef CONFIG_REISERFS_FS_XATTR
static struct reiserfs_xattr_handler *find_xattr_handler_prefix(const char
								*prefix);

/* Returns a dentry corresponding to a specific extended attribute file
 * for the inode. If flags allow, the file is created. Otherwise, a
 * valid or negative dentry, or an error is returned. */
static struct dentry *get_xa_file_dentry(const struct inode *inode,
					 const char *name, int flags)
{
	struct dentry *xadir, *xafile;
	int err = 0;

	xadir = open_xa_dir(inode, flags);
	if (IS_ERR(xadir))
		return ERR_CAST(xadir);

	xafile = lookup_one_len(name, xadir, strlen(name));
	if (IS_ERR(xafile)) {
		err = PTR_ERR(xafile);
		goto out;
	}

	if (xafile->d_inode && (flags & XATTR_CREATE))
		err = -EEXIST;

	if (!xafile->d_inode) {
		err = -ENODATA;
		if (xattr_may_create(flags)) {
			mutex_lock_nested(&xadir->d_inode->i_mutex,
					  I_MUTEX_XATTR);
			err = xattr_create(xadir->d_inode, xafile,
					      0700|S_IFREG);
			mutex_unlock(&xadir->d_inode->i_mutex);
		}
	}

	if (err)
		dput(xafile);
out:
	dput(xadir);
	if (err)
		return ERR_PTR(err);
	return xafile;
}

/* Internal operations on file data */
static inline void reiserfs_put_page(struct page *page)
{
	kunmap(page);
	page_cache_release(page);
}

static struct page *reiserfs_get_page(struct inode *dir, size_t n)
{
	struct address_space *mapping = dir->i_mapping;
	struct page *page;
	/* We can deadlock if we try to free dentries,
	   and an unlink/rmdir has just occured - GFP_NOFS avoids this */
	mapping_set_gfp_mask(mapping, GFP_NOFS);
	page = read_mapping_page(mapping, n >> PAGE_CACHE_SHIFT, NULL);
	if (!IS_ERR(page)) {
		kmap(page);
		if (PageError(page))
			goto fail;
	}
	return page;

      fail:
	reiserfs_put_page(page);
	return ERR_PTR(-EIO);
}

static inline __u32 xattr_hash(const char *msg, int len)
{
	return csum_partial(msg, len, 0);
}

int reiserfs_commit_write(struct file *f, struct page *page,
			  unsigned from, unsigned to);
int reiserfs_prepare_write(struct file *f, struct page *page,
			   unsigned from, unsigned to);


/* Generic extended attribute operations that can be used by xa plugins */

/*
 * inode->i_mutex: down
 */
int
reiserfs_xattr_set(struct inode *inode, const char *name, const void *buffer,
		   size_t buffer_size, int flags)
{
	int err = 0;
	struct dentry *dentry;
	struct page *page;
	char *data;
	size_t file_pos = 0;
	size_t buffer_pos = 0;
	struct iattr newattrs;
	__u32 xahash = 0;

	if (get_inode_sd_version(inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	if (!buffer)
		return reiserfs_xattr_del(inode, name);

	dentry = get_xa_file_dentry(inode, name, flags);
	if (IS_ERR(dentry)) {
		err = PTR_ERR(dentry);
		goto out;
	}

	down_write(&REISERFS_I(inode)->i_xattr_sem);

	xahash = xattr_hash(buffer, buffer_size);
	REISERFS_I(inode)->i_flags |= i_has_xattr_dir;

	/* Resize it so we're ok to write there */
	newattrs.ia_size = buffer_size;
	newattrs.ia_ctime = current_fs_time(inode->i_sb);
	newattrs.ia_valid = ATTR_SIZE | ATTR_CTIME;
	mutex_lock_nested(&dentry->d_inode->i_mutex, I_MUTEX_XATTR);
	down_write(&dentry->d_inode->i_alloc_sem);
	err = reiserfs_setattr(dentry, &newattrs);
	up_write(&dentry->d_inode->i_alloc_sem);
	mutex_unlock(&dentry->d_inode->i_mutex);
	if (err)
		goto out_filp;

	while (buffer_pos < buffer_size || buffer_pos == 0) {
		size_t chunk;
		size_t skip = 0;
		size_t page_offset = (file_pos & (PAGE_CACHE_SIZE - 1));
		if (buffer_size - buffer_pos > PAGE_CACHE_SIZE)
			chunk = PAGE_CACHE_SIZE;
		else
			chunk = buffer_size - buffer_pos;

		page = reiserfs_get_page(dentry->d_inode, file_pos);
		if (IS_ERR(page)) {
			err = PTR_ERR(page);
			goto out_filp;
		}

		lock_page(page);
		data = page_address(page);

		if (file_pos == 0) {
			struct reiserfs_xattr_header *rxh;
			skip = file_pos = sizeof(struct reiserfs_xattr_header);
			if (chunk + skip > PAGE_CACHE_SIZE)
				chunk = PAGE_CACHE_SIZE - skip;
			rxh = (struct reiserfs_xattr_header *)data;
			rxh->h_magic = cpu_to_le32(REISERFS_XATTR_MAGIC);
			rxh->h_hash = cpu_to_le32(xahash);
		}

		err = reiserfs_prepare_write(NULL, page, page_offset,
					    page_offset + chunk + skip);
		if (!err) {
			if (buffer)
				memcpy(data + skip, buffer + buffer_pos, chunk);
			err = reiserfs_commit_write(NULL, page, page_offset,
						    page_offset + chunk +
						    skip);
		}
		unlock_page(page);
		reiserfs_put_page(page);
		buffer_pos += chunk;
		file_pos += chunk;
		skip = 0;
		if (err || buffer_size == 0 || !buffer)
			break;
	}

	/* We can't mark the inode dirty if it's not hashed. This is the case
	 * when we're inheriting the default ACL. If we dirty it, the inode
	 * gets marked dirty, but won't (ever) make it onto the dirty list until
	 * it's synced explicitly to clear I_DIRTY. This is bad. */
	if (!hlist_unhashed(&inode->i_hash)) {
		inode->i_ctime = CURRENT_TIME_SEC;
		mark_inode_dirty(inode);
	}

      out_filp:
	up_write(&REISERFS_I(inode)->i_xattr_sem);
	dput(dentry);

      out:
	return err;
}

/*
 * inode->i_mutex: down
 */
int
reiserfs_xattr_get(const struct inode *inode, const char *name, void *buffer,
		   size_t buffer_size)
{
	ssize_t err = 0;
	struct dentry *dentry;
	size_t isize;
	size_t file_pos = 0;
	size_t buffer_pos = 0;
	struct page *page;
	__u32 hash = 0;

	if (name == NULL)
		return -EINVAL;

	/* We can't have xattrs attached to v1 items since they don't have
	 * generation numbers */
	if (get_inode_sd_version(inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	dentry = get_xa_file_dentry(inode, name, XATTR_REPLACE);
	if (IS_ERR(dentry)) {
		err = PTR_ERR(dentry);
		goto out;
	}

	down_read(&REISERFS_I(inode)->i_xattr_sem);

	isize = i_size_read(dentry->d_inode);
	REISERFS_I(inode)->i_flags |= i_has_xattr_dir;

	/* Just return the size needed */
	if (buffer == NULL) {
		err = isize - sizeof(struct reiserfs_xattr_header);
		goto out_unlock;
	}

	if (buffer_size < isize - sizeof(struct reiserfs_xattr_header)) {
		err = -ERANGE;
		goto out_unlock;
	}

	while (file_pos < isize) {
		size_t chunk;
		char *data;
		size_t skip = 0;
		if (isize - file_pos > PAGE_CACHE_SIZE)
			chunk = PAGE_CACHE_SIZE;
		else
			chunk = isize - file_pos;

		page = reiserfs_get_page(dentry->d_inode, file_pos);
		if (IS_ERR(page)) {
			err = PTR_ERR(page);
			goto out_unlock;
		}

		lock_page(page);
		data = page_address(page);
		if (file_pos == 0) {
			struct reiserfs_xattr_header *rxh =
			    (struct reiserfs_xattr_header *)data;
			skip = file_pos = sizeof(struct reiserfs_xattr_header);
			chunk -= skip;
			/* Magic doesn't match up.. */
			if (rxh->h_magic != cpu_to_le32(REISERFS_XATTR_MAGIC)) {
				unlock_page(page);
				reiserfs_put_page(page);
				reiserfs_warning(inode->i_sb, "jdm-20001",
						 "Invalid magic for xattr (%s) "
						 "associated with %k", name,
						 INODE_PKEY(inode));
				err = -EIO;
				goto out_unlock;
			}
			hash = le32_to_cpu(rxh->h_hash);
		}
		memcpy(buffer + buffer_pos, data + skip, chunk);
		unlock_page(page);
		reiserfs_put_page(page);
		file_pos += chunk;
		buffer_pos += chunk;
		skip = 0;
	}
	err = isize - sizeof(struct reiserfs_xattr_header);

	if (xattr_hash(buffer, isize - sizeof(struct reiserfs_xattr_header)) !=
	    hash) {
		reiserfs_warning(inode->i_sb, "jdm-20002",
				 "Invalid hash for xattr (%s) associated "
				 "with %k", name, INODE_PKEY(inode));
		err = -EIO;
	}

out_unlock:
	up_read(&REISERFS_I(inode)->i_xattr_sem);
	dput(dentry);

out:
	return err;
}

int reiserfs_xattr_del(struct inode *inode, const char *name)
{
	struct dentry *dir;
	int err;

	dir = open_xa_dir(inode, XATTR_REPLACE);
	if (IS_ERR(dir)) {
		err = PTR_ERR(dir);
		goto out;
	}

	mutex_lock_nested(&dir->d_inode->i_mutex, I_MUTEX_XATTR);
	err = __reiserfs_xattr_del(dir, name, strlen(name));
	mutex_unlock(&dir->d_inode->i_mutex);
	dput(dir);

	if (!err) {
		inode->i_ctime = CURRENT_TIME_SEC;
		mark_inode_dirty(inode);
	}

      out:
	return err;
}

/* Actual operations that are exported to VFS-land */
/*
 * Inode operation getxattr()
 */
ssize_t
reiserfs_getxattr(struct dentry * dentry, const char *name, void *buffer,
		  size_t size)
{
	struct reiserfs_xattr_handler *xah = find_xattr_handler_prefix(name);
	int err;

	if (!xah || !reiserfs_xattrs(dentry->d_sb) ||
	    get_inode_sd_version(dentry->d_inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	err = xah->get(dentry->d_inode, name, buffer, size);
	return err;
}

/*
 * Inode operation setxattr()
 *
 * dentry->d_inode->i_mutex down
 */
int
reiserfs_setxattr(struct dentry *dentry, const char *name, const void *value,
		  size_t size, int flags)
{
	struct reiserfs_xattr_handler *xah = find_xattr_handler_prefix(name);
	int err;

	if (!xah || !reiserfs_xattrs(dentry->d_sb) ||
	    get_inode_sd_version(dentry->d_inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	err = xah->set(dentry->d_inode, name, value, size, flags);
	return err;
}

/*
 * Inode operation removexattr()
 *
 * dentry->d_inode->i_mutex down
 */
int reiserfs_removexattr(struct dentry *dentry, const char *name)
{
	int err;
	struct reiserfs_xattr_handler *xah = find_xattr_handler_prefix(name);

	if (!xah || !reiserfs_xattrs(dentry->d_sb) ||
	    get_inode_sd_version(dentry->d_inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	err = reiserfs_xattr_del(dentry->d_inode, name);

	dentry->d_inode->i_ctime = CURRENT_TIME_SEC;
	mark_inode_dirty(dentry->d_inode);

	return err;
}

/* This is what filldir will use:
 * r_pos will always contain the amount of space required for the entire
 * list. If r_pos becomes larger than r_size, we need more space and we
 * return an error indicating this. If r_pos is less than r_size, then we've
 * filled the buffer successfully and we return success */
struct reiserfs_listxattr_buf {
	int r_pos;
	int r_size;
	char *r_buf;
	struct inode *r_inode;
};

static int
reiserfs_listxattr_filler(void *buf, const char *name, int namelen,
			  loff_t offset, u64 ino, unsigned int d_type)
{
	struct reiserfs_listxattr_buf *b = (struct reiserfs_listxattr_buf *)buf;
	int len = 0;
	if (name[0] != '.'
	    || (namelen != 1 && (name[1] != '.' || namelen != 2))) {
		struct reiserfs_xattr_handler *xah =
		    find_xattr_handler_prefix(name);
		if (!xah)
			return 0;	/* Unsupported xattr name, skip it */

		/* We call ->list() twice because the operation isn't required to just
		 * return the name back - we want to make sure we have enough space */
		len += xah->list(b->r_inode, name, namelen, NULL);

		if (len) {
			if (b->r_pos + len + 1 <= b->r_size) {
				char *p = b->r_buf + b->r_pos;
				p += xah->list(b->r_inode, name, namelen, p);
				*p++ = '\0';
			}
			b->r_pos += len + 1;
		}
	}

	return 0;
}

/*
 * Inode operation listxattr()
 */
ssize_t reiserfs_listxattr(struct dentry * dentry, char *buffer, size_t size)
{
	struct dentry *dir;
	int err = 0;
	struct reiserfs_listxattr_buf buf;

	if (!dentry->d_inode)
		return -EINVAL;

	if (!reiserfs_xattrs(dentry->d_sb) ||
	    get_inode_sd_version(dentry->d_inode) == STAT_DATA_V1)
		return -EOPNOTSUPP;

	dir = open_xa_dir(dentry->d_inode, XATTR_REPLACE);
	if (IS_ERR(dir)) {
		err = PTR_ERR(dir);
		if (err == -ENODATA)
			err = 0;	/* Not an error if there aren't any xattrs */
		goto out;
	}

	buf.r_buf = buffer;
	buf.r_size = buffer ? size : 0;
	buf.r_pos = 0;
	buf.r_inode = dentry->d_inode;

	REISERFS_I(dentry->d_inode)->i_flags |= i_has_xattr_dir;

	mutex_lock_nested(&dir->d_inode->i_mutex, I_MUTEX_XATTR);
	err = xattr_readdir(dir->d_inode, reiserfs_listxattr_filler, &buf);
	mutex_unlock(&dir->d_inode->i_mutex);

	if (!err) {
		if (buf.r_pos > buf.r_size && buffer != NULL)
			err = -ERANGE;
		else
			err = buf.r_pos;
	}

	dput(dir);
out:
	return err;
}

/* This is the implementation for the xattr plugin infrastructure */
static LIST_HEAD(xattr_handlers);
static DEFINE_RWLOCK(handler_lock);

static struct reiserfs_xattr_handler *find_xattr_handler_prefix(const char
								*prefix)
{
	struct reiserfs_xattr_handler *xah = NULL;
	struct list_head *p;

	read_lock(&handler_lock);
	list_for_each(p, &xattr_handlers) {
		xah = list_entry(p, struct reiserfs_xattr_handler, handlers);
		if (strncmp(xah->prefix, prefix, strlen(xah->prefix)) == 0)
			break;
		xah = NULL;
	}

	read_unlock(&handler_lock);
	return xah;
}

static void __unregister_handlers(void)
{
	struct reiserfs_xattr_handler *xah;
	struct list_head *p, *tmp;

	list_for_each_safe(p, tmp, &xattr_handlers) {
		xah = list_entry(p, struct reiserfs_xattr_handler, handlers);
		if (xah->exit)
			xah->exit();

		list_del_init(p);
	}
	INIT_LIST_HEAD(&xattr_handlers);
}

int __init reiserfs_xattr_register_handlers(void)
{
	int err = 0;
	struct reiserfs_xattr_handler *xah;
	struct list_head *p;

	write_lock(&handler_lock);

	/* If we're already initialized, nothing to do */
	if (!list_empty(&xattr_handlers)) {
		write_unlock(&handler_lock);
		return 0;
	}

	/* Add the handlers */
	list_add_tail(&user_handler.handlers, &xattr_handlers);
	list_add_tail(&trusted_handler.handlers, &xattr_handlers);
#ifdef CONFIG_REISERFS_FS_SECURITY
	list_add_tail(&security_handler.handlers, &xattr_handlers);
#endif
#ifdef CONFIG_REISERFS_FS_POSIX_ACL
	list_add_tail(&posix_acl_access_handler.handlers, &xattr_handlers);
	list_add_tail(&posix_acl_default_handler.handlers, &xattr_handlers);
#endif

	/* Run initializers, if available */
	list_for_each(p, &xattr_handlers) {
		xah = list_entry(p, struct reiserfs_xattr_handler, handlers);
		if (xah->init) {
			err = xah->init();
			if (err) {
				list_del_init(p);
				break;
			}
		}
	}

	/* Clean up other handlers, if any failed */
	if (err)
		__unregister_handlers();

	write_unlock(&handler_lock);
	return err;
}

void reiserfs_xattr_unregister_handlers(void)
{
	write_lock(&handler_lock);
	__unregister_handlers();
	write_unlock(&handler_lock);
}

static int reiserfs_check_acl(struct inode *inode, int mask)
{
	struct posix_acl *acl;
	int error = -EAGAIN; /* do regular unix permission checks by default */

	acl = reiserfs_get_acl(inode, ACL_TYPE_ACCESS);

	if (acl) {
		if (!IS_ERR(acl)) {
			error = posix_acl_permission(inode, acl, mask);
			posix_acl_release(acl);
		} else if (PTR_ERR(acl) != -ENODATA)
			error = PTR_ERR(acl);
	}

	return error;
}

int reiserfs_permission(struct inode *inode, int mask)
{
	/*
	 * We don't do permission checks on the internal objects.
	 * Permissions are determined by the "owning" object.
	 */
	if (IS_PRIVATE(inode))
		return 0;
	/*
	 * Stat data v1 doesn't support ACLs.
	 */
	if (get_inode_sd_version(inode) == STAT_DATA_V1)
		return generic_permission(inode, mask, NULL);
	else
		return generic_permission(inode, mask, reiserfs_check_acl);
}

static int create_privroot(struct dentry *dentry)
{
	int err;
	struct inode *inode = dentry->d_parent->d_inode;
	mutex_lock_nested(&inode->i_mutex, I_MUTEX_XATTR);
	err = xattr_mkdir(inode, dentry, 0700);
	mutex_unlock(&inode->i_mutex);
	if (err) {
		dput(dentry);
		dentry = NULL;
	}

	if (dentry && dentry->d_inode)
		reiserfs_info(dentry->d_sb, "Created %s - reserved for xattr "
			      "storage.\n", PRIVROOT_NAME);

	return err;
}

static int xattr_mount_check(struct super_block *s)
{
	/* We need generation numbers to ensure that the oid mapping is correct
	 * v3.5 filesystems don't have them. */
	if (!old_format_only(s)) {
		set_bit(REISERFS_XATTRS, &(REISERFS_SB(s)->s_mount_opt));
	} else if (reiserfs_xattrs_optional(s)) {
		/* Old format filesystem, but optional xattrs have been enabled
		 * at mount time. Error out. */
		reiserfs_warning(s, "jdm-20005",
				 "xattrs/ACLs not supported on pre v3.6 "
				 "format filesystem. Failing mount.");
		return -EOPNOTSUPP;
	} else {
		/* Old format filesystem, but no optional xattrs have
		 * been enabled. This means we silently disable xattrs
		 * on the filesystem. */
		clear_bit(REISERFS_XATTRS, &(REISERFS_SB(s)->s_mount_opt));
	}

	return 0;
}

#else
int __init reiserfs_xattr_register_handlers(void) { return 0; }
void reiserfs_xattr_unregister_handlers(void) {}
#endif

/* This will catch lookups from the fs root to .reiserfs_priv */
static int
xattr_lookup_poison(struct dentry *dentry, struct qstr *q1, struct qstr *name)
{
	struct dentry *priv_root = REISERFS_SB(dentry->d_sb)->priv_root;
	if (name->len == priv_root->d_name.len &&
	    name->hash == priv_root->d_name.hash &&
	    !memcmp(name->name, priv_root->d_name.name, name->len)) {
		return -ENOENT;
	} else if (q1->len == name->len &&
		   !memcmp(q1->name, name->name, name->len))
		return 0;
	return 1;
}

static struct dentry_operations xattr_lookup_poison_ops = {
	.d_compare = xattr_lookup_poison,
};

/* We need to take a copy of the mount flags since things like
 * MS_RDONLY don't get set until *after* we're called.
 * mount_flags != mount_options */
int reiserfs_xattr_init(struct super_block *s, int mount_flags)
{
	int err = 0;

#ifdef CONFIG_REISERFS_FS_XATTR
	err = xattr_mount_check(s);
	if (err)
		goto error;
#endif

	/* If we don't have the privroot located yet - go find it */
	if (!REISERFS_SB(s)->priv_root) {
		struct dentry *dentry;
		dentry = lookup_one_len(PRIVROOT_NAME, s->s_root,
					strlen(PRIVROOT_NAME));
		if (!IS_ERR(dentry)) {
#ifdef CONFIG_REISERFS_FS_XATTR
			if (!(mount_flags & MS_RDONLY) && !dentry->d_inode)
				err = create_privroot(dentry);
#endif
			if (!dentry->d_inode) {
				dput(dentry);
				dentry = NULL;
			}
		} else
			err = PTR_ERR(dentry);

		if (!err && dentry) {
			s->s_root->d_op = &xattr_lookup_poison_ops;
			dentry->d_inode->i_flags |= S_PRIVATE;
			REISERFS_SB(s)->priv_root = dentry;
#ifdef CONFIG_REISERFS_FS_XATTR
		/* xattrs are unavailable */
		} else if (!(mount_flags & MS_RDONLY)) {
			/* If we're read-only it just means that the dir
			 * hasn't been created. Not an error -- just no
			 * xattrs on the fs. We'll check again if we
			 * go read-write */
			reiserfs_warning(s, "jdm-20006",
					 "xattrs/ACLs enabled and couldn't "
					 "find/create .reiserfs_priv. "
					 "Failing mount.");
			err = -EOPNOTSUPP;
#endif
		}
	}

#ifdef CONFIG_REISERFS_FS_XATTR
error:
	if (err) {
		clear_bit(REISERFS_XATTRS, &(REISERFS_SB(s)->s_mount_opt));
		clear_bit(REISERFS_XATTRS_USER, &(REISERFS_SB(s)->s_mount_opt));
		clear_bit(REISERFS_POSIXACL, &(REISERFS_SB(s)->s_mount_opt));
	}
#endif

	/* The super_block MS_POSIXACL must mirror the (no)acl mount option. */
	s->s_flags = s->s_flags & ~MS_POSIXACL;
#ifdef CONFIG_REISERFS_FS_POSIX_ACL
	if (reiserfs_posixacl(s))
		s->s_flags |= MS_POSIXACL;
#endif

	return err;
}
