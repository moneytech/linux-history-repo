#ifndef _ASM_SH_TOPOLOGY_H
#define _ASM_SH_TOPOLOGY_H

#ifdef CONFIG_NUMA

/* sched_domains SD_NODE_INIT for sh machines */
#define SD_NODE_INIT (struct sched_domain) {		\
	.span			= CPU_MASK_NONE,	\
	.parent			= NULL,			\
	.child			= NULL,			\
	.groups			= NULL,			\
	.min_interval		= 8,			\
	.max_interval		= 32,			\
	.busy_factor		= 32,			\
	.imbalance_pct		= 125,			\
	.cache_nice_tries	= 2,			\
	.busy_idx		= 3,			\
	.idle_idx		= 2,			\
	.newidle_idx		= 0,			\
	.wake_idx		= 1,			\
	.forkexec_idx		= 1,			\
	.flags			= SD_LOAD_BALANCE	\
				| SD_BALANCE_FORK	\
				| SD_BALANCE_EXEC	\
				| SD_SERIALIZE		\
				| SD_WAKE_BALANCE,	\
	.last_balance		= jiffies,		\
	.balance_interval	= 1,			\
	.nr_balance_failed	= 0,			\
}

#endif

#include <asm-generic/topology.h>

#endif /* _ASM_SH_TOPOLOGY_H */
