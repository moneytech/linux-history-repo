/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Cisco Systems.  All rights reserved.
 * Copyright (c) 2005 Mellanox Technologies. All rights reserved.
 * Copyright (c) 2005 Voltaire, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id: uverbs_main.c 2733 2005-06-28 19:14:34Z roland $
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/mount.h>

#include <asm/uaccess.h>

#include "uverbs.h"

MODULE_AUTHOR("Roland Dreier");
MODULE_DESCRIPTION("InfiniBand userspace verbs access");
MODULE_LICENSE("Dual BSD/GPL");

#define INFINIBANDEVENTFS_MAGIC	0x49426576	/* "IBev" */

enum {
	IB_UVERBS_MAJOR       = 231,
	IB_UVERBS_BASE_MINOR  = 192,
	IB_UVERBS_MAX_DEVICES = 32
};

#define IB_UVERBS_BASE_DEV	MKDEV(IB_UVERBS_MAJOR, IB_UVERBS_BASE_MINOR)

DECLARE_MUTEX(ib_uverbs_idr_mutex);
DEFINE_IDR(ib_uverbs_pd_idr);
DEFINE_IDR(ib_uverbs_mr_idr);
DEFINE_IDR(ib_uverbs_mw_idr);
DEFINE_IDR(ib_uverbs_ah_idr);
DEFINE_IDR(ib_uverbs_cq_idr);
DEFINE_IDR(ib_uverbs_qp_idr);
DEFINE_IDR(ib_uverbs_srq_idr);

static spinlock_t map_lock;
static DECLARE_BITMAP(dev_map, IB_UVERBS_MAX_DEVICES);

static ssize_t (*uverbs_cmd_table[])(struct ib_uverbs_file *file,
				     const char __user *buf, int in_len,
				     int out_len) = {
	[IB_USER_VERBS_CMD_QUERY_PARAMS]  = ib_uverbs_query_params,
	[IB_USER_VERBS_CMD_GET_CONTEXT]   = ib_uverbs_get_context,
	[IB_USER_VERBS_CMD_QUERY_DEVICE]  = ib_uverbs_query_device,
	[IB_USER_VERBS_CMD_QUERY_PORT]    = ib_uverbs_query_port,
	[IB_USER_VERBS_CMD_QUERY_GID]     = ib_uverbs_query_gid,
	[IB_USER_VERBS_CMD_QUERY_PKEY]    = ib_uverbs_query_pkey,
	[IB_USER_VERBS_CMD_ALLOC_PD]      = ib_uverbs_alloc_pd,
	[IB_USER_VERBS_CMD_DEALLOC_PD]    = ib_uverbs_dealloc_pd,
	[IB_USER_VERBS_CMD_REG_MR]        = ib_uverbs_reg_mr,
	[IB_USER_VERBS_CMD_DEREG_MR]      = ib_uverbs_dereg_mr,
	[IB_USER_VERBS_CMD_CREATE_CQ]     = ib_uverbs_create_cq,
	[IB_USER_VERBS_CMD_DESTROY_CQ]    = ib_uverbs_destroy_cq,
	[IB_USER_VERBS_CMD_CREATE_QP]     = ib_uverbs_create_qp,
	[IB_USER_VERBS_CMD_MODIFY_QP]     = ib_uverbs_modify_qp,
	[IB_USER_VERBS_CMD_DESTROY_QP]    = ib_uverbs_destroy_qp,
	[IB_USER_VERBS_CMD_ATTACH_MCAST]  = ib_uverbs_attach_mcast,
	[IB_USER_VERBS_CMD_DETACH_MCAST]  = ib_uverbs_detach_mcast,
	[IB_USER_VERBS_CMD_CREATE_SRQ]    = ib_uverbs_create_srq,
	[IB_USER_VERBS_CMD_MODIFY_SRQ]    = ib_uverbs_modify_srq,
	[IB_USER_VERBS_CMD_DESTROY_SRQ]   = ib_uverbs_destroy_srq,
};

static struct vfsmount *uverbs_event_mnt;

static void ib_uverbs_add_one(struct ib_device *device);
static void ib_uverbs_remove_one(struct ib_device *device);

static int ib_dealloc_ucontext(struct ib_ucontext *context)
{
	struct ib_uobject *uobj, *tmp;

	if (!context)
		return 0;

	down(&ib_uverbs_idr_mutex);

	/* XXX Free AHs */

	list_for_each_entry_safe(uobj, tmp, &context->qp_list, list) {
		struct ib_qp *qp = idr_find(&ib_uverbs_qp_idr, uobj->id);
		idr_remove(&ib_uverbs_qp_idr, uobj->id);
		ib_destroy_qp(qp);
		list_del(&uobj->list);
		kfree(uobj);
	}

	list_for_each_entry_safe(uobj, tmp, &context->cq_list, list) {
		struct ib_cq *cq = idr_find(&ib_uverbs_cq_idr, uobj->id);
		idr_remove(&ib_uverbs_cq_idr, uobj->id);
		ib_destroy_cq(cq);
		list_del(&uobj->list);
		kfree(uobj);
	}

	list_for_each_entry_safe(uobj, tmp, &context->srq_list, list) {
		struct ib_srq *srq = idr_find(&ib_uverbs_srq_idr, uobj->id);
		idr_remove(&ib_uverbs_srq_idr, uobj->id);
		ib_destroy_srq(srq);
		list_del(&uobj->list);
		kfree(uobj);
	}

	/* XXX Free MWs */

	list_for_each_entry_safe(uobj, tmp, &context->mr_list, list) {
		struct ib_mr *mr = idr_find(&ib_uverbs_mr_idr, uobj->id);
		struct ib_device *mrdev = mr->device;
		struct ib_umem_object *memobj;

		idr_remove(&ib_uverbs_mr_idr, uobj->id);
		ib_dereg_mr(mr);

		memobj = container_of(uobj, struct ib_umem_object, uobject);
		ib_umem_release_on_close(mrdev, &memobj->umem);

		list_del(&uobj->list);
		kfree(memobj);
	}

	list_for_each_entry_safe(uobj, tmp, &context->pd_list, list) {
		struct ib_pd *pd = idr_find(&ib_uverbs_pd_idr, uobj->id);
		idr_remove(&ib_uverbs_pd_idr, uobj->id);
		ib_dealloc_pd(pd);
		list_del(&uobj->list);
		kfree(uobj);
	}

	up(&ib_uverbs_idr_mutex);

	return context->device->dealloc_ucontext(context);
}

static void ib_uverbs_release_file(struct kref *ref)
{
	struct ib_uverbs_file *file =
		container_of(ref, struct ib_uverbs_file, ref);

	module_put(file->device->ib_dev->owner);
	kfree(file);
}

static ssize_t ib_uverbs_event_read(struct file *filp, char __user *buf,
				    size_t count, loff_t *pos)
{
	struct ib_uverbs_event_file *file = filp->private_data;
	void *event;
	int eventsz;
	int ret = 0;

	spin_lock_irq(&file->lock);

	while (list_empty(&file->event_list) && file->fd >= 0) {
		spin_unlock_irq(&file->lock);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		if (wait_event_interruptible(file->poll_wait,
					     !list_empty(&file->event_list) ||
					     file->fd < 0))
			return -ERESTARTSYS;

		spin_lock_irq(&file->lock);
	}

	if (file->fd < 0) {
		spin_unlock_irq(&file->lock);
		return -ENODEV;
	}

	if (file->is_async) {
		event   = list_entry(file->event_list.next,
				     struct ib_uverbs_async_event, list);
		eventsz = sizeof (struct ib_uverbs_async_event_desc);
	} else {
		event   = list_entry(file->event_list.next,
				     struct ib_uverbs_comp_event, list);
		eventsz = sizeof (struct ib_uverbs_comp_event_desc);
	}

	if (eventsz > count) {
		ret   = -EINVAL;
		event = NULL;
	} else
		list_del(file->event_list.next);

	spin_unlock_irq(&file->lock);

	if (event) {
		if (copy_to_user(buf, event, eventsz))
			ret = -EFAULT;
		else
			ret = eventsz;
	}

	kfree(event);

	return ret;
}

static unsigned int ib_uverbs_event_poll(struct file *filp,
					 struct poll_table_struct *wait)
{
	unsigned int pollflags = 0;
	struct ib_uverbs_event_file *file = filp->private_data;

	poll_wait(filp, &file->poll_wait, wait);

	spin_lock_irq(&file->lock);
	if (file->fd < 0)
		pollflags = POLLERR;
	else if (!list_empty(&file->event_list))
		pollflags = POLLIN | POLLRDNORM;
	spin_unlock_irq(&file->lock);

	return pollflags;
}

static void ib_uverbs_event_release(struct ib_uverbs_event_file *file)
{
	struct list_head *entry, *tmp;

	spin_lock_irq(&file->lock);
	if (file->fd != -1) {
		file->fd = -1;
		list_for_each_safe(entry, tmp, &file->event_list)
			if (file->is_async)
				kfree(list_entry(entry, struct ib_uverbs_async_event, list));
			else
				kfree(list_entry(entry, struct ib_uverbs_comp_event, list));
	}
	spin_unlock_irq(&file->lock);
}

static int ib_uverbs_event_fasync(int fd, struct file *filp, int on)
{
	struct ib_uverbs_event_file *file = filp->private_data;

	return fasync_helper(fd, filp, on, &file->async_queue);
}

static int ib_uverbs_event_close(struct inode *inode, struct file *filp)
{
	struct ib_uverbs_event_file *file = filp->private_data;

	ib_uverbs_event_release(file);
	ib_uverbs_event_fasync(-1, filp, 0);
	kref_put(&file->uverbs_file->ref, ib_uverbs_release_file);

	return 0;
}

static struct file_operations uverbs_event_fops = {
	/*
	 * No .owner field since we artificially create event files,
	 * so there is no increment to the module reference count in
	 * the open path.  All event files come from a uverbs command
	 * file, which already takes a module reference, so this is OK.
	 */
	.read 	 = ib_uverbs_event_read,
	.poll    = ib_uverbs_event_poll,
	.release = ib_uverbs_event_close,
	.fasync  = ib_uverbs_event_fasync
};

void ib_uverbs_comp_handler(struct ib_cq *cq, void *cq_context)
{
	struct ib_uverbs_file       *file = cq_context;
	struct ib_uverbs_comp_event *entry;
	unsigned long                flags;

	entry = kmalloc(sizeof *entry, GFP_ATOMIC);
	if (!entry)
		return;

	entry->desc.cq_handle = cq->uobject->user_handle;

	spin_lock_irqsave(&file->comp_file[0].lock, flags);
	list_add_tail(&entry->list, &file->comp_file[0].event_list);
	spin_unlock_irqrestore(&file->comp_file[0].lock, flags);

	wake_up_interruptible(&file->comp_file[0].poll_wait);
	kill_fasync(&file->comp_file[0].async_queue, SIGIO, POLL_IN);
}

static void ib_uverbs_async_handler(struct ib_uverbs_file *file,
				    __u64 element, __u64 event)
{
	struct ib_uverbs_async_event *entry;
	unsigned long flags;

	entry = kmalloc(sizeof *entry, GFP_ATOMIC);
	if (!entry)
		return;

	entry->desc.element    = element;
	entry->desc.event_type = event;

	spin_lock_irqsave(&file->async_file.lock, flags);
	list_add_tail(&entry->list, &file->async_file.event_list);
	spin_unlock_irqrestore(&file->async_file.lock, flags);

	wake_up_interruptible(&file->async_file.poll_wait);
	kill_fasync(&file->async_file.async_queue, SIGIO, POLL_IN);
}

void ib_uverbs_cq_event_handler(struct ib_event *event, void *context_ptr)
{
	ib_uverbs_async_handler(context_ptr,
				event->element.cq->uobject->user_handle,
				event->event);
}

void ib_uverbs_qp_event_handler(struct ib_event *event, void *context_ptr)
{
	ib_uverbs_async_handler(context_ptr,
				event->element.qp->uobject->user_handle,
				event->event);
}

void ib_uverbs_srq_event_handler(struct ib_event *event, void *context_ptr)
{
	ib_uverbs_async_handler(context_ptr,
				event->element.srq->uobject->user_handle,
				event->event);
}

static void ib_uverbs_event_handler(struct ib_event_handler *handler,
				    struct ib_event *event)
{
	struct ib_uverbs_file *file =
		container_of(handler, struct ib_uverbs_file, event_handler);

	ib_uverbs_async_handler(file, event->element.port_num, event->event);
}

static int ib_uverbs_event_init(struct ib_uverbs_event_file *file,
				struct ib_uverbs_file *uverbs_file)
{
	struct file *filp;

	spin_lock_init(&file->lock);
	INIT_LIST_HEAD(&file->event_list);
	init_waitqueue_head(&file->poll_wait);
	file->uverbs_file = uverbs_file;
	file->async_queue = NULL;

	file->fd = get_unused_fd();
	if (file->fd < 0)
		return file->fd;

	filp = get_empty_filp();
	if (!filp) {
		put_unused_fd(file->fd);
		return -ENFILE;
	}

	filp->f_op 	   = &uverbs_event_fops;
	filp->f_vfsmnt 	   = mntget(uverbs_event_mnt);
	filp->f_dentry 	   = dget(uverbs_event_mnt->mnt_root);
	filp->f_mapping    = filp->f_dentry->d_inode->i_mapping;
	filp->f_flags      = O_RDONLY;
	filp->f_mode       = FMODE_READ;
	filp->private_data = file;

	fd_install(file->fd, filp);

	return 0;
}

static ssize_t ib_uverbs_write(struct file *filp, const char __user *buf,
			     size_t count, loff_t *pos)
{
	struct ib_uverbs_file *file = filp->private_data;
	struct ib_uverbs_cmd_hdr hdr;

	if (count < sizeof hdr)
		return -EINVAL;

	if (copy_from_user(&hdr, buf, sizeof hdr))
		return -EFAULT;

	if (hdr.in_words * 4 != count)
		return -EINVAL;

	if (hdr.command < 0 || hdr.command >= ARRAY_SIZE(uverbs_cmd_table))
		return -EINVAL;

	if (!file->ucontext                               &&
	    hdr.command != IB_USER_VERBS_CMD_QUERY_PARAMS &&
	    hdr.command != IB_USER_VERBS_CMD_GET_CONTEXT)
		return -EINVAL;

	return uverbs_cmd_table[hdr.command](file, buf + sizeof hdr,
					     hdr.in_words * 4, hdr.out_words * 4);
}

static int ib_uverbs_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct ib_uverbs_file *file = filp->private_data;

	if (!file->ucontext)
		return -ENODEV;
	else
		return file->device->ib_dev->mmap(file->ucontext, vma);
}

static int ib_uverbs_open(struct inode *inode, struct file *filp)
{
	struct ib_uverbs_device *dev =
		container_of(inode->i_cdev, struct ib_uverbs_device, dev);
	struct ib_uverbs_file *file;
	int i = 0;
	int ret;

	if (!try_module_get(dev->ib_dev->owner))
		return -ENODEV;

	file = kmalloc(sizeof *file +
		       (dev->num_comp - 1) * sizeof (struct ib_uverbs_event_file),
		       GFP_KERNEL);
	if (!file)
		return -ENOMEM;

	file->device = dev;
	kref_init(&file->ref);

	file->ucontext = NULL;

	ret = ib_uverbs_event_init(&file->async_file, file);
	if (ret)
		goto err;

	file->async_file.is_async = 1;

	kref_get(&file->ref);

	for (i = 0; i < dev->num_comp; ++i) {
		ret = ib_uverbs_event_init(&file->comp_file[i], file);
		if (ret)
			goto err_async;
		kref_get(&file->ref);
		file->comp_file[i].is_async = 0;
	}


	filp->private_data = file;

	INIT_IB_EVENT_HANDLER(&file->event_handler, dev->ib_dev,
			      ib_uverbs_event_handler);
	if (ib_register_event_handler(&file->event_handler))
		goto err_async;

	return 0;

err_async:
	while (i--)
		ib_uverbs_event_release(&file->comp_file[i]);

	ib_uverbs_event_release(&file->async_file);

err:
	kref_put(&file->ref, ib_uverbs_release_file);

	return ret;
}

static int ib_uverbs_close(struct inode *inode, struct file *filp)
{
	struct ib_uverbs_file *file = filp->private_data;
	int i;

	ib_unregister_event_handler(&file->event_handler);
	ib_uverbs_event_release(&file->async_file);
	ib_dealloc_ucontext(file->ucontext);

	for (i = 0; i < file->device->num_comp; ++i)
		ib_uverbs_event_release(&file->comp_file[i]);

	kref_put(&file->ref, ib_uverbs_release_file);

	return 0;
}

static struct file_operations uverbs_fops = {
	.owner 	 = THIS_MODULE,
	.write 	 = ib_uverbs_write,
	.open 	 = ib_uverbs_open,
	.release = ib_uverbs_close
};

static struct file_operations uverbs_mmap_fops = {
	.owner 	 = THIS_MODULE,
	.write 	 = ib_uverbs_write,
	.mmap    = ib_uverbs_mmap,
	.open 	 = ib_uverbs_open,
	.release = ib_uverbs_close
};

static struct ib_client uverbs_client = {
	.name   = "uverbs",
	.add    = ib_uverbs_add_one,
	.remove = ib_uverbs_remove_one
};

static ssize_t show_ibdev(struct class_device *class_dev, char *buf)
{
	struct ib_uverbs_device *dev =
		container_of(class_dev, struct ib_uverbs_device, class_dev);

	return sprintf(buf, "%s\n", dev->ib_dev->name);
}
static CLASS_DEVICE_ATTR(ibdev, S_IRUGO, show_ibdev, NULL);

static void ib_uverbs_release_class_dev(struct class_device *class_dev)
{
	struct ib_uverbs_device *dev =
		container_of(class_dev, struct ib_uverbs_device, class_dev);

	cdev_del(&dev->dev);
	clear_bit(dev->devnum, dev_map);
	kfree(dev);
}

static struct class uverbs_class = {
	.name    = "infiniband_verbs",
	.release = ib_uverbs_release_class_dev
};

static ssize_t show_abi_version(struct class *class, char *buf)
{
	return sprintf(buf, "%d\n", IB_USER_VERBS_ABI_VERSION);
}
static CLASS_ATTR(abi_version, S_IRUGO, show_abi_version, NULL);

static void ib_uverbs_add_one(struct ib_device *device)
{
	struct ib_uverbs_device *uverbs_dev;

	if (!device->alloc_ucontext)
		return;

	uverbs_dev = kmalloc(sizeof *uverbs_dev, GFP_KERNEL);
	if (!uverbs_dev)
		return;

	memset(uverbs_dev, 0, sizeof *uverbs_dev);

	spin_lock(&map_lock);
	uverbs_dev->devnum = find_first_zero_bit(dev_map, IB_UVERBS_MAX_DEVICES);
	if (uverbs_dev->devnum >= IB_UVERBS_MAX_DEVICES) {
		spin_unlock(&map_lock);
		goto err;
	}
	set_bit(uverbs_dev->devnum, dev_map);
	spin_unlock(&map_lock);

	uverbs_dev->ib_dev   = device;
	uverbs_dev->num_comp = 1;

	if (device->mmap)
		cdev_init(&uverbs_dev->dev, &uverbs_mmap_fops);
	else
		cdev_init(&uverbs_dev->dev, &uverbs_fops);
	uverbs_dev->dev.owner = THIS_MODULE;
	kobject_set_name(&uverbs_dev->dev.kobj, "uverbs%d", uverbs_dev->devnum);
	if (cdev_add(&uverbs_dev->dev, IB_UVERBS_BASE_DEV + uverbs_dev->devnum, 1))
		goto err;

	uverbs_dev->class_dev.class = &uverbs_class;
	uverbs_dev->class_dev.dev   = device->dma_device;
	uverbs_dev->class_dev.devt  = uverbs_dev->dev.dev;
	snprintf(uverbs_dev->class_dev.class_id, BUS_ID_SIZE, "uverbs%d", uverbs_dev->devnum);
	if (class_device_register(&uverbs_dev->class_dev))
		goto err_cdev;

	if (class_device_create_file(&uverbs_dev->class_dev, &class_device_attr_ibdev))
		goto err_class;

	ib_set_client_data(device, &uverbs_client, uverbs_dev);

	return;

err_class:
	class_device_unregister(&uverbs_dev->class_dev);

err_cdev:
	cdev_del(&uverbs_dev->dev);
	clear_bit(uverbs_dev->devnum, dev_map);

err:
	kfree(uverbs_dev);
	return;
}

static void ib_uverbs_remove_one(struct ib_device *device)
{
	struct ib_uverbs_device *uverbs_dev = ib_get_client_data(device, &uverbs_client);

	if (!uverbs_dev)
		return;

	class_device_unregister(&uverbs_dev->class_dev);
}

static struct super_block *uverbs_event_get_sb(struct file_system_type *fs_type, int flags,
					       const char *dev_name, void *data)
{
	return get_sb_pseudo(fs_type, "infinibandevent:", NULL,
			     INFINIBANDEVENTFS_MAGIC);
}

static struct file_system_type uverbs_event_fs = {
	/* No owner field so module can be unloaded */
	.name    = "infinibandeventfs",
	.get_sb  = uverbs_event_get_sb,
	.kill_sb = kill_litter_super
};

static int __init ib_uverbs_init(void)
{
	int ret;

	spin_lock_init(&map_lock);

	ret = register_chrdev_region(IB_UVERBS_BASE_DEV, IB_UVERBS_MAX_DEVICES,
				     "infiniband_verbs");
	if (ret) {
		printk(KERN_ERR "user_verbs: couldn't register device number\n");
		goto out;
	}

	ret = class_register(&uverbs_class);
	if (ret) {
		printk(KERN_ERR "user_verbs: couldn't create class infiniband_verbs\n");
		goto out_chrdev;
	}

	ret = class_create_file(&uverbs_class, &class_attr_abi_version);
	if (ret) {
		printk(KERN_ERR "user_verbs: couldn't create abi_version attribute\n");
		goto out_class;
	}

	ret = register_filesystem(&uverbs_event_fs);
	if (ret) {
		printk(KERN_ERR "user_verbs: couldn't register infinibandeventfs\n");
		goto out_class;
	}

	uverbs_event_mnt = kern_mount(&uverbs_event_fs);
	if (IS_ERR(uverbs_event_mnt)) {
		ret = PTR_ERR(uverbs_event_mnt);
		printk(KERN_ERR "user_verbs: couldn't mount infinibandeventfs\n");
		goto out_fs;
	}

	ret = ib_register_client(&uverbs_client);
	if (ret) {
		printk(KERN_ERR "user_verbs: couldn't register client\n");
		goto out_mnt;
	}

	return 0;

out_mnt:
	mntput(uverbs_event_mnt);

out_fs:
	unregister_filesystem(&uverbs_event_fs);

out_class:
	class_unregister(&uverbs_class);

out_chrdev:
	unregister_chrdev_region(IB_UVERBS_BASE_DEV, IB_UVERBS_MAX_DEVICES);

out:
	return ret;
}

static void __exit ib_uverbs_cleanup(void)
{
	ib_unregister_client(&uverbs_client);
	mntput(uverbs_event_mnt);
	unregister_filesystem(&uverbs_event_fs);
	class_unregister(&uverbs_class);
	unregister_chrdev_region(IB_UVERBS_BASE_DEV, IB_UVERBS_MAX_DEVICES);
}

module_init(ib_uverbs_init);
module_exit(ib_uverbs_cleanup);
