/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2008 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * Cross Partition Communication (XPC) sn2-based functions.
 *
 *     Architecture specific implementation of common functions.
 *
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/uncached.h>
#include <asm/sn/sn_sal.h>
#include "xpc.h"

static struct xpc_vars_sn2 *xpc_vars;	/* >>> Add _sn2 suffix? */
static struct xpc_vars_part_sn2 *xpc_vars_part; /* >>> Add _sn2 suffix? */

/* SH_IPI_ACCESS shub register value on startup */
static u64 xpc_sh1_IPI_access;
static u64 xpc_sh2_IPI_access0;
static u64 xpc_sh2_IPI_access1;
static u64 xpc_sh2_IPI_access2;
static u64 xpc_sh2_IPI_access3;

/*
 * Change protections to allow IPI operations.
 */
static void
xpc_allow_IPI_ops_sn2(void)
{
	int node;
	int nasid;

	/* >>> The following should get moved into SAL. */
	if (is_shub2()) {
		xpc_sh2_IPI_access0 =
		    (u64)HUB_L((u64 *)LOCAL_MMR_ADDR(SH2_IPI_ACCESS0));
		xpc_sh2_IPI_access1 =
		    (u64)HUB_L((u64 *)LOCAL_MMR_ADDR(SH2_IPI_ACCESS1));
		xpc_sh2_IPI_access2 =
		    (u64)HUB_L((u64 *)LOCAL_MMR_ADDR(SH2_IPI_ACCESS2));
		xpc_sh2_IPI_access3 =
		    (u64)HUB_L((u64 *)LOCAL_MMR_ADDR(SH2_IPI_ACCESS3));

		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS0),
			      -1UL);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS1),
			      -1UL);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS2),
			      -1UL);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS3),
			      -1UL);
		}
	} else {
		xpc_sh1_IPI_access =
		    (u64)HUB_L((u64 *)LOCAL_MMR_ADDR(SH1_IPI_ACCESS));

		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH1_IPI_ACCESS),
			      -1UL);
		}
	}
}

/*
 * Restrict protections to disallow IPI operations.
 */
static void
xpc_disallow_IPI_ops_sn2(void)
{
	int node;
	int nasid;

	/* >>> The following should get moved into SAL. */
	if (is_shub2()) {
		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS0),
			      xpc_sh2_IPI_access0);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS1),
			      xpc_sh2_IPI_access1);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS2),
			      xpc_sh2_IPI_access2);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH2_IPI_ACCESS3),
			      xpc_sh2_IPI_access3);
		}
	} else {
		for_each_online_node(node) {
			nasid = cnodeid_to_nasid(node);
			HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid, SH1_IPI_ACCESS),
			      xpc_sh1_IPI_access);
		}
	}
}

/*
 * The following set of functions are used for the sending and receiving of
 * IRQs (also known as IPIs). There are two flavors of IRQs, one that is
 * associated with partition activity (SGI_XPC_ACTIVATE) and the other that
 * is associated with channel activity (SGI_XPC_NOTIFY).
 */

static u64
xpc_receive_IRQ_amo_sn2(struct amo *amo)
{
	return FETCHOP_LOAD_OP(TO_AMO((u64)&amo->variable), FETCHOP_CLEAR);
}

static enum xp_retval
xpc_send_IRQ_sn2(struct amo *amo, u64 flag, int nasid, int phys_cpuid,
		 int vector)
{
	int ret = 0;
	unsigned long irq_flags;

	local_irq_save(irq_flags);

	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_OR, flag);
	sn_send_IPI_phys(nasid, phys_cpuid, vector, 0);

	/*
	 * We must always use the nofault function regardless of whether we
	 * are on a Shub 1.1 system or a Shub 1.2 slice 0xc processor. If we
	 * didn't, we'd never know that the other partition is down and would
	 * keep sending IRQs and amos to it until the heartbeat times out.
	 */
	ret = xp_nofault_PIOR((u64 *)GLOBAL_MMR_ADDR(NASID_GET(&amo->variable),
						     xp_nofault_PIOR_target));

	local_irq_restore(irq_flags);

	return ((ret == 0) ? xpSuccess : xpPioReadError);
}

static struct amo *
xpc_init_IRQ_amo_sn2(int index)
{
	struct amo *amo = xpc_vars->amos_page + index;

	(void)xpc_receive_IRQ_amo_sn2(amo);	/* clear amo variable */
	return amo;
}

/*
 * Functions associated with SGI_XPC_ACTIVATE IRQ.
 */

/*
 * Notify the heartbeat check thread that an activate IRQ has been received.
 */
static irqreturn_t
xpc_handle_activate_IRQ_sn2(int irq, void *dev_id)
{
	atomic_inc(&xpc_activate_IRQ_rcvd);
	wake_up_interruptible(&xpc_activate_IRQ_wq);
	return IRQ_HANDLED;
}

/*
 * Flag the appropriate amo variable and send an IRQ to the specified node.
 */
static void
xpc_send_activate_IRQ_sn2(u64 amos_page_pa, int from_nasid, int to_nasid,
			  int to_phys_cpuid)
{
	int w_index = XPC_NASID_W_INDEX(from_nasid);
	int b_index = XPC_NASID_B_INDEX(from_nasid);
	struct amo *amos = (struct amo *)__va(amos_page_pa +
					      (XPC_ACTIVATE_IRQ_AMOS *
					      sizeof(struct amo)));

	(void)xpc_send_IRQ_sn2(&amos[w_index], (1UL << b_index), to_nasid,
			       to_phys_cpuid, SGI_XPC_ACTIVATE);
}

static void
xpc_send_local_activate_IRQ_sn2(int from_nasid)
{
	int w_index = XPC_NASID_W_INDEX(from_nasid);
	int b_index = XPC_NASID_B_INDEX(from_nasid);
	struct amo *amos = (struct amo *)__va(xpc_vars->amos_page_pa +
					      (XPC_ACTIVATE_IRQ_AMOS *
					      sizeof(struct amo)));

	/* fake the sending and receipt of an activate IRQ from remote nasid */
	FETCHOP_STORE_OP(TO_AMO((u64)&amos[w_index].variable), FETCHOP_OR,
			 (1UL << b_index));
	atomic_inc(&xpc_activate_IRQ_rcvd);
	wake_up_interruptible(&xpc_activate_IRQ_wq);
}

/*
 * Functions associated with SGI_XPC_NOTIFY IRQ.
 */

/*
 * Check to see if any chctl flags were sent from the specified partition.
 */
static void
xpc_check_for_sent_chctl_flags_sn2(struct xpc_partition *part)
{
	union xpc_channel_ctl_flags chctl;
	unsigned long irq_flags;

	chctl.all_flags = xpc_receive_IRQ_amo_sn2(part->sn.sn2.
						  local_chctl_amo_va);
	if (chctl.all_flags == 0)
		return;

	spin_lock_irqsave(&part->chctl_lock, irq_flags);
	part->chctl.all_flags |= chctl.all_flags;
	spin_unlock_irqrestore(&part->chctl_lock, irq_flags);

	dev_dbg(xpc_chan, "received notify IRQ from partid=%d, chctl.all_flags="
		"0x%lx\n", XPC_PARTID(part), chctl.all_flags);

	xpc_wakeup_channel_mgr(part);
}

/*
 * Handle the receipt of a SGI_XPC_NOTIFY IRQ by seeing whether the specified
 * partition actually sent it. Since SGI_XPC_NOTIFY IRQs may be shared by more
 * than one partition, we use an amo structure per partition to indicate
 * whether a partition has sent an IRQ or not.  If it has, then wake up the
 * associated kthread to handle it.
 *
 * All SGI_XPC_NOTIFY IRQs received by XPC are the result of IRQs sent by XPC
 * running on other partitions.
 *
 * Noteworthy Arguments:
 *
 *	irq - Interrupt ReQuest number. NOT USED.
 *
 *	dev_id - partid of IRQ's potential sender.
 */
static irqreturn_t
xpc_handle_notify_IRQ_sn2(int irq, void *dev_id)
{
	short partid = (short)(u64)dev_id;
	struct xpc_partition *part = &xpc_partitions[partid];

	DBUG_ON(partid < 0 || partid >= xp_max_npartitions);

	if (xpc_part_ref(part)) {
		xpc_check_for_sent_chctl_flags_sn2(part);

		xpc_part_deref(part);
	}
	return IRQ_HANDLED;
}

/*
 * Check to see if xpc_handle_notify_IRQ_sn2() dropped any IRQs on the floor
 * because the write to their associated amo variable completed after the IRQ
 * was received.
 */
static void
xpc_check_for_dropped_notify_IRQ_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;

	if (xpc_part_ref(part)) {
		xpc_check_for_sent_chctl_flags_sn2(part);

		part_sn2->dropped_notify_IRQ_timer.expires = jiffies +
		    XPC_DROPPED_NOTIFY_IRQ_WAIT_INTERVAL;
		add_timer(&part_sn2->dropped_notify_IRQ_timer);
		xpc_part_deref(part);
	}
}

/*
 * Send a notify IRQ to the remote partition that is associated with the
 * specified channel.
 */
static void
xpc_send_notify_IRQ_sn2(struct xpc_channel *ch, u8 chctl_flag,
			char *chctl_flag_string, unsigned long *irq_flags)
{
	struct xpc_partition *part = &xpc_partitions[ch->partid];
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	union xpc_channel_ctl_flags chctl = { 0 };
	enum xp_retval ret;

	if (likely(part->act_state != XPC_P_DEACTIVATING)) {
		chctl.flags[ch->number] = chctl_flag;
		ret = xpc_send_IRQ_sn2(part_sn2->remote_chctl_amo_va,
				       chctl.all_flags,
				       part_sn2->notify_IRQ_nasid,
				       part_sn2->notify_IRQ_phys_cpuid,
				       SGI_XPC_NOTIFY);
		dev_dbg(xpc_chan, "%s sent to partid=%d, channel=%d, ret=%d\n",
			chctl_flag_string, ch->partid, ch->number, ret);
		if (unlikely(ret != xpSuccess)) {
			if (irq_flags != NULL)
				spin_unlock_irqrestore(&ch->lock, *irq_flags);
			XPC_DEACTIVATE_PARTITION(part, ret);
			if (irq_flags != NULL)
				spin_lock_irqsave(&ch->lock, *irq_flags);
		}
	}
}

#define XPC_SEND_NOTIFY_IRQ_SN2(_ch, _ipi_f, _irq_f) \
		xpc_send_notify_IRQ_sn2(_ch, _ipi_f, #_ipi_f, _irq_f)

/*
 * Make it look like the remote partition, which is associated with the
 * specified channel, sent us a notify IRQ. This faked IRQ will be handled
 * by xpc_check_for_dropped_notify_IRQ_sn2().
 */
static void
xpc_send_local_notify_IRQ_sn2(struct xpc_channel *ch, u8 chctl_flag,
			      char *chctl_flag_string)
{
	struct xpc_partition *part = &xpc_partitions[ch->partid];
	union xpc_channel_ctl_flags chctl = { 0 };

	chctl.flags[ch->number] = chctl_flag;
	FETCHOP_STORE_OP(TO_AMO((u64)&part->sn.sn2.local_chctl_amo_va->
				variable), FETCHOP_OR, chctl.all_flags);
	dev_dbg(xpc_chan, "%s sent local from partid=%d, channel=%d\n",
		chctl_flag_string, ch->partid, ch->number);
}

#define XPC_SEND_LOCAL_NOTIFY_IRQ_SN2(_ch, _ipi_f) \
		xpc_send_local_notify_IRQ_sn2(_ch, _ipi_f, #_ipi_f)

static void
xpc_send_chctl_closerequest_sn2(struct xpc_channel *ch,
				unsigned long *irq_flags)
{
	struct xpc_openclose_args *args = ch->local_openclose_args;

	args->reason = ch->reason;
	XPC_SEND_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_CLOSEREQUEST, irq_flags);
}

static void
xpc_send_chctl_closereply_sn2(struct xpc_channel *ch, unsigned long *irq_flags)
{
	XPC_SEND_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_CLOSEREPLY, irq_flags);
}

static void
xpc_send_chctl_openrequest_sn2(struct xpc_channel *ch, unsigned long *irq_flags)
{
	struct xpc_openclose_args *args = ch->local_openclose_args;

	args->msg_size = ch->msg_size;
	args->local_nentries = ch->local_nentries;
	XPC_SEND_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_OPENREQUEST, irq_flags);
}

static void
xpc_send_chctl_openreply_sn2(struct xpc_channel *ch, unsigned long *irq_flags)
{
	struct xpc_openclose_args *args = ch->local_openclose_args;

	args->remote_nentries = ch->remote_nentries;
	args->local_nentries = ch->local_nentries;
	args->local_msgqueue_pa = __pa(ch->local_msgqueue);
	XPC_SEND_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_OPENREPLY, irq_flags);
}

static void
xpc_send_chctl_msgrequest_sn2(struct xpc_channel *ch)
{
	XPC_SEND_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_MSGREQUEST, NULL);
}

static void
xpc_send_chctl_local_msgrequest_sn2(struct xpc_channel *ch)
{
	XPC_SEND_LOCAL_NOTIFY_IRQ_SN2(ch, XPC_CHCTL_MSGREQUEST);
}

/*
 * This next set of functions are used to keep track of when a partition is
 * potentially engaged in accessing memory belonging to another partition.
 */

static void
xpc_indicate_partition_engaged_sn2(struct xpc_partition *part)
{
	unsigned long irq_flags;
	struct amo *amo = (struct amo *)__va(part->sn.sn2.remote_amos_page_pa +
					     (XPC_ENGAGED_PARTITIONS_AMO *
					     sizeof(struct amo)));

	local_irq_save(irq_flags);

	/* set bit corresponding to our partid in remote partition's amo */
	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_OR,
			 (1UL << sn_partition_id));
	/*
	 * We must always use the nofault function regardless of whether we
	 * are on a Shub 1.1 system or a Shub 1.2 slice 0xc processor. If we
	 * didn't, we'd never know that the other partition is down and would
	 * keep sending IRQs and amos to it until the heartbeat times out.
	 */
	(void)xp_nofault_PIOR((u64 *)GLOBAL_MMR_ADDR(NASID_GET(&amo->
							       variable),
						     xp_nofault_PIOR_target));

	local_irq_restore(irq_flags);
}

static void
xpc_indicate_partition_disengaged_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	unsigned long irq_flags;
	struct amo *amo = (struct amo *)__va(part_sn2->remote_amos_page_pa +
					     (XPC_ENGAGED_PARTITIONS_AMO *
					     sizeof(struct amo)));

	local_irq_save(irq_flags);

	/* clear bit corresponding to our partid in remote partition's amo */
	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_AND,
			 ~(1UL << sn_partition_id));
	/*
	 * We must always use the nofault function regardless of whether we
	 * are on a Shub 1.1 system or a Shub 1.2 slice 0xc processor. If we
	 * didn't, we'd never know that the other partition is down and would
	 * keep sending IRQs and amos to it until the heartbeat times out.
	 */
	(void)xp_nofault_PIOR((u64 *)GLOBAL_MMR_ADDR(NASID_GET(&amo->
							       variable),
						     xp_nofault_PIOR_target));

	local_irq_restore(irq_flags);

	/*
	 * Send activate IRQ to get other side to see that we've cleared our
	 * bit in their engaged partitions amo.
	 */
	xpc_send_activate_IRQ_sn2(part_sn2->remote_amos_page_pa,
				  cnodeid_to_nasid(0),
				  part_sn2->activate_IRQ_nasid,
				  part_sn2->activate_IRQ_phys_cpuid);
}

static int
xpc_partition_engaged_sn2(short partid)
{
	struct amo *amo = xpc_vars->amos_page + XPC_ENGAGED_PARTITIONS_AMO;

	/* our partition's amo variable ANDed with partid mask */
	return (FETCHOP_LOAD_OP(TO_AMO((u64)&amo->variable), FETCHOP_LOAD) &
		(1UL << partid)) != 0;
}

static int
xpc_any_partition_engaged_sn2(void)
{
	struct amo *amo = xpc_vars->amos_page + XPC_ENGAGED_PARTITIONS_AMO;

	/* our partition's amo variable */
	return FETCHOP_LOAD_OP(TO_AMO((u64)&amo->variable), FETCHOP_LOAD) != 0;
}

static void
xpc_assume_partition_disengaged_sn2(short partid)
{
	struct amo *amo = xpc_vars->amos_page + XPC_ENGAGED_PARTITIONS_AMO;

	/* clear bit(s) based on partid mask in our partition's amo */
	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_AND,
			 ~(1UL << partid));
}

/* original protection values for each node */
static u64 xpc_prot_vec_sn2[MAX_NUMNODES];

/*
 * Change protections to allow amo operations on non-Shub 1.1 systems.
 */
static enum xp_retval
xpc_allow_amo_ops_sn2(struct amo *amos_page)
{
	u64 nasid_array = 0;
	int ret;

	/*
	 * On SHUB 1.1, we cannot call sn_change_memprotect() since the BIST
	 * collides with memory operations. On those systems we call
	 * xpc_allow_amo_ops_shub_wars_1_1_sn2() instead.
	 */
	if (!enable_shub_wars_1_1()) {
		ret = sn_change_memprotect(ia64_tpa((u64)amos_page), PAGE_SIZE,
					   SN_MEMPROT_ACCESS_CLASS_1,
					   &nasid_array);
		if (ret != 0)
			return xpSalError;
	}
	return xpSuccess;
}

/*
 * Change protections to allow amo operations on Shub 1.1 systems.
 */
static void
xpc_allow_amo_ops_shub_wars_1_1_sn2(void)
{
	int node;
	int nasid;

	if (!enable_shub_wars_1_1())
		return;

	for_each_online_node(node) {
		nasid = cnodeid_to_nasid(node);
		/* save current protection values */
		xpc_prot_vec_sn2[node] =
		    (u64)HUB_L((u64 *)GLOBAL_MMR_ADDR(nasid,
						  SH1_MD_DQLP_MMR_DIR_PRIVEC0));
		/* open up everything */
		HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid,
					     SH1_MD_DQLP_MMR_DIR_PRIVEC0),
		      -1UL);
		HUB_S((u64 *)GLOBAL_MMR_ADDR(nasid,
					     SH1_MD_DQRP_MMR_DIR_PRIVEC0),
		      -1UL);
	}
}

static enum xp_retval
xpc_rsvd_page_init_sn2(struct xpc_rsvd_page *rp)
{
	struct amo *amos_page;
	int i;
	int ret;

	xpc_vars = XPC_RP_VARS(rp);

	rp->sn.vars_pa = __pa(xpc_vars);

	/* vars_part array follows immediately after vars */
	xpc_vars_part = (struct xpc_vars_part_sn2 *)((u8 *)XPC_RP_VARS(rp) +
						     XPC_RP_VARS_SIZE);

	/*
	 * Before clearing xpc_vars, see if a page of amos had been previously
	 * allocated. If not we'll need to allocate one and set permissions
	 * so that cross-partition amos are allowed.
	 *
	 * The allocated amo page needs MCA reporting to remain disabled after
	 * XPC has unloaded.  To make this work, we keep a copy of the pointer
	 * to this page (i.e., amos_page) in the struct xpc_vars structure,
	 * which is pointed to by the reserved page, and re-use that saved copy
	 * on subsequent loads of XPC. This amo page is never freed, and its
	 * memory protections are never restricted.
	 */
	amos_page = xpc_vars->amos_page;
	if (amos_page == NULL) {
		amos_page = (struct amo *)TO_AMO(uncached_alloc_page(0, 1));
		if (amos_page == NULL) {
			dev_err(xpc_part, "can't allocate page of amos\n");
			return xpNoMemory;
		}

		/*
		 * Open up amo-R/W to cpu.  This is done on Shub 1.1 systems
		 * when xpc_allow_amo_ops_shub_wars_1_1_sn2() is called.
		 */
		ret = xpc_allow_amo_ops_sn2(amos_page);
		if (ret != xpSuccess) {
			dev_err(xpc_part, "can't allow amo operations\n");
			uncached_free_page(__IA64_UNCACHED_OFFSET |
					   TO_PHYS((u64)amos_page), 1);
			return ret;
		}
	}

	/* clear xpc_vars */
	memset(xpc_vars, 0, sizeof(struct xpc_vars_sn2));

	xpc_vars->version = XPC_V_VERSION;
	xpc_vars->activate_IRQ_nasid = cpuid_to_nasid(0);
	xpc_vars->activate_IRQ_phys_cpuid = cpu_physical_id(0);
	xpc_vars->vars_part_pa = __pa(xpc_vars_part);
	xpc_vars->amos_page_pa = ia64_tpa((u64)amos_page);
	xpc_vars->amos_page = amos_page;	/* save for next load of XPC */

	/* clear xpc_vars_part */
	memset((u64 *)xpc_vars_part, 0, sizeof(struct xpc_vars_part_sn2) *
	       xp_max_npartitions);

	/* initialize the activate IRQ related amo variables */
	for (i = 0; i < xp_nasid_mask_words; i++)
		(void)xpc_init_IRQ_amo_sn2(XPC_ACTIVATE_IRQ_AMOS + i);

	/* initialize the engaged remote partitions related amo variables */
	(void)xpc_init_IRQ_amo_sn2(XPC_ENGAGED_PARTITIONS_AMO);
	(void)xpc_init_IRQ_amo_sn2(XPC_DEACTIVATE_REQUEST_AMO);

	return xpSuccess;
}

static void
xpc_increment_heartbeat_sn2(void)
{
	xpc_vars->heartbeat++;
}

static void
xpc_offline_heartbeat_sn2(void)
{
	xpc_increment_heartbeat_sn2();
	xpc_vars->heartbeat_offline = 1;
}

static void
xpc_online_heartbeat_sn2(void)
{
	xpc_increment_heartbeat_sn2();
	xpc_vars->heartbeat_offline = 0;
}

static void
xpc_heartbeat_init_sn2(void)
{
	DBUG_ON(xpc_vars == NULL);

	bitmap_zero(xpc_vars->heartbeating_to_mask, XP_MAX_NPARTITIONS_SN2);
	xpc_heartbeating_to_mask = &xpc_vars->heartbeating_to_mask[0];
	xpc_online_heartbeat_sn2();
}

static void
xpc_heartbeat_exit_sn2(void)
{
	xpc_offline_heartbeat_sn2();
}

/*
 * At periodic intervals, scan through all active partitions and ensure
 * their heartbeat is still active.  If not, the partition is deactivated.
 */
static void
xpc_check_remote_hb_sn2(void)
{
	struct xpc_vars_sn2 *remote_vars;
	struct xpc_partition *part;
	short partid;
	enum xp_retval ret;

	remote_vars = (struct xpc_vars_sn2 *)xpc_remote_copy_buffer;

	for (partid = 0; partid < xp_max_npartitions; partid++) {

		if (xpc_exiting)
			break;

		if (partid == sn_partition_id)
			continue;

		part = &xpc_partitions[partid];

		if (part->act_state == XPC_P_INACTIVE ||
		    part->act_state == XPC_P_DEACTIVATING) {
			continue;
		}

		/* pull the remote_hb cache line */
		ret = xp_remote_memcpy(remote_vars,
				       (void *)part->sn.sn2.remote_vars_pa,
				       XPC_RP_VARS_SIZE);
		if (ret != xpSuccess) {
			XPC_DEACTIVATE_PARTITION(part, ret);
			continue;
		}

		dev_dbg(xpc_part, "partid = %d, heartbeat = %ld, last_heartbeat"
			" = %ld, heartbeat_offline = %ld, HB_mask[0] = 0x%lx\n",
			partid, remote_vars->heartbeat, part->last_heartbeat,
			remote_vars->heartbeat_offline,
			remote_vars->heartbeating_to_mask[0]);

		if (((remote_vars->heartbeat == part->last_heartbeat) &&
		     (remote_vars->heartbeat_offline == 0)) ||
		    !xpc_hb_allowed(sn_partition_id,
				    &remote_vars->heartbeating_to_mask)) {

			XPC_DEACTIVATE_PARTITION(part, xpNoHeartbeat);
			continue;
		}

		part->last_heartbeat = remote_vars->heartbeat;
	}
}

/*
 * Get a copy of the remote partition's XPC variables from the reserved page.
 *
 * remote_vars points to a buffer that is cacheline aligned for BTE copies and
 * assumed to be of size XPC_RP_VARS_SIZE.
 */
static enum xp_retval
xpc_get_remote_vars_sn2(u64 remote_vars_pa, struct xpc_vars_sn2 *remote_vars)
{
	enum xp_retval ret;

	if (remote_vars_pa == 0)
		return xpVarsNotSet;

	/* pull over the cross partition variables */
	ret = xp_remote_memcpy(remote_vars, (void *)remote_vars_pa,
			       XPC_RP_VARS_SIZE);
	if (ret != xpSuccess)
		return ret;

	if (XPC_VERSION_MAJOR(remote_vars->version) !=
	    XPC_VERSION_MAJOR(XPC_V_VERSION)) {
		return xpBadVersion;
	}

	return xpSuccess;
}

static void
xpc_request_partition_activation_sn2(struct xpc_rsvd_page *remote_rp,
				     u64 remote_rp_pa, int nasid)
{
	xpc_send_local_activate_IRQ_sn2(nasid);
}

static void
xpc_request_partition_reactivation_sn2(struct xpc_partition *part)
{
	xpc_send_local_activate_IRQ_sn2(part->sn.sn2.activate_IRQ_nasid);
}

static void
xpc_request_partition_deactivation_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	unsigned long irq_flags;
	struct amo *amo = (struct amo *)__va(part_sn2->remote_amos_page_pa +
					     (XPC_DEACTIVATE_REQUEST_AMO *
					     sizeof(struct amo)));

	local_irq_save(irq_flags);

	/* set bit corresponding to our partid in remote partition's amo */
	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_OR,
			 (1UL << sn_partition_id));
	/*
	 * We must always use the nofault function regardless of whether we
	 * are on a Shub 1.1 system or a Shub 1.2 slice 0xc processor. If we
	 * didn't, we'd never know that the other partition is down and would
	 * keep sending IRQs and amos to it until the heartbeat times out.
	 */
	(void)xp_nofault_PIOR((u64 *)GLOBAL_MMR_ADDR(NASID_GET(&amo->
							       variable),
						     xp_nofault_PIOR_target));

	local_irq_restore(irq_flags);

	/*
	 * Send activate IRQ to get other side to see that we've set our
	 * bit in their deactivate request amo.
	 */
	xpc_send_activate_IRQ_sn2(part_sn2->remote_amos_page_pa,
				  cnodeid_to_nasid(0),
				  part_sn2->activate_IRQ_nasid,
				  part_sn2->activate_IRQ_phys_cpuid);
}

static void
xpc_cancel_partition_deactivation_request_sn2(struct xpc_partition *part)
{
	unsigned long irq_flags;
	struct amo *amo = (struct amo *)__va(part->sn.sn2.remote_amos_page_pa +
					     (XPC_DEACTIVATE_REQUEST_AMO *
					     sizeof(struct amo)));

	local_irq_save(irq_flags);

	/* clear bit corresponding to our partid in remote partition's amo */
	FETCHOP_STORE_OP(TO_AMO((u64)&amo->variable), FETCHOP_AND,
			 ~(1UL << sn_partition_id));
	/*
	 * We must always use the nofault function regardless of whether we
	 * are on a Shub 1.1 system or a Shub 1.2 slice 0xc processor. If we
	 * didn't, we'd never know that the other partition is down and would
	 * keep sending IRQs and amos to it until the heartbeat times out.
	 */
	(void)xp_nofault_PIOR((u64 *)GLOBAL_MMR_ADDR(NASID_GET(&amo->
							       variable),
						     xp_nofault_PIOR_target));

	local_irq_restore(irq_flags);
}

static int
xpc_partition_deactivation_requested_sn2(short partid)
{
	struct amo *amo = xpc_vars->amos_page + XPC_DEACTIVATE_REQUEST_AMO;

	/* our partition's amo variable ANDed with partid mask */
	return (FETCHOP_LOAD_OP(TO_AMO((u64)&amo->variable), FETCHOP_LOAD) &
		(1UL << partid)) != 0;
}

/*
 * Update the remote partition's info.
 */
static void
xpc_update_partition_info_sn2(struct xpc_partition *part, u8 remote_rp_version,
			      unsigned long *remote_rp_stamp, u64 remote_rp_pa,
			      u64 remote_vars_pa,
			      struct xpc_vars_sn2 *remote_vars)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;

	part->remote_rp_version = remote_rp_version;
	dev_dbg(xpc_part, "  remote_rp_version = 0x%016x\n",
		part->remote_rp_version);

	part->remote_rp_stamp = *remote_rp_stamp;
	dev_dbg(xpc_part, "  remote_rp_stamp = 0x%016lx\n",
		part->remote_rp_stamp);

	part->remote_rp_pa = remote_rp_pa;
	dev_dbg(xpc_part, "  remote_rp_pa = 0x%016lx\n", part->remote_rp_pa);

	part_sn2->remote_vars_pa = remote_vars_pa;
	dev_dbg(xpc_part, "  remote_vars_pa = 0x%016lx\n",
		part_sn2->remote_vars_pa);

	part->last_heartbeat = remote_vars->heartbeat;
	dev_dbg(xpc_part, "  last_heartbeat = 0x%016lx\n",
		part->last_heartbeat);

	part_sn2->remote_vars_part_pa = remote_vars->vars_part_pa;
	dev_dbg(xpc_part, "  remote_vars_part_pa = 0x%016lx\n",
		part_sn2->remote_vars_part_pa);

	part_sn2->activate_IRQ_nasid = remote_vars->activate_IRQ_nasid;
	dev_dbg(xpc_part, "  activate_IRQ_nasid = 0x%x\n",
		part_sn2->activate_IRQ_nasid);

	part_sn2->activate_IRQ_phys_cpuid =
	    remote_vars->activate_IRQ_phys_cpuid;
	dev_dbg(xpc_part, "  activate_IRQ_phys_cpuid = 0x%x\n",
		part_sn2->activate_IRQ_phys_cpuid);

	part_sn2->remote_amos_page_pa = remote_vars->amos_page_pa;
	dev_dbg(xpc_part, "  remote_amos_page_pa = 0x%lx\n",
		part_sn2->remote_amos_page_pa);

	part_sn2->remote_vars_version = remote_vars->version;
	dev_dbg(xpc_part, "  remote_vars_version = 0x%x\n",
		part_sn2->remote_vars_version);
}

/*
 * Prior code has determined the nasid which generated a activate IRQ.
 * Inspect that nasid to determine if its partition needs to be activated
 * or deactivated.
 *
 * A partition is considered "awaiting activation" if our partition
 * flags indicate it is not active and it has a heartbeat.  A
 * partition is considered "awaiting deactivation" if our partition
 * flags indicate it is active but it has no heartbeat or it is not
 * sending its heartbeat to us.
 *
 * To determine the heartbeat, the remote nasid must have a properly
 * initialized reserved page.
 */
static void
xpc_identify_activate_IRQ_req_sn2(int nasid)
{
	struct xpc_rsvd_page *remote_rp;
	struct xpc_vars_sn2 *remote_vars;
	u64 remote_rp_pa;
	u64 remote_vars_pa;
	int remote_rp_version;
	int reactivate = 0;
	unsigned long remote_rp_stamp = 0;
	short partid;
	struct xpc_partition *part;
	struct xpc_partition_sn2 *part_sn2;
	enum xp_retval ret;

	/* pull over the reserved page structure */

	remote_rp = (struct xpc_rsvd_page *)xpc_remote_copy_buffer;

	ret = xpc_get_remote_rp(nasid, NULL, remote_rp, &remote_rp_pa);
	if (ret != xpSuccess) {
		dev_warn(xpc_part, "unable to get reserved page from nasid %d, "
			 "which sent interrupt, reason=%d\n", nasid, ret);
		return;
	}

	remote_vars_pa = remote_rp->sn.vars_pa;
	remote_rp_version = remote_rp->version;
	remote_rp_stamp = remote_rp->stamp;

	partid = remote_rp->SAL_partid;
	part = &xpc_partitions[partid];
	part_sn2 = &part->sn.sn2;

	/* pull over the cross partition variables */

	remote_vars = (struct xpc_vars_sn2 *)xpc_remote_copy_buffer;

	ret = xpc_get_remote_vars_sn2(remote_vars_pa, remote_vars);
	if (ret != xpSuccess) {
		dev_warn(xpc_part, "unable to get XPC variables from nasid %d, "
			 "which sent interrupt, reason=%d\n", nasid, ret);

		XPC_DEACTIVATE_PARTITION(part, ret);
		return;
	}

	part->activate_IRQ_rcvd++;

	dev_dbg(xpc_part, "partid for nasid %d is %d; IRQs = %d; HB = "
		"%ld:0x%lx\n", (int)nasid, (int)partid, part->activate_IRQ_rcvd,
		remote_vars->heartbeat, remote_vars->heartbeating_to_mask[0]);

	if (xpc_partition_disengaged(part) &&
	    part->act_state == XPC_P_INACTIVE) {

		xpc_update_partition_info_sn2(part, remote_rp_version,
					      &remote_rp_stamp, remote_rp_pa,
					      remote_vars_pa, remote_vars);

		if (xpc_partition_deactivation_requested_sn2(partid)) {
			/*
			 * Other side is waiting on us to deactivate even though
			 * we already have.
			 */
			return;
		}

		xpc_activate_partition(part);
		return;
	}

	DBUG_ON(part->remote_rp_version == 0);
	DBUG_ON(part_sn2->remote_vars_version == 0);

	if (remote_rp_stamp != part->remote_rp_stamp) {

		/* the other side rebooted */

		DBUG_ON(xpc_partition_engaged_sn2(partid));
		DBUG_ON(xpc_partition_deactivation_requested_sn2(partid));

		xpc_update_partition_info_sn2(part, remote_rp_version,
					      &remote_rp_stamp, remote_rp_pa,
					      remote_vars_pa, remote_vars);
		reactivate = 1;
	}

	if (part->disengage_timeout > 0 && !xpc_partition_disengaged(part)) {
		/* still waiting on other side to disengage from us */
		return;
	}

	if (reactivate)
		XPC_DEACTIVATE_PARTITION(part, xpReactivating);
	else if (xpc_partition_deactivation_requested_sn2(partid))
		XPC_DEACTIVATE_PARTITION(part, xpOtherGoingDown);
}

/*
 * Loop through the activation amo variables and process any bits
 * which are set.  Each bit indicates a nasid sending a partition
 * activation or deactivation request.
 *
 * Return #of IRQs detected.
 */
int
xpc_identify_activate_IRQ_sender_sn2(void)
{
	int word, bit;
	u64 nasid_mask;
	u64 nasid;		/* remote nasid */
	int n_IRQs_detected = 0;
	struct amo *act_amos;

	act_amos = xpc_vars->amos_page + XPC_ACTIVATE_IRQ_AMOS;

	/* scan through act amo variable looking for non-zero entries */
	for (word = 0; word < xp_nasid_mask_words; word++) {

		if (xpc_exiting)
			break;

		nasid_mask = xpc_receive_IRQ_amo_sn2(&act_amos[word]);
		if (nasid_mask == 0) {
			/* no IRQs from nasids in this variable */
			continue;
		}

		dev_dbg(xpc_part, "amo[%d] gave back 0x%lx\n", word,
			nasid_mask);

		/*
		 * If this nasid has been added to the machine since
		 * our partition was reset, this will retain the
		 * remote nasid in our reserved pages machine mask.
		 * This is used in the event of module reload.
		 */
		xpc_mach_nasids[word] |= nasid_mask;

		/* locate the nasid(s) which sent interrupts */

		for (bit = 0; bit < (8 * sizeof(u64)); bit++) {
			if (nasid_mask & (1UL << bit)) {
				n_IRQs_detected++;
				nasid = XPC_NASID_FROM_W_B(word, bit);
				dev_dbg(xpc_part, "interrupt from nasid %ld\n",
					nasid);
				xpc_identify_activate_IRQ_req_sn2(nasid);
			}
		}
	}
	return n_IRQs_detected;
}

static void
xpc_process_activate_IRQ_rcvd_sn2(int n_IRQs_expected)
{
	int n_IRQs_detected;

	n_IRQs_detected = xpc_identify_activate_IRQ_sender_sn2();
	if (n_IRQs_detected < n_IRQs_expected) {
		/* retry once to help avoid missing amo */
		(void)xpc_identify_activate_IRQ_sender_sn2();
	}
}

/*
 * Guarantee that the kzalloc'd memory is cacheline aligned.
 */
static void *
xpc_kzalloc_cacheline_aligned_sn2(size_t size, gfp_t flags, void **base)
{
	/* see if kzalloc will give us cachline aligned memory by default */
	*base = kzalloc(size, flags);
	if (*base == NULL)
		return NULL;

	if ((u64)*base == L1_CACHE_ALIGN((u64)*base))
		return *base;

	kfree(*base);

	/* nope, we'll have to do it ourselves */
	*base = kzalloc(size + L1_CACHE_BYTES, flags);
	if (*base == NULL)
		return NULL;

	return (void *)L1_CACHE_ALIGN((u64)*base);
}

/*
 * Setup the infrastructure necessary to support XPartition Communication
 * between the specified remote partition and the local one.
 */
static enum xp_retval
xpc_setup_infrastructure_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	enum xp_retval retval;
	int ret;
	int cpuid;
	int ch_number;
	struct xpc_channel *ch;
	struct timer_list *timer;
	short partid = XPC_PARTID(part);

	/*
	 * Allocate all of the channel structures as a contiguous chunk of
	 * memory.
	 */
	DBUG_ON(part->channels != NULL);
	part->channels = kzalloc(sizeof(struct xpc_channel) * XPC_MAX_NCHANNELS,
				 GFP_KERNEL);
	if (part->channels == NULL) {
		dev_err(xpc_chan, "can't get memory for channels\n");
		return xpNoMemory;
	}

	/* allocate all the required GET/PUT values */

	part_sn2->local_GPs =
	    xpc_kzalloc_cacheline_aligned_sn2(XPC_GP_SIZE, GFP_KERNEL,
					      &part_sn2->local_GPs_base);
	if (part_sn2->local_GPs == NULL) {
		dev_err(xpc_chan, "can't get memory for local get/put "
			"values\n");
		retval = xpNoMemory;
		goto out_1;
	}

	part_sn2->remote_GPs =
	    xpc_kzalloc_cacheline_aligned_sn2(XPC_GP_SIZE, GFP_KERNEL,
					      &part_sn2->remote_GPs_base);
	if (part_sn2->remote_GPs == NULL) {
		dev_err(xpc_chan, "can't get memory for remote get/put "
			"values\n");
		retval = xpNoMemory;
		goto out_2;
	}

	part_sn2->remote_GPs_pa = 0;

	/* allocate all the required open and close args */

	part->local_openclose_args =
	    xpc_kzalloc_cacheline_aligned_sn2(XPC_OPENCLOSE_ARGS_SIZE,
					      GFP_KERNEL,
					      &part->local_openclose_args_base);
	if (part->local_openclose_args == NULL) {
		dev_err(xpc_chan, "can't get memory for local connect args\n");
		retval = xpNoMemory;
		goto out_3;
	}

	part->remote_openclose_args =
	    xpc_kzalloc_cacheline_aligned_sn2(XPC_OPENCLOSE_ARGS_SIZE,
					      GFP_KERNEL,
					     &part->remote_openclose_args_base);
	if (part->remote_openclose_args == NULL) {
		dev_err(xpc_chan, "can't get memory for remote connect args\n");
		retval = xpNoMemory;
		goto out_4;
	}

	part_sn2->remote_openclose_args_pa = 0;

	part_sn2->local_chctl_amo_va = xpc_init_IRQ_amo_sn2(partid);
	part->chctl.all_flags = 0;
	spin_lock_init(&part->chctl_lock);

	part_sn2->notify_IRQ_nasid = 0;
	part_sn2->notify_IRQ_phys_cpuid = 0;
	part_sn2->remote_chctl_amo_va = NULL;

	atomic_set(&part->channel_mgr_requests, 1);
	init_waitqueue_head(&part->channel_mgr_wq);

	sprintf(part_sn2->notify_IRQ_owner, "xpc%02d", partid);
	ret = request_irq(SGI_XPC_NOTIFY, xpc_handle_notify_IRQ_sn2,
			  IRQF_SHARED, part_sn2->notify_IRQ_owner,
			  (void *)(u64)partid);
	if (ret != 0) {
		dev_err(xpc_chan, "can't register NOTIFY IRQ handler, "
			"errno=%d\n", -ret);
		retval = xpLackOfResources;
		goto out_5;
	}

	/* Setup a timer to check for dropped notify IRQs */
	timer = &part_sn2->dropped_notify_IRQ_timer;
	init_timer(timer);
	timer->function =
	    (void (*)(unsigned long))xpc_check_for_dropped_notify_IRQ_sn2;
	timer->data = (unsigned long)part;
	timer->expires = jiffies + XPC_DROPPED_NOTIFY_IRQ_WAIT_INTERVAL;
	add_timer(timer);

	part->nchannels = XPC_MAX_NCHANNELS;

	atomic_set(&part->nchannels_active, 0);
	atomic_set(&part->nchannels_engaged, 0);

	for (ch_number = 0; ch_number < part->nchannels; ch_number++) {
		ch = &part->channels[ch_number];

		ch->partid = partid;
		ch->number = ch_number;
		ch->flags = XPC_C_DISCONNECTED;

		ch->sn.sn2.local_GP = &part_sn2->local_GPs[ch_number];
		ch->local_openclose_args =
		    &part->local_openclose_args[ch_number];

		atomic_set(&ch->kthreads_assigned, 0);
		atomic_set(&ch->kthreads_idle, 0);
		atomic_set(&ch->kthreads_active, 0);

		atomic_set(&ch->references, 0);
		atomic_set(&ch->n_to_notify, 0);

		spin_lock_init(&ch->lock);
		mutex_init(&ch->sn.sn2.msg_to_pull_mutex);
		init_completion(&ch->wdisconnect_wait);

		atomic_set(&ch->n_on_msg_allocate_wq, 0);
		init_waitqueue_head(&ch->msg_allocate_wq);
		init_waitqueue_head(&ch->idle_wq);
	}

	/*
	 * With the setting of the partition setup_state to XPC_P_SETUP, we're
	 * declaring that this partition is ready to go.
	 */
	part->setup_state = XPC_P_SETUP;

	/*
	 * Setup the per partition specific variables required by the
	 * remote partition to establish channel connections with us.
	 *
	 * The setting of the magic # indicates that these per partition
	 * specific variables are ready to be used.
	 */
	xpc_vars_part[partid].GPs_pa = __pa(part_sn2->local_GPs);
	xpc_vars_part[partid].openclose_args_pa =
	    __pa(part->local_openclose_args);
	xpc_vars_part[partid].chctl_amo_pa = __pa(part_sn2->local_chctl_amo_va);
	cpuid = raw_smp_processor_id();	/* any CPU in this partition will do */
	xpc_vars_part[partid].notify_IRQ_nasid = cpuid_to_nasid(cpuid);
	xpc_vars_part[partid].notify_IRQ_phys_cpuid = cpu_physical_id(cpuid);
	xpc_vars_part[partid].nchannels = part->nchannels;
	xpc_vars_part[partid].magic = XPC_VP_MAGIC1;

	return xpSuccess;

	/* setup of infrastructure failed */
out_5:
	kfree(part->remote_openclose_args_base);
	part->remote_openclose_args = NULL;
out_4:
	kfree(part->local_openclose_args_base);
	part->local_openclose_args = NULL;
out_3:
	kfree(part_sn2->remote_GPs_base);
	part_sn2->remote_GPs = NULL;
out_2:
	kfree(part_sn2->local_GPs_base);
	part_sn2->local_GPs = NULL;
out_1:
	kfree(part->channels);
	part->channels = NULL;
	return retval;
}

/*
 * Teardown the infrastructure necessary to support XPartition Communication
 * between the specified remote partition and the local one.
 */
static void
xpc_teardown_infrastructure_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	short partid = XPC_PARTID(part);

	/*
	 * We start off by making this partition inaccessible to local
	 * processes by marking it as no longer setup. Then we make it
	 * inaccessible to remote processes by clearing the XPC per partition
	 * specific variable's magic # (which indicates that these variables
	 * are no longer valid) and by ignoring all XPC notify IRQs sent to
	 * this partition.
	 */

	DBUG_ON(atomic_read(&part->nchannels_engaged) != 0);
	DBUG_ON(atomic_read(&part->nchannels_active) != 0);
	DBUG_ON(part->setup_state != XPC_P_SETUP);
	part->setup_state = XPC_P_WTEARDOWN;

	xpc_vars_part[partid].magic = 0;

	free_irq(SGI_XPC_NOTIFY, (void *)(u64)partid);

	/*
	 * Before proceeding with the teardown we have to wait until all
	 * existing references cease.
	 */
	wait_event(part->teardown_wq, (atomic_read(&part->references) == 0));

	/* now we can begin tearing down the infrastructure */

	part->setup_state = XPC_P_TORNDOWN;

	/* in case we've still got outstanding timers registered... */
	del_timer_sync(&part_sn2->dropped_notify_IRQ_timer);

	kfree(part->remote_openclose_args_base);
	part->remote_openclose_args = NULL;
	kfree(part->local_openclose_args_base);
	part->local_openclose_args = NULL;
	kfree(part_sn2->remote_GPs_base);
	part_sn2->remote_GPs = NULL;
	kfree(part_sn2->local_GPs_base);
	part_sn2->local_GPs = NULL;
	kfree(part->channels);
	part->channels = NULL;
	part_sn2->local_chctl_amo_va = NULL;
}

/*
 * Create a wrapper that hides the underlying mechanism for pulling a cacheline
 * (or multiple cachelines) from a remote partition.
 *
 * src must be a cacheline aligned physical address on the remote partition.
 * dst must be a cacheline aligned virtual address on this partition.
 * cnt must be cacheline sized
 */
/* >>> Replace this function by call to xp_remote_memcpy() or bte_copy()? */
static enum xp_retval
xpc_pull_remote_cachelines_sn2(struct xpc_partition *part, void *dst,
			       const void *src, size_t cnt)
{
	enum xp_retval ret;

	DBUG_ON((u64)src != L1_CACHE_ALIGN((u64)src));
	DBUG_ON((u64)dst != L1_CACHE_ALIGN((u64)dst));
	DBUG_ON(cnt != L1_CACHE_ALIGN(cnt));

	if (part->act_state == XPC_P_DEACTIVATING)
		return part->reason;

	ret = xp_remote_memcpy(dst, src, cnt);
	if (ret != xpSuccess) {
		dev_dbg(xpc_chan, "xp_remote_memcpy() from partition %d failed,"
			" ret=%d\n", XPC_PARTID(part), ret);
	}
	return ret;
}

/*
 * Pull the remote per partition specific variables from the specified
 * partition.
 */
static enum xp_retval
xpc_pull_remote_vars_part_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	u8 buffer[L1_CACHE_BYTES * 2];
	struct xpc_vars_part_sn2 *pulled_entry_cacheline =
	    (struct xpc_vars_part_sn2 *)L1_CACHE_ALIGN((u64)buffer);
	struct xpc_vars_part_sn2 *pulled_entry;
	u64 remote_entry_cacheline_pa, remote_entry_pa;
	short partid = XPC_PARTID(part);
	enum xp_retval ret;

	/* pull the cacheline that contains the variables we're interested in */

	DBUG_ON(part_sn2->remote_vars_part_pa !=
		L1_CACHE_ALIGN(part_sn2->remote_vars_part_pa));
	DBUG_ON(sizeof(struct xpc_vars_part_sn2) != L1_CACHE_BYTES / 2);

	remote_entry_pa = part_sn2->remote_vars_part_pa +
	    sn_partition_id * sizeof(struct xpc_vars_part_sn2);

	remote_entry_cacheline_pa = (remote_entry_pa & ~(L1_CACHE_BYTES - 1));

	pulled_entry = (struct xpc_vars_part_sn2 *)((u64)pulled_entry_cacheline
						    + (remote_entry_pa &
						    (L1_CACHE_BYTES - 1)));

	ret = xpc_pull_remote_cachelines_sn2(part, pulled_entry_cacheline,
					     (void *)remote_entry_cacheline_pa,
					     L1_CACHE_BYTES);
	if (ret != xpSuccess) {
		dev_dbg(xpc_chan, "failed to pull XPC vars_part from "
			"partition %d, ret=%d\n", partid, ret);
		return ret;
	}

	/* see if they've been set up yet */

	if (pulled_entry->magic != XPC_VP_MAGIC1 &&
	    pulled_entry->magic != XPC_VP_MAGIC2) {

		if (pulled_entry->magic != 0) {
			dev_dbg(xpc_chan, "partition %d's XPC vars_part for "
				"partition %d has bad magic value (=0x%lx)\n",
				partid, sn_partition_id, pulled_entry->magic);
			return xpBadMagic;
		}

		/* they've not been initialized yet */
		return xpRetry;
	}

	if (xpc_vars_part[partid].magic == XPC_VP_MAGIC1) {

		/* validate the variables */

		if (pulled_entry->GPs_pa == 0 ||
		    pulled_entry->openclose_args_pa == 0 ||
		    pulled_entry->chctl_amo_pa == 0) {

			dev_err(xpc_chan, "partition %d's XPC vars_part for "
				"partition %d are not valid\n", partid,
				sn_partition_id);
			return xpInvalidAddress;
		}

		/* the variables we imported look to be valid */

		part_sn2->remote_GPs_pa = pulled_entry->GPs_pa;
		part_sn2->remote_openclose_args_pa =
		    pulled_entry->openclose_args_pa;
		part_sn2->remote_chctl_amo_va =
		    (struct amo *)__va(pulled_entry->chctl_amo_pa);
		part_sn2->notify_IRQ_nasid = pulled_entry->notify_IRQ_nasid;
		part_sn2->notify_IRQ_phys_cpuid =
		    pulled_entry->notify_IRQ_phys_cpuid;

		if (part->nchannels > pulled_entry->nchannels)
			part->nchannels = pulled_entry->nchannels;

		/* let the other side know that we've pulled their variables */

		xpc_vars_part[partid].magic = XPC_VP_MAGIC2;
	}

	if (pulled_entry->magic == XPC_VP_MAGIC1)
		return xpRetry;

	return xpSuccess;
}

/*
 * Establish first contact with the remote partititon. This involves pulling
 * the XPC per partition variables from the remote partition and waiting for
 * the remote partition to pull ours.
 */
static enum xp_retval
xpc_make_first_contact_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	enum xp_retval ret;

	/*
	 * Register the remote partition's amos with SAL so it can handle
	 * and cleanup errors within that address range should the remote
	 * partition go down. We don't unregister this range because it is
	 * difficult to tell when outstanding writes to the remote partition
	 * are finished and thus when it is safe to unregister. This should
	 * not result in wasted space in the SAL xp_addr_region table because
	 * we should get the same page for remote_amos_page_pa after module
	 * reloads and system reboots.
	 */
	if (sn_register_xp_addr_region(part_sn2->remote_amos_page_pa,
				       PAGE_SIZE, 1) < 0) {
		dev_warn(xpc_part, "xpc_activating(%d) failed to register "
			 "xp_addr region\n", XPC_PARTID(part));

		ret = xpPhysAddrRegFailed;
		XPC_DEACTIVATE_PARTITION(part, ret);
		return ret;
	}

	/*
	 * Send activate IRQ to get other side to activate if they've not
	 * already begun to do so.
	 */
	xpc_send_activate_IRQ_sn2(part_sn2->remote_amos_page_pa,
				  cnodeid_to_nasid(0),
				  part_sn2->activate_IRQ_nasid,
				  part_sn2->activate_IRQ_phys_cpuid);

	while ((ret = xpc_pull_remote_vars_part_sn2(part)) != xpSuccess) {
		if (ret != xpRetry) {
			XPC_DEACTIVATE_PARTITION(part, ret);
			return ret;
		}

		dev_dbg(xpc_part, "waiting to make first contact with "
			"partition %d\n", XPC_PARTID(part));

		/* wait a 1/4 of a second or so */
		(void)msleep_interruptible(250);

		if (part->act_state == XPC_P_DEACTIVATING)
			return part->reason;
	}

	return xpSuccess;
}

/*
 * Get the chctl flags and pull the openclose args and/or remote GPs as needed.
 */
static u64
xpc_get_chctl_all_flags_sn2(struct xpc_partition *part)
{
	struct xpc_partition_sn2 *part_sn2 = &part->sn.sn2;
	unsigned long irq_flags;
	union xpc_channel_ctl_flags chctl;
	enum xp_retval ret;

	/*
	 * See if there are any chctl flags to be handled.
	 */

	spin_lock_irqsave(&part->chctl_lock, irq_flags);
	chctl = part->chctl;
	if (chctl.all_flags != 0)
		part->chctl.all_flags = 0;

	spin_unlock_irqrestore(&part->chctl_lock, irq_flags);

	if (xpc_any_openclose_chctl_flags_set(&chctl)) {
		ret = xpc_pull_remote_cachelines_sn2(part, part->
						     remote_openclose_args,
						     (void *)part_sn2->
						     remote_openclose_args_pa,
						     XPC_OPENCLOSE_ARGS_SIZE);
		if (ret != xpSuccess) {
			XPC_DEACTIVATE_PARTITION(part, ret);

			dev_dbg(xpc_chan, "failed to pull openclose args from "
				"partition %d, ret=%d\n", XPC_PARTID(part),
				ret);

			/* don't bother processing chctl flags anymore */
			chctl.all_flags = 0;
		}
	}

	if (xpc_any_msg_chctl_flags_set(&chctl)) {
		ret = xpc_pull_remote_cachelines_sn2(part, part_sn2->remote_GPs,
						(void *)part_sn2->remote_GPs_pa,
						     XPC_GP_SIZE);
		if (ret != xpSuccess) {
			XPC_DEACTIVATE_PARTITION(part, ret);

			dev_dbg(xpc_chan, "failed to pull GPs from partition "
				"%d, ret=%d\n", XPC_PARTID(part), ret);

			/* don't bother processing chctl flags anymore */
			chctl.all_flags = 0;
		}
	}

	return chctl.all_flags;
}

/*
 * Allocate the local message queue and the notify queue.
 */
static enum xp_retval
xpc_allocate_local_msgqueue_sn2(struct xpc_channel *ch)
{
	unsigned long irq_flags;
	int nentries;
	size_t nbytes;

	for (nentries = ch->local_nentries; nentries > 0; nentries--) {

		nbytes = nentries * ch->msg_size;
		ch->local_msgqueue =
		    xpc_kzalloc_cacheline_aligned_sn2(nbytes, GFP_KERNEL,
						      &ch->local_msgqueue_base);
		if (ch->local_msgqueue == NULL)
			continue;

		nbytes = nentries * sizeof(struct xpc_notify);
		ch->notify_queue = kzalloc(nbytes, GFP_KERNEL);
		if (ch->notify_queue == NULL) {
			kfree(ch->local_msgqueue_base);
			ch->local_msgqueue = NULL;
			continue;
		}

		spin_lock_irqsave(&ch->lock, irq_flags);
		if (nentries < ch->local_nentries) {
			dev_dbg(xpc_chan, "nentries=%d local_nentries=%d, "
				"partid=%d, channel=%d\n", nentries,
				ch->local_nentries, ch->partid, ch->number);

			ch->local_nentries = nentries;
		}
		spin_unlock_irqrestore(&ch->lock, irq_flags);
		return xpSuccess;
	}

	dev_dbg(xpc_chan, "can't get memory for local message queue and notify "
		"queue, partid=%d, channel=%d\n", ch->partid, ch->number);
	return xpNoMemory;
}

/*
 * Allocate the cached remote message queue.
 */
static enum xp_retval
xpc_allocate_remote_msgqueue_sn2(struct xpc_channel *ch)
{
	unsigned long irq_flags;
	int nentries;
	size_t nbytes;

	DBUG_ON(ch->remote_nentries <= 0);

	for (nentries = ch->remote_nentries; nentries > 0; nentries--) {

		nbytes = nentries * ch->msg_size;
		ch->remote_msgqueue =
		    xpc_kzalloc_cacheline_aligned_sn2(nbytes, GFP_KERNEL,
						     &ch->remote_msgqueue_base);
		if (ch->remote_msgqueue == NULL)
			continue;

		spin_lock_irqsave(&ch->lock, irq_flags);
		if (nentries < ch->remote_nentries) {
			dev_dbg(xpc_chan, "nentries=%d remote_nentries=%d, "
				"partid=%d, channel=%d\n", nentries,
				ch->remote_nentries, ch->partid, ch->number);

			ch->remote_nentries = nentries;
		}
		spin_unlock_irqrestore(&ch->lock, irq_flags);
		return xpSuccess;
	}

	dev_dbg(xpc_chan, "can't get memory for cached remote message queue, "
		"partid=%d, channel=%d\n", ch->partid, ch->number);
	return xpNoMemory;
}

/*
 * Allocate message queues and other stuff associated with a channel.
 *
 * Note: Assumes all of the channel sizes are filled in.
 */
static enum xp_retval
xpc_allocate_msgqueues_sn2(struct xpc_channel *ch)
{
	enum xp_retval ret;

	DBUG_ON(ch->flags & XPC_C_SETUP);

	ret = xpc_allocate_local_msgqueue_sn2(ch);
	if (ret == xpSuccess) {

		ret = xpc_allocate_remote_msgqueue_sn2(ch);
		if (ret != xpSuccess) {
			kfree(ch->local_msgqueue_base);
			ch->local_msgqueue = NULL;
			kfree(ch->notify_queue);
			ch->notify_queue = NULL;
		}
	}
	return ret;
}

/*
 * Free up message queues and other stuff that were allocated for the specified
 * channel.
 *
 * Note: ch->reason and ch->reason_line are left set for debugging purposes,
 * they're cleared when XPC_C_DISCONNECTED is cleared.
 */
static void
xpc_free_msgqueues_sn2(struct xpc_channel *ch)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;

	DBUG_ON(!spin_is_locked(&ch->lock));
	DBUG_ON(atomic_read(&ch->n_to_notify) != 0);

	ch->remote_msgqueue_pa = 0;
	ch->func = NULL;
	ch->key = NULL;
	ch->msg_size = 0;
	ch->local_nentries = 0;
	ch->remote_nentries = 0;
	ch->kthreads_assigned_limit = 0;
	ch->kthreads_idle_limit = 0;

	ch_sn2->local_GP->get = 0;
	ch_sn2->local_GP->put = 0;
	ch_sn2->remote_GP.get = 0;
	ch_sn2->remote_GP.put = 0;
	ch_sn2->w_local_GP.get = 0;
	ch_sn2->w_local_GP.put = 0;
	ch_sn2->w_remote_GP.get = 0;
	ch_sn2->w_remote_GP.put = 0;
	ch_sn2->next_msg_to_pull = 0;

	if (ch->flags & XPC_C_SETUP) {
		dev_dbg(xpc_chan, "ch->flags=0x%x, partid=%d, channel=%d\n",
			ch->flags, ch->partid, ch->number);

		kfree(ch->local_msgqueue_base);
		ch->local_msgqueue = NULL;
		kfree(ch->remote_msgqueue_base);
		ch->remote_msgqueue = NULL;
		kfree(ch->notify_queue);
		ch->notify_queue = NULL;
	}
}

/*
 * Notify those who wanted to be notified upon delivery of their message.
 */
static void
xpc_notify_senders_sn2(struct xpc_channel *ch, enum xp_retval reason, s64 put)
{
	struct xpc_notify *notify;
	u8 notify_type;
	s64 get = ch->sn.sn2.w_remote_GP.get - 1;

	while (++get < put && atomic_read(&ch->n_to_notify) > 0) {

		notify = &ch->notify_queue[get % ch->local_nentries];

		/*
		 * See if the notify entry indicates it was associated with
		 * a message who's sender wants to be notified. It is possible
		 * that it is, but someone else is doing or has done the
		 * notification.
		 */
		notify_type = notify->type;
		if (notify_type == 0 ||
		    cmpxchg(&notify->type, notify_type, 0) != notify_type) {
			continue;
		}

		DBUG_ON(notify_type != XPC_N_CALL);

		atomic_dec(&ch->n_to_notify);

		if (notify->func != NULL) {
			dev_dbg(xpc_chan, "notify->func() called, notify=0x%p, "
				"msg_number=%ld, partid=%d, channel=%d\n",
				(void *)notify, get, ch->partid, ch->number);

			notify->func(reason, ch->partid, ch->number,
				     notify->key);

			dev_dbg(xpc_chan, "notify->func() returned, "
				"notify=0x%p, msg_number=%ld, partid=%d, "
				"channel=%d\n", (void *)notify, get,
				ch->partid, ch->number);
		}
	}
}

static void
xpc_notify_senders_of_disconnect_sn2(struct xpc_channel *ch)
{
	xpc_notify_senders_sn2(ch, ch->reason, ch->sn.sn2.w_local_GP.put);
}

/*
 * Clear some of the msg flags in the local message queue.
 */
static inline void
xpc_clear_local_msgqueue_flags_sn2(struct xpc_channel *ch)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg *msg;
	s64 get;

	get = ch_sn2->w_remote_GP.get;
	do {
		msg = (struct xpc_msg *)((u64)ch->local_msgqueue +
					 (get % ch->local_nentries) *
					 ch->msg_size);
		msg->flags = 0;
	} while (++get < ch_sn2->remote_GP.get);
}

/*
 * Clear some of the msg flags in the remote message queue.
 */
static inline void
xpc_clear_remote_msgqueue_flags_sn2(struct xpc_channel *ch)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg *msg;
	s64 put;

	put = ch_sn2->w_remote_GP.put;
	do {
		msg = (struct xpc_msg *)((u64)ch->remote_msgqueue +
					 (put % ch->remote_nentries) *
					 ch->msg_size);
		msg->flags = 0;
	} while (++put < ch_sn2->remote_GP.put);
}

static void
xpc_process_msg_chctl_flags_sn2(struct xpc_partition *part, int ch_number)
{
	struct xpc_channel *ch = &part->channels[ch_number];
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	int nmsgs_sent;

	ch_sn2->remote_GP = part->sn.sn2.remote_GPs[ch_number];

	/* See what, if anything, has changed for each connected channel */

	xpc_msgqueue_ref(ch);

	if (ch_sn2->w_remote_GP.get == ch_sn2->remote_GP.get &&
	    ch_sn2->w_remote_GP.put == ch_sn2->remote_GP.put) {
		/* nothing changed since GPs were last pulled */
		xpc_msgqueue_deref(ch);
		return;
	}

	if (!(ch->flags & XPC_C_CONNECTED)) {
		xpc_msgqueue_deref(ch);
		return;
	}

	/*
	 * First check to see if messages recently sent by us have been
	 * received by the other side. (The remote GET value will have
	 * changed since we last looked at it.)
	 */

	if (ch_sn2->w_remote_GP.get != ch_sn2->remote_GP.get) {

		/*
		 * We need to notify any senders that want to be notified
		 * that their sent messages have been received by their
		 * intended recipients. We need to do this before updating
		 * w_remote_GP.get so that we don't allocate the same message
		 * queue entries prematurely (see xpc_allocate_msg()).
		 */
		if (atomic_read(&ch->n_to_notify) > 0) {
			/*
			 * Notify senders that messages sent have been
			 * received and delivered by the other side.
			 */
			xpc_notify_senders_sn2(ch, xpMsgDelivered,
					       ch_sn2->remote_GP.get);
		}

		/*
		 * Clear msg->flags in previously sent messages, so that
		 * they're ready for xpc_allocate_msg().
		 */
		xpc_clear_local_msgqueue_flags_sn2(ch);

		ch_sn2->w_remote_GP.get = ch_sn2->remote_GP.get;

		dev_dbg(xpc_chan, "w_remote_GP.get changed to %ld, partid=%d, "
			"channel=%d\n", ch_sn2->w_remote_GP.get, ch->partid,
			ch->number);

		/*
		 * If anyone was waiting for message queue entries to become
		 * available, wake them up.
		 */
		if (atomic_read(&ch->n_on_msg_allocate_wq) > 0)
			wake_up(&ch->msg_allocate_wq);
	}

	/*
	 * Now check for newly sent messages by the other side. (The remote
	 * PUT value will have changed since we last looked at it.)
	 */

	if (ch_sn2->w_remote_GP.put != ch_sn2->remote_GP.put) {
		/*
		 * Clear msg->flags in previously received messages, so that
		 * they're ready for xpc_get_deliverable_msg().
		 */
		xpc_clear_remote_msgqueue_flags_sn2(ch);

		ch_sn2->w_remote_GP.put = ch_sn2->remote_GP.put;

		dev_dbg(xpc_chan, "w_remote_GP.put changed to %ld, partid=%d, "
			"channel=%d\n", ch_sn2->w_remote_GP.put, ch->partid,
			ch->number);

		nmsgs_sent = ch_sn2->w_remote_GP.put - ch_sn2->w_local_GP.get;
		if (nmsgs_sent > 0) {
			dev_dbg(xpc_chan, "msgs waiting to be copied and "
				"delivered=%d, partid=%d, channel=%d\n",
				nmsgs_sent, ch->partid, ch->number);

			if (ch->flags & XPC_C_CONNECTEDCALLOUT_MADE)
				xpc_activate_kthreads(ch, nmsgs_sent);
		}
	}

	xpc_msgqueue_deref(ch);
}

static struct xpc_msg *
xpc_pull_remote_msg_sn2(struct xpc_channel *ch, s64 get)
{
	struct xpc_partition *part = &xpc_partitions[ch->partid];
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg *remote_msg, *msg;
	u32 msg_index, nmsgs;
	u64 msg_offset;
	enum xp_retval ret;

	if (mutex_lock_interruptible(&ch_sn2->msg_to_pull_mutex) != 0) {
		/* we were interrupted by a signal */
		return NULL;
	}

	while (get >= ch_sn2->next_msg_to_pull) {

		/* pull as many messages as are ready and able to be pulled */

		msg_index = ch_sn2->next_msg_to_pull % ch->remote_nentries;

		DBUG_ON(ch_sn2->next_msg_to_pull >= ch_sn2->w_remote_GP.put);
		nmsgs = ch_sn2->w_remote_GP.put - ch_sn2->next_msg_to_pull;
		if (msg_index + nmsgs > ch->remote_nentries) {
			/* ignore the ones that wrap the msg queue for now */
			nmsgs = ch->remote_nentries - msg_index;
		}

		msg_offset = msg_index * ch->msg_size;
		msg = (struct xpc_msg *)((u64)ch->remote_msgqueue + msg_offset);
		remote_msg = (struct xpc_msg *)(ch->remote_msgqueue_pa +
						msg_offset);

		ret = xpc_pull_remote_cachelines_sn2(part, msg, remote_msg,
						     nmsgs * ch->msg_size);
		if (ret != xpSuccess) {

			dev_dbg(xpc_chan, "failed to pull %d msgs starting with"
				" msg %ld from partition %d, channel=%d, "
				"ret=%d\n", nmsgs, ch_sn2->next_msg_to_pull,
				ch->partid, ch->number, ret);

			XPC_DEACTIVATE_PARTITION(part, ret);

			mutex_unlock(&ch_sn2->msg_to_pull_mutex);
			return NULL;
		}

		ch_sn2->next_msg_to_pull += nmsgs;
	}

	mutex_unlock(&ch_sn2->msg_to_pull_mutex);

	/* return the message we were looking for */
	msg_offset = (get % ch->remote_nentries) * ch->msg_size;
	msg = (struct xpc_msg *)((u64)ch->remote_msgqueue + msg_offset);

	return msg;
}

static int
xpc_n_of_deliverable_msgs_sn2(struct xpc_channel *ch)
{
	return ch->sn.sn2.w_remote_GP.put - ch->sn.sn2.w_local_GP.get;
}

/*
 * Get a message to be delivered.
 */
static struct xpc_msg *
xpc_get_deliverable_msg_sn2(struct xpc_channel *ch)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg *msg = NULL;
	s64 get;

	do {
		if (ch->flags & XPC_C_DISCONNECTING)
			break;

		get = ch_sn2->w_local_GP.get;
		rmb();	/* guarantee that .get loads before .put */
		if (get == ch_sn2->w_remote_GP.put)
			break;

		/* There are messages waiting to be pulled and delivered.
		 * We need to try to secure one for ourselves. We'll do this
		 * by trying to increment w_local_GP.get and hope that no one
		 * else beats us to it. If they do, we'll we'll simply have
		 * to try again for the next one.
		 */

		if (cmpxchg(&ch_sn2->w_local_GP.get, get, get + 1) == get) {
			/* we got the entry referenced by get */

			dev_dbg(xpc_chan, "w_local_GP.get changed to %ld, "
				"partid=%d, channel=%d\n", get + 1,
				ch->partid, ch->number);

			/* pull the message from the remote partition */

			msg = xpc_pull_remote_msg_sn2(ch, get);

			DBUG_ON(msg != NULL && msg->number != get);
			DBUG_ON(msg != NULL && (msg->flags & XPC_M_DONE));
			DBUG_ON(msg != NULL && !(msg->flags & XPC_M_READY));

			break;
		}

	} while (1);

	return msg;
}

/*
 * Now we actually send the messages that are ready to be sent by advancing
 * the local message queue's Put value and then send a chctl msgrequest to the
 * recipient partition.
 */
static void
xpc_send_msgs_sn2(struct xpc_channel *ch, s64 initial_put)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg *msg;
	s64 put = initial_put + 1;
	int send_msgrequest = 0;

	while (1) {

		while (1) {
			if (put == ch_sn2->w_local_GP.put)
				break;

			msg = (struct xpc_msg *)((u64)ch->local_msgqueue +
						 (put % ch->local_nentries) *
						 ch->msg_size);

			if (!(msg->flags & XPC_M_READY))
				break;

			put++;
		}

		if (put == initial_put) {
			/* nothing's changed */
			break;
		}

		if (cmpxchg_rel(&ch_sn2->local_GP->put, initial_put, put) !=
		    initial_put) {
			/* someone else beat us to it */
			DBUG_ON(ch_sn2->local_GP->put < initial_put);
			break;
		}

		/* we just set the new value of local_GP->put */

		dev_dbg(xpc_chan, "local_GP->put changed to %ld, partid=%d, "
			"channel=%d\n", put, ch->partid, ch->number);

		send_msgrequest = 1;

		/*
		 * We need to ensure that the message referenced by
		 * local_GP->put is not XPC_M_READY or that local_GP->put
		 * equals w_local_GP.put, so we'll go have a look.
		 */
		initial_put = put;
	}

	if (send_msgrequest)
		xpc_send_chctl_msgrequest_sn2(ch);
}

/*
 * Allocate an entry for a message from the message queue associated with the
 * specified channel.
 */
static enum xp_retval
xpc_allocate_msg_sn2(struct xpc_channel *ch, u32 flags,
		     struct xpc_msg **address_of_msg)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg *msg;
	enum xp_retval ret;
	s64 put;

	/*
	 * Get the next available message entry from the local message queue.
	 * If none are available, we'll make sure that we grab the latest
	 * GP values.
	 */
	ret = xpTimeout;

	while (1) {

		put = ch_sn2->w_local_GP.put;
		rmb();	/* guarantee that .put loads before .get */
		if (put - ch_sn2->w_remote_GP.get < ch->local_nentries) {

			/* There are available message entries. We need to try
			 * to secure one for ourselves. We'll do this by trying
			 * to increment w_local_GP.put as long as someone else
			 * doesn't beat us to it. If they do, we'll have to
			 * try again.
			 */
			if (cmpxchg(&ch_sn2->w_local_GP.put, put, put + 1) ==
			    put) {
				/* we got the entry referenced by put */
				break;
			}
			continue;	/* try again */
		}

		/*
		 * There aren't any available msg entries at this time.
		 *
		 * In waiting for a message entry to become available,
		 * we set a timeout in case the other side is not sending
		 * completion interrupts. This lets us fake a notify IRQ
		 * that will cause the notify IRQ handler to fetch the latest
		 * GP values as if an interrupt was sent by the other side.
		 */
		if (ret == xpTimeout)
			xpc_send_chctl_local_msgrequest_sn2(ch);

		if (flags & XPC_NOWAIT)
			return xpNoWait;

		ret = xpc_allocate_msg_wait(ch);
		if (ret != xpInterrupted && ret != xpTimeout)
			return ret;
	}

	/* get the message's address and initialize it */
	msg = (struct xpc_msg *)((u64)ch->local_msgqueue +
				 (put % ch->local_nentries) * ch->msg_size);

	DBUG_ON(msg->flags != 0);
	msg->number = put;

	dev_dbg(xpc_chan, "w_local_GP.put changed to %ld; msg=0x%p, "
		"msg_number=%ld, partid=%d, channel=%d\n", put + 1,
		(void *)msg, msg->number, ch->partid, ch->number);

	*address_of_msg = msg;
	return xpSuccess;
}

/*
 * Common code that does the actual sending of the message by advancing the
 * local message queue's Put value and sends a chctl msgrequest to the
 * partition the message is being sent to.
 */
static enum xp_retval
xpc_send_msg_sn2(struct xpc_channel *ch, u32 flags, void *payload,
		 u16 payload_size, u8 notify_type, xpc_notify_func func,
		 void *key)
{
	enum xp_retval ret = xpSuccess;
	struct xpc_msg *msg = msg;
	struct xpc_notify *notify = notify;
	s64 msg_number;
	s64 put;

	DBUG_ON(notify_type == XPC_N_CALL && func == NULL);

	if (XPC_MSG_SIZE(payload_size) > ch->msg_size)
		return xpPayloadTooBig;

	xpc_msgqueue_ref(ch);

	if (ch->flags & XPC_C_DISCONNECTING) {
		ret = ch->reason;
		goto out_1;
	}
	if (!(ch->flags & XPC_C_CONNECTED)) {
		ret = xpNotConnected;
		goto out_1;
	}

	ret = xpc_allocate_msg_sn2(ch, flags, &msg);
	if (ret != xpSuccess)
		goto out_1;

	msg_number = msg->number;

	if (notify_type != 0) {
		/*
		 * Tell the remote side to send an ACK interrupt when the
		 * message has been delivered.
		 */
		msg->flags |= XPC_M_INTERRUPT;

		atomic_inc(&ch->n_to_notify);

		notify = &ch->notify_queue[msg_number % ch->local_nentries];
		notify->func = func;
		notify->key = key;
		notify->type = notify_type;

		/* >>> is a mb() needed here? */

		if (ch->flags & XPC_C_DISCONNECTING) {
			/*
			 * An error occurred between our last error check and
			 * this one. We will try to clear the type field from
			 * the notify entry. If we succeed then
			 * xpc_disconnect_channel() didn't already process
			 * the notify entry.
			 */
			if (cmpxchg(&notify->type, notify_type, 0) ==
			    notify_type) {
				atomic_dec(&ch->n_to_notify);
				ret = ch->reason;
			}
			goto out_1;
		}
	}

	memcpy(&msg->payload, payload, payload_size);

	msg->flags |= XPC_M_READY;

	/*
	 * The preceding store of msg->flags must occur before the following
	 * load of local_GP->put.
	 */
	mb();

	/* see if the message is next in line to be sent, if so send it */

	put = ch->sn.sn2.local_GP->put;
	if (put == msg_number)
		xpc_send_msgs_sn2(ch, put);

out_1:
	xpc_msgqueue_deref(ch);
	return ret;
}

/*
 * Now we actually acknowledge the messages that have been delivered and ack'd
 * by advancing the cached remote message queue's Get value and if requested
 * send a chctl msgrequest to the message sender's partition.
 */
static void
xpc_acknowledge_msgs_sn2(struct xpc_channel *ch, s64 initial_get, u8 msg_flags)
{
	struct xpc_channel_sn2 *ch_sn2 = &ch->sn.sn2;
	struct xpc_msg *msg;
	s64 get = initial_get + 1;
	int send_msgrequest = 0;

	while (1) {

		while (1) {
			if (get == ch_sn2->w_local_GP.get)
				break;

			msg = (struct xpc_msg *)((u64)ch->remote_msgqueue +
						 (get % ch->remote_nentries) *
						 ch->msg_size);

			if (!(msg->flags & XPC_M_DONE))
				break;

			msg_flags |= msg->flags;
			get++;
		}

		if (get == initial_get) {
			/* nothing's changed */
			break;
		}

		if (cmpxchg_rel(&ch_sn2->local_GP->get, initial_get, get) !=
		    initial_get) {
			/* someone else beat us to it */
			DBUG_ON(ch_sn2->local_GP->get <= initial_get);
			break;
		}

		/* we just set the new value of local_GP->get */

		dev_dbg(xpc_chan, "local_GP->get changed to %ld, partid=%d, "
			"channel=%d\n", get, ch->partid, ch->number);

		send_msgrequest = (msg_flags & XPC_M_INTERRUPT);

		/*
		 * We need to ensure that the message referenced by
		 * local_GP->get is not XPC_M_DONE or that local_GP->get
		 * equals w_local_GP.get, so we'll go have a look.
		 */
		initial_get = get;
	}

	if (send_msgrequest)
		xpc_send_chctl_msgrequest_sn2(ch);
}

static void
xpc_received_msg_sn2(struct xpc_channel *ch, struct xpc_msg *msg)
{
	s64 get;
	s64 msg_number = msg->number;

	dev_dbg(xpc_chan, "msg=0x%p, msg_number=%ld, partid=%d, channel=%d\n",
		(void *)msg, msg_number, ch->partid, ch->number);

	DBUG_ON((((u64)msg - (u64)ch->remote_msgqueue) / ch->msg_size) !=
		msg_number % ch->remote_nentries);
	DBUG_ON(msg->flags & XPC_M_DONE);

	msg->flags |= XPC_M_DONE;

	/*
	 * The preceding store of msg->flags must occur before the following
	 * load of local_GP->get.
	 */
	mb();

	/*
	 * See if this message is next in line to be acknowledged as having
	 * been delivered.
	 */
	get = ch->sn.sn2.local_GP->get;
	if (get == msg_number)
		xpc_acknowledge_msgs_sn2(ch, get, msg->flags);
}

int
xpc_init_sn2(void)
{
	int ret;

	xpc_rsvd_page_init = xpc_rsvd_page_init_sn2;
	xpc_increment_heartbeat = xpc_increment_heartbeat_sn2;
	xpc_offline_heartbeat = xpc_offline_heartbeat_sn2;
	xpc_online_heartbeat = xpc_online_heartbeat_sn2;
	xpc_heartbeat_init = xpc_heartbeat_init_sn2;
	xpc_heartbeat_exit = xpc_heartbeat_exit_sn2;
	xpc_check_remote_hb = xpc_check_remote_hb_sn2;

	xpc_request_partition_activation = xpc_request_partition_activation_sn2;
	xpc_request_partition_reactivation =
	    xpc_request_partition_reactivation_sn2;
	xpc_request_partition_deactivation =
	    xpc_request_partition_deactivation_sn2;
	xpc_cancel_partition_deactivation_request =
	    xpc_cancel_partition_deactivation_request_sn2;

	xpc_process_activate_IRQ_rcvd = xpc_process_activate_IRQ_rcvd_sn2;
	xpc_setup_infrastructure = xpc_setup_infrastructure_sn2;
	xpc_teardown_infrastructure = xpc_teardown_infrastructure_sn2;
	xpc_make_first_contact = xpc_make_first_contact_sn2;
	xpc_get_chctl_all_flags = xpc_get_chctl_all_flags_sn2;
	xpc_allocate_msgqueues = xpc_allocate_msgqueues_sn2;
	xpc_free_msgqueues = xpc_free_msgqueues_sn2;
	xpc_notify_senders_of_disconnect = xpc_notify_senders_of_disconnect_sn2;
	xpc_process_msg_chctl_flags = xpc_process_msg_chctl_flags_sn2;
	xpc_n_of_deliverable_msgs = xpc_n_of_deliverable_msgs_sn2;
	xpc_get_deliverable_msg = xpc_get_deliverable_msg_sn2;

	xpc_indicate_partition_engaged = xpc_indicate_partition_engaged_sn2;
	xpc_partition_engaged = xpc_partition_engaged_sn2;
	xpc_any_partition_engaged = xpc_any_partition_engaged_sn2;
	xpc_indicate_partition_disengaged =
	    xpc_indicate_partition_disengaged_sn2;
	xpc_assume_partition_disengaged = xpc_assume_partition_disengaged_sn2;

	xpc_send_chctl_closerequest = xpc_send_chctl_closerequest_sn2;
	xpc_send_chctl_closereply = xpc_send_chctl_closereply_sn2;
	xpc_send_chctl_openrequest = xpc_send_chctl_openrequest_sn2;
	xpc_send_chctl_openreply = xpc_send_chctl_openreply_sn2;

	xpc_send_msg = xpc_send_msg_sn2;
	xpc_received_msg = xpc_received_msg_sn2;

	/* open up protections for IPI and [potentially] amo operations */
	xpc_allow_IPI_ops_sn2();
	xpc_allow_amo_ops_shub_wars_1_1_sn2();

	/*
	 * This is safe to do before the xpc_hb_checker thread has started
	 * because the handler releases a wait queue.  If an interrupt is
	 * received before the thread is waiting, it will not go to sleep,
	 * but rather immediately process the interrupt.
	 */
	ret = request_irq(SGI_XPC_ACTIVATE, xpc_handle_activate_IRQ_sn2, 0,
			  "xpc hb", NULL);
	if (ret != 0) {
		dev_err(xpc_part, "can't register ACTIVATE IRQ handler, "
			"errno=%d\n", -ret);
		xpc_disallow_IPI_ops_sn2();
	}
	return ret;
}

void
xpc_exit_sn2(void)
{
	free_irq(SGI_XPC_ACTIVATE, NULL);
	xpc_disallow_IPI_ops_sn2();
}
