/* memcontrol.h - Memory Controller
 *
 * Copyright IBM Corporation, 2007
 * Author Balbir Singh <balbir@linux.vnet.ibm.com>
 *
 * Copyright 2007 OpenVZ SWsoft Inc
 * Author: Pavel Emelianov <xemul@openvz.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_MEMCONTROL_H
#define _LINUX_MEMCONTROL_H

struct mem_cgroup;
struct page_cgroup;

#ifdef CONFIG_CGROUP_MEM_CONT

extern void mm_init_cgroup(struct mm_struct *mm, struct task_struct *p);
extern void mm_free_cgroup(struct mm_struct *mm);
extern void page_assign_page_cgroup(struct page *page,
					struct page_cgroup *pc);
extern struct page_cgroup *page_get_page_cgroup(struct page *page);

#else /* CONFIG_CGROUP_MEM_CONT */
static inline void mm_init_cgroup(struct mm_struct *mm,
					struct task_struct *p)
{
}

static inline void mm_free_cgroup(struct mm_struct *mm)
{
}

static inline void page_assign_page_cgroup(struct page *page,
						struct page_cgroup *pc)
{
}

static inline struct page_cgroup *page_get_page_cgroup(struct page *page)
{
	return NULL;
}

#endif /* CONFIG_CGROUP_MEM_CONT */

#endif /* _LINUX_MEMCONTROL_H */

