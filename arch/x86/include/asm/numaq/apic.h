#ifndef __ASM_NUMAQ_APIC_H
#define __ASM_NUMAQ_APIC_H

#include <asm/io.h>
#include <linux/mmzone.h>
#include <linux/nodemask.h>

#define APIC_DFR_VALUE	(APIC_DFR_CLUSTER)

static inline const cpumask_t *numaq_target_cpus(void)
{
	return &CPU_MASK_ALL;
}

static inline unsigned long
numaq_check_apicid_used(physid_mask_t bitmap, int apicid)
{
	return physid_isset(apicid, bitmap);
}
static inline unsigned long numaq_check_apicid_present(int bit)
{
	return physid_isset(bit, phys_cpu_present_map);
}
#define apicid_cluster(apicid) (apicid & 0xF0)

static inline int numaq_apic_id_registered(void)
{
	return 1;
}

static inline void numaq_init_apic_ldr(void)
{
	/* Already done in NUMA-Q firmware */
}

static inline void numaq_setup_apic_routing(void)
{
	printk("Enabling APIC mode:  %s.  Using %d I/O APICs\n",
		"NUMA-Q", nr_ioapics);
}

/*
 * Skip adding the timer int on secondary nodes, which causes
 * a small but painful rift in the time-space continuum.
 */
static inline int numaq_multi_timer_check(int apic, int irq)
{
	return apic != 0 && irq == 0;
}

static inline physid_mask_t numaq_ioapic_phys_id_map(physid_mask_t phys_map)
{
	/* We don't have a good way to do this yet - hack */
	return physids_promote(0xFUL);
}

/* Mapping from cpu number to logical apicid */
extern u8 cpu_2_logical_apicid[];

static inline int numaq_cpu_to_logical_apicid(int cpu)
{
	if (cpu >= nr_cpu_ids)
		return BAD_APICID;
	return (int)cpu_2_logical_apicid[cpu];
}

/*
 * Supporting over 60 cpus on NUMA-Q requires a locality-dependent
 * cpu to APIC ID relation to properly interact with the intelligent
 * mode of the cluster controller.
 */
static inline int numaq_cpu_present_to_apicid(int mps_cpu)
{
	if (mps_cpu < 60)
		return ((mps_cpu >> 2) << 4) | (1 << (mps_cpu & 0x3));
	else
		return BAD_APICID;
}

static inline int numaq_apicid_to_node(int logical_apicid) 
{
	return logical_apicid >> 4;
}

static inline physid_mask_t numaq_apicid_to_cpu_present(int logical_apicid)
{
	int node = numaq_apicid_to_node(logical_apicid);
	int cpu = __ffs(logical_apicid & 0xf);

	return physid_mask_of_physid(cpu + 4*node);
}

extern void *xquad_portio;

static inline int numaq_check_phys_apicid_present(int boot_cpu_physical_apicid)
{
	return 1;
}

/*
 * We use physical apicids here, not logical, so just return the default
 * physical broadcast to stop people from breaking us
 */
static inline unsigned int cpu_mask_to_apicid(const cpumask_t *cpumask)
{
	return (int) 0xF;
}

static inline unsigned int cpu_mask_to_apicid_and(const struct cpumask *cpumask,
						  const struct cpumask *andmask)
{
	return (int) 0xF;
}

/* No NUMA-Q box has a HT CPU, but it can't hurt to use the default code. */
static inline u32 phys_pkg_id(u32 cpuid_apic, int index_msb)
{
	return cpuid_apic >> index_msb;
}

#endif /* __ASM_NUMAQ_APIC_H */
