#if !defined(_TRACE_IRQ_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_IRQ_H

#include <linux/tracepoint.h>
#include <linux/interrupt.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM irq

/*
 * Tracepoint for entry of interrupt handler:
 */
TRACE_FORMAT(irq_handler_entry,
	TP_PROTO(int irq, struct irqaction *action),
	TP_ARGS(irq, action),
	TP_FMT("irq=%d handler=%s", irq, action->name)
	);

/*
 * Tracepoint for return of an interrupt handler:
 */
TRACE_EVENT(irq_handler_exit,

	TP_PROTO(int irq, struct irqaction *action, int ret),

	TP_ARGS(irq, action, ret),

	TP_STRUCT__entry(
		__field(	int,	irq	)
		__field(	int,	ret	)
	),

	TP_fast_assign(
		__entry->irq	= irq;
		__entry->ret	= ret;
	),

	TP_printk("irq=%d return=%s",
		  __entry->irq, __entry->ret ? "handled" : "unhandled")
);

TRACE_FORMAT(softirq_entry,
	TP_PROTO(struct softirq_action *h, struct softirq_action *vec),
	TP_ARGS(h, vec),
	TP_FMT("softirq=%d action=%s", (int)(h - vec), softirq_to_name[h-vec])
	);

TRACE_FORMAT(softirq_exit,
	TP_PROTO(struct softirq_action *h, struct softirq_action *vec),
	TP_ARGS(h, vec),
	TP_FMT("softirq=%d action=%s", (int)(h - vec), softirq_to_name[h-vec])
	);

#endif /*  _TRACE_IRQ_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
