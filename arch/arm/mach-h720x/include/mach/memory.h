/*
 * arch/arm/mach-h720x/include/mach/memory.h
 *
 * Copyright (c) 2000 Jungjun Kim
 *
 */
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define PHYS_OFFSET	UL(0x40000000)
/*
 * This is the maximum DMA address that can be DMAd to.
 * There should not be more than (0xd0000000 - 0xc0000000)
 * bytes of RAM.
 *
 * If you set this, you must also set ISA_DMA_THRESHOLD and setup a DMA
 * zone if this does not cover all possible RAM.
 */
#define MAX_DMA_ADDRESS		(PAGE_OFFSET + SZ_256M)

#endif
