/*
 * File:         include/asm-blackfin/mach-bf538/mem_map.h
 * Based on:
 * Author:
 *
 * Created:
 * Description:
 *
 * Rev:
 *
 * Modified:
 *
 * Bugs:         Enter bugs at http://blackfin.uclinux.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.
 * If not, write to the Free Software Foundation,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _MEM_MAP_538_H_
#define _MEM_MAP_538_H_

#define COREMMR_BASE           0xFFE00000	 /* Core MMRs */
#define SYSMMR_BASE            0xFFC00000	 /* System MMRs */

/* Async Memory Banks */
#define ASYNC_BANK3_BASE	0x20300000	 /* Async Bank 3 */
#define ASYNC_BANK3_SIZE	0x00100000	/* 1M */
#define ASYNC_BANK2_BASE	0x20200000	 /* Async Bank 2 */
#define ASYNC_BANK2_SIZE	0x00100000	/* 1M */
#define ASYNC_BANK1_BASE	0x20100000	 /* Async Bank 1 */
#define ASYNC_BANK1_SIZE	0x00100000	/* 1M */
#define ASYNC_BANK0_BASE	0x20000000	 /* Async Bank 0 */
#define ASYNC_BANK0_SIZE	0x00100000	/* 1M */

/* Boot ROM Memory */

#define BOOT_ROM_START		0xEF000000
#define BOOT_ROM_LENGTH		0x400

/* Level 1 Memory */

#ifdef CONFIG_BFIN_ICACHE
#define BFIN_ICACHESIZE	(16*1024)
#else
#define BFIN_ICACHESIZE	(0*1024)
#endif

/* Memory Map for ADSP-BF538/9 processors */

#define L1_CODE_START       0xFFA00000
#define L1_DATA_A_START     0xFF800000
#define L1_DATA_B_START     0xFF900000

#ifdef CONFIG_BFIN_ICACHE
#define L1_CODE_LENGTH      (0x14000 - 0x4000)
#else
#define L1_CODE_LENGTH      0x14000
#endif

#ifdef CONFIG_BFIN_DCACHE

#ifdef CONFIG_BFIN_DCACHE_BANKA
#define DMEM_CNTR (ACACHE_BSRAM | ENDCPLB | PORT_PREF0)
#define L1_DATA_A_LENGTH      (0x8000 - 0x4000)
#define L1_DATA_B_LENGTH      0x8000
#define BFIN_DCACHESIZE	(16*1024)
#define BFIN_DSUPBANKS	1
#else
#define DMEM_CNTR (ACACHE_BCACHE | ENDCPLB | PORT_PREF0)
#define L1_DATA_A_LENGTH      (0x8000 - 0x4000)
#define L1_DATA_B_LENGTH      (0x8000 - 0x4000)
#define BFIN_DCACHESIZE	(32*1024)
#define BFIN_DSUPBANKS	2
#endif

#else
#define DMEM_CNTR (ASRAM_BSRAM | ENDCPLB | PORT_PREF0)
#define L1_DATA_A_LENGTH      0x8000
#define L1_DATA_B_LENGTH      0x8000
#define BFIN_DCACHESIZE	(0*1024)
#define BFIN_DSUPBANKS	0
#endif /*CONFIG_BFIN_DCACHE*/


/* Level 2 Memory - none */

#define L2_START	0
#define L2_LENGTH	0

/* Scratch Pad Memory */

#define L1_SCRATCH_START	0xFFB00000
#define L1_SCRATCH_LENGTH	0x1000

#define get_l1_scratch_start_cpu(cpu)		L1_SCRATCH_START
#define get_l1_code_start_cpu(cpu)		L1_CODE_START
#define get_l1_data_a_start_cpu(cpu)		L1_DATA_A_START
#define get_l1_data_b_start_cpu(cpu)		L1_DATA_B_START
#define get_l1_scratch_start()			L1_SCRATCH_START
#define get_l1_code_start()			L1_CODE_START
#define get_l1_data_a_start()			L1_DATA_A_START
#define get_l1_data_b_start()			L1_DATA_B_START

#define GET_PDA_SAFE(preg)		\
	preg.l = _cpu_pda;		\
	preg.h = _cpu_pda;

#define GET_PDA(preg, dreg)	GET_PDA_SAFE(preg)

#endif				/* _MEM_MAP_538_H_ */
