/*
 * CIPSO - Commercial IP Security Option
 *
 * This is an implementation of the CIPSO 2.2 protocol as specified in
 * draft-ietf-cipso-ipsecurity-01.txt with additional tag types as found in
 * FIPS-188, copies of both documents can be found in the Documentation
 * directory.  While CIPSO never became a full IETF RFC standard many vendors
 * have chosen to adopt the protocol and over the years it has become a
 * de-facto standard for labeled networking.
 *
 * Author: Paul Moore <paul.moore@hp.com>
 *
 */

/*
 * (c) Copyright Hewlett-Packard Development Company, L.P., 2006
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY;  without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;  if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/rcupdate.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/jhash.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/tcp.h>
#include <net/netlabel.h>
#include <net/cipso_ipv4.h>
#include <asm/bug.h>

struct cipso_v4_domhsh_entry {
	char *domain;
	u32 valid;
	struct list_head list;
	struct rcu_head rcu;
};

/* List of available DOI definitions */
/* XXX - Updates should be minimal so having a single lock for the
 * cipso_v4_doi_list and the cipso_v4_doi_list->dom_list should be
 * okay. */
/* XXX - This currently assumes a minimal number of different DOIs in use,
 * if in practice there are a lot of different DOIs this list should
 * probably be turned into a hash table or something similar so we
 * can do quick lookups. */
static DEFINE_SPINLOCK(cipso_v4_doi_list_lock);
static struct list_head cipso_v4_doi_list = LIST_HEAD_INIT(cipso_v4_doi_list);

/* Label mapping cache */
int cipso_v4_cache_enabled = 1;
int cipso_v4_cache_bucketsize = 10;
#define CIPSO_V4_CACHE_BUCKETBITS     7
#define CIPSO_V4_CACHE_BUCKETS        (1 << CIPSO_V4_CACHE_BUCKETBITS)
#define CIPSO_V4_CACHE_REORDERLIMIT   10
struct cipso_v4_map_cache_bkt {
	spinlock_t lock;
	u32 size;
	struct list_head list;
};
struct cipso_v4_map_cache_entry {
	u32 hash;
	unsigned char *key;
	size_t key_len;

	struct netlbl_lsm_cache lsm_data;

	u32 activity;
	struct list_head list;
};
static struct cipso_v4_map_cache_bkt *cipso_v4_cache = NULL;

/* Restricted bitmap (tag #1) flags */
int cipso_v4_rbm_optfmt = 0;
int cipso_v4_rbm_strictvalid = 1;

/*
 * Helper Functions
 */

/**
 * cipso_v4_bitmap_walk - Walk a bitmap looking for a bit
 * @bitmap: the bitmap
 * @bitmap_len: length in bits
 * @offset: starting offset
 * @state: if non-zero, look for a set (1) bit else look for a cleared (0) bit
 *
 * Description:
 * Starting at @offset, walk the bitmap from left to right until either the
 * desired bit is found or we reach the end.  Return the bit offset, -1 if
 * not found, or -2 if error.
 */
static int cipso_v4_bitmap_walk(const unsigned char *bitmap,
				u32 bitmap_len,
				u32 offset,
				u8 state)
{
	u32 bit_spot;
	u32 byte_offset;
	unsigned char bitmask;
	unsigned char byte;

	/* gcc always rounds to zero when doing integer division */
	byte_offset = offset / 8;
	byte = bitmap[byte_offset];
	bit_spot = offset;
	bitmask = 0x80 >> (offset % 8);

	while (bit_spot < bitmap_len) {
		if ((state && (byte & bitmask) == bitmask) ||
		    (state == 0 && (byte & bitmask) == 0))
			return bit_spot;

		bit_spot++;
		bitmask >>= 1;
		if (bitmask == 0) {
			byte = bitmap[++byte_offset];
			bitmask = 0x80;
		}
	}

	return -1;
}

/**
 * cipso_v4_bitmap_setbit - Sets a single bit in a bitmap
 * @bitmap: the bitmap
 * @bit: the bit
 * @state: if non-zero, set the bit (1) else clear the bit (0)
 *
 * Description:
 * Set a single bit in the bitmask.  Returns zero on success, negative values
 * on error.
 */
static void cipso_v4_bitmap_setbit(unsigned char *bitmap,
				   u32 bit,
				   u8 state)
{
	u32 byte_spot;
	u8 bitmask;

	/* gcc always rounds to zero when doing integer division */
	byte_spot = bit / 8;
	bitmask = 0x80 >> (bit % 8);
	if (state)
		bitmap[byte_spot] |= bitmask;
	else
		bitmap[byte_spot] &= ~bitmask;
}

/**
 * cipso_v4_doi_domhsh_free - Frees a domain list entry
 * @entry: the entry's RCU field
 *
 * Description:
 * This function is designed to be used as a callback to the call_rcu()
 * function so that the memory allocated to a domain list entry can be released
 * safely.
 *
 */
static void cipso_v4_doi_domhsh_free(struct rcu_head *entry)
{
	struct cipso_v4_domhsh_entry *ptr;

	ptr = container_of(entry, struct cipso_v4_domhsh_entry, rcu);
	kfree(ptr->domain);
	kfree(ptr);
}

/**
 * cipso_v4_cache_entry_free - Frees a cache entry
 * @entry: the entry to free
 *
 * Description:
 * This function frees the memory associated with a cache entry.
 *
 */
static void cipso_v4_cache_entry_free(struct cipso_v4_map_cache_entry *entry)
{
	if (entry->lsm_data.free)
		entry->lsm_data.free(entry->lsm_data.data);
	kfree(entry->key);
	kfree(entry);
}

/**
 * cipso_v4_map_cache_hash - Hashing function for the CIPSO cache
 * @key: the hash key
 * @key_len: the length of the key in bytes
 *
 * Description:
 * The CIPSO tag hashing function.  Returns a 32-bit hash value.
 *
 */
static u32 cipso_v4_map_cache_hash(const unsigned char *key, u32 key_len)
{
	return jhash(key, key_len, 0);
}

/*
 * Label Mapping Cache Functions
 */

/**
 * cipso_v4_cache_init - Initialize the CIPSO cache
 *
 * Description:
 * Initializes the CIPSO label mapping cache, this function should be called
 * before any of the other functions defined in this file.  Returns zero on
 * success, negative values on error.
 *
 */
static int cipso_v4_cache_init(void)
{
	u32 iter;

	cipso_v4_cache = kcalloc(CIPSO_V4_CACHE_BUCKETS,
				 sizeof(struct cipso_v4_map_cache_bkt),
				 GFP_KERNEL);
	if (cipso_v4_cache == NULL)
		return -ENOMEM;

	for (iter = 0; iter < CIPSO_V4_CACHE_BUCKETS; iter++) {
		spin_lock_init(&cipso_v4_cache[iter].lock);
		cipso_v4_cache[iter].size = 0;
		INIT_LIST_HEAD(&cipso_v4_cache[iter].list);
	}

	return 0;
}

/**
 * cipso_v4_cache_invalidate - Invalidates the current CIPSO cache
 *
 * Description:
 * Invalidates and frees any entries in the CIPSO cache.  Returns zero on
 * success and negative values on failure.
 *
 */
void cipso_v4_cache_invalidate(void)
{
	struct cipso_v4_map_cache_entry *entry, *tmp_entry;
	u32 iter;

	for (iter = 0; iter < CIPSO_V4_CACHE_BUCKETS; iter++) {
		spin_lock(&cipso_v4_cache[iter].lock);
		list_for_each_entry_safe(entry,
					 tmp_entry,
					 &cipso_v4_cache[iter].list, list) {
			list_del(&entry->list);
			cipso_v4_cache_entry_free(entry);
		}
		cipso_v4_cache[iter].size = 0;
		spin_unlock(&cipso_v4_cache[iter].lock);
	}

	return;
}

/**
 * cipso_v4_cache_check - Check the CIPSO cache for a label mapping
 * @key: the buffer to check
 * @key_len: buffer length in bytes
 * @secattr: the security attribute struct to use
 *
 * Description:
 * This function checks the cache to see if a label mapping already exists for
 * the given key.  If there is a match then the cache is adjusted and the
 * @secattr struct is populated with the correct LSM security attributes.  The
 * cache is adjusted in the following manner if the entry is not already the
 * first in the cache bucket:
 *
 *  1. The cache entry's activity counter is incremented
 *  2. The previous (higher ranking) entry's activity counter is decremented
 *  3. If the difference between the two activity counters is geater than
 *     CIPSO_V4_CACHE_REORDERLIMIT the two entries are swapped
 *
 * Returns zero on success, -ENOENT for a cache miss, and other negative values
 * on error.
 *
 */
static int cipso_v4_cache_check(const unsigned char *key,
				u32 key_len,
				struct netlbl_lsm_secattr *secattr)
{
	u32 bkt;
	struct cipso_v4_map_cache_entry *entry;
	struct cipso_v4_map_cache_entry *prev_entry = NULL;
	u32 hash;

	if (!cipso_v4_cache_enabled)
		return -ENOENT;

	hash = cipso_v4_map_cache_hash(key, key_len);
	bkt = hash & (CIPSO_V4_CACHE_BUCKETBITS - 1);
	spin_lock(&cipso_v4_cache[bkt].lock);
	list_for_each_entry(entry, &cipso_v4_cache[bkt].list, list) {
		if (entry->hash == hash &&
		    entry->key_len == key_len &&
		    memcmp(entry->key, key, key_len) == 0) {
			entry->activity += 1;
			secattr->cache.free = entry->lsm_data.free;
			secattr->cache.data = entry->lsm_data.data;
			if (prev_entry == NULL) {
				spin_unlock(&cipso_v4_cache[bkt].lock);
				return 0;
			}

			if (prev_entry->activity > 0)
				prev_entry->activity -= 1;
			if (entry->activity > prev_entry->activity &&
			    entry->activity - prev_entry->activity >
			    CIPSO_V4_CACHE_REORDERLIMIT) {
				__list_del(entry->list.prev, entry->list.next);
				__list_add(&entry->list,
					   prev_entry->list.prev,
					   &prev_entry->list);
			}

			spin_unlock(&cipso_v4_cache[bkt].lock);
			return 0;
		}
		prev_entry = entry;
	}
	spin_unlock(&cipso_v4_cache[bkt].lock);

	return -ENOENT;
}

/**
 * cipso_v4_cache_add - Add an entry to the CIPSO cache
 * @skb: the packet
 * @secattr: the packet's security attributes
 *
 * Description:
 * Add a new entry into the CIPSO label mapping cache.  Add the new entry to
 * head of the cache bucket's list, if the cache bucket is out of room remove
 * the last entry in the list first.  It is important to note that there is
 * currently no checking for duplicate keys.  Returns zero on success,
 * negative values on failure.
 *
 */
int cipso_v4_cache_add(const struct sk_buff *skb,
		       const struct netlbl_lsm_secattr *secattr)
{
	int ret_val = -EPERM;
	u32 bkt;
	struct cipso_v4_map_cache_entry *entry = NULL;
	struct cipso_v4_map_cache_entry *old_entry = NULL;
	unsigned char *cipso_ptr;
	u32 cipso_ptr_len;

	if (!cipso_v4_cache_enabled || cipso_v4_cache_bucketsize <= 0)
		return 0;

	cipso_ptr = CIPSO_V4_OPTPTR(skb);
	cipso_ptr_len = cipso_ptr[1];

	entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
	if (entry == NULL)
		return -ENOMEM;
	entry->key = kmalloc(cipso_ptr_len, GFP_ATOMIC);
	if (entry->key == NULL) {
		ret_val = -ENOMEM;
		goto cache_add_failure;
	}
	memcpy(entry->key, cipso_ptr, cipso_ptr_len);
	entry->key_len = cipso_ptr_len;
	entry->hash = cipso_v4_map_cache_hash(cipso_ptr, cipso_ptr_len);
	entry->lsm_data.free = secattr->cache.free;
	entry->lsm_data.data = secattr->cache.data;

	bkt = entry->hash & (CIPSO_V4_CACHE_BUCKETBITS - 1);
	spin_lock(&cipso_v4_cache[bkt].lock);
	if (cipso_v4_cache[bkt].size < cipso_v4_cache_bucketsize) {
		list_add(&entry->list, &cipso_v4_cache[bkt].list);
		cipso_v4_cache[bkt].size += 1;
	} else {
		old_entry = list_entry(cipso_v4_cache[bkt].list.prev,
				       struct cipso_v4_map_cache_entry, list);
		list_del(&old_entry->list);
		list_add(&entry->list, &cipso_v4_cache[bkt].list);
		cipso_v4_cache_entry_free(old_entry);
	}
	spin_unlock(&cipso_v4_cache[bkt].lock);

	return 0;

cache_add_failure:
	if (entry)
		cipso_v4_cache_entry_free(entry);
	return ret_val;
}

/*
 * DOI List Functions
 */

/**
 * cipso_v4_doi_search - Searches for a DOI definition
 * @doi: the DOI to search for
 *
 * Description:
 * Search the DOI definition list for a DOI definition with a DOI value that
 * matches @doi.  The caller is responsibile for calling rcu_read_[un]lock().
 * Returns a pointer to the DOI definition on success and NULL on failure.
 */
static struct cipso_v4_doi *cipso_v4_doi_search(u32 doi)
{
	struct cipso_v4_doi *iter;

	list_for_each_entry_rcu(iter, &cipso_v4_doi_list, list)
		if (iter->doi == doi && iter->valid)
			return iter;
	return NULL;
}

/**
 * cipso_v4_doi_add - Add a new DOI to the CIPSO protocol engine
 * @doi_def: the DOI structure
 *
 * Description:
 * The caller defines a new DOI for use by the CIPSO engine and calls this
 * function to add it to the list of acceptable domains.  The caller must
 * ensure that the mapping table specified in @doi_def->map meets all of the
 * requirements of the mapping type (see cipso_ipv4.h for details).  Returns
 * zero on success and non-zero on failure.
 *
 */
int cipso_v4_doi_add(struct cipso_v4_doi *doi_def)
{
	if (doi_def == NULL || doi_def->doi == CIPSO_V4_DOI_UNKNOWN)
		return -EINVAL;

	doi_def->valid = 1;
	INIT_RCU_HEAD(&doi_def->rcu);
	INIT_LIST_HEAD(&doi_def->dom_list);

	rcu_read_lock();
	if (cipso_v4_doi_search(doi_def->doi) != NULL)
		goto doi_add_failure_rlock;
	spin_lock(&cipso_v4_doi_list_lock);
	if (cipso_v4_doi_search(doi_def->doi) != NULL)
		goto doi_add_failure_slock;
	list_add_tail_rcu(&doi_def->list, &cipso_v4_doi_list);
	spin_unlock(&cipso_v4_doi_list_lock);
	rcu_read_unlock();

	return 0;

doi_add_failure_slock:
	spin_unlock(&cipso_v4_doi_list_lock);
doi_add_failure_rlock:
	rcu_read_unlock();
	return -EEXIST;
}

/**
 * cipso_v4_doi_remove - Remove an existing DOI from the CIPSO protocol engine
 * @doi: the DOI value
 * @callback: the DOI cleanup/free callback
 *
 * Description:
 * Removes a DOI definition from the CIPSO engine, @callback is called to
 * free any memory.  The NetLabel routines will be called to release their own
 * LSM domain mappings as well as our own domain list.  Returns zero on
 * success and negative values on failure.
 *
 */
int cipso_v4_doi_remove(u32 doi, void (*callback) (struct rcu_head * head))
{
	struct cipso_v4_doi *doi_def;
	struct cipso_v4_domhsh_entry *dom_iter;

	rcu_read_lock();
	if (cipso_v4_doi_search(doi) != NULL) {
		spin_lock(&cipso_v4_doi_list_lock);
		doi_def = cipso_v4_doi_search(doi);
		if (doi_def == NULL) {
			spin_unlock(&cipso_v4_doi_list_lock);
			rcu_read_unlock();
			return -ENOENT;
		}
		doi_def->valid = 0;
		list_del_rcu(&doi_def->list);
		spin_unlock(&cipso_v4_doi_list_lock);
		list_for_each_entry_rcu(dom_iter, &doi_def->dom_list, list)
			if (dom_iter->valid)
				netlbl_domhsh_remove(dom_iter->domain);
		cipso_v4_cache_invalidate();
		rcu_read_unlock();

		call_rcu(&doi_def->rcu, callback);
		return 0;
	}
	rcu_read_unlock();

	return -ENOENT;
}

/**
 * cipso_v4_doi_getdef - Returns a pointer to a valid DOI definition
 * @doi: the DOI value
 *
 * Description:
 * Searches for a valid DOI definition and if one is found it is returned to
 * the caller.  Otherwise NULL is returned.  The caller must ensure that
 * rcu_read_lock() is held while accessing the returned definition.
 *
 */
struct cipso_v4_doi *cipso_v4_doi_getdef(u32 doi)
{
	return cipso_v4_doi_search(doi);
}

/**
 * cipso_v4_doi_dump_all - Dump all the CIPSO DOI definitions into a sk_buff
 * @headroom: the amount of headroom to allocate for the sk_buff
 *
 * Description:
 * Dump a list of all the configured DOI values into a sk_buff.  The returned
 * sk_buff has room at the front of the sk_buff for @headroom bytes.  See
 * net/netlabel/netlabel_cipso_v4.h for the LISTALL message format.  This
 * function may fail if another process is changing the DOI list at the same
 * time.  Returns a pointer to a sk_buff on success, NULL on error.
 *
 */
struct sk_buff *cipso_v4_doi_dump_all(size_t headroom)
{
	struct sk_buff *skb = NULL;
	struct cipso_v4_doi *iter;
	u32 doi_cnt = 0;
	ssize_t buf_len;

	buf_len = NETLBL_LEN_U32;
	rcu_read_lock();
	list_for_each_entry_rcu(iter, &cipso_v4_doi_list, list)
		if (iter->valid) {
			doi_cnt += 1;
			buf_len += 2 * NETLBL_LEN_U32;
		}

	skb = netlbl_netlink_alloc_skb(headroom, buf_len, GFP_ATOMIC);
	if (skb == NULL)
		goto doi_dump_all_failure;

	if (nla_put_u32(skb, NLA_U32, doi_cnt) != 0)
		goto doi_dump_all_failure;
	buf_len -= NETLBL_LEN_U32;
	list_for_each_entry_rcu(iter, &cipso_v4_doi_list, list)
		if (iter->valid) {
			if (buf_len < 2 * NETLBL_LEN_U32)
				goto doi_dump_all_failure;
			if (nla_put_u32(skb, NLA_U32, iter->doi) != 0)
				goto doi_dump_all_failure;
			if (nla_put_u32(skb, NLA_U32, iter->type) != 0)
				goto doi_dump_all_failure;
			buf_len -= 2 * NETLBL_LEN_U32;
		}
	rcu_read_unlock();

	return skb;

doi_dump_all_failure:
	rcu_read_unlock();
	kfree(skb);
	return NULL;
}

/**
 * cipso_v4_doi_dump - Dump a CIPSO DOI definition into a sk_buff
 * @doi: the DOI value
 * @headroom: the amount of headroom to allocate for the sk_buff
 *
 * Description:
 * Lookup the DOI definition matching @doi and dump it's contents into a
 * sk_buff.  The returned sk_buff has room at the front of the sk_buff for
 * @headroom bytes.  See net/netlabel/netlabel_cipso_v4.h for the LIST message
 * format.  This function may fail if another process is changing the DOI list
 * at the same time.  Returns a pointer to a sk_buff on success, NULL on error.
 *
 */
struct sk_buff *cipso_v4_doi_dump(u32 doi, size_t headroom)
{
	struct sk_buff *skb = NULL;
	struct cipso_v4_doi *iter;
	u32 tag_cnt = 0;
	u32 lvl_cnt = 0;
	u32 cat_cnt = 0;
	ssize_t buf_len;
	ssize_t tmp;

	rcu_read_lock();
	iter = cipso_v4_doi_getdef(doi);
	if (iter == NULL)
		goto doi_dump_failure;
	buf_len = NETLBL_LEN_U32;
	switch (iter->type) {
	case CIPSO_V4_MAP_PASS:
		buf_len += NETLBL_LEN_U32;
		while(tag_cnt < CIPSO_V4_TAG_MAXCNT &&
		      iter->tags[tag_cnt] != CIPSO_V4_TAG_INVALID) {
			tag_cnt += 1;
			buf_len += NETLBL_LEN_U8;
		}
		break;
	case CIPSO_V4_MAP_STD:
		buf_len += 3 * NETLBL_LEN_U32;
		while (tag_cnt < CIPSO_V4_TAG_MAXCNT &&
		       iter->tags[tag_cnt] != CIPSO_V4_TAG_INVALID) {
			tag_cnt += 1;
			buf_len += NETLBL_LEN_U8;
		}
		for (tmp = 0; tmp < iter->map.std->lvl.local_size; tmp++)
			if (iter->map.std->lvl.local[tmp] !=
			    CIPSO_V4_INV_LVL) {
				lvl_cnt += 1;
				buf_len += NETLBL_LEN_U32 + NETLBL_LEN_U8;
			}
		for (tmp = 0; tmp < iter->map.std->cat.local_size; tmp++)
			if (iter->map.std->cat.local[tmp] !=
			    CIPSO_V4_INV_CAT) {
				cat_cnt += 1;
				buf_len += NETLBL_LEN_U32 + NETLBL_LEN_U16;
			}
		break;
	}

	skb = netlbl_netlink_alloc_skb(headroom, buf_len, GFP_ATOMIC);
	if (skb == NULL)
		goto doi_dump_failure;

	if (nla_put_u32(skb, NLA_U32, iter->type) != 0)
		goto doi_dump_failure;
	buf_len -= NETLBL_LEN_U32;
	if (iter != cipso_v4_doi_getdef(doi))
		goto doi_dump_failure;
	switch (iter->type) {
	case CIPSO_V4_MAP_PASS:
		if (nla_put_u32(skb, NLA_U32, tag_cnt) != 0)
			goto doi_dump_failure;
		buf_len -= NETLBL_LEN_U32;
		for (tmp = 0;
		     tmp < CIPSO_V4_TAG_MAXCNT &&
			     iter->tags[tmp] != CIPSO_V4_TAG_INVALID;
		     tmp++) {
			if (buf_len < NETLBL_LEN_U8)
				goto doi_dump_failure;
			if (nla_put_u8(skb, NLA_U8, iter->tags[tmp]) != 0)
				goto doi_dump_failure;
			buf_len -= NETLBL_LEN_U8;
		}
		break;
	case CIPSO_V4_MAP_STD:
		if (nla_put_u32(skb, NLA_U32, tag_cnt) != 0)
			goto doi_dump_failure;
		if (nla_put_u32(skb, NLA_U32, lvl_cnt) != 0)
			goto doi_dump_failure;
		if (nla_put_u32(skb, NLA_U32, cat_cnt) != 0)
			goto doi_dump_failure;
		buf_len -= 3 * NETLBL_LEN_U32;
		for (tmp = 0;
		     tmp < CIPSO_V4_TAG_MAXCNT &&
			     iter->tags[tmp] != CIPSO_V4_TAG_INVALID;
		     tmp++) {
			if (buf_len < NETLBL_LEN_U8)
				goto doi_dump_failure;
			if (nla_put_u8(skb, NLA_U8, iter->tags[tmp]) != 0)
				goto doi_dump_failure;
			buf_len -= NETLBL_LEN_U8;
		}
		for (tmp = 0; tmp < iter->map.std->lvl.local_size; tmp++)
			if (iter->map.std->lvl.local[tmp] !=
			    CIPSO_V4_INV_LVL) {
				if (buf_len < NETLBL_LEN_U32 + NETLBL_LEN_U8)
					goto doi_dump_failure;
				if (nla_put_u32(skb, NLA_U32, tmp) != 0)
					goto doi_dump_failure;
				if (nla_put_u8(skb,
					   NLA_U8,
					   iter->map.std->lvl.local[tmp]) != 0)
					goto doi_dump_failure;
				buf_len -= NETLBL_LEN_U32 + NETLBL_LEN_U8;
			}
		for (tmp = 0; tmp < iter->map.std->cat.local_size; tmp++)
			if (iter->map.std->cat.local[tmp] !=
			    CIPSO_V4_INV_CAT) {
				if (buf_len < NETLBL_LEN_U32 + NETLBL_LEN_U16)
					goto doi_dump_failure;
				if (nla_put_u32(skb, NLA_U32, tmp) != 0)
					goto doi_dump_failure;
				if (nla_put_u16(skb,
					   NLA_U16,
					   iter->map.std->cat.local[tmp]) != 0)
					goto doi_dump_failure;
				buf_len -= NETLBL_LEN_U32 + NETLBL_LEN_U16;
			}
		break;
	}
	rcu_read_unlock();

	return skb;

doi_dump_failure:
	rcu_read_unlock();
	kfree(skb);
	return NULL;
}

/**
 * cipso_v4_doi_domhsh_add - Adds a domain entry to a DOI definition
 * @doi_def: the DOI definition
 * @domain: the domain to add
 *
 * Description:
 * Adds the @domain to the the DOI specified by @doi_def, this function
 * should only be called by external functions (i.e. NetLabel).  This function
 * does allocate memory.  Returns zero on success, negative values on failure.
 *
 */
int cipso_v4_doi_domhsh_add(struct cipso_v4_doi *doi_def, const char *domain)
{
	struct cipso_v4_domhsh_entry *iter;
	struct cipso_v4_domhsh_entry *new_dom;

	new_dom = kzalloc(sizeof(*new_dom), GFP_KERNEL);
	if (new_dom == NULL)
		return -ENOMEM;
	if (domain) {
		new_dom->domain = kstrdup(domain, GFP_KERNEL);
		if (new_dom->domain == NULL) {
			kfree(new_dom);
			return -ENOMEM;
		}
	}
	new_dom->valid = 1;
	INIT_RCU_HEAD(&new_dom->rcu);

	rcu_read_lock();
	spin_lock(&cipso_v4_doi_list_lock);
	list_for_each_entry_rcu(iter, &doi_def->dom_list, list)
		if (iter->valid &&
		    ((domain != NULL && iter->domain != NULL &&
		      strcmp(iter->domain, domain) == 0) ||
		     (domain == NULL && iter->domain == NULL))) {
			spin_unlock(&cipso_v4_doi_list_lock);
			rcu_read_unlock();
			kfree(new_dom->domain);
			kfree(new_dom);
			return -EEXIST;
		}
	list_add_tail_rcu(&new_dom->list, &doi_def->dom_list);
	spin_unlock(&cipso_v4_doi_list_lock);
	rcu_read_unlock();

	return 0;
}

/**
 * cipso_v4_doi_domhsh_remove - Removes a domain entry from a DOI definition
 * @doi_def: the DOI definition
 * @domain: the domain to remove
 *
 * Description:
 * Removes the @domain from the DOI specified by @doi_def, this function
 * should only be called by external functions (i.e. NetLabel).   Returns zero
 * on success and negative values on error.
 *
 */
int cipso_v4_doi_domhsh_remove(struct cipso_v4_doi *doi_def,
			       const char *domain)
{
	struct cipso_v4_domhsh_entry *iter;

	rcu_read_lock();
	spin_lock(&cipso_v4_doi_list_lock);
	list_for_each_entry_rcu(iter, &doi_def->dom_list, list)
		if (iter->valid &&
		    ((domain != NULL && iter->domain != NULL &&
		      strcmp(iter->domain, domain) == 0) ||
		     (domain == NULL && iter->domain == NULL))) {
			iter->valid = 0;
			list_del_rcu(&iter->list);
			spin_unlock(&cipso_v4_doi_list_lock);
			rcu_read_unlock();
			call_rcu(&iter->rcu, cipso_v4_doi_domhsh_free);

			return 0;
		}
	spin_unlock(&cipso_v4_doi_list_lock);
	rcu_read_unlock();

	return -ENOENT;
}

/*
 * Label Mapping Functions
 */

/**
 * cipso_v4_map_lvl_valid - Checks to see if the given level is understood
 * @doi_def: the DOI definition
 * @level: the level to check
 *
 * Description:
 * Checks the given level against the given DOI definition and returns a
 * negative value if the level does not have a valid mapping and a zero value
 * if the level is defined by the DOI.
 *
 */
static int cipso_v4_map_lvl_valid(const struct cipso_v4_doi *doi_def, u8 level)
{
	switch (doi_def->type) {
	case CIPSO_V4_MAP_PASS:
		return 0;
	case CIPSO_V4_MAP_STD:
		if (doi_def->map.std->lvl.cipso[level] < CIPSO_V4_INV_LVL)
			return 0;
		break;
	}

	return -EFAULT;
}

/**
 * cipso_v4_map_lvl_hton - Perform a level mapping from the host to the network
 * @doi_def: the DOI definition
 * @host_lvl: the host MLS level
 * @net_lvl: the network/CIPSO MLS level
 *
 * Description:
 * Perform a label mapping to translate a local MLS level to the correct
 * CIPSO level using the given DOI definition.  Returns zero on success,
 * negative values otherwise.
 *
 */
static int cipso_v4_map_lvl_hton(const struct cipso_v4_doi *doi_def,
				 u32 host_lvl,
				 u32 *net_lvl)
{
	switch (doi_def->type) {
	case CIPSO_V4_MAP_PASS:
		*net_lvl = host_lvl;
		return 0;
	case CIPSO_V4_MAP_STD:
		if (host_lvl < doi_def->map.std->lvl.local_size) {
			*net_lvl = doi_def->map.std->lvl.local[host_lvl];
			return 0;
		}
		break;
	}

	return -EINVAL;
}

/**
 * cipso_v4_map_lvl_ntoh - Perform a level mapping from the network to the host
 * @doi_def: the DOI definition
 * @net_lvl: the network/CIPSO MLS level
 * @host_lvl: the host MLS level
 *
 * Description:
 * Perform a label mapping to translate a CIPSO level to the correct local MLS
 * level using the given DOI definition.  Returns zero on success, negative
 * values otherwise.
 *
 */
static int cipso_v4_map_lvl_ntoh(const struct cipso_v4_doi *doi_def,
				 u32 net_lvl,
				 u32 *host_lvl)
{
	struct cipso_v4_std_map_tbl *map_tbl;

	switch (doi_def->type) {
	case CIPSO_V4_MAP_PASS:
		*host_lvl = net_lvl;
		return 0;
	case CIPSO_V4_MAP_STD:
		map_tbl = doi_def->map.std;
		if (net_lvl < map_tbl->lvl.cipso_size &&
		    map_tbl->lvl.cipso[net_lvl] < CIPSO_V4_INV_LVL) {
			*host_lvl = doi_def->map.std->lvl.cipso[net_lvl];
			return 0;
		}
		break;
	}

	return -EINVAL;
}

/**
 * cipso_v4_map_cat_rbm_valid - Checks to see if the category bitmap is valid
 * @doi_def: the DOI definition
 * @bitmap: category bitmap
 * @bitmap_len: bitmap length in bytes
 *
 * Description:
 * Checks the given category bitmap against the given DOI definition and
 * returns a negative value if any of the categories in the bitmap do not have
 * a valid mapping and a zero value if all of the categories are valid.
 *
 */
static int cipso_v4_map_cat_rbm_valid(const struct cipso_v4_doi *doi_def,
				      const unsigned char *bitmap,
				      u32 bitmap_len)
{
	int cat = -1;
	u32 bitmap_len_bits = bitmap_len * 8;
	u32 cipso_cat_size = doi_def->map.std->cat.cipso_size;
	u32 *cipso_array = doi_def->map.std->cat.cipso;

	switch (doi_def->type) {
	case CIPSO_V4_MAP_PASS:
		return 0;
	case CIPSO_V4_MAP_STD:
		for (;;) {
			cat = cipso_v4_bitmap_walk(bitmap,
						   bitmap_len_bits,
						   cat + 1,
						   1);
			if (cat < 0)
				break;
			if (cat >= cipso_cat_size ||
			    cipso_array[cat] >= CIPSO_V4_INV_CAT)
				return -EFAULT;
		}

		if (cat == -1)
			return 0;
		break;
	}

	return -EFAULT;
}

/**
 * cipso_v4_map_cat_rbm_hton - Perform a category mapping from host to network
 * @doi_def: the DOI definition
 * @host_cat: the category bitmap in host format
 * @host_cat_len: the length of the host's category bitmap in bytes
 * @net_cat: the zero'd out category bitmap in network/CIPSO format
 * @net_cat_len: the length of the CIPSO bitmap in bytes
 *
 * Description:
 * Perform a label mapping to translate a local MLS category bitmap to the
 * correct CIPSO bitmap using the given DOI definition.  Returns the minimum
 * size in bytes of the network bitmap on success, negative values otherwise.
 *
 */
static int cipso_v4_map_cat_rbm_hton(const struct cipso_v4_doi *doi_def,
				     const unsigned char *host_cat,
				     u32 host_cat_len,
				     unsigned char *net_cat,
				     u32 net_cat_len)
{
	int host_spot = -1;
	u32 net_spot;
	u32 net_spot_max = 0;
	u32 host_clen_bits = host_cat_len * 8;
	u32 net_clen_bits = net_cat_len * 8;
	u32 host_cat_size = doi_def->map.std->cat.local_size;
	u32 *host_cat_array = doi_def->map.std->cat.local;

	switch (doi_def->type) {
	case CIPSO_V4_MAP_PASS:
		net_spot_max = host_cat_len - 1;
		while (net_spot_max > 0 && host_cat[net_spot_max] == 0)
			net_spot_max--;
		if (net_spot_max > net_cat_len)
			return -EINVAL;
		memcpy(net_cat, host_cat, net_spot_max);
		return net_spot_max;
	case CIPSO_V4_MAP_STD:
		for (;;) {
			host_spot = cipso_v4_bitmap_walk(host_cat,
							 host_clen_bits,
							 host_spot + 1,
							 1);
			if (host_spot < 0)
				break;
			if (host_spot >= host_cat_size)
				return -EPERM;

			net_spot = host_cat_array[host_spot];
			if (net_spot >= net_clen_bits)
				return -ENOSPC;
			cipso_v4_bitmap_setbit(net_cat, net_spot, 1);

			if (net_spot > net_spot_max)
				net_spot_max = net_spot;
		}

		if (host_spot == -2)
			return -EFAULT;

		if (++net_spot_max % 8)
			return net_spot_max / 8 + 1;
		return net_spot_max / 8;
	}

	return -EINVAL;
}

/**
 * cipso_v4_map_cat_rbm_ntoh - Perform a category mapping from network to host
 * @doi_def: the DOI definition
 * @net_cat: the category bitmap in network/CIPSO format
 * @net_cat_len: the length of the CIPSO bitmap in bytes
 * @host_cat: the zero'd out category bitmap in host format
 * @host_cat_len: the length of the host's category bitmap in bytes
 *
 * Description:
 * Perform a label mapping to translate a CIPSO bitmap to the correct local
 * MLS category bitmap using the given DOI definition.  Returns the minimum
 * size in bytes of the host bitmap on success, negative values otherwise.
 *
 */
static int cipso_v4_map_cat_rbm_ntoh(const struct cipso_v4_doi *doi_def,
				     const unsigned char *net_cat,
				     u32 net_cat_len,
				     unsigned char *host_cat,
				     u32 host_cat_len)
{
	u32 host_spot;
	u32 host_spot_max = 0;
	int net_spot = -1;
	u32 net_clen_bits = net_cat_len * 8;
	u32 host_clen_bits = host_cat_len * 8;
	u32 net_cat_size = doi_def->map.std->cat.cipso_size;
	u32 *net_cat_array = doi_def->map.std->cat.cipso;

	switch (doi_def->type) {
	case CIPSO_V4_MAP_PASS:
		if (net_cat_len > host_cat_len)
			return -EINVAL;
		memcpy(host_cat, net_cat, net_cat_len);
		return net_cat_len;
	case CIPSO_V4_MAP_STD:
		for (;;) {
			net_spot = cipso_v4_bitmap_walk(net_cat,
							net_clen_bits,
							net_spot + 1,
							1);
			if (net_spot < 0)
				break;
			if (net_spot >= net_cat_size ||
			    net_cat_array[net_spot] >= CIPSO_V4_INV_CAT)
				return -EPERM;

			host_spot = net_cat_array[net_spot];
			if (host_spot >= host_clen_bits)
				return -ENOSPC;
			cipso_v4_bitmap_setbit(host_cat, host_spot, 1);

			if (host_spot > host_spot_max)
				host_spot_max = host_spot;
		}

		if (net_spot == -2)
			return -EFAULT;

		if (++host_spot_max % 8)
			return host_spot_max / 8 + 1;
		return host_spot_max / 8;
	}

	return -EINVAL;
}

/*
 * Protocol Handling Functions
 */

#define CIPSO_V4_HDR_LEN              6

/**
 * cipso_v4_gentag_hdr - Generate a CIPSO option header
 * @doi_def: the DOI definition
 * @len: the total tag length in bytes
 * @buf: the CIPSO option buffer
 *
 * Description:
 * Write a CIPSO header into the beginning of @buffer.  Return zero on success,
 * negative values on failure.
 *
 */
static int cipso_v4_gentag_hdr(const struct cipso_v4_doi *doi_def,
			       u32 len,
			       unsigned char *buf)
{
	if (CIPSO_V4_HDR_LEN + len > 40)
		return -ENOSPC;

	buf[0] = IPOPT_CIPSO;
	buf[1] = CIPSO_V4_HDR_LEN + len;
	*(u32 *)&buf[2] = htonl(doi_def->doi);

	return 0;
}

#define CIPSO_V4_TAG1_CAT_LEN         30

/**
 * cipso_v4_gentag_rbm - Generate a CIPSO restricted bitmap tag (type #1)
 * @doi_def: the DOI definition
 * @secattr: the security attributes
 * @buffer: the option buffer
 * @buffer_len: length of buffer in bytes
 *
 * Description:
 * Generate a CIPSO option using the restricted bitmap tag, tag type #1.  The
 * actual buffer length may be larger than the indicated size due to
 * translation between host and network category bitmaps.  Returns zero on
 * success, negative values on failure.
 *
 */
static int cipso_v4_gentag_rbm(const struct cipso_v4_doi *doi_def,
			       const struct netlbl_lsm_secattr *secattr,
			       unsigned char **buffer,
			       u32 *buffer_len)
{
	int ret_val = -EPERM;
	unsigned char *buf = NULL;
	u32 buf_len;
	u32 level;

	if (secattr->mls_cat) {
		buf = kzalloc(CIPSO_V4_HDR_LEN + 4 + CIPSO_V4_TAG1_CAT_LEN,
			      GFP_ATOMIC);
		if (buf == NULL)
			return -ENOMEM;

		ret_val = cipso_v4_map_cat_rbm_hton(doi_def,
						    secattr->mls_cat,
						    secattr->mls_cat_len,
						    &buf[CIPSO_V4_HDR_LEN + 4],
						    CIPSO_V4_TAG1_CAT_LEN);
		if (ret_val < 0)
			goto gentag_failure;

		/* This will send packets using the "optimized" format when
		 * possibile as specified in  section 3.4.2.6 of the
		 * CIPSO draft. */
		if (cipso_v4_rbm_optfmt && (ret_val > 0 && ret_val < 10))
			ret_val = 10;

		buf_len = 4 + ret_val;
	} else {
		buf = kzalloc(CIPSO_V4_HDR_LEN + 4, GFP_ATOMIC);
		if (buf == NULL)
			return -ENOMEM;
		buf_len = 4;
	}

	ret_val = cipso_v4_map_lvl_hton(doi_def, secattr->mls_lvl, &level);
	if (ret_val != 0)
		goto gentag_failure;

	ret_val = cipso_v4_gentag_hdr(doi_def, buf_len, buf);
	if (ret_val != 0)
		goto gentag_failure;

	buf[CIPSO_V4_HDR_LEN] = 0x01;
	buf[CIPSO_V4_HDR_LEN + 1] = buf_len;
	buf[CIPSO_V4_HDR_LEN + 3] = level;

	*buffer = buf;
	*buffer_len = CIPSO_V4_HDR_LEN + buf_len;

	return 0;

gentag_failure:
	kfree(buf);
	return ret_val;
}

/**
 * cipso_v4_parsetag_rbm - Parse a CIPSO restricted bitmap tag
 * @doi_def: the DOI definition
 * @tag: the CIPSO tag
 * @secattr: the security attributes
 *
 * Description:
 * Parse a CIPSO restricted bitmap tag (tag type #1) and return the security
 * attributes in @secattr.  Return zero on success, negatives values on
 * failure.
 *
 */
static int cipso_v4_parsetag_rbm(const struct cipso_v4_doi *doi_def,
				 const unsigned char *tag,
				 struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	u8 tag_len = tag[1];
	u32 level;

	ret_val = cipso_v4_map_lvl_ntoh(doi_def, tag[3], &level);
	if (ret_val != 0)
		return ret_val;
	secattr->mls_lvl = level;
	secattr->mls_lvl_vld = 1;

	if (tag_len > 4) {
		switch (doi_def->type) {
		case CIPSO_V4_MAP_PASS:
			secattr->mls_cat_len = tag_len - 4;
			break;
		case CIPSO_V4_MAP_STD:
			secattr->mls_cat_len =
				doi_def->map.std->cat.local_size;
			break;
		}
		secattr->mls_cat = kzalloc(secattr->mls_cat_len, GFP_ATOMIC);
		if (secattr->mls_cat == NULL)
			return -ENOMEM;

		ret_val = cipso_v4_map_cat_rbm_ntoh(doi_def,
						    &tag[4],
						    tag_len - 4,
						    secattr->mls_cat,
						    secattr->mls_cat_len);
		if (ret_val < 0) {
			kfree(secattr->mls_cat);
			return ret_val;
		}
		secattr->mls_cat_len = ret_val;
	}

	return 0;
}

/**
 * cipso_v4_validate - Validate a CIPSO option
 * @option: the start of the option, on error it is set to point to the error
 *
 * Description:
 * This routine is called to validate a CIPSO option, it checks all of the
 * fields to ensure that they are at least valid, see the draft snippet below
 * for details.  If the option is valid then a zero value is returned and
 * the value of @option is unchanged.  If the option is invalid then a
 * non-zero value is returned and @option is adjusted to point to the
 * offending portion of the option.  From the IETF draft ...
 *
 *  "If any field within the CIPSO options, such as the DOI identifier, is not
 *   recognized the IP datagram is discarded and an ICMP 'parameter problem'
 *   (type 12) is generated and returned.  The ICMP code field is set to 'bad
 *   parameter' (code 0) and the pointer is set to the start of the CIPSO field
 *   that is unrecognized."
 *
 */
int cipso_v4_validate(unsigned char **option)
{
	unsigned char *opt = *option;
	unsigned char *tag;
	unsigned char opt_iter;
	unsigned char err_offset = 0;
	u8 opt_len;
	u8 tag_len;
	struct cipso_v4_doi *doi_def = NULL;
	u32 tag_iter;

	/* caller already checks for length values that are too large */
	opt_len = opt[1];
	if (opt_len < 8) {
		err_offset = 1;
		goto validate_return;
	}

	rcu_read_lock();
	doi_def = cipso_v4_doi_getdef(ntohl(*((u32 *)&opt[2])));
	if (doi_def == NULL) {
		err_offset = 2;
		goto validate_return_locked;
	}

	opt_iter = 6;
	tag = opt + opt_iter;
	while (opt_iter < opt_len) {
		for (tag_iter = 0; doi_def->tags[tag_iter] != tag[0];)
			if (doi_def->tags[tag_iter] == CIPSO_V4_TAG_INVALID ||
			    ++tag_iter == CIPSO_V4_TAG_MAXCNT) {
				err_offset = opt_iter;
				goto validate_return_locked;
			}

		tag_len = tag[1];
		if (tag_len > (opt_len - opt_iter)) {
			err_offset = opt_iter + 1;
			goto validate_return_locked;
		}

		switch (tag[0]) {
		case CIPSO_V4_TAG_RBITMAP:
			if (tag_len < 4) {
				err_offset = opt_iter + 1;
				goto validate_return_locked;
			}

			/* We are already going to do all the verification
			 * necessary at the socket layer so from our point of
			 * view it is safe to turn these checks off (and less
			 * work), however, the CIPSO draft says we should do
			 * all the CIPSO validations here but it doesn't
			 * really specify _exactly_ what we need to validate
			 * ... so, just make it a sysctl tunable. */
			if (cipso_v4_rbm_strictvalid) {
				if (cipso_v4_map_lvl_valid(doi_def,
							   tag[3]) < 0) {
					err_offset = opt_iter + 3;
					goto validate_return_locked;
				}
				if (tag_len > 4 &&
				    cipso_v4_map_cat_rbm_valid(doi_def,
							    &tag[4],
							    tag_len - 4) < 0) {
					err_offset = opt_iter + 4;
					goto validate_return_locked;
				}
			}
			break;
		default:
			err_offset = opt_iter;
			goto validate_return_locked;
		}

		tag += tag_len;
		opt_iter += tag_len;
	}

validate_return_locked:
	rcu_read_unlock();
validate_return:
	*option = opt + err_offset;
	return err_offset;
}

/**
 * cipso_v4_error - Send the correct reponse for a bad packet
 * @skb: the packet
 * @error: the error code
 * @gateway: CIPSO gateway flag
 *
 * Description:
 * Based on the error code given in @error, send an ICMP error message back to
 * the originating host.  From the IETF draft ...
 *
 *  "If the contents of the CIPSO [option] are valid but the security label is
 *   outside of the configured host or port label range, the datagram is
 *   discarded and an ICMP 'destination unreachable' (type 3) is generated and
 *   returned.  The code field of the ICMP is set to 'communication with
 *   destination network administratively prohibited' (code 9) or to
 *   'communication with destination host administratively prohibited'
 *   (code 10).  The value of the code is dependent on whether the originator
 *   of the ICMP message is acting as a CIPSO host or a CIPSO gateway.  The
 *   recipient of the ICMP message MUST be able to handle either value.  The
 *   same procedure is performed if a CIPSO [option] can not be added to an
 *   IP packet because it is too large to fit in the IP options area."
 *
 *  "If the error is triggered by receipt of an ICMP message, the message is
 *   discarded and no response is permitted (consistent with general ICMP
 *   processing rules)."
 *
 */
void cipso_v4_error(struct sk_buff *skb, int error, u32 gateway)
{
	if (skb->nh.iph->protocol == IPPROTO_ICMP || error != -EACCES)
		return;

	if (gateway)
		icmp_send(skb, ICMP_DEST_UNREACH, ICMP_NET_ANO, 0);
	else
		icmp_send(skb, ICMP_DEST_UNREACH, ICMP_HOST_ANO, 0);
}

/**
 * cipso_v4_socket_setattr - Add a CIPSO option to a socket
 * @sock: the socket
 * @doi_def: the CIPSO DOI to use
 * @secattr: the specific security attributes of the socket
 *
 * Description:
 * Set the CIPSO option on the given socket using the DOI definition and
 * security attributes passed to the function.  This function requires
 * exclusive access to @sock->sk, which means it either needs to be in the
 * process of being created or locked via lock_sock(sock->sk).  Returns zero on
 * success and negative values on failure.
 *
 */
int cipso_v4_socket_setattr(const struct socket *sock,
			    const struct cipso_v4_doi *doi_def,
			    const struct netlbl_lsm_secattr *secattr)
{
	int ret_val = -EPERM;
	u32 iter;
	unsigned char *buf = NULL;
	u32 buf_len = 0;
	u32 opt_len;
	struct ip_options *opt = NULL;
	struct sock *sk;
	struct inet_sock *sk_inet;
	struct inet_connection_sock *sk_conn;

	/* In the case of sock_create_lite(), the sock->sk field is not
	 * defined yet but it is not a problem as the only users of these
	 * "lite" PF_INET sockets are functions which do an accept() call
	 * afterwards so we will label the socket as part of the accept(). */
	sk = sock->sk;
	if (sk == NULL)
		return 0;

	/* XXX - This code assumes only one tag per CIPSO option which isn't
	 * really a good assumption to make but since we only support the MAC
	 * tags right now it is a safe assumption. */
	iter = 0;
	do {
		switch (doi_def->tags[iter]) {
		case CIPSO_V4_TAG_RBITMAP:
			ret_val = cipso_v4_gentag_rbm(doi_def,
						      secattr,
						      &buf,
						      &buf_len);
			break;
		default:
			ret_val = -EPERM;
			goto socket_setattr_failure;
		}

		iter++;
	} while (ret_val != 0 &&
		 iter < CIPSO_V4_TAG_MAXCNT &&
		 doi_def->tags[iter] != CIPSO_V4_TAG_INVALID);
	if (ret_val != 0)
		goto socket_setattr_failure;

	/* We can't use ip_options_get() directly because it makes a call to
	 * ip_options_get_alloc() which allocates memory with GFP_KERNEL and
	 * we can't block here. */
	opt_len = (buf_len + 3) & ~3;
	opt = kzalloc(sizeof(*opt) + opt_len, GFP_ATOMIC);
	if (opt == NULL) {
		ret_val = -ENOMEM;
		goto socket_setattr_failure;
	}
	memcpy(opt->__data, buf, buf_len);
	opt->optlen = opt_len;
	opt->is_data = 1;
	kfree(buf);
	buf = NULL;
	ret_val = ip_options_compile(opt, NULL);
	if (ret_val != 0)
		goto socket_setattr_failure;

	sk_inet = inet_sk(sk);
	if (sk_inet->is_icsk) {
		sk_conn = inet_csk(sk);
		if (sk_inet->opt)
			sk_conn->icsk_ext_hdr_len -= sk_inet->opt->optlen;
		sk_conn->icsk_ext_hdr_len += opt->optlen;
		sk_conn->icsk_sync_mss(sk, sk_conn->icsk_pmtu_cookie);
	}
	opt = xchg(&sk_inet->opt, opt);
	kfree(opt);

	return 0;

socket_setattr_failure:
	kfree(buf);
	kfree(opt);
	return ret_val;
}

/**
 * cipso_v4_socket_getattr - Get the security attributes from a socket
 * @sock: the socket
 * @secattr: the security attributes
 *
 * Description:
 * Query @sock to see if there is a CIPSO option attached to the socket and if
 * there is return the CIPSO security attributes in @secattr.  Returns zero on
 * success and negative values on failure.
 *
 */
int cipso_v4_socket_getattr(const struct socket *sock,
			    struct netlbl_lsm_secattr *secattr)
{
	int ret_val = -ENOMSG;
	struct sock *sk;
	struct inet_sock *sk_inet;
	unsigned char *cipso_ptr;
	u32 doi;
	struct cipso_v4_doi *doi_def;

	sk = sock->sk;
	lock_sock(sk);
	sk_inet = inet_sk(sk);
	if (sk_inet->opt == NULL || sk_inet->opt->cipso == 0)
		goto socket_getattr_return;
	cipso_ptr = sk_inet->opt->__data + sk_inet->opt->cipso -
		sizeof(struct iphdr);
	ret_val = cipso_v4_cache_check(cipso_ptr, cipso_ptr[1], secattr);
	if (ret_val == 0)
		goto socket_getattr_return;

	doi = ntohl(*(u32 *)&cipso_ptr[2]);
	rcu_read_lock();
	doi_def = cipso_v4_doi_getdef(doi);
	if (doi_def == NULL) {
		rcu_read_unlock();
		goto socket_getattr_return;
	}
	switch (cipso_ptr[6]) {
	case CIPSO_V4_TAG_RBITMAP:
		ret_val = cipso_v4_parsetag_rbm(doi_def,
						&cipso_ptr[6],
						secattr);
		break;
	}
	rcu_read_unlock();

socket_getattr_return:
	release_sock(sk);
	return ret_val;
}

/**
 * cipso_v4_skbuff_getattr - Get the security attributes from the CIPSO option
 * @skb: the packet
 * @secattr: the security attributes
 *
 * Description:
 * Parse the given packet's CIPSO option and return the security attributes.
 * Returns zero on success and negative values on failure.
 *
 */
int cipso_v4_skbuff_getattr(const struct sk_buff *skb,
			    struct netlbl_lsm_secattr *secattr)
{
	int ret_val = -ENOMSG;
	unsigned char *cipso_ptr;
	u32 doi;
	struct cipso_v4_doi *doi_def;

	if (!CIPSO_V4_OPTEXIST(skb))
		return -ENOMSG;
	cipso_ptr = CIPSO_V4_OPTPTR(skb);
	if (cipso_v4_cache_check(cipso_ptr, cipso_ptr[1], secattr) == 0)
		return 0;

	doi = ntohl(*(u32 *)&cipso_ptr[2]);
	rcu_read_lock();
	doi_def = cipso_v4_doi_getdef(doi);
	if (doi_def == NULL)
		goto skbuff_getattr_return;
	switch (cipso_ptr[6]) {
	case CIPSO_V4_TAG_RBITMAP:
		ret_val = cipso_v4_parsetag_rbm(doi_def,
						&cipso_ptr[6],
						secattr);
		break;
	}

skbuff_getattr_return:
	rcu_read_unlock();
	return ret_val;
}

/*
 * Setup Functions
 */

/**
 * cipso_v4_init - Initialize the CIPSO module
 *
 * Description:
 * Initialize the CIPSO module and prepare it for use.  Returns zero on success
 * and negative values on failure.
 *
 */
static int __init cipso_v4_init(void)
{
	int ret_val;

	ret_val = cipso_v4_cache_init();
	if (ret_val != 0)
		panic("Failed to initialize the CIPSO/IPv4 cache (%d)\n",
		      ret_val);

	return 0;
}

subsys_initcall(cipso_v4_init);
