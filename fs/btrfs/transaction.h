#ifndef __TRANSACTION__
#define __TRANSACTION__
#include "btrfs_inode.h"

struct btrfs_transaction {
	u64 transid;
	unsigned long num_writers;
	int in_commit;
	int use_count;
	int commit_done;
	struct list_head list;
	struct radix_tree_root dirty_pages;
	unsigned long start_time;
	wait_queue_head_t writer_wait;
	wait_queue_head_t commit_wait;
};

struct btrfs_trans_handle {
	u64 transid;
	unsigned long blocks_reserved;
	unsigned long blocks_used;
	struct btrfs_transaction *transaction;
	struct btrfs_block_group_cache *block_group;
};


static inline void btrfs_set_trans_block_group(struct btrfs_trans_handle *trans,
					       struct inode *inode)
{
	trans->block_group = BTRFS_I(inode)->block_group;
}

static inline void btrfs_update_inode_block_group(struct
						  btrfs_trans_handle *trans,
						  struct inode *inode)
{
	BTRFS_I(inode)->block_group = trans->block_group;
}

int btrfs_end_transaction(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root);
struct btrfs_trans_handle *btrfs_start_transaction(struct btrfs_root *root,
						   int num_blocks);
int btrfs_write_and_wait_transaction(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root);
int btrfs_commit_tree_roots(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root);

void btrfs_transaction_cleaner(struct work_struct *work);
void btrfs_transaction_flush_work(struct btrfs_root *root);
void btrfs_transaction_queue_work(struct btrfs_root *root, int delay);
void btrfs_init_transaction_sys(void);
void btrfs_exit_transaction_sys(void);

#endif
