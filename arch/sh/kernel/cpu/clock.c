/*
 * arch/sh/kernel/cpu/clock.c - SuperH clock framework
 *
 *  Copyright (C) 2005  Paul Mundt
 *
 * This clock framework is derived from the OMAP version by:
 *
 *	Copyright (C) 2004 Nokia Corporation
 *	Written by Tuukka Tikkanen <tuukka.tikkanen@elektrobit.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <asm/clock.h>
#include <asm/timer.h>

static LIST_HEAD(clock_list);
static DEFINE_SPINLOCK(clock_lock);
static DECLARE_MUTEX(clock_list_sem);

/*
 * Each subtype is expected to define the init routines for these clocks,
 * as each subtype (or processor family) will have these clocks at the
 * very least. These are all provided through the CPG, which even some of
 * the more quirky parts (such as ST40, SH4-202, etc.) still have.
 *
 * The processor-specific code is expected to register any additional
 * clock sources that are of interest.
 */
static struct clk master_clk = {
	.name		= "master_clk",
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
#ifdef CONFIG_SH_PCLK_FREQ_BOOL
	.rate		= CONFIG_SH_PCLK_FREQ,
#endif
};

static struct clk module_clk = {
	.name		= "module_clk",
	.parent		= &master_clk,
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
};

static struct clk bus_clk = {
	.name		= "bus_clk",
	.parent		= &master_clk,
	.flags		= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
};

static struct clk cpu_clk = {
	.name		= "cpu_clk",
	.parent		= &master_clk,
	.flags		= CLK_ALWAYS_ENABLED,
};

/*
 * The ordering of these clocks matters, do not change it.
 */
static struct clk *onchip_clocks[] = {
	&master_clk,
	&module_clk,
	&bus_clk,
	&cpu_clk,
};

static void propagate_rate(struct clk *clk)
{
	struct clk *clkp;

	list_for_each_entry(clkp, &clock_list, node) {
		if (likely(clkp->parent != clk))
			continue;
		if (likely(clkp->ops && clkp->ops->recalc))
			clkp->ops->recalc(clkp);
	}
}

int __clk_enable(struct clk *clk)
{
	/*
	 * See if this is the first time we're enabling the clock, some
	 * clocks that are always enabled still require "special"
	 * initialization. This is especially true if the clock mode
	 * changes and the clock needs to hunt for the proper set of
	 * divisors to use before it can effectively recalc.
	 */
	if (unlikely(atomic_read(&clk->kref.refcount) == 1))
		if (clk->ops && clk->ops->init)
			clk->ops->init(clk);

	if (clk->flags & CLK_ALWAYS_ENABLED)
		return 0;

	if (likely(clk->ops && clk->ops->enable))
		clk->ops->enable(clk);

	kref_get(&clk->kref);
	return 0;
}

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&clock_lock, flags);
	ret = __clk_enable(clk);
	spin_unlock_irqrestore(&clock_lock, flags);

	return ret;
}

static void clk_kref_release(struct kref *kref)
{
	/* Nothing to do */
}

void __clk_disable(struct clk *clk)
{
	if (clk->flags & CLK_ALWAYS_ENABLED)
		return;

	kref_put(&clk->kref, clk_kref_release);
}

void clk_disable(struct clk *clk)
{
	unsigned long flags;

	spin_lock_irqsave(&clock_lock, flags);
	__clk_disable(clk);
	spin_unlock_irqrestore(&clock_lock, flags);
}

int clk_register(struct clk *clk)
{
	down(&clock_list_sem);

	list_add(&clk->node, &clock_list);
	kref_init(&clk->kref);

	up(&clock_list_sem);

	return 0;
}

void clk_unregister(struct clk *clk)
{
	down(&clock_list_sem);
	list_del(&clk->node);
	up(&clock_list_sem);
}

inline unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = -EOPNOTSUPP;

	if (likely(clk->ops && clk->ops->set_rate)) {
		unsigned long flags;

		spin_lock_irqsave(&clock_lock, flags);
		ret = clk->ops->set_rate(clk, rate);
		spin_unlock_irqrestore(&clock_lock, flags);
	}

	if (unlikely(clk->flags & CLK_RATE_PROPAGATES))
		propagate_rate(clk);

	return ret;
}

void clk_recalc_rate(struct clk *clk)
{
	if (likely(clk->ops && clk->ops->recalc)) {
		unsigned long flags;

		spin_lock_irqsave(&clock_lock, flags);
		clk->ops->recalc(clk);
		spin_unlock_irqrestore(&clock_lock, flags);
	}

	if (unlikely(clk->flags & CLK_RATE_PROPAGATES))
		propagate_rate(clk);
}

struct clk *clk_get(const char *id)
{
	struct clk *p, *clk = ERR_PTR(-ENOENT);

	down(&clock_list_sem);
	list_for_each_entry(p, &clock_list, node) {
		if (strcmp(id, p->name) == 0 && try_module_get(p->owner)) {
			clk = p;
			break;
		}
	}
	up(&clock_list_sem);

	return clk;
}

void clk_put(struct clk *clk)
{
	if (clk && !IS_ERR(clk))
		module_put(clk->owner);
}

void __init __attribute__ ((weak))
arch_init_clk_ops(struct clk_ops **ops, int type)
{
}

int __init clk_init(void)
{
	int i, ret = 0;

	if (unlikely(!master_clk.rate))
		/*
		 * NOTE: This will break if the default divisor has been
		 * changed.
		 *
		 * No one should be changing the default on us however,
		 * expect that a sane value for CONFIG_SH_PCLK_FREQ will
		 * be defined in the event of a different divisor.
		 */
		master_clk.rate = get_timer_frequency() * 4;

	for (i = 0; i < ARRAY_SIZE(onchip_clocks); i++) {
		struct clk *clk = onchip_clocks[i];

		arch_init_clk_ops(&clk->ops, i);
		ret |= clk_register(clk);
		clk_enable(clk);
	}

	/* Kick the child clocks.. */
	propagate_rate(&master_clk);
	propagate_rate(&bus_clk);

	return ret;
}

int show_clocks(struct seq_file *m)
{
	struct clk *clk;

	list_for_each_entry_reverse(clk, &clock_list, node) {
		unsigned long rate = clk_get_rate(clk);

		/*
		 * Don't bother listing dummy clocks with no ancestry
		 * that only support enable and disable ops.
		 */
		if (unlikely(!rate && !clk->parent))
			continue;

		seq_printf(m, "%-12s\t: %ld.%02ldMHz\n", clk->name,
			   rate / 1000000, (rate % 1000000) / 10000);
	}

	return 0;
}

EXPORT_SYMBOL_GPL(clk_register);
EXPORT_SYMBOL_GPL(clk_unregister);
EXPORT_SYMBOL_GPL(clk_get);
EXPORT_SYMBOL_GPL(clk_put);
EXPORT_SYMBOL_GPL(clk_enable);
EXPORT_SYMBOL_GPL(clk_disable);
EXPORT_SYMBOL_GPL(__clk_enable);
EXPORT_SYMBOL_GPL(__clk_disable);
EXPORT_SYMBOL_GPL(clk_get_rate);
EXPORT_SYMBOL_GPL(clk_set_rate);
EXPORT_SYMBOL_GPL(clk_recalc_rate);
