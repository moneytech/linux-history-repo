/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "xfs.h"
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_trans.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_attr_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_alloc.h"
#include "xfs_btree.h"
#include "xfs_error.h"
#include "xfs_rw.h"
#include "xfs_iomap.h"
#include <linux/mpage.h>
#include <linux/writeback.h>

STATIC void xfs_count_page_state(struct page *, int *, int *, int *);

#if defined(XFS_RW_TRACE)
void
xfs_page_trace(
	int		tag,
	struct inode	*inode,
	struct page	*page,
	int		mask)
{
	xfs_inode_t	*ip;
	bhv_desc_t	*bdp;
	vnode_t		*vp = LINVFS_GET_VP(inode);
	loff_t		isize = i_size_read(inode);
	loff_t		offset = page_offset(page);
	int		delalloc = -1, unmapped = -1, unwritten = -1;

	if (page_has_buffers(page))
		xfs_count_page_state(page, &delalloc, &unmapped, &unwritten);

	bdp = vn_bhv_lookup(VN_BHV_HEAD(vp), &xfs_vnodeops);
	ip = XFS_BHVTOI(bdp);
	if (!ip->i_rwtrace)
		return;

	ktrace_enter(ip->i_rwtrace,
		(void *)((unsigned long)tag),
		(void *)ip,
		(void *)inode,
		(void *)page,
		(void *)((unsigned long)mask),
		(void *)((unsigned long)((ip->i_d.di_size >> 32) & 0xffffffff)),
		(void *)((unsigned long)(ip->i_d.di_size & 0xffffffff)),
		(void *)((unsigned long)((isize >> 32) & 0xffffffff)),
		(void *)((unsigned long)(isize & 0xffffffff)),
		(void *)((unsigned long)((offset >> 32) & 0xffffffff)),
		(void *)((unsigned long)(offset & 0xffffffff)),
		(void *)((unsigned long)delalloc),
		(void *)((unsigned long)unmapped),
		(void *)((unsigned long)unwritten),
		(void *)NULL,
		(void *)NULL);
}
#else
#define xfs_page_trace(tag, inode, page, mask)
#endif

/*
 * Schedule IO completion handling on a xfsdatad if this was
 * the final hold on this ioend.
 */
STATIC void
xfs_finish_ioend(
	xfs_ioend_t		*ioend)
{
	if (atomic_dec_and_test(&ioend->io_remaining))
		queue_work(xfsdatad_workqueue, &ioend->io_work);
}

/*
 * We're now finished for good with this ioend structure.
 * Update the page state via the associated buffer_heads,
 * release holds on the inode and bio, and finally free
 * up memory.  Do not use the ioend after this.
 */
STATIC void
xfs_destroy_ioend(
	xfs_ioend_t		*ioend)
{
	struct buffer_head	*bh, *next;

	for (bh = ioend->io_buffer_head; bh; bh = next) {
		next = bh->b_private;
		bh->b_end_io(bh, ioend->io_uptodate);
	}

	vn_iowake(ioend->io_vnode);
	mempool_free(ioend, xfs_ioend_pool);
}

/*
 * Buffered IO write completion for delayed allocate extents.
 * TODO: Update ondisk isize now that we know the file data
 * has been flushed (i.e. the notorious "NULL file" problem).
 */
STATIC void
xfs_end_bio_delalloc(
	void			*data)
{
	xfs_ioend_t		*ioend = data;

	xfs_destroy_ioend(ioend);
}

/*
 * Buffered IO write completion for regular, written extents.
 */
STATIC void
xfs_end_bio_written(
	void			*data)
{
	xfs_ioend_t		*ioend = data;

	xfs_destroy_ioend(ioend);
}

/*
 * IO write completion for unwritten extents.
 *
 * Issue transactions to convert a buffer range from unwritten
 * to written extents.
 */
STATIC void
xfs_end_bio_unwritten(
	void			*data)
{
	xfs_ioend_t		*ioend = data;
	vnode_t			*vp = ioend->io_vnode;
	xfs_off_t		offset = ioend->io_offset;
	size_t			size = ioend->io_size;
	int			error;

	if (ioend->io_uptodate)
		VOP_BMAP(vp, offset, size, BMAPI_UNWRITTEN, NULL, NULL, error);
	xfs_destroy_ioend(ioend);
}

/*
 * Allocate and initialise an IO completion structure.
 * We need to track unwritten extent write completion here initially.
 * We'll need to extend this for updating the ondisk inode size later
 * (vs. incore size).
 */
STATIC xfs_ioend_t *
xfs_alloc_ioend(
	struct inode		*inode,
	unsigned int		type)
{
	xfs_ioend_t		*ioend;

	ioend = mempool_alloc(xfs_ioend_pool, GFP_NOFS);

	/*
	 * Set the count to 1 initially, which will prevent an I/O
	 * completion callback from happening before we have started
	 * all the I/O from calling the completion routine too early.
	 */
	atomic_set(&ioend->io_remaining, 1);
	ioend->io_uptodate = 1; /* cleared if any I/O fails */
	ioend->io_list = NULL;
	ioend->io_type = type;
	ioend->io_vnode = LINVFS_GET_VP(inode);
	ioend->io_buffer_head = NULL;
	ioend->io_buffer_tail = NULL;
	atomic_inc(&ioend->io_vnode->v_iocount);
	ioend->io_offset = 0;
	ioend->io_size = 0;

	if (type == IOMAP_UNWRITTEN)
		INIT_WORK(&ioend->io_work, xfs_end_bio_unwritten, ioend);
	else if (type == IOMAP_DELAY)
		INIT_WORK(&ioend->io_work, xfs_end_bio_delalloc, ioend);
	else
		INIT_WORK(&ioend->io_work, xfs_end_bio_written, ioend);

	return ioend;
}

STATIC int
xfs_map_blocks(
	struct inode		*inode,
	loff_t			offset,
	ssize_t			count,
	xfs_iomap_t		*mapp,
	int			flags)
{
	vnode_t			*vp = LINVFS_GET_VP(inode);
	int			error, nmaps = 1;

	VOP_BMAP(vp, offset, count, flags, mapp, &nmaps, error);
	if (!error && (flags & (BMAPI_WRITE|BMAPI_ALLOCATE)))
		VMODIFY(vp);
	return -error;
}

/*
 * Finds the corresponding mapping in block @map array of the
 * given @offset within a @page.
 */
STATIC xfs_iomap_t *
xfs_offset_to_map(
	struct page		*page,
	xfs_iomap_t		*iomapp,
	unsigned long		offset)
{
	xfs_off_t		full_offset;	/* offset from start of file */

	ASSERT(offset < PAGE_CACHE_SIZE);

	full_offset = page->index;		/* NB: using 64bit number */
	full_offset <<= PAGE_CACHE_SHIFT;	/* offset from file start */
	full_offset += offset;			/* offset from page start */

	if (full_offset < iomapp->iomap_offset)
		return NULL;
	if (iomapp->iomap_offset + (iomapp->iomap_bsize -1) >= full_offset)
		return iomapp;
	return NULL;
}

/*
 * BIO completion handler for buffered IO.
 */
STATIC int
xfs_end_bio(
	struct bio		*bio,
	unsigned int		bytes_done,
	int			error)
{
	xfs_ioend_t		*ioend = bio->bi_private;

	if (bio->bi_size)
		return 1;

	ASSERT(ioend);
	ASSERT(atomic_read(&bio->bi_cnt) >= 1);

	/* Toss bio and pass work off to an xfsdatad thread */
	if (!test_bit(BIO_UPTODATE, &bio->bi_flags))
		ioend->io_uptodate = 0;
	bio->bi_private = NULL;
	bio->bi_end_io = NULL;

	bio_put(bio);
	xfs_finish_ioend(ioend);
	return 0;
}

STATIC void
xfs_submit_ioend_bio(
	xfs_ioend_t	*ioend,
	struct bio	*bio)
{
	atomic_inc(&ioend->io_remaining);

	bio->bi_private = ioend;
	bio->bi_end_io = xfs_end_bio;

	submit_bio(WRITE, bio);
	ASSERT(!bio_flagged(bio, BIO_EOPNOTSUPP));
	bio_put(bio);
}

STATIC struct bio *
xfs_alloc_ioend_bio(
	struct buffer_head	*bh)
{
	struct bio		*bio;
	int			nvecs = bio_get_nr_vecs(bh->b_bdev);

	do {
		bio = bio_alloc(GFP_NOIO, nvecs);
		nvecs >>= 1;
	} while (!bio);

	ASSERT(bio->bi_private == NULL);
	bio->bi_sector = bh->b_blocknr * (bh->b_size >> 9);
	bio->bi_bdev = bh->b_bdev;
	bio_get(bio);
	return bio;
}

STATIC void
xfs_start_buffer_writeback(
	struct buffer_head	*bh)
{
	ASSERT(buffer_mapped(bh));
	ASSERT(buffer_locked(bh));
	ASSERT(!buffer_delay(bh));
	ASSERT(!buffer_unwritten(bh));

	mark_buffer_async_write(bh);
	set_buffer_uptodate(bh);
	clear_buffer_dirty(bh);
}

STATIC void
xfs_start_page_writeback(
	struct page		*page,
	struct writeback_control *wbc,
	int			clear_dirty,
	int			buffers)
{
	ASSERT(PageLocked(page));
	ASSERT(!PageWriteback(page));
	set_page_writeback(page);
	if (clear_dirty)
		clear_page_dirty(page);
	unlock_page(page);
	if (!buffers) {
		end_page_writeback(page);
		wbc->pages_skipped++;	/* We didn't write this page */
	}
}

static inline int bio_add_buffer(struct bio *bio, struct buffer_head *bh)
{
	return bio_add_page(bio, bh->b_page, bh->b_size, bh_offset(bh));
}

/*
 * Submit all of the bios for all of the ioends we have saved up,
 * covering the initial writepage page and also any probed pages.
 */
STATIC void
xfs_submit_ioend(
	xfs_ioend_t		*ioend)
{
	xfs_ioend_t		*next;
	struct buffer_head	*bh;
	struct bio		*bio;
	sector_t		lastblock = 0;

	do {
		next = ioend->io_list;
		bio = NULL;

		for (bh = ioend->io_buffer_head; bh; bh = bh->b_private) {
			xfs_start_buffer_writeback(bh);

			if (!bio) {
 retry:
				bio = xfs_alloc_ioend_bio(bh);
			} else if (bh->b_blocknr != lastblock + 1) {
				xfs_submit_ioend_bio(ioend, bio);
				goto retry;
			}

			if (bio_add_buffer(bio, bh) != bh->b_size) {
				xfs_submit_ioend_bio(ioend, bio);
				goto retry;
			}

			lastblock = bh->b_blocknr;
		}
		if (bio)
			xfs_submit_ioend_bio(ioend, bio);
		xfs_finish_ioend(ioend);
	} while ((ioend = next) != NULL);
}

/*
 * Cancel submission of all buffer_heads so far in this endio.
 * Toss the endio too.  Only ever called for the initial page
 * in a writepage request, so only ever one page.
 */
STATIC void
xfs_cancel_ioend(
	xfs_ioend_t		*ioend)
{
	xfs_ioend_t		*next;
	struct buffer_head	*bh, *next_bh;

	do {
		next = ioend->io_list;
		bh = ioend->io_buffer_head;
		do {
			next_bh = bh->b_private;
			clear_buffer_async_write(bh);
			unlock_buffer(bh);
		} while ((bh = next_bh) != NULL);

		vn_iowake(ioend->io_vnode);
		mempool_free(ioend, xfs_ioend_pool);
	} while ((ioend = next) != NULL);
}

/*
 * Test to see if we've been building up a completion structure for
 * earlier buffers -- if so, we try to append to this ioend if we
 * can, otherwise we finish off any current ioend and start another.
 * Return true if we've finished the given ioend.
 */
STATIC void
xfs_add_to_ioend(
	struct inode		*inode,
	struct buffer_head	*bh,
	unsigned int		p_offset,
	unsigned int		type,
	xfs_ioend_t		**result,
	int			need_ioend)
{
	xfs_ioend_t		*ioend = *result;

	if (!ioend || need_ioend || type != ioend->io_type) {
		xfs_ioend_t	*previous = *result;
		xfs_off_t	offset;

		offset = (xfs_off_t)bh->b_page->index << PAGE_CACHE_SHIFT;
		offset += p_offset;
		ioend = xfs_alloc_ioend(inode, type);
		ioend->io_offset = offset;
		ioend->io_buffer_head = bh;
		ioend->io_buffer_tail = bh;
		if (previous)
			previous->io_list = ioend;
		*result = ioend;
	} else {
		ioend->io_buffer_tail->b_private = bh;
		ioend->io_buffer_tail = bh;
	}

	bh->b_private = NULL;
	ioend->io_size += bh->b_size;
}

STATIC void
xfs_map_at_offset(
	struct page		*page,
	struct buffer_head	*bh,
	unsigned long		offset,
	int			block_bits,
	xfs_iomap_t		*iomapp,
	xfs_ioend_t		*ioend)
{
	xfs_daddr_t		bn;
	xfs_off_t		delta;
	int			sector_shift;

	ASSERT(!(iomapp->iomap_flags & IOMAP_HOLE));
	ASSERT(!(iomapp->iomap_flags & IOMAP_DELAY));
	ASSERT(iomapp->iomap_bn != IOMAP_DADDR_NULL);

	delta = page->index;
	delta <<= PAGE_CACHE_SHIFT;
	delta += offset;
	delta -= iomapp->iomap_offset;
	delta >>= block_bits;

	sector_shift = block_bits - BBSHIFT;
	bn = iomapp->iomap_bn >> sector_shift;
	bn += delta;
	BUG_ON(!bn && !(iomapp->iomap_flags & IOMAP_REALTIME));
	ASSERT((bn << sector_shift) >= iomapp->iomap_bn);

	lock_buffer(bh);
	bh->b_blocknr = bn;
	bh->b_bdev = iomapp->iomap_target->bt_bdev;
	set_buffer_mapped(bh);
	clear_buffer_delay(bh);
	clear_buffer_unwritten(bh);
}

/*
 * Look for a page at index which is unlocked and not mapped
 * yet - clustering for mmap write case.
 */
STATIC unsigned int
xfs_probe_unmapped_page(
	struct address_space	*mapping,
	pgoff_t			index,
	unsigned int		pg_offset)
{
	struct page		*page;
	int			ret = 0;

	page = find_trylock_page(mapping, index);
	if (!page)
		return 0;
	if (PageWriteback(page))
		goto out;

	if (page->mapping && PageDirty(page)) {
		if (page_has_buffers(page)) {
			struct buffer_head	*bh, *head;

			bh = head = page_buffers(page);
			do {
				if (buffer_mapped(bh) || !buffer_uptodate(bh))
					break;
				ret += bh->b_size;
				if (ret >= pg_offset)
					break;
			} while ((bh = bh->b_this_page) != head);
		} else
			ret = PAGE_CACHE_SIZE;
	}

out:
	unlock_page(page);
	return ret;
}

STATIC size_t
xfs_probe_unmapped_cluster(
	struct inode		*inode,
	struct page		*startpage,
	struct buffer_head	*bh,
	struct buffer_head	*head)
{
	size_t			len, total = 0;
	pgoff_t			tindex, tlast, tloff;
	unsigned int		pg_offset;
	struct address_space	*mapping = inode->i_mapping;

	/* First sum forwards in this page */
	do {
		if (buffer_mapped(bh))
			break;
		total += bh->b_size;
	} while ((bh = bh->b_this_page) != head);

	/* If we reached the end of the page, sum forwards in
	 * following pages.
	 */
	if (bh == head) {
		tlast = i_size_read(inode) >> PAGE_CACHE_SHIFT;
		/* Prune this back to avoid pathological behavior */
		tloff = min(tlast, startpage->index + 64);
		for (tindex = startpage->index + 1; tindex < tloff; tindex++) {
			len = xfs_probe_unmapped_page(mapping, tindex,
							PAGE_CACHE_SIZE);
			if (!len)
				return total;
			total += len;
		}
		if (tindex == tlast &&
		    (pg_offset = i_size_read(inode) & (PAGE_CACHE_SIZE - 1))) {
			total += xfs_probe_unmapped_page(mapping,
							tindex, pg_offset);
		}
	}
	return total;
}

/*
 * Probe for a given page (index) in the inode and test if it is suitable
 * for writing as part of an unwritten or delayed allocate extent.
 * Returns page locked and with an extra reference count if so, else NULL.
 */
STATIC struct page *
xfs_probe_delayed_page(
	struct inode		*inode,
	pgoff_t			index,
	unsigned int		type)
{
	struct page		*page;

	page = find_trylock_page(inode->i_mapping, index);
	if (!page)
		return NULL;
	if (PageWriteback(page))
		goto out;

	if (page->mapping && page_has_buffers(page)) {
		struct buffer_head	*bh, *head;
		int			acceptable = 0;

		bh = head = page_buffers(page);
		do {
			if (buffer_unwritten(bh))
				acceptable = (type == IOMAP_UNWRITTEN);
			else if (buffer_delay(bh))
				acceptable = (type == IOMAP_DELAY);
			else
				break;
		} while ((bh = bh->b_this_page) != head);

		if (acceptable)
			return page;
	}

out:
	unlock_page(page);
	return NULL;
}

/*
 * Allocate & map buffers for page given the extent map. Write it out.
 * except for the original page of a writepage, this is called on
 * delalloc/unwritten pages only, for the original page it is possible
 * that the page has no mapping at all.
 */
STATIC int
xfs_convert_page(
	struct inode		*inode,
	struct page		*page,
	xfs_iomap_t		*iomapp,
	xfs_ioend_t		**ioendp,
	struct writeback_control *wbc,
	void			*private,
	int			startio,
	int			all_bh)
{
	struct buffer_head	*bh, *head;
	xfs_iomap_t		*mp = iomapp, *tmp;
	unsigned long		p_offset, end_offset;
	unsigned int		type;
	int			bbits = inode->i_blkbits;
	int			len, page_dirty;
	int			count = 0, done = 0, uptodate = 1;

	end_offset = (i_size_read(inode) & (PAGE_CACHE_SIZE - 1));

	/*
	 * page_dirty is initially a count of buffers on the page before
	 * EOF and is decrememted as we move each into a cleanable state.
	 */
	len = 1 << inode->i_blkbits;
	end_offset = max(end_offset, PAGE_CACHE_SIZE);
	end_offset = roundup(end_offset, len);
	page_dirty = end_offset / len;

	p_offset = 0;
	bh = head = page_buffers(page);
	do {
		if (p_offset >= end_offset)
			break;
		if (!buffer_uptodate(bh))
			uptodate = 0;
		if (!(PageUptodate(page) || buffer_uptodate(bh))) {
			done = 1;
			continue;
		}

		if (buffer_unwritten(bh))
			type = IOMAP_UNWRITTEN;
		else if (buffer_delay(bh))
			type = IOMAP_DELAY;
		else {
			type = 0;
			if (!(buffer_mapped(bh) && all_bh && startio)) {
				done = 1;
			} else if (startio) {
				lock_buffer(bh);
				xfs_add_to_ioend(inode, bh, p_offset,
						type, ioendp, done);
				count++;
				page_dirty--;
			}
			continue;
		}
		tmp = xfs_offset_to_map(page, mp, p_offset);
		if (!tmp) {
			done = 1;
			continue;
		}
		ASSERT(!(tmp->iomap_flags & IOMAP_HOLE));
		ASSERT(!(tmp->iomap_flags & IOMAP_DELAY));

		xfs_map_at_offset(page, bh, p_offset, bbits, tmp, *ioendp);
		if (startio) {
			xfs_add_to_ioend(inode, bh, p_offset,
					type, ioendp, done);
			count++;
		} else {
			set_buffer_dirty(bh);
			unlock_buffer(bh);
			mark_buffer_dirty(bh);
		}
		page_dirty--;
	} while (p_offset += len, (bh = bh->b_this_page) != head);

	if (uptodate && bh == head)
		SetPageUptodate(page);

	if (startio) {
		if (count)
			wbc->nr_to_write--;
		xfs_start_page_writeback(page, wbc, !page_dirty, count);
	}

	return done;
}

/*
 * Convert & write out a cluster of pages in the same extent as defined
 * by mp and following the start page.
 */
STATIC void
xfs_cluster_write(
	struct inode		*inode,
	pgoff_t			tindex,
	xfs_iomap_t		*iomapp,
	xfs_ioend_t		**ioendp,
	struct writeback_control *wbc,
	int			startio,
	int			all_bh,
	pgoff_t			tlast)
{
	struct page		*page;
	unsigned int		type = (*ioendp)->io_type;
	int			done;

	for (done = 0; tindex <= tlast && !done; tindex++) {
		page = xfs_probe_delayed_page(inode, tindex, type);
		if (!page)
			break;
		done = xfs_convert_page(inode, page, iomapp, ioendp,
						wbc, NULL, startio, all_bh);
	}
}

/*
 * Calling this without startio set means we are being asked to make a dirty
 * page ready for freeing it's buffers.  When called with startio set then
 * we are coming from writepage.
 *
 * When called with startio set it is important that we write the WHOLE
 * page if possible.
 * The bh->b_state's cannot know if any of the blocks or which block for
 * that matter are dirty due to mmap writes, and therefore bh uptodate is
 * only vaild if the page itself isn't completely uptodate.  Some layers
 * may clear the page dirty flag prior to calling write page, under the
 * assumption the entire page will be written out; by not writing out the
 * whole page the page can be reused before all valid dirty data is
 * written out.  Note: in the case of a page that has been dirty'd by
 * mapwrite and but partially setup by block_prepare_write the
 * bh->b_states's will not agree and only ones setup by BPW/BCW will have
 * valid state, thus the whole page must be written out thing.
 */

STATIC int
xfs_page_state_convert(
	struct inode	*inode,
	struct page	*page,
	struct writeback_control *wbc,
	int		startio,
	int		unmapped) /* also implies page uptodate */
{
	struct buffer_head	*bh, *head;
	xfs_iomap_t		*iomp, iomap;
	xfs_ioend_t		*ioend = NULL, *iohead = NULL;
	loff_t			offset;
	unsigned long           p_offset = 0;
	unsigned int		type;
	__uint64_t              end_offset;
	pgoff_t                 end_index, last_index, tlast;
	int			flags, len, err, done = 1;
	int			uptodate = 1;
	int			page_dirty, count = 0, trylock_flag = 0;

	/* wait for other IO threads? */
	if (startio && wbc->sync_mode != WB_SYNC_NONE)
		trylock_flag |= BMAPI_TRYLOCK;

	/* Is this page beyond the end of the file? */
	offset = i_size_read(inode);
	end_index = offset >> PAGE_CACHE_SHIFT;
	last_index = (offset - 1) >> PAGE_CACHE_SHIFT;
	if (page->index >= end_index) {
		if ((page->index >= end_index + 1) ||
		    !(i_size_read(inode) & (PAGE_CACHE_SIZE - 1))) {
			if (startio)
				unlock_page(page);
			return 0;
		}
	}

	/*
	 * page_dirty is initially a count of buffers on the page before
	 * EOF and is decrememted as we move each into a cleanable state.
	 *
	 * Derivation:
	 *
	 * End offset is the highest offset that this page should represent.
	 * If we are on the last page, (end_offset & (PAGE_CACHE_SIZE - 1))
	 * will evaluate non-zero and be less than PAGE_CACHE_SIZE and
	 * hence give us the correct page_dirty count. On any other page,
	 * it will be zero and in that case we need page_dirty to be the
	 * count of buffers on the page.
 	 */
	end_offset = min_t(unsigned long long,
			(xfs_off_t)(page->index + 1) << PAGE_CACHE_SHIFT, offset);
	len = 1 << inode->i_blkbits;
	p_offset = min_t(unsigned long, end_offset & (PAGE_CACHE_SIZE - 1),
					PAGE_CACHE_SIZE);
	p_offset = p_offset ? roundup(p_offset, len) : PAGE_CACHE_SIZE;
	page_dirty = p_offset / len;

	iomp = NULL;
	bh = head = page_buffers(page);
	offset = page_offset(page);

	/* TODO: fix up "done" variable and iomap pointer (boolean) */
	/* TODO: cleanup count and page_dirty */

	do {
		if (offset >= end_offset)
			break;
		if (!buffer_uptodate(bh))
			uptodate = 0;
		if (!(PageUptodate(page) || buffer_uptodate(bh)) && !startio) {
			done = 1;
			continue;
		}

		if (iomp) {
			iomp = xfs_offset_to_map(page, &iomap, p_offset);
			done = (iomp == NULL);
		}

		/*
		 * First case, map an unwritten extent and prepare for
		 * extent state conversion transaction on completion.
		 *
		 * Second case, allocate space for a delalloc buffer.
		 * We can return EAGAIN here in the release page case.
		 */
		if (buffer_unwritten(bh) || buffer_delay(bh)) {
			if (buffer_unwritten(bh)) {
				type = IOMAP_UNWRITTEN;
				flags = BMAPI_WRITE|BMAPI_IGNSTATE;
			} else {
				type = IOMAP_DELAY;
				flags = BMAPI_ALLOCATE;
				if (!startio)
					flags |= trylock_flag;
			}

			if (!iomp) {
				done = 1;
				err = xfs_map_blocks(inode, offset, len, &iomap,
						flags);
				if (err)
					goto error;
				iomp = xfs_offset_to_map(page, &iomap,
								p_offset);
				done = (iomp == NULL);
			}
			if (iomp) {
				xfs_map_at_offset(page, bh, p_offset,
						inode->i_blkbits, iomp, ioend);
				if (startio) {
					xfs_add_to_ioend(inode, bh, p_offset,
						type, &ioend, done);
				} else {
					set_buffer_dirty(bh);
					unlock_buffer(bh);
					mark_buffer_dirty(bh);
				}
				page_dirty--;
				count++;
			} else {
				done = 1;
			}
		} else if ((buffer_uptodate(bh) || PageUptodate(page)) &&
			   (unmapped || startio)) {

			type = 0;
			if (!buffer_mapped(bh)) {

				/*
				 * Getting here implies an unmapped buffer
				 * was found, and we are in a path where we
				 * need to write the whole page out.
				 */
				if (!iomp) {
					int	size;

					size = xfs_probe_unmapped_cluster(
							inode, page, bh, head);
					err = xfs_map_blocks(inode, offset,
							size, &iomap,
							BMAPI_WRITE|BMAPI_MMAP);
					if (err) {
						goto error;
					}
					iomp = xfs_offset_to_map(page, &iomap,
								     p_offset);
					done = (iomp == NULL);
				}
				if (iomp) {
					xfs_map_at_offset(page, bh, p_offset,
							inode->i_blkbits, iomp,
							ioend);
					if (startio) {
						xfs_add_to_ioend(inode,
							bh, p_offset, type,
							&ioend, done);
					} else {
						set_buffer_dirty(bh);
						unlock_buffer(bh);
						mark_buffer_dirty(bh);
					}
					page_dirty--;
					count++;
				} else {
					done = 1;
				}
			} else if (startio) {
				if (buffer_uptodate(bh) &&
				    !test_and_set_bit(BH_Lock, &bh->b_state)) {
					ASSERT(buffer_mapped(bh));
					xfs_add_to_ioend(inode,
							bh, p_offset, type,
							&ioend, done);
					page_dirty--;
					count++;
				} else {
					done = 1;
				}
			} else {
				done = 1;
			}
		}

		if (!iohead)
			iohead = ioend;

	} while (offset += len, ((bh = bh->b_this_page) != head));

	if (uptodate && bh == head)
		SetPageUptodate(page);

	if (startio)
		xfs_start_page_writeback(page, wbc, 1, count);

	if (ioend && iomp && !done) {
		offset = (iomp->iomap_offset + iomp->iomap_bsize - 1) >>
					PAGE_CACHE_SHIFT;
		tlast = min_t(pgoff_t, offset, last_index);
		xfs_cluster_write(inode, page->index + 1, iomp, &ioend,
					wbc, startio, unmapped, tlast);
	}

	if (iohead)
		xfs_submit_ioend(iohead);

	return page_dirty;

error:
	if (iohead)
		xfs_cancel_ioend(iohead);

	/*
	 * If it's delalloc and we have nowhere to put it,
	 * throw it away, unless the lower layers told
	 * us to try again.
	 */
	if (err != -EAGAIN) {
		if (!unmapped)
			block_invalidatepage(page, 0);
		ClearPageUptodate(page);
	}
	return err;
}

STATIC int
__linvfs_get_block(
	struct inode		*inode,
	sector_t		iblock,
	unsigned long		blocks,
	struct buffer_head	*bh_result,
	int			create,
	int			direct,
	bmapi_flags_t		flags)
{
	vnode_t			*vp = LINVFS_GET_VP(inode);
	xfs_iomap_t		iomap;
	xfs_off_t		offset;
	ssize_t			size;
	int			retpbbm = 1;
	int			error;

	offset = (xfs_off_t)iblock << inode->i_blkbits;
	if (blocks)
		size = (ssize_t) min_t(xfs_off_t, LONG_MAX,
					(xfs_off_t)blocks << inode->i_blkbits);
	else
		size = 1 << inode->i_blkbits;

	VOP_BMAP(vp, offset, size,
		create ? flags : BMAPI_READ, &iomap, &retpbbm, error);
	if (error)
		return -error;

	if (retpbbm == 0)
		return 0;

	if (iomap.iomap_bn != IOMAP_DADDR_NULL) {
		xfs_daddr_t	bn;
		xfs_off_t	delta;

		/* For unwritten extents do not report a disk address on
		 * the read case (treat as if we're reading into a hole).
		 */
		if (create || !(iomap.iomap_flags & IOMAP_UNWRITTEN)) {
			delta = offset - iomap.iomap_offset;
			delta >>= inode->i_blkbits;

			bn = iomap.iomap_bn >> (inode->i_blkbits - BBSHIFT);
			bn += delta;
			BUG_ON(!bn && !(iomap.iomap_flags & IOMAP_REALTIME));
			bh_result->b_blocknr = bn;
			set_buffer_mapped(bh_result);
		}
		if (create && (iomap.iomap_flags & IOMAP_UNWRITTEN)) {
			if (direct)
				bh_result->b_private = inode;
			set_buffer_unwritten(bh_result);
			set_buffer_delay(bh_result);
		}
	}

	/* If this is a realtime file, data might be on a new device */
	bh_result->b_bdev = iomap.iomap_target->bt_bdev;

	/* If we previously allocated a block out beyond eof and
	 * we are now coming back to use it then we will need to
	 * flag it as new even if it has a disk address.
	 */
	if (create &&
	    ((!buffer_mapped(bh_result) && !buffer_uptodate(bh_result)) ||
	     (offset >= i_size_read(inode)) || (iomap.iomap_flags & IOMAP_NEW)))
		set_buffer_new(bh_result);

	if (iomap.iomap_flags & IOMAP_DELAY) {
		BUG_ON(direct);
		if (create) {
			set_buffer_uptodate(bh_result);
			set_buffer_mapped(bh_result);
			set_buffer_delay(bh_result);
		}
	}

	if (blocks) {
		ASSERT(iomap.iomap_bsize - iomap.iomap_delta > 0);
		offset = min_t(xfs_off_t,
				iomap.iomap_bsize - iomap.iomap_delta,
				(xfs_off_t)blocks << inode->i_blkbits);
		bh_result->b_size = (u32) min_t(xfs_off_t, UINT_MAX, offset);
	}

	return 0;
}

int
linvfs_get_block(
	struct inode		*inode,
	sector_t		iblock,
	struct buffer_head	*bh_result,
	int			create)
{
	return __linvfs_get_block(inode, iblock, 0, bh_result,
					create, 0, BMAPI_WRITE);
}

STATIC int
linvfs_get_blocks_direct(
	struct inode		*inode,
	sector_t		iblock,
	unsigned long		max_blocks,
	struct buffer_head	*bh_result,
	int			create)
{
	return __linvfs_get_block(inode, iblock, max_blocks, bh_result,
					create, 1, BMAPI_WRITE|BMAPI_DIRECT);
}

STATIC void
linvfs_end_io_direct(
	struct kiocb	*iocb,
	loff_t		offset,
	ssize_t		size,
	void		*private)
{
	xfs_ioend_t	*ioend = iocb->private;

	/*
	 * Non-NULL private data means we need to issue a transaction to
	 * convert a range from unwritten to written extents.  This needs
	 * to happen from process contect but aio+dio I/O completion
	 * happens from irq context so we need to defer it to a workqueue.
	 * This is not nessecary for synchronous direct I/O, but we do
	 * it anyway to keep the code uniform and simpler.
	 *
	 * The core direct I/O code might be changed to always call the
	 * completion handler in the future, in which case all this can
	 * go away.
	 */
	if (private && size > 0) {
		ioend->io_offset = offset;
		ioend->io_size = size;
		xfs_finish_ioend(ioend);
	} else {
		ASSERT(size >= 0);
		xfs_destroy_ioend(ioend);
	}

	/*
	 * blockdev_direct_IO can return an error even afer the I/O
	 * completion handler was called.  Thus we need to protect
	 * against double-freeing.
	 */
	iocb->private = NULL;
}

STATIC ssize_t
linvfs_direct_IO(
	int			rw,
	struct kiocb		*iocb,
	const struct iovec	*iov,
	loff_t			offset,
	unsigned long		nr_segs)
{
	struct file	*file = iocb->ki_filp;
	struct inode	*inode = file->f_mapping->host;
	vnode_t		*vp = LINVFS_GET_VP(inode);
	xfs_iomap_t	iomap;
	int		maps = 1;
	int		error;
	ssize_t		ret;

	VOP_BMAP(vp, offset, 0, BMAPI_DEVICE, &iomap, &maps, error);
	if (error)
		return -error;

	iocb->private = xfs_alloc_ioend(inode, IOMAP_UNWRITTEN);

	ret = blockdev_direct_IO_own_locking(rw, iocb, inode,
		iomap.iomap_target->bt_bdev,
		iov, offset, nr_segs,
		linvfs_get_blocks_direct,
		linvfs_end_io_direct);

	if (unlikely(ret <= 0 && iocb->private))
		xfs_destroy_ioend(iocb->private);
	return ret;
}


STATIC sector_t
linvfs_bmap(
	struct address_space	*mapping,
	sector_t		block)
{
	struct inode		*inode = (struct inode *)mapping->host;
	vnode_t			*vp = LINVFS_GET_VP(inode);
	int			error;

	vn_trace_entry(vp, "linvfs_bmap", (inst_t *)__return_address);

	VOP_RWLOCK(vp, VRWLOCK_READ);
	VOP_FLUSH_PAGES(vp, (xfs_off_t)0, -1, 0, FI_REMAPF, error);
	VOP_RWUNLOCK(vp, VRWLOCK_READ);
	return generic_block_bmap(mapping, block, linvfs_get_block);
}

STATIC int
linvfs_readpage(
	struct file		*unused,
	struct page		*page)
{
	return mpage_readpage(page, linvfs_get_block);
}

STATIC int
linvfs_readpages(
	struct file		*unused,
	struct address_space	*mapping,
	struct list_head	*pages,
	unsigned		nr_pages)
{
	return mpage_readpages(mapping, pages, nr_pages, linvfs_get_block);
}

STATIC void
xfs_count_page_state(
	struct page		*page,
	int			*delalloc,
	int			*unmapped,
	int			*unwritten)
{
	struct buffer_head	*bh, *head;

	*delalloc = *unmapped = *unwritten = 0;

	bh = head = page_buffers(page);
	do {
		if (buffer_uptodate(bh) && !buffer_mapped(bh))
			(*unmapped) = 1;
		else if (buffer_unwritten(bh) && !buffer_delay(bh))
			clear_buffer_unwritten(bh);
		else if (buffer_unwritten(bh))
			(*unwritten) = 1;
		else if (buffer_delay(bh))
			(*delalloc) = 1;
	} while ((bh = bh->b_this_page) != head);
}


/*
 * writepage: Called from one of two places:
 *
 * 1. we are flushing a delalloc buffer head.
 *
 * 2. we are writing out a dirty page. Typically the page dirty
 *    state is cleared before we get here. In this case is it
 *    conceivable we have no buffer heads.
 *
 * For delalloc space on the page we need to allocate space and
 * flush it. For unmapped buffer heads on the page we should
 * allocate space if the page is uptodate. For any other dirty
 * buffer heads on the page we should flush them.
 *
 * If we detect that a transaction would be required to flush
 * the page, we have to check the process flags first, if we
 * are already in a transaction or disk I/O during allocations
 * is off, we need to fail the writepage and redirty the page.
 */

STATIC int
linvfs_writepage(
	struct page		*page,
	struct writeback_control *wbc)
{
	int			error;
	int			need_trans;
	int			delalloc, unmapped, unwritten;
	struct inode		*inode = page->mapping->host;

	xfs_page_trace(XFS_WRITEPAGE_ENTER, inode, page, 0);

	/*
	 * We need a transaction if:
	 *  1. There are delalloc buffers on the page
	 *  2. The page is uptodate and we have unmapped buffers
	 *  3. The page is uptodate and we have no buffers
	 *  4. There are unwritten buffers on the page
	 */

	if (!page_has_buffers(page)) {
		unmapped = 1;
		need_trans = 1;
	} else {
		xfs_count_page_state(page, &delalloc, &unmapped, &unwritten);
		if (!PageUptodate(page))
			unmapped = 0;
		need_trans = delalloc + unmapped + unwritten;
	}

	/*
	 * If we need a transaction and the process flags say
	 * we are already in a transaction, or no IO is allowed
	 * then mark the page dirty again and leave the page
	 * as is.
	 */
	if (PFLAGS_TEST_FSTRANS() && need_trans)
		goto out_fail;

	/*
	 * Delay hooking up buffer heads until we have
	 * made our go/no-go decision.
	 */
	if (!page_has_buffers(page))
		create_empty_buffers(page, 1 << inode->i_blkbits, 0);

	/*
	 * Convert delayed allocate, unwritten or unmapped space
	 * to real space and flush out to disk.
	 */
	error = xfs_page_state_convert(inode, page, wbc, 1, unmapped);
	if (error == -EAGAIN)
		goto out_fail;
	if (unlikely(error < 0))
		goto out_unlock;

	return 0;

out_fail:
	redirty_page_for_writepage(wbc, page);
	unlock_page(page);
	return 0;
out_unlock:
	unlock_page(page);
	return error;
}

STATIC int
linvfs_invalidate_page(
	struct page		*page,
	unsigned long		offset)
{
	xfs_page_trace(XFS_INVALIDPAGE_ENTER,
			page->mapping->host, page, offset);
	return block_invalidatepage(page, offset);
}

/*
 * Called to move a page into cleanable state - and from there
 * to be released. Possibly the page is already clean. We always
 * have buffer heads in this call.
 *
 * Returns 0 if the page is ok to release, 1 otherwise.
 *
 * Possible scenarios are:
 *
 * 1. We are being called to release a page which has been written
 *    to via regular I/O. buffer heads will be dirty and possibly
 *    delalloc. If no delalloc buffer heads in this case then we
 *    can just return zero.
 *
 * 2. We are called to release a page which has been written via
 *    mmap, all we need to do is ensure there is no delalloc
 *    state in the buffer heads, if not we can let the caller
 *    free them and we should come back later via writepage.
 */
STATIC int
linvfs_release_page(
	struct page		*page,
	gfp_t			gfp_mask)
{
	struct inode		*inode = page->mapping->host;
	int			dirty, delalloc, unmapped, unwritten;
	struct writeback_control wbc = {
		.sync_mode = WB_SYNC_ALL,
		.nr_to_write = 1,
	};

	xfs_page_trace(XFS_RELEASEPAGE_ENTER, inode, page, gfp_mask);

	xfs_count_page_state(page, &delalloc, &unmapped, &unwritten);
	if (!delalloc && !unwritten)
		goto free_buffers;

	if (!(gfp_mask & __GFP_FS))
		return 0;

	/* If we are already inside a transaction or the thread cannot
	 * do I/O, we cannot release this page.
	 */
	if (PFLAGS_TEST_FSTRANS())
		return 0;

	/*
	 * Convert delalloc space to real space, do not flush the
	 * data out to disk, that will be done by the caller.
	 * Never need to allocate space here - we will always
	 * come back to writepage in that case.
	 */
	dirty = xfs_page_state_convert(inode, page, &wbc, 0, 0);
	if (dirty == 0 && !unwritten)
		goto free_buffers;
	return 0;

free_buffers:
	return try_to_free_buffers(page);
}

STATIC int
linvfs_prepare_write(
	struct file		*file,
	struct page		*page,
	unsigned int		from,
	unsigned int		to)
{
	return block_prepare_write(page, from, to, linvfs_get_block);
}

struct address_space_operations linvfs_aops = {
	.readpage		= linvfs_readpage,
	.readpages		= linvfs_readpages,
	.writepage		= linvfs_writepage,
	.sync_page		= block_sync_page,
	.releasepage		= linvfs_release_page,
	.invalidatepage		= linvfs_invalidate_page,
	.prepare_write		= linvfs_prepare_write,
	.commit_write		= generic_commit_write,
	.bmap			= linvfs_bmap,
	.direct_IO		= linvfs_direct_IO,
};
