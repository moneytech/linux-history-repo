
#include <linux/irq.h>

void move_masked_irq(int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	cpumask_t tmp;

	if (likely(!(desc->status & IRQ_MOVE_PENDING)))
		return;

	/*
	 * Paranoia: cpu-local interrupts shouldn't be calling in here anyway.
	 */
	if (CHECK_IRQ_PER_CPU(desc->status)) {
		WARN_ON(1);
		return;
	}

	desc->status &= ~IRQ_MOVE_PENDING;

	if (unlikely(cpus_empty(desc->pending_mask)))
		return;

	if (!desc->chip->set_affinity)
		return;

	assert_spin_locked(&desc->lock);

	cpus_and(tmp, desc->pending_mask, cpu_online_map);

	/*
	 * If there was a valid mask to work with, please
	 * do the disable, re-program, enable sequence.
	 * This is *not* particularly important for level triggered
	 * but in a edge trigger case, we might be setting rte
	 * when an active trigger is comming in. This could
	 * cause some ioapics to mal-function.
	 * Being paranoid i guess!
	 *
	 * For correct operation this depends on the caller
	 * masking the irqs.
	 */
	if (likely(!cpus_empty(tmp))) {
		desc->chip->set_affinity(irq,tmp);
	}
	cpus_clear(desc->pending_mask);
}

void move_native_irq(int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (likely(!(desc->status & IRQ_MOVE_PENDING)))
		return;

	if (unlikely(desc->status & IRQ_DISABLED))
		return;

	desc->chip->mask(irq);
	move_masked_irq(irq);
	desc->chip->unmask(irq);
}

