#ifndef _ASM_GENERIC_PERCPU_H_
#define _ASM_GENERIC_PERCPU_H_
#include <linux/compiler.h>
#include <linux/threads.h>

#ifdef CONFIG_SMP

extern unsigned long __per_cpu_offset[NR_CPUS];

#define per_cpu_offset(x) (__per_cpu_offset[x])

/* var is in discarded region: offset to particular copy we want */
#define per_cpu(var, cpu) (*({				\
	extern int simple_identifier_##var(void);	\
	RELOC_HIDE(&per_cpu__##var, __per_cpu_offset[cpu]); }))
#define __get_cpu_var(var) per_cpu(var, smp_processor_id())
#define __raw_get_cpu_var(var) per_cpu(var, raw_smp_processor_id())

/* A macro to avoid #include hell... */
#define percpu_modcopy(pcpudst, src, size)			\
do {								\
	unsigned int __i;					\
	for_each_possible_cpu(__i)				\
		memcpy((pcpudst)+__per_cpu_offset[__i],		\
		       (src), (size));				\
} while (0)
#else /* ! SMP */

#define per_cpu(var, cpu)			(*((void)(cpu), &per_cpu__##var))
#define __get_cpu_var(var)			per_cpu__##var
#define __raw_get_cpu_var(var)			per_cpu__##var

#endif	/* SMP */

#define DECLARE_PER_CPU(type, name) extern __typeof__(type) per_cpu__##name

#endif /* _ASM_GENERIC_PERCPU_H_ */
