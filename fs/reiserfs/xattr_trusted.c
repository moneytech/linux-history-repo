#include <linux/reiserfs_fs.h>
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/xattr.h>
#include <linux/reiserfs_xattr.h>
#include <asm/uaccess.h>

static int
trusted_get(struct inode *inode, const char *name, void *buffer, size_t size)
{
	if (strlen(name) < sizeof(XATTR_TRUSTED_PREFIX))
		return -EINVAL;

	if (!capable(CAP_SYS_ADMIN) || IS_PRIVATE(inode))
		return -EPERM;

	return reiserfs_xattr_get(inode, name, buffer, size);
}

static int
trusted_set(struct inode *inode, const char *name, const void *buffer,
	    size_t size, int flags)
{
	if (strlen(name) < sizeof(XATTR_TRUSTED_PREFIX))
		return -EINVAL;

	if (!capable(CAP_SYS_ADMIN) || IS_PRIVATE(inode))
		return -EPERM;

	return reiserfs_xattr_set(inode, name, buffer, size, flags);
}

static size_t trusted_list(struct inode *inode, char *list, size_t list_size,
			   const char *name, size_t name_len)
{
	const size_t len = name_len + 1;

	if (!capable(CAP_SYS_ADMIN) || IS_PRIVATE(inode))
		return 0;

	if (list && len <= list_size) {
		memcpy(list, name, name_len);
		list[name_len] = '\0';
	}
	return len;
}

struct xattr_handler reiserfs_xattr_trusted_handler = {
	.prefix = XATTR_TRUSTED_PREFIX,
	.get = trusted_get,
	.set = trusted_set,
	.list = trusted_list,
};
