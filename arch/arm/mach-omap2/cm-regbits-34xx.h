#ifndef __ARCH_ARM_MACH_OMAP2_CM_REGBITS_34XX_H
#define __ARCH_ARM_MACH_OMAP2_CM_REGBITS_34XX_H

/*
 * OMAP3430 Clock Management register bits
 *
 * Copyright (C) 2007-2008 Texas Instruments, Inc.
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Written by Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "cm.h"

/* Bits shared between registers */

/* CM_FCLKEN1_CORE and CM_ICLKEN1_CORE shared bits */
#define OMAP3430ES2_EN_MMC3_MASK			(1 << 30)
#define OMAP3430ES2_EN_MMC3_SHIFT			30
#define OMAP3430_EN_MSPRO				(1 << 23)
#define OMAP3430_EN_MSPRO_SHIFT				23
#define OMAP3430_EN_HDQ					(1 << 22)
#define OMAP3430_EN_HDQ_SHIFT				22
#define OMAP3430ES1_EN_FSHOSTUSB			(1 << 5)
#define OMAP3430ES1_EN_FSHOSTUSB_SHIFT			5
#define OMAP3430ES1_EN_D2D				(1 << 3)
#define OMAP3430ES1_EN_D2D_SHIFT			3
#define OMAP3430_EN_SSI					(1 << 0)
#define OMAP3430_EN_SSI_SHIFT				0

/* CM_FCLKEN3_CORE and CM_ICLKEN3_CORE shared bits */
#define OMAP3430ES2_EN_USBTLL_SHIFT			2
#define OMAP3430ES2_EN_USBTLL_MASK			(1 << 2)

/* CM_FCLKEN_WKUP and CM_ICLKEN_WKUP shared bits */
#define OMAP3430_EN_WDT2				(1 << 5)
#define OMAP3430_EN_WDT2_SHIFT				5

/* CM_ICLKEN_CAM, CM_FCLKEN_CAM shared bits */
#define OMAP3430_EN_CAM					(1 << 0)
#define OMAP3430_EN_CAM_SHIFT				0

/* CM_FCLKEN_PER, CM_ICLKEN_PER shared bits */
#define OMAP3430_EN_WDT3				(1 << 12)
#define OMAP3430_EN_WDT3_SHIFT				12

/* CM_CLKSEL2_EMU, CM_CLKSEL3_EMU shared bits */
#define OMAP3430_OVERRIDE_ENABLE			(1 << 19)


/* Bits specific to each register */

/* CM_FCLKEN_IVA2 */
#define OMAP3430_CM_FCLKEN_IVA2_EN_IVA2			(1 << 0)
#define OMAP3430_CM_FCLKEN_IVA2_EN_IVA2_SHIFT		0

/* CM_CLKEN_PLL_IVA2 */
#define OMAP3430_IVA2_DPLL_RAMPTIME_SHIFT		8
#define OMAP3430_IVA2_DPLL_RAMPTIME_MASK		(0x3 << 8)
#define OMAP3430_IVA2_DPLL_FREQSEL_SHIFT		4
#define OMAP3430_IVA2_DPLL_FREQSEL_MASK			(0xf << 4)
#define OMAP3430_EN_IVA2_DPLL_DRIFTGUARD_SHIFT		3
#define OMAP3430_EN_IVA2_DPLL_DRIFTGUARD_MASK		(1 << 3)
#define OMAP3430_EN_IVA2_DPLL_SHIFT			0
#define OMAP3430_EN_IVA2_DPLL_MASK			(0x7 << 0)

/* CM_IDLEST_IVA2 */
#define OMAP3430_ST_IVA2				(1 << 0)

/* CM_IDLEST_PLL_IVA2 */
#define OMAP3430_ST_IVA2_CLK_SHIFT			0
#define OMAP3430_ST_IVA2_CLK_MASK			(1 << 0)

/* CM_AUTOIDLE_PLL_IVA2 */
#define OMAP3430_AUTO_IVA2_DPLL_SHIFT			0
#define OMAP3430_AUTO_IVA2_DPLL_MASK			(0x7 << 0)

/* CM_CLKSEL1_PLL_IVA2 */
#define OMAP3430_IVA2_CLK_SRC_SHIFT			19
#define OMAP3430_IVA2_CLK_SRC_MASK			(0x3 << 19)
#define OMAP3430_IVA2_DPLL_MULT_SHIFT			8
#define OMAP3430_IVA2_DPLL_MULT_MASK			(0x7ff << 8)
#define OMAP3430_IVA2_DPLL_DIV_SHIFT			0
#define OMAP3430_IVA2_DPLL_DIV_MASK			(0x7f << 0)

/* CM_CLKSEL2_PLL_IVA2 */
#define OMAP3430_IVA2_DPLL_CLKOUT_DIV_SHIFT		0
#define OMAP3430_IVA2_DPLL_CLKOUT_DIV_MASK		(0x1f << 0)

/* CM_CLKSTCTRL_IVA2 */
#define OMAP3430_CLKTRCTRL_IVA2_SHIFT			0
#define OMAP3430_CLKTRCTRL_IVA2_MASK			(0x3 << 0)

/* CM_CLKSTST_IVA2 */
#define OMAP3430_CLKACTIVITY_IVA2_SHIFT			0
#define OMAP3430_CLKACTIVITY_IVA2_MASK			(1 << 0)

/* CM_REVISION specific bits */

/* CM_SYSCONFIG specific bits */

/* CM_CLKEN_PLL_MPU */
#define OMAP3430_MPU_DPLL_RAMPTIME_SHIFT		8
#define OMAP3430_MPU_DPLL_RAMPTIME_MASK			(0x3 << 8)
#define OMAP3430_MPU_DPLL_FREQSEL_SHIFT			4
#define OMAP3430_MPU_DPLL_FREQSEL_MASK			(0xf << 4)
#define OMAP3430_EN_MPU_DPLL_DRIFTGUARD_SHIFT		3
#define OMAP3430_EN_MPU_DPLL_DRIFTGUARD_MASK		(1 << 3)
#define OMAP3430_EN_MPU_DPLL_SHIFT			0
#define OMAP3430_EN_MPU_DPLL_MASK			(0x7 << 0)

/* CM_IDLEST_MPU */
#define OMAP3430_ST_MPU					(1 << 0)

/* CM_IDLEST_PLL_MPU */
#define OMAP3430_ST_MPU_CLK_SHIFT			0
#define OMAP3430_ST_MPU_CLK_MASK			(1 << 0)

/* CM_AUTOIDLE_PLL_MPU */
#define OMAP3430_AUTO_MPU_DPLL_SHIFT			0
#define OMAP3430_AUTO_MPU_DPLL_MASK			(0x7 << 0)

/* CM_CLKSEL1_PLL_MPU */
#define OMAP3430_MPU_CLK_SRC_SHIFT			19
#define OMAP3430_MPU_CLK_SRC_MASK			(0x3 << 19)
#define OMAP3430_MPU_DPLL_MULT_SHIFT			8
#define OMAP3430_MPU_DPLL_MULT_MASK			(0x7ff << 8)
#define OMAP3430_MPU_DPLL_DIV_SHIFT			0
#define OMAP3430_MPU_DPLL_DIV_MASK			(0x7f << 0)

/* CM_CLKSEL2_PLL_MPU */
#define OMAP3430_MPU_DPLL_CLKOUT_DIV_SHIFT		0
#define OMAP3430_MPU_DPLL_CLKOUT_DIV_MASK		(0x1f << 0)

/* CM_CLKSTCTRL_MPU */
#define OMAP3430_CLKTRCTRL_MPU_SHIFT			0
#define OMAP3430_CLKTRCTRL_MPU_MASK			(0x3 << 0)

/* CM_CLKSTST_MPU */
#define OMAP3430_CLKACTIVITY_MPU_SHIFT			0
#define OMAP3430_CLKACTIVITY_MPU_MASK			(1 << 0)

/* CM_FCLKEN1_CORE specific bits */

/* CM_ICLKEN1_CORE specific bits */
#define OMAP3430_EN_ICR					(1 << 29)
#define OMAP3430_EN_ICR_SHIFT				29
#define OMAP3430_EN_AES2				(1 << 28)
#define OMAP3430_EN_AES2_SHIFT				28
#define OMAP3430_EN_SHA12				(1 << 27)
#define OMAP3430_EN_SHA12_SHIFT				27
#define OMAP3430_EN_DES2				(1 << 26)
#define OMAP3430_EN_DES2_SHIFT				26
#define OMAP3430ES1_EN_FAC				(1 << 8)
#define OMAP3430ES1_EN_FAC_SHIFT			8
#define OMAP3430_EN_MAILBOXES				(1 << 7)
#define OMAP3430_EN_MAILBOXES_SHIFT			7
#define OMAP3430_EN_OMAPCTRL				(1 << 6)
#define OMAP3430_EN_OMAPCTRL_SHIFT			6
#define OMAP3430_EN_SDRC				(1 << 1)
#define OMAP3430_EN_SDRC_SHIFT				1

/* CM_ICLKEN2_CORE */
#define OMAP3430_EN_PKA					(1 << 4)
#define OMAP3430_EN_PKA_SHIFT				4
#define OMAP3430_EN_AES1				(1 << 3)
#define OMAP3430_EN_AES1_SHIFT				3
#define OMAP3430_EN_RNG					(1 << 2)
#define OMAP3430_EN_RNG_SHIFT				2
#define OMAP3430_EN_SHA11				(1 << 1)
#define OMAP3430_EN_SHA11_SHIFT				1
#define OMAP3430_EN_DES1				(1 << 0)
#define OMAP3430_EN_DES1_SHIFT				0

/* CM_FCLKEN3_CORE specific bits */
#define OMAP3430ES2_EN_TS_SHIFT				1
#define OMAP3430ES2_EN_TS_MASK				(1 << 1)
#define OMAP3430ES2_EN_CPEFUSE_SHIFT			0
#define OMAP3430ES2_EN_CPEFUSE_MASK			(1 << 0)

/* CM_IDLEST1_CORE specific bits */
#define OMAP3430_ST_ICR					(1 << 29)
#define OMAP3430_ST_AES2				(1 << 28)
#define OMAP3430_ST_SHA12				(1 << 27)
#define OMAP3430_ST_DES2				(1 << 26)
#define OMAP3430_ST_MSPRO				(1 << 23)
#define OMAP3430_ST_HDQ					(1 << 22)
#define OMAP3430ES1_ST_FAC				(1 << 8)
#define OMAP3430ES1_ST_MAILBOXES			(1 << 7)
#define OMAP3430_ST_OMAPCTRL				(1 << 6)
#define OMAP3430_ST_SDMA				(1 << 2)
#define OMAP3430_ST_SDRC				(1 << 1)
#define OMAP3430_ST_SSI					(1 << 0)

/* CM_IDLEST2_CORE */
#define OMAP3430_ST_PKA					(1 << 4)
#define OMAP3430_ST_AES1				(1 << 3)
#define OMAP3430_ST_RNG					(1 << 2)
#define OMAP3430_ST_SHA11				(1 << 1)
#define OMAP3430_ST_DES1				(1 << 0)

/* CM_IDLEST3_CORE */
#define OMAP3430ES2_ST_USBTLL_SHIFT			2
#define OMAP3430ES2_ST_USBTLL_MASK			(1 << 2)

/* CM_AUTOIDLE1_CORE */
#define OMAP3430ES2_AUTO_MMC3				(1 << 30)
#define OMAP3430ES2_AUTO_MMC3_SHIFT			30
#define OMAP3430ES2_AUTO_ICR				(1 << 29)
#define OMAP3430ES2_AUTO_ICR_SHIFT			29
#define OMAP3430_AUTO_AES2				(1 << 28)
#define OMAP3430_AUTO_AES2_SHIFT			28
#define OMAP3430_AUTO_SHA12				(1 << 27)
#define OMAP3430_AUTO_SHA12_SHIFT			27
#define OMAP3430_AUTO_DES2				(1 << 26)
#define OMAP3430_AUTO_DES2_SHIFT			26
#define OMAP3430_AUTO_MMC2				(1 << 25)
#define OMAP3430_AUTO_MMC2_SHIFT			25
#define OMAP3430_AUTO_MMC1				(1 << 24)
#define OMAP3430_AUTO_MMC1_SHIFT			24
#define OMAP3430_AUTO_MSPRO				(1 << 23)
#define OMAP3430_AUTO_MSPRO_SHIFT			23
#define OMAP3430_AUTO_HDQ				(1 << 22)
#define OMAP3430_AUTO_HDQ_SHIFT				22
#define OMAP3430_AUTO_MCSPI4				(1 << 21)
#define OMAP3430_AUTO_MCSPI4_SHIFT			21
#define OMAP3430_AUTO_MCSPI3				(1 << 20)
#define OMAP3430_AUTO_MCSPI3_SHIFT			20
#define OMAP3430_AUTO_MCSPI2				(1 << 19)
#define OMAP3430_AUTO_MCSPI2_SHIFT			19
#define OMAP3430_AUTO_MCSPI1				(1 << 18)
#define OMAP3430_AUTO_MCSPI1_SHIFT			18
#define OMAP3430_AUTO_I2C3				(1 << 17)
#define OMAP3430_AUTO_I2C3_SHIFT			17
#define OMAP3430_AUTO_I2C2				(1 << 16)
#define OMAP3430_AUTO_I2C2_SHIFT			16
#define OMAP3430_AUTO_I2C1				(1 << 15)
#define OMAP3430_AUTO_I2C1_SHIFT			15
#define OMAP3430_AUTO_UART2				(1 << 14)
#define OMAP3430_AUTO_UART2_SHIFT			14
#define OMAP3430_AUTO_UART1				(1 << 13)
#define OMAP3430_AUTO_UART1_SHIFT			13
#define OMAP3430_AUTO_GPT11				(1 << 12)
#define OMAP3430_AUTO_GPT11_SHIFT			12
#define OMAP3430_AUTO_GPT10				(1 << 11)
#define OMAP3430_AUTO_GPT10_SHIFT			11
#define OMAP3430_AUTO_MCBSP5				(1 << 10)
#define OMAP3430_AUTO_MCBSP5_SHIFT			10
#define OMAP3430_AUTO_MCBSP1				(1 << 9)
#define OMAP3430_AUTO_MCBSP1_SHIFT			9
#define OMAP3430ES1_AUTO_FAC				(1 << 8)
#define OMAP3430ES1_AUTO_FAC_SHIFT			8
#define OMAP3430_AUTO_MAILBOXES				(1 << 7)
#define OMAP3430_AUTO_MAILBOXES_SHIFT			7
#define OMAP3430_AUTO_OMAPCTRL				(1 << 6)
#define OMAP3430_AUTO_OMAPCTRL_SHIFT			6
#define OMAP3430ES1_AUTO_FSHOSTUSB			(1 << 5)
#define OMAP3430ES1_AUTO_FSHOSTUSB_SHIFT		5
#define OMAP3430_AUTO_HSOTGUSB				(1 << 4)
#define OMAP3430_AUTO_HSOTGUSB_SHIFT			4
#define OMAP3430ES1_AUTO_D2D				(1 << 3)
#define OMAP3430ES1_AUTO_D2D_SHIFT			3
#define OMAP3430_AUTO_SSI				(1 << 0)
#define OMAP3430_AUTO_SSI_SHIFT				0

/* CM_AUTOIDLE2_CORE */
#define OMAP3430_AUTO_PKA				(1 << 4)
#define OMAP3430_AUTO_PKA_SHIFT				4
#define OMAP3430_AUTO_AES1				(1 << 3)
#define OMAP3430_AUTO_AES1_SHIFT			3
#define OMAP3430_AUTO_RNG				(1 << 2)
#define OMAP3430_AUTO_RNG_SHIFT				2
#define OMAP3430_AUTO_SHA11				(1 << 1)
#define OMAP3430_AUTO_SHA11_SHIFT			1
#define OMAP3430_AUTO_DES1				(1 << 0)
#define OMAP3430_AUTO_DES1_SHIFT			0

/* CM_AUTOIDLE3_CORE */
#define	OMAP3430ES2_AUTO_USBHOST			(1 << 0)
#define	OMAP3430ES2_AUTO_USBHOST_SHIFT			0
#define	OMAP3430ES2_AUTO_USBTLL				(1 << 2)
#define OMAP3430ES2_AUTO_USBTLL_SHIFT			2
#define OMAP3430ES2_AUTO_USBTLL_MASK			(1 << 2)

/* CM_CLKSEL_CORE */
#define OMAP3430_CLKSEL_SSI_SHIFT			8
#define OMAP3430_CLKSEL_SSI_MASK			(0xf << 8)
#define OMAP3430_CLKSEL_GPT11_MASK			(1 << 7)
#define OMAP3430_CLKSEL_GPT11_SHIFT			7
#define OMAP3430_CLKSEL_GPT10_MASK			(1 << 6)
#define OMAP3430_CLKSEL_GPT10_SHIFT			6
#define OMAP3430ES1_CLKSEL_FSHOSTUSB_SHIFT		4
#define OMAP3430ES1_CLKSEL_FSHOSTUSB_MASK		(0x3 << 4)
#define OMAP3430_CLKSEL_L4_SHIFT			2
#define OMAP3430_CLKSEL_L4_MASK				(0x3 << 2)
#define OMAP3430_CLKSEL_L3_SHIFT			0
#define OMAP3430_CLKSEL_L3_MASK				(0x3 << 0)

/* CM_CLKSTCTRL_CORE */
#define OMAP3430ES1_CLKTRCTRL_D2D_SHIFT			4
#define OMAP3430ES1_CLKTRCTRL_D2D_MASK			(0x3 << 4)
#define OMAP3430_CLKTRCTRL_L4_SHIFT			2
#define OMAP3430_CLKTRCTRL_L4_MASK			(0x3 << 2)
#define OMAP3430_CLKTRCTRL_L3_SHIFT			0
#define OMAP3430_CLKTRCTRL_L3_MASK			(0x3 << 0)

/* CM_CLKSTST_CORE */
#define OMAP3430ES1_CLKACTIVITY_D2D_SHIFT		2
#define OMAP3430ES1_CLKACTIVITY_D2D_MASK		(1 << 2)
#define OMAP3430_CLKACTIVITY_L4_SHIFT			1
#define OMAP3430_CLKACTIVITY_L4_MASK			(1 << 1)
#define OMAP3430_CLKACTIVITY_L3_SHIFT			0
#define OMAP3430_CLKACTIVITY_L3_MASK			(1 << 0)

/* CM_FCLKEN_GFX */
#define OMAP3430ES1_EN_3D				(1 << 2)
#define OMAP3430ES1_EN_3D_SHIFT				2
#define OMAP3430ES1_EN_2D				(1 << 1)
#define OMAP3430ES1_EN_2D_SHIFT				1

/* CM_ICLKEN_GFX specific bits */

/* CM_IDLEST_GFX specific bits */

/* CM_CLKSEL_GFX specific bits */

/* CM_SLEEPDEP_GFX specific bits */

/* CM_CLKSTCTRL_GFX */
#define OMAP3430ES1_CLKTRCTRL_GFX_SHIFT			0
#define OMAP3430ES1_CLKTRCTRL_GFX_MASK			(0x3 << 0)

/* CM_CLKSTST_GFX */
#define OMAP3430ES1_CLKACTIVITY_GFX_SHIFT		0
#define OMAP3430ES1_CLKACTIVITY_GFX_MASK		(1 << 0)

/* CM_FCLKEN_SGX */
#define OMAP3430ES2_CM_FCLKEN_SGX_EN_SGX_SHIFT		1
#define OMAP3430ES2_CM_FCLKEN_SGX_EN_SGX_MASK		(1 << 1)

/* CM_ICLKEN_SGX */
#define OMAP3430ES2_CM_ICLKEN_SGX_EN_SGX_SHIFT		0
#define OMAP3430ES2_CM_ICLKEN_SGX_EN_SGX_MASK		(1 << 0)

/* CM_CLKSEL_SGX */
#define OMAP3430ES2_CLKSEL_SGX_SHIFT			0
#define OMAP3430ES2_CLKSEL_SGX_MASK			(0x7 << 0)

/* CM_CLKSTCTRL_SGX */
#define OMAP3430ES2_CLKTRCTRL_SGX_SHIFT			0
#define OMAP3430ES2_CLKTRCTRL_SGX_MASK			(0x3 << 0)

/* CM_CLKSTST_SGX */
#define OMAP3430ES2_CLKACTIVITY_SGX_SHIFT		0
#define OMAP3430ES2_CLKACTIVITY_SGX_MASK		(1 << 0)

/* CM_FCLKEN_WKUP specific bits */
#define OMAP3430ES2_EN_USIMOCP_SHIFT			9

/* CM_ICLKEN_WKUP specific bits */
#define OMAP3430_EN_WDT1				(1 << 4)
#define OMAP3430_EN_WDT1_SHIFT				4
#define OMAP3430_EN_32KSYNC				(1 << 2)
#define OMAP3430_EN_32KSYNC_SHIFT			2

/* CM_IDLEST_WKUP specific bits */
#define OMAP3430_ST_WDT2				(1 << 5)
#define OMAP3430_ST_WDT1				(1 << 4)
#define OMAP3430_ST_32KSYNC				(1 << 2)

/* CM_AUTOIDLE_WKUP */
#define OMAP3430_AUTO_WDT2				(1 << 5)
#define OMAP3430_AUTO_WDT2_SHIFT			5
#define OMAP3430_AUTO_WDT1				(1 << 4)
#define OMAP3430_AUTO_WDT1_SHIFT			4
#define OMAP3430_AUTO_GPIO1				(1 << 3)
#define OMAP3430_AUTO_GPIO1_SHIFT			3
#define OMAP3430_AUTO_32KSYNC				(1 << 2)
#define OMAP3430_AUTO_32KSYNC_SHIFT			2
#define OMAP3430_AUTO_GPT12				(1 << 1)
#define OMAP3430_AUTO_GPT12_SHIFT			1
#define OMAP3430_AUTO_GPT1				(1 << 0)
#define OMAP3430_AUTO_GPT1_SHIFT			0

/* CM_CLKSEL_WKUP */
#define OMAP3430ES2_CLKSEL_USIMOCP_MASK			(0xf << 3)
#define OMAP3430_CLKSEL_RM_SHIFT			1
#define OMAP3430_CLKSEL_RM_MASK				(0x3 << 1)
#define OMAP3430_CLKSEL_GPT1_SHIFT			0
#define OMAP3430_CLKSEL_GPT1_MASK			(1 << 0)

/* CM_CLKEN_PLL */
#define OMAP3430_PWRDN_EMU_PERIPH_SHIFT			31
#define OMAP3430_PWRDN_CAM_SHIFT			30
#define OMAP3430_PWRDN_DSS1_SHIFT			29
#define OMAP3430_PWRDN_TV_SHIFT				28
#define OMAP3430_PWRDN_96M_SHIFT			27
#define OMAP3430_PERIPH_DPLL_RAMPTIME_SHIFT		24
#define OMAP3430_PERIPH_DPLL_RAMPTIME_MASK		(0x3 << 24)
#define OMAP3430_PERIPH_DPLL_FREQSEL_SHIFT		20
#define OMAP3430_PERIPH_DPLL_FREQSEL_MASK		(0xf << 20)
#define OMAP3430_EN_PERIPH_DPLL_DRIFTGUARD_SHIFT	19
#define OMAP3430_EN_PERIPH_DPLL_DRIFTGUARD_MASK		(1 << 19)
#define OMAP3430_EN_PERIPH_DPLL_SHIFT			16
#define OMAP3430_EN_PERIPH_DPLL_MASK			(0x7 << 16)
#define OMAP3430_PWRDN_EMU_CORE_SHIFT			12
#define OMAP3430_CORE_DPLL_RAMPTIME_SHIFT		8
#define OMAP3430_CORE_DPLL_RAMPTIME_MASK		(0x3 << 8)
#define OMAP3430_CORE_DPLL_FREQSEL_SHIFT		4
#define OMAP3430_CORE_DPLL_FREQSEL_MASK			(0xf << 4)
#define OMAP3430_EN_CORE_DPLL_DRIFTGUARD_SHIFT		3
#define OMAP3430_EN_CORE_DPLL_DRIFTGUARD_MASK		(1 << 3)
#define OMAP3430_EN_CORE_DPLL_SHIFT			0
#define OMAP3430_EN_CORE_DPLL_MASK			(0x7 << 0)

/* CM_CLKEN2_PLL */
#define OMAP3430ES2_EN_PERIPH2_DPLL_LPMODE_SHIFT		10
#define OMAP3430ES2_PERIPH2_DPLL_RAMPTIME_MASK		(0x3 << 8)
#define OMAP3430ES2_PERIPH2_DPLL_FREQSEL_SHIFT		4
#define OMAP3430ES2_PERIPH2_DPLL_FREQSEL_MASK		(0xf << 4)
#define OMAP3430ES2_EN_PERIPH2_DPLL_DRIFTGUARD_SHIFT	3
#define OMAP3430ES2_EN_PERIPH2_DPLL_SHIFT		0
#define OMAP3430ES2_EN_PERIPH2_DPLL_MASK		(0x7 << 0)

/* CM_IDLEST_CKGEN */
#define OMAP3430_ST_54M_CLK				(1 << 5)
#define OMAP3430_ST_12M_CLK				(1 << 4)
#define OMAP3430_ST_48M_CLK				(1 << 3)
#define OMAP3430_ST_96M_CLK				(1 << 2)
#define OMAP3430_ST_PERIPH_CLK_SHIFT			1
#define OMAP3430_ST_PERIPH_CLK_MASK			(1 << 1)
#define OMAP3430_ST_CORE_CLK_SHIFT			0
#define OMAP3430_ST_CORE_CLK_MASK			(1 << 0)

/* CM_IDLEST2_CKGEN */
#define OMAP3430ES2_ST_120M_CLK_SHIFT			1
#define OMAP3430ES2_ST_120M_CLK_MASK			(1 << 1)
#define OMAP3430ES2_ST_PERIPH2_CLK_SHIFT		0
#define OMAP3430ES2_ST_PERIPH2_CLK_MASK			(1 << 0)

/* CM_AUTOIDLE_PLL */
#define OMAP3430_AUTO_PERIPH_DPLL_SHIFT			3
#define OMAP3430_AUTO_PERIPH_DPLL_MASK			(0x7 << 3)
#define OMAP3430_AUTO_CORE_DPLL_SHIFT			0
#define OMAP3430_AUTO_CORE_DPLL_MASK			(0x7 << 0)

/* CM_AUTOIDLE2_PLL */
#define OMAP3430ES2_AUTO_PERIPH2_DPLL_SHIFT		0
#define OMAP3430ES2_AUTO_PERIPH2_DPLL_MASK		(0x7 << 0)

/* CM_CLKSEL1_PLL */
/* Note that OMAP3430_CORE_DPLL_CLKOUT_DIV_MASK was (0x3 << 27) on 3430ES1 */
#define OMAP3430_CORE_DPLL_CLKOUT_DIV_SHIFT		27
#define OMAP3430_CORE_DPLL_CLKOUT_DIV_MASK		(0x1f << 27)
#define OMAP3430_CORE_DPLL_MULT_SHIFT			16
#define OMAP3430_CORE_DPLL_MULT_MASK			(0x7ff << 16)
#define OMAP3430_CORE_DPLL_DIV_SHIFT			8
#define OMAP3430_CORE_DPLL_DIV_MASK			(0x7f << 8)
#define OMAP3430_SOURCE_96M_SHIFT			6
#define OMAP3430_SOURCE_96M_MASK			(1 << 6)
#define OMAP3430_SOURCE_54M_SHIFT			5
#define OMAP3430_SOURCE_54M_MASK			(1 << 5)
#define OMAP3430_SOURCE_48M_SHIFT			3
#define OMAP3430_SOURCE_48M_MASK			(1 << 3)

/* CM_CLKSEL2_PLL */
#define OMAP3430_PERIPH_DPLL_MULT_SHIFT			8
#define OMAP3430_PERIPH_DPLL_MULT_MASK			(0x7ff << 8)
#define OMAP3430_PERIPH_DPLL_DIV_SHIFT			0
#define OMAP3430_PERIPH_DPLL_DIV_MASK			(0x7f << 0)

/* CM_CLKSEL3_PLL */
#define OMAP3430_DIV_96M_SHIFT				0
#define OMAP3430_DIV_96M_MASK				(0x1f << 0)

/* CM_CLKSEL4_PLL */
#define OMAP3430ES2_PERIPH2_DPLL_MULT_SHIFT		8
#define OMAP3430ES2_PERIPH2_DPLL_MULT_MASK		(0x7ff << 8)
#define OMAP3430ES2_PERIPH2_DPLL_DIV_SHIFT		0
#define OMAP3430ES2_PERIPH2_DPLL_DIV_MASK		(0x7f << 0)

/* CM_CLKSEL5_PLL */
#define OMAP3430ES2_DIV_120M_SHIFT			0
#define OMAP3430ES2_DIV_120M_MASK			(0x1f << 0)

/* CM_CLKOUT_CTRL */
#define OMAP3430_CLKOUT2_EN_SHIFT			7
#define OMAP3430_CLKOUT2_EN				(1 << 7)
#define OMAP3430_CLKOUT2_DIV_SHIFT			3
#define OMAP3430_CLKOUT2_DIV_MASK			(0x7 << 3)
#define OMAP3430_CLKOUT2SOURCE_SHIFT			0
#define OMAP3430_CLKOUT2SOURCE_MASK			(0x3 << 0)

/* CM_FCLKEN_DSS */
#define OMAP3430_EN_TV					(1 << 2)
#define OMAP3430_EN_TV_SHIFT				2
#define OMAP3430_EN_DSS2				(1 << 1)
#define OMAP3430_EN_DSS2_SHIFT				1
#define OMAP3430_EN_DSS1				(1 << 0)
#define OMAP3430_EN_DSS1_SHIFT				0

/* CM_ICLKEN_DSS */
#define OMAP3430_CM_ICLKEN_DSS_EN_DSS			(1 << 0)
#define OMAP3430_CM_ICLKEN_DSS_EN_DSS_SHIFT		0

/* CM_IDLEST_DSS */
#define OMAP3430_ST_DSS					(1 << 0)

/* CM_AUTOIDLE_DSS */
#define OMAP3430_AUTO_DSS				(1 << 0)
#define OMAP3430_AUTO_DSS_SHIFT				0

/* CM_CLKSEL_DSS */
#define OMAP3430_CLKSEL_TV_SHIFT			8
#define OMAP3430_CLKSEL_TV_MASK				(0x1f << 8)
#define OMAP3430_CLKSEL_DSS1_SHIFT			0
#define OMAP3430_CLKSEL_DSS1_MASK			(0x1f << 0)

/* CM_SLEEPDEP_DSS specific bits */

/* CM_CLKSTCTRL_DSS */
#define OMAP3430_CLKTRCTRL_DSS_SHIFT			0
#define OMAP3430_CLKTRCTRL_DSS_MASK			(0x3 << 0)

/* CM_CLKSTST_DSS */
#define OMAP3430_CLKACTIVITY_DSS_SHIFT			0
#define OMAP3430_CLKACTIVITY_DSS_MASK			(1 << 0)

/* CM_FCLKEN_CAM specific bits */
#define OMAP3430_EN_CSI2				(1 << 1)
#define OMAP3430_EN_CSI2_SHIFT				1

/* CM_ICLKEN_CAM specific bits */

/* CM_IDLEST_CAM */
#define OMAP3430_ST_CAM					(1 << 0)

/* CM_AUTOIDLE_CAM */
#define OMAP3430_AUTO_CAM				(1 << 0)
#define OMAP3430_AUTO_CAM_SHIFT				0

/* CM_CLKSEL_CAM */
#define OMAP3430_CLKSEL_CAM_SHIFT			0
#define OMAP3430_CLKSEL_CAM_MASK			(0x1f << 0)

/* CM_SLEEPDEP_CAM specific bits */

/* CM_CLKSTCTRL_CAM */
#define OMAP3430_CLKTRCTRL_CAM_SHIFT			0
#define OMAP3430_CLKTRCTRL_CAM_MASK			(0x3 << 0)

/* CM_CLKSTST_CAM */
#define OMAP3430_CLKACTIVITY_CAM_SHIFT			0
#define OMAP3430_CLKACTIVITY_CAM_MASK			(1 << 0)

/* CM_FCLKEN_PER specific bits */

/* CM_ICLKEN_PER specific bits */

/* CM_IDLEST_PER */
#define OMAP3430_ST_WDT3				(1 << 12)
#define OMAP3430_ST_MCBSP4				(1 << 2)
#define OMAP3430_ST_MCBSP3				(1 << 1)
#define OMAP3430_ST_MCBSP2				(1 << 0)

/* CM_AUTOIDLE_PER */
#define OMAP3430_AUTO_GPIO6				(1 << 17)
#define OMAP3430_AUTO_GPIO6_SHIFT			17
#define OMAP3430_AUTO_GPIO5				(1 << 16)
#define OMAP3430_AUTO_GPIO5_SHIFT			16
#define OMAP3430_AUTO_GPIO4				(1 << 15)
#define OMAP3430_AUTO_GPIO4_SHIFT			15
#define OMAP3430_AUTO_GPIO3				(1 << 14)
#define OMAP3430_AUTO_GPIO3_SHIFT			14
#define OMAP3430_AUTO_GPIO2				(1 << 13)
#define OMAP3430_AUTO_GPIO2_SHIFT			13
#define OMAP3430_AUTO_WDT3				(1 << 12)
#define OMAP3430_AUTO_WDT3_SHIFT			12
#define OMAP3430_AUTO_UART3				(1 << 11)
#define OMAP3430_AUTO_UART3_SHIFT			11
#define OMAP3430_AUTO_GPT9				(1 << 10)
#define OMAP3430_AUTO_GPT9_SHIFT			10
#define OMAP3430_AUTO_GPT8				(1 << 9)
#define OMAP3430_AUTO_GPT8_SHIFT			9
#define OMAP3430_AUTO_GPT7				(1 << 8)
#define OMAP3430_AUTO_GPT7_SHIFT			8
#define OMAP3430_AUTO_GPT6				(1 << 7)
#define OMAP3430_AUTO_GPT6_SHIFT			7
#define OMAP3430_AUTO_GPT5				(1 << 6)
#define OMAP3430_AUTO_GPT5_SHIFT			6
#define OMAP3430_AUTO_GPT4				(1 << 5)
#define OMAP3430_AUTO_GPT4_SHIFT			5
#define OMAP3430_AUTO_GPT3				(1 << 4)
#define OMAP3430_AUTO_GPT3_SHIFT			4
#define OMAP3430_AUTO_GPT2				(1 << 3)
#define OMAP3430_AUTO_GPT2_SHIFT			3
#define OMAP3430_AUTO_MCBSP4				(1 << 2)
#define OMAP3430_AUTO_MCBSP4_SHIFT			2
#define OMAP3430_AUTO_MCBSP3				(1 << 1)
#define OMAP3430_AUTO_MCBSP3_SHIFT			1
#define OMAP3430_AUTO_MCBSP2				(1 << 0)
#define OMAP3430_AUTO_MCBSP2_SHIFT			0

/* CM_CLKSEL_PER */
#define OMAP3430_CLKSEL_GPT9_MASK			(1 << 7)
#define OMAP3430_CLKSEL_GPT9_SHIFT			7
#define OMAP3430_CLKSEL_GPT8_MASK			(1 << 6)
#define OMAP3430_CLKSEL_GPT8_SHIFT			6
#define OMAP3430_CLKSEL_GPT7_MASK			(1 << 5)
#define OMAP3430_CLKSEL_GPT7_SHIFT			5
#define OMAP3430_CLKSEL_GPT6_MASK			(1 << 4)
#define OMAP3430_CLKSEL_GPT6_SHIFT			4
#define OMAP3430_CLKSEL_GPT5_MASK			(1 << 3)
#define OMAP3430_CLKSEL_GPT5_SHIFT			3
#define OMAP3430_CLKSEL_GPT4_MASK			(1 << 2)
#define OMAP3430_CLKSEL_GPT4_SHIFT			2
#define OMAP3430_CLKSEL_GPT3_MASK			(1 << 1)
#define OMAP3430_CLKSEL_GPT3_SHIFT			1
#define OMAP3430_CLKSEL_GPT2_MASK			(1 << 0)
#define OMAP3430_CLKSEL_GPT2_SHIFT			0

/* CM_SLEEPDEP_PER specific bits */
#define OMAP3430_CM_SLEEPDEP_PER_EN_IVA2		(1 << 2)

/* CM_CLKSTCTRL_PER */
#define OMAP3430_CLKTRCTRL_PER_SHIFT			0
#define OMAP3430_CLKTRCTRL_PER_MASK			(0x3 << 0)

/* CM_CLKSTST_PER */
#define OMAP3430_CLKACTIVITY_PER_SHIFT			0
#define OMAP3430_CLKACTIVITY_PER_MASK			(1 << 0)

/* CM_CLKSEL1_EMU */
#define OMAP3430_DIV_DPLL4_SHIFT			24
#define OMAP3430_DIV_DPLL4_MASK				(0x1f << 24)
#define OMAP3430_DIV_DPLL3_SHIFT			16
#define OMAP3430_DIV_DPLL3_MASK				(0x1f << 16)
#define OMAP3430_CLKSEL_TRACECLK_SHIFT			11
#define OMAP3430_CLKSEL_TRACECLK_MASK			(0x7 << 11)
#define OMAP3430_CLKSEL_PCLK_SHIFT			8
#define OMAP3430_CLKSEL_PCLK_MASK			(0x7 << 8)
#define OMAP3430_CLKSEL_PCLKX2_SHIFT			6
#define OMAP3430_CLKSEL_PCLKX2_MASK			(0x3 << 6)
#define OMAP3430_CLKSEL_ATCLK_SHIFT			4
#define OMAP3430_CLKSEL_ATCLK_MASK			(0x3 << 4)
#define OMAP3430_TRACE_MUX_CTRL_SHIFT			2
#define OMAP3430_TRACE_MUX_CTRL_MASK			(0x3 << 2)
#define OMAP3430_MUX_CTRL_SHIFT				0
#define OMAP3430_MUX_CTRL_MASK				(0x3 << 0)

/* CM_CLKSTCTRL_EMU */
#define OMAP3430_CLKTRCTRL_EMU_SHIFT			0
#define OMAP3430_CLKTRCTRL_EMU_MASK			(0x3 << 0)

/* CM_CLKSTST_EMU */
#define OMAP3430_CLKACTIVITY_EMU_SHIFT			0
#define OMAP3430_CLKACTIVITY_EMU_MASK			(1 << 0)

/* CM_CLKSEL2_EMU specific bits */
#define OMAP3430_CORE_DPLL_EMU_MULT_SHIFT		8
#define OMAP3430_CORE_DPLL_EMU_MULT_MASK		(0x7ff << 8)
#define OMAP3430_CORE_DPLL_EMU_DIV_SHIFT		0
#define OMAP3430_CORE_DPLL_EMU_DIV_MASK			(0x7f << 0)

/* CM_CLKSEL3_EMU specific bits */
#define OMAP3430_PERIPH_DPLL_EMU_MULT_SHIFT		8
#define OMAP3430_PERIPH_DPLL_EMU_MULT_MASK		(0x7ff << 8)
#define OMAP3430_PERIPH_DPLL_EMU_DIV_SHIFT		0
#define OMAP3430_PERIPH_DPLL_EMU_DIV_MASK		(0x7f << 0)

/* CM_POLCTRL */
#define OMAP3430_CLKOUT2_POL				(1 << 0)

/* CM_IDLEST_NEON */
#define OMAP3430_ST_NEON				(1 << 0)

/* CM_CLKSTCTRL_NEON */
#define OMAP3430_CLKTRCTRL_NEON_SHIFT			0
#define OMAP3430_CLKTRCTRL_NEON_MASK			(0x3 << 0)

/* CM_FCLKEN_USBHOST */
#define OMAP3430ES2_EN_USBHOST2_SHIFT			1
#define OMAP3430ES2_EN_USBHOST2_MASK			(1 << 1)
#define OMAP3430ES2_EN_USBHOST1_SHIFT			0
#define OMAP3430ES2_EN_USBHOST1_MASK			(1 << 0)

/* CM_ICLKEN_USBHOST */
#define OMAP3430ES2_EN_USBHOST_SHIFT			0
#define OMAP3430ES2_EN_USBHOST_MASK			(1 << 0)

/* CM_IDLEST_USBHOST */

/* CM_AUTOIDLE_USBHOST */
#define OMAP3430ES2_AUTO_USBHOST_SHIFT			0
#define OMAP3430ES2_AUTO_USBHOST_MASK			(1 << 0)

/* CM_SLEEPDEP_USBHOST */
#define OMAP3430ES2_EN_MPU_SHIFT			1
#define OMAP3430ES2_EN_MPU_MASK				(1 << 1)
#define OMAP3430ES2_EN_IVA2_SHIFT			2
#define OMAP3430ES2_EN_IVA2_MASK			(1 << 2)

/* CM_CLKSTCTRL_USBHOST */
#define OMAP3430ES2_CLKTRCTRL_USBHOST_SHIFT		0
#define OMAP3430ES2_CLKTRCTRL_USBHOST_MASK		(3 << 0)

/* CM_CLKSTST_USBHOST */
#define OMAP3430ES2_CLKACTIVITY_USBHOST_SHIFT		0
#define OMAP3430ES2_CLKACTIVITY_USBHOST_MASK		(1 << 0)

#endif
