/*
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_MXC_MX31_H__
#define __ASM_ARCH_MXC_MX31_H__

/*
 * MX31 memory map:
 *
 * Virt		Phys		Size	What
 * ---------------------------------------------------------------------------
 * FC000000	43F00000	1M	AIPS 1
 * FC100000	50000000	1M	SPBA
 * FC200000	53F00000	1M	AIPS 2
 * FC500000	60000000	128M	ROMPATCH
 * FC400000	68000000	128M	AVIC
 *         	70000000	256M	IPU (MAX M2)
 *         	80000000	256M	CSD0 SDRAM/DDR
 *         	90000000	256M	CSD1 SDRAM/DDR
 *         	A0000000	128M	CS0 Flash
 *         	A8000000	128M	CS1 Flash
 *         	B0000000	32M	CS2
 *         	B2000000	32M	CS3
 * F4000000	B4000000	32M	CS4
 *         	B6000000	32M	CS5
 * FC320000	B8000000	64K	NAND, SDRAM, WEIM, M3IF, EMI controllers
 *         	C0000000	64M	PCMCIA/CF
 */

/*
 * L2CC
 */
#define L2CC_BASE_ADDR		0x30000000
#define L2CC_SIZE		SZ_1M

/*
 * AIPS 1
 */
#define AIPS1_BASE_ADDR		0x43F00000
#define AIPS1_BASE_ADDR_VIRT	0xFC000000
#define AIPS1_SIZE		SZ_1M

#define MAX_BASE_ADDR		(AIPS1_BASE_ADDR + 0x00004000)
#define EVTMON_BASE_ADDR	(AIPS1_BASE_ADDR + 0x00008000)
#define CLKCTL_BASE_ADDR	(AIPS1_BASE_ADDR + 0x0000C000)
#define ETB_SLOT4_BASE_ADDR	(AIPS1_BASE_ADDR + 0x00010000)
#define ETB_SLOT5_BASE_ADDR	(AIPS1_BASE_ADDR + 0x00014000)
#define ECT_CTIO_BASE_ADDR	(AIPS1_BASE_ADDR + 0x00018000)
#define I2C_BASE_ADDR		(AIPS1_BASE_ADDR + 0x00080000)
#define I2C3_BASE_ADDR		(AIPS1_BASE_ADDR + 0x00084000)
#define UART1_BASE_ADDR 	(AIPS1_BASE_ADDR + 0x00090000)
#define UART2_BASE_ADDR 	(AIPS1_BASE_ADDR + 0x00094000)
#define I2C2_BASE_ADDR		(AIPS1_BASE_ADDR + 0x00098000)
#define OWIRE_BASE_ADDR 	(AIPS1_BASE_ADDR + 0x0009C000)
#define SSI1_BASE_ADDR		(AIPS1_BASE_ADDR + 0x000A0000)
#define CSPI1_BASE_ADDR 	(AIPS1_BASE_ADDR + 0x000A4000)
#define KPP_BASE_ADDR		(AIPS1_BASE_ADDR + 0x000A8000)
#define IOMUXC_BASE_ADDR	(AIPS1_BASE_ADDR + 0x000AC000)
#define ECT_IP1_BASE_ADDR	(AIPS1_BASE_ADDR + 0x000B8000)
#define ECT_IP2_BASE_ADDR	(AIPS1_BASE_ADDR + 0x000BC000)

/*
 * SPBA global module enabled #0
 */
#define SPBA0_BASE_ADDR 	0x50000000
#define SPBA0_BASE_ADDR_VIRT	0xFC100000
#define SPBA0_SIZE		SZ_1M

#define UART3_BASE_ADDR 	(SPBA0_BASE_ADDR + 0x0000C000)
#define CSPI2_BASE_ADDR 	(SPBA0_BASE_ADDR + 0x00010000)
#define SSI2_BASE_ADDR		(SPBA0_BASE_ADDR + 0x00014000)
#define ATA_DMA_BASE_ADDR	(SPBA0_BASE_ADDR + 0x00020000)
#define MSHC1_BASE_ADDR		(SPBA0_BASE_ADDR + 0x00024000)
#define SPBA_CTRL_BASE_ADDR	(SPBA0_BASE_ADDR + 0x0003C000)

/*
 * AIPS 2
 */
#define AIPS2_BASE_ADDR		0x53F00000
#define AIPS2_BASE_ADDR_VIRT	0xFC200000
#define AIPS2_SIZE		SZ_1M

#define CCM_BASE_ADDR		(AIPS2_BASE_ADDR + 0x00080000)
#define GPT1_BASE_ADDR		(AIPS2_BASE_ADDR + 0x00090000)
#define EPIT1_BASE_ADDR		(AIPS2_BASE_ADDR + 0x00094000)
#define EPIT2_BASE_ADDR		(AIPS2_BASE_ADDR + 0x00098000)
#define GPIO3_BASE_ADDR		(AIPS2_BASE_ADDR + 0x000A4000)
#define SCC_BASE_ADDR		(AIPS2_BASE_ADDR + 0x000AC000)
#define RNGA_BASE_ADDR		(AIPS2_BASE_ADDR + 0x000B0000)
#define IPU_CTRL_BASE_ADDR	(AIPS2_BASE_ADDR + 0x000C0000)
#define AUDMUX_BASE_ADDR	(AIPS2_BASE_ADDR + 0x000C4000)
#define GPIO1_BASE_ADDR		(AIPS2_BASE_ADDR + 0x000CC000)
#define GPIO2_BASE_ADDR		(AIPS2_BASE_ADDR + 0x000D0000)
#define SDMA_BASE_ADDR		(AIPS2_BASE_ADDR + 0x000D4000)
#define RTC_BASE_ADDR		(AIPS2_BASE_ADDR + 0x000D8000)
#define WDOG_BASE_ADDR		(AIPS2_BASE_ADDR + 0x000DC000)
#define PWM_BASE_ADDR		(AIPS2_BASE_ADDR + 0x000E0000)
#define RTIC_BASE_ADDR		(AIPS2_BASE_ADDR + 0x000EC000)

/*
 * ROMP and AVIC
 */
#define ROMP_BASE_ADDR		0x60000000
#define ROMP_BASE_ADDR_VIRT	0xFC500000
#define ROMP_SIZE		SZ_1M

#define AVIC_BASE_ADDR		0x68000000
#define AVIC_BASE_ADDR_VIRT	0xFC400000
#define AVIC_SIZE		SZ_1M

/*
 * Memory regions and CS
 */
#define IPU_MEM_BASE_ADDR	0x70000000
#define CSD0_BASE_ADDR		0x80000000
#define CSD1_BASE_ADDR		0x90000000

#define CS0_BASE_ADDR		0xA0000000
#define CS1_BASE_ADDR		0xA8000000
#define CS2_BASE_ADDR		0xB0000000
#define CS3_BASE_ADDR		0xB2000000

#define CS4_BASE_ADDR		0xB4000000
#define CS4_BASE_ADDR_VIRT	0xF4000000
#define CS4_SIZE		SZ_32M

#define CS5_BASE_ADDR		0xB6000000
#define CS5_BASE_ADDR_VIRT	0xF6000000
#define CS5_SIZE		SZ_32M


/*
 * NAND, SDRAM, WEIM, M3IF, EMI controllers
 */
#define X_MEMC_BASE_ADDR	0xB8000000
#define X_MEMC_BASE_ADDR_VIRT	0xFC320000
#define X_MEMC_SIZE		SZ_64K

#define ESDCTL_BASE_ADDR	(X_MEMC_BASE_ADDR + 0x1000)
#define WEIM_BASE_ADDR		(X_MEMC_BASE_ADDR + 0x2000)
#define M3IF_BASE_ADDR		(X_MEMC_BASE_ADDR + 0x3000)
#define EMI_CTL_BASE_ADDR	(X_MEMC_BASE_ADDR + 0x4000)
#define PCMCIA_CTL_BASE_ADDR	EMI_CTL_BASE_ADDR

#define PCMCIA_MEM_BASE_ADDR	0xBC000000

/*!
 * This macro defines the physical to virtual address mapping for all the
 * peripheral modules. It is used by passing in the physical address as x
 * and returning the virtual address. If the physical address is not mapped,
 * it returns 0xDEADBEEF
 */
#define IO_ADDRESS(x)   \
	(void __force __iomem *) \
	(((x >= AIPS1_BASE_ADDR) && (x < (AIPS1_BASE_ADDR + AIPS1_SIZE))) ? AIPS1_IO_ADDRESS(x):\
	((x >= SPBA0_BASE_ADDR) && (x < (SPBA0_BASE_ADDR + SPBA0_SIZE))) ? SPBA0_IO_ADDRESS(x):\
	((x >= AIPS2_BASE_ADDR) && (x < (AIPS2_BASE_ADDR + AIPS2_SIZE))) ? AIPS2_IO_ADDRESS(x):\
	((x >= ROMP_BASE_ADDR) && (x < (ROMP_BASE_ADDR + ROMP_SIZE))) ? ROMP_IO_ADDRESS(x):\
	((x >= AVIC_BASE_ADDR) && (x < (AVIC_BASE_ADDR + AVIC_SIZE))) ? AVIC_IO_ADDRESS(x):\
	((x >= CS4_BASE_ADDR) && (x < (CS4_BASE_ADDR + CS4_SIZE))) ? CS4_IO_ADDRESS(x):\
	((x >= X_MEMC_BASE_ADDR) && (x < (X_MEMC_BASE_ADDR + X_MEMC_SIZE))) ? X_MEMC_IO_ADDRESS(x):\
	0xDEADBEEF)

/*
 * define the address mapping macros: in physical address order
 */
#define L2CC_IO_ADDRESS(x)  \
	(((x) - L2CC_BASE_ADDR) + L2CC_BASE_ADDR_VIRT)

#define AIPS1_IO_ADDRESS(x)  \
	(((x) - AIPS1_BASE_ADDR) + AIPS1_BASE_ADDR_VIRT)

#define SPBA0_IO_ADDRESS(x)  \
	(((x) - SPBA0_BASE_ADDR) + SPBA0_BASE_ADDR_VIRT)

#define AIPS2_IO_ADDRESS(x)  \
	(((x) - AIPS2_BASE_ADDR) + AIPS2_BASE_ADDR_VIRT)

#define ROMP_IO_ADDRESS(x)  \
	(((x) - ROMP_BASE_ADDR) + ROMP_BASE_ADDR_VIRT)

#define AVIC_IO_ADDRESS(x)  \
	(((x) - AVIC_BASE_ADDR) + AVIC_BASE_ADDR_VIRT)

#define CS4_IO_ADDRESS(x)  \
	(((x) - CS4_BASE_ADDR) + CS4_BASE_ADDR_VIRT)

#define CS5_IO_ADDRESS(x)  \
	(((x) - CS5_BASE_ADDR) + CS5_BASE_ADDR_VIRT)

#define X_MEMC_IO_ADDRESS(x)  \
	(((x) - X_MEMC_BASE_ADDR) + X_MEMC_BASE_ADDR_VIRT)

#define PCMCIA_IO_ADDRESS(x) \
	(((x) - X_MEMC_BASE_ADDR) + X_MEMC_BASE_ADDR_VIRT)

/*
 * Interrupt numbers
 */
#define MXC_INT_I2C3		3
#define MXC_INT_I2C2		4
#define MXC_INT_RTIC		6
#define MXC_INT_I2C		10
#define MXC_INT_CSPI2		13
#define MXC_INT_CSPI1		14
#define MXC_INT_ATA		15
#define MXC_INT_UART3		18
#define MXC_INT_IIM		19
#define MXC_INT_RNGA		22
#define MXC_INT_EVTMON		23
#define MXC_INT_KPP		24
#define MXC_INT_RTC		25
#define MXC_INT_PWM		26
#define MXC_INT_EPIT2		27
#define MXC_INT_EPIT1		28
#define MXC_INT_GPT		29
#define MXC_INT_POWER_FAIL	30
#define MXC_INT_UART2		32
#define MXC_INT_NANDFC		33
#define MXC_INT_SDMA		34
#define MXC_INT_MSHC1		39
#define MXC_INT_IPU_ERR		41
#define MXC_INT_IPU_SYN		42
#define MXC_INT_UART1		45
#define MXC_INT_ECT		48
#define MXC_INT_SCC_SCM		49
#define MXC_INT_SCC_SMN		50
#define MXC_INT_GPIO2		51
#define MXC_INT_GPIO1		52
#define MXC_INT_WDOG		55
#define MXC_INT_GPIO3		56
#define MXC_INT_EXT_POWER	58
#define MXC_INT_EXT_TEMPER	59
#define MXC_INT_EXT_SENSOR60	60
#define MXC_INT_EXT_SENSOR61	61
#define MXC_INT_EXT_WDOG	62
#define MXC_INT_EXT_TV		63

#define PROD_SIGNATURE		0x1	/* For MX31 */

/* silicon revisions specific to i.MX31 */
#define CHIP_REV_1_0		0x10
#define CHIP_REV_1_1		0x11
#define CHIP_REV_1_2		0x12
#define CHIP_REV_1_3		0x13
#define CHIP_REV_2_0		0x20
#define CHIP_REV_2_1		0x21
#define CHIP_REV_2_2		0x22
#define CHIP_REV_2_3		0x23
#define CHIP_REV_3_0		0x30
#define CHIP_REV_3_1		0x31
#define CHIP_REV_3_2		0x32

#define SYSTEM_REV_MIN		CHIP_REV_1_0
#define SYSTEM_REV_NUM		3

/* Mandatory defines used globally */

#if !defined(__ASSEMBLY__) && !defined(__MXC_BOOT_UNCOMPRESS)

extern unsigned int system_rev;

static inline int mx31_revision(void)
{
	return system_rev;
}
#endif

#endif /*  __ASM_ARCH_MXC_MX31_H__ */
