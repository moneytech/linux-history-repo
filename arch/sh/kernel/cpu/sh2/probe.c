/*
 * arch/sh/kernel/cpu/sh2/probe.c
 *
 * CPU Subtype Probing for SH-2.
 *
 * Copyright (C) 2002 Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */


#include <linux/init.h>
#include <asm/processor.h>
#include <asm/cache.h>

int __init detect_cpu_and_cache_system(void)
{
#if defined(CONFIG_CPU_SUBTYPE_SH7604)
	cpu_data->type			= CPU_SH7604;
	cpu_data->dcache.ways		= 4;
	cpu_data->dcache.way_incr	= (1<<10);
	cpu_data->dcache.sets		= 64;
	cpu_data->dcache.entry_shift	= 4;
	cpu_data->dcache.linesz		= L1_CACHE_BYTES;
	cpu_data->dcache.flags		= 0;
#elif defined(CONFIG_CPU_SUBTYPE_SH7619)
	cpu_data->type			= CPU_SH7619;
	cpu_data->dcache.ways		= 4;
	cpu_data->dcache.way_incr	= (1<<12);
	cpu_data->dcache.sets		= 256;
	cpu_data->dcache.entry_shift	= 4;
	cpu_data->dcache.linesz		= L1_CACHE_BYTES;
	cpu_data->dcache.flags		= 0;
#endif
	/*
	 * SH-2 doesn't have separate caches
	 */
	cpu_data->dcache.flags |= SH_CACHE_COMBINED;
	cpu_data->icache = cpu_data->dcache;

	return 0;
}

