/*
 * linux/include/asm-arm/arch-iop3xx/memory.h
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#include <asm/hardware.h>

/*
 * Physical DRAM offset.
 */
#ifndef CONFIG_ARCH_IOP33X
#define PHYS_OFFSET	UL(0xa0000000)
#else
#define PHYS_OFFSET	UL(0x00000000)
#endif

/*
 * Virtual view <-> PCI DMA view memory address translations
 * virt_to_bus: Used to translate the virtual address to an
 *		address suitable to be passed to set_dma_addr
 * bus_to_virt: Used to convert an address for DMA operations
 *		to an address that the kernel can use.
 */
#if defined(CONFIG_ARCH_IOP32X)

#define __virt_to_bus(x)	(((__virt_to_phys(x)) & ~(*IOP321_IATVR2)) | ((*IOP321_IABAR2) & 0xfffffff0))
#define __bus_to_virt(x)    (__phys_to_virt(((x) & ~(*IOP321_IALR2)) | ( *IOP321_IATVR2)))

#elif defined(CONFIG_ARCH_IOP33X)

#define __virt_to_bus(x)	(((__virt_to_phys(x)) & ~(*IOP331_IATVR2)) | ((*IOP331_IABAR2) & 0xfffffff0))
#define __bus_to_virt(x)    (__phys_to_virt(((x) & ~(*IOP331_IALR2)) | ( *IOP331_IATVR2)))

#endif

#endif
