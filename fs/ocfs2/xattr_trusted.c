/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * xattr_trusted.c
 *
 * Copyright (C) 2008 Oracle.  All rights reserved.
 *
 * CREDITS:
 * Lots of code in this file is taken from ext3.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>

#define MLOG_MASK_PREFIX ML_INODE
#include <cluster/masklog.h>

#include "ocfs2.h"
#include "alloc.h"
#include "dlmglue.h"
#include "file.h"
#include "ocfs2_fs.h"
#include "xattr.h"

#define XATTR_TRUSTED_PREFIX "trusted."

static size_t ocfs2_xattr_trusted_list(struct inode *inode, char *list,
				       size_t list_size, const char *name,
				       size_t name_len)
{
	const size_t prefix_len = sizeof(XATTR_TRUSTED_PREFIX) - 1;
	const size_t total_len = prefix_len + name_len + 1;

	if (list && total_len <= list_size) {
		memcpy(list, XATTR_TRUSTED_PREFIX, prefix_len);
		memcpy(list + prefix_len, name, name_len);
		list[prefix_len + name_len] = '\0';
	}
	return total_len;
}

static int ocfs2_xattr_trusted_get(struct inode *inode, const char *name,
				   void *buffer, size_t size)
{
	if (strcmp(name, "") == 0)
		return -EINVAL;
	return ocfs2_xattr_get(inode, OCFS2_XATTR_INDEX_TRUSTED, name,
			       buffer, size);
}

static int ocfs2_xattr_trusted_set(struct inode *inode, const char *name,
				   const void *value, size_t size, int flags)
{
	if (strcmp(name, "") == 0)
		return -EINVAL;

	return ocfs2_xattr_set(inode, OCFS2_XATTR_INDEX_TRUSTED, name, value,
			       size, flags);
}

struct xattr_handler ocfs2_xattr_trusted_handler = {
	.prefix	= XATTR_TRUSTED_PREFIX,
	.list	= ocfs2_xattr_trusted_list,
	.get	= ocfs2_xattr_trusted_get,
	.set	= ocfs2_xattr_trusted_set,
};
