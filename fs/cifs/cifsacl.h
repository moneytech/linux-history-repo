/*
 *   fs/cifs/cifsacl.h
 *
 *   Copyright (c) International Business Machines  Corp., 2007
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _CIFSACL_H
#define _CIFSACL_H


#define NUM_WK_SIDS 7 /* number of well known sids */
#define SIDNAMELENGTH 20 /* long enough for the ones we care about */

struct cifs_ntsd {
	__u16 revision; /* revision level */
	__u16 type;
	__u32 osidoffset;
	__u32 gsidoffset;
	__u32 sacloffset;
	__u32 dacloffset;
} __attribute__((packed));

struct cifs_sid {
	__u8 revision; /* revision level */
	__u8 num_subauth;
	__u8 authority[6];
	__u32 sub_auth[5]; /* sub_auth[num_subauth] */
} __attribute__((packed));

struct cifs_acl {
	__u16 revision; /* revision level */
	__u16 size;
	__u32 num_aces;
} __attribute__((packed));

struct cifs_ntace { /* first part of ACE which contains perms */
	__u8 type;
	__u8 flags;
	__u16 size;
	__u32 access_req;
} __attribute__((packed));

struct cifs_ace { /* last part of ACE which includes user info */
	__u8 revision; /* revision level */
	__u8 num_subauth;
	__u8 authority[6];
	__u32 sub_auth[5];
} __attribute__((packed));

struct cifs_wksid {
	struct cifs_sid cifssid;
	char sidname[SIDNAMELENGTH];
} __attribute__((packed));

#ifdef CONFIG_CIFS_EXPERIMENTAL

extern struct cifs_wksid wksidarr[NUM_WK_SIDS];
extern int match_sid(struct cifs_sid *);
extern int compare_sids(struct cifs_sid *, struct cifs_sid *);

#endif /*  CONFIG_CIFS_EXPERIMENTAL */

#endif /* _CIFSACL_H */
