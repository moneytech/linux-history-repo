/*
 * arch/arm/mach-pxa/include/mach/mfp-pxa320.h
 *
 * PXA320 specific MFP configuration definitions
 *
 * Copyright (C) 2007 Marvell International Ltd.
 * 2007-08-21: eric miao <eric.miao@marvell.com>
 *             initial version
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_MFP_PXA320_H
#define __ASM_ARCH_MFP_PXA320_H

#include <mach/mfp-pxa3xx.h>

/* GPIO */
#define GPIO46_GPIO		MFP_CFG(GPIO46, AF0)
#define GPIO49_GPIO		MFP_CFG(GPIO49, AF0)
#define GPIO50_GPIO		MFP_CFG(GPIO50, AF0)
#define GPIO51_GPIO		MFP_CFG(GPIO51, AF0)
#define GPIO52_GPIO		MFP_CFG(GPIO52, AF0)

#define GPIO7_2_GPIO		MFP_CFG(GPIO7_2, AF0)
#define GPIO8_2_GPIO		MFP_CFG(GPIO8_2, AF0)
#define GPIO9_2_GPIO		MFP_CFG(GPIO9_2, AF0)
#define GPIO10_2_GPIO		MFP_CFG(GPIO10_2, AF0)
#define GPIO11_2_GPIO		MFP_CFG(GPIO11_2, AF0)
#define GPIO12_2_GPIO		MFP_CFG(GPIO12_2, AF0)
#define GPIO13_2_GPIO		MFP_CFG(GPIO13_2, AF0)
#define GPIO14_2_GPIO		MFP_CFG(GPIO14_2, AF0)
#define GPIO15_2_GPIO		MFP_CFG(GPIO15_2, AF0)
#define GPIO16_2_GPIO		MFP_CFG(GPIO16_2, AF0)
#define GPIO17_2_GPIO		MFP_CFG(GPIO17_2, AF0)

/* Chip Select */
#define GPIO3_nCS2		MFP_CFG(GPIO3, AF1)
#define GPIO4_nCS3		MFP_CFG(GPIO4, AF1)

/* AC97 */
#define GPIO34_AC97_SYSCLK	MFP_CFG(GPIO34, AF1)
#define GPIO39_AC97_BITCLK	MFP_CFG(GPIO39, AF1)
#define GPIO40_AC97_nACRESET	MFP_CFG(GPIO40, AF1)
#define GPIO35_AC97_SDATA_IN_0	MFP_CFG(GPIO35, AF1)
#define GPIO36_AC97_SDATA_IN_1	MFP_CFG(GPIO36, AF1)
#define GPIO32_AC97_SDATA_IN_2	MFP_CFG(GPIO32, AF2)
#define GPIO33_AC97_SDATA_IN_3	MFP_CFG(GPIO33, AF2)
#define GPIO11_AC97_SDATA_IN_2	MFP_CFG(GPIO11, AF3)
#define GPIO12_AC97_SDATA_IN_3	MFP_CFG(GPIO12, AF3)
#define GPIO37_AC97_SDATA_OUT	MFP_CFG(GPIO37, AF1)
#define GPIO38_AC97_SYNC	MFP_CFG(GPIO38, AF1)

/* I2C */
#define GPIO32_I2C_SCL		MFP_CFG_LPM(GPIO32, AF1, PULL_HIGH)
#define GPIO33_I2C_SDA		MFP_CFG_LPM(GPIO33, AF1, PULL_HIGH)

/* QCI */
#define GPIO49_CI_DD_0		MFP_CFG_DRV(GPIO49, AF1, DS04X)
#define GPIO50_CI_DD_1		MFP_CFG_DRV(GPIO50, AF1, DS04X)
#define GPIO51_CI_DD_2		MFP_CFG_DRV(GPIO51, AF1, DS04X)
#define GPIO52_CI_DD_3		MFP_CFG_DRV(GPIO52, AF1, DS04X)
#define GPIO53_CI_DD_4		MFP_CFG_DRV(GPIO53, AF1, DS04X)
#define GPIO54_CI_DD_5		MFP_CFG_DRV(GPIO54, AF1, DS04X)
#define GPIO55_CI_DD_6		MFP_CFG_DRV(GPIO55, AF1, DS04X)
#define GPIO56_CI_DD_7		MFP_CFG_DRV(GPIO56, AF0, DS04X)
#define GPIO57_CI_DD_8		MFP_CFG_DRV(GPIO57, AF1, DS04X)
#define GPIO58_CI_DD_9		MFP_CFG_DRV(GPIO58, AF1, DS04X)
#define GPIO59_CI_MCLK		MFP_CFG_DRV(GPIO59, AF0, DS04X)
#define GPIO60_CI_PCLK		MFP_CFG_DRV(GPIO60, AF0, DS04X)
#define GPIO61_CI_HSYNC		MFP_CFG_DRV(GPIO61, AF0, DS04X)
#define GPIO62_CI_VSYNC		MFP_CFG_DRV(GPIO62, AF0, DS04X)

#define GPIO31_CIR_OUT		MFP_CFG(GPIO31, AF5)

#define GPIO0_2_CLK_EXT		MFP_CFG(GPIO0_2, AF3)
#define GPIO0_DRQ		MFP_CFG(GPIO0, AF2)
#define GPIO11_EXT_SYNC0	MFP_CFG(GPIO11, AF5)
#define GPIO12_EXT_SYNC1	MFP_CFG(GPIO12, AF6)
#define GPIO0_2_HZ_CLK		MFP_CFG(GPIO0_2, AF1)
#define GPIO14_HZ_CLK		MFP_CFG(GPIO14, AF4)
#define GPIO30_ICP_RXD		MFP_CFG(GPIO30, AF1)
#define GPIO31_ICP_TXD		MFP_CFG(GPIO31, AF1)

#define GPIO83_KP_DKIN_0	MFP_CFG_LPM(GPIO83, AF3, FLOAT)
#define GPIO84_KP_DKIN_1	MFP_CFG_LPM(GPIO84, AF3, FLOAT)
#define GPIO85_KP_DKIN_2	MFP_CFG_LPM(GPIO85, AF3, FLOAT)
#define GPIO86_KP_DKIN_3	MFP_CFG_LPM(GPIO86, AF3, FLOAT)

#define GPIO105_KP_DKIN_0	MFP_CFG_LPM(GPIO105, AF2, FLOAT)
#define GPIO106_KP_DKIN_1	MFP_CFG_LPM(GPIO106, AF2, FLOAT)
#define GPIO107_KP_DKIN_2	MFP_CFG_LPM(GPIO107, AF2, FLOAT)
#define GPIO108_KP_DKIN_3	MFP_CFG_LPM(GPIO108, AF2, FLOAT)
#define GPIO109_KP_DKIN_4	MFP_CFG_LPM(GPIO109, AF2, FLOAT)
#define GPIO110_KP_DKIN_5	MFP_CFG_LPM(GPIO110, AF2, FLOAT)
#define GPIO111_KP_DKIN_6	MFP_CFG_LPM(GPIO111, AF2, FLOAT)
#define GPIO112_KP_DKIN_7	MFP_CFG_LPM(GPIO112, AF2, FLOAT)

#define GPIO113_KP_DKIN_0	MFP_CFG_LPM(GPIO113, AF2, FLOAT)
#define GPIO114_KP_DKIN_1	MFP_CFG_LPM(GPIO114, AF2, FLOAT)
#define GPIO115_KP_DKIN_2	MFP_CFG_LPM(GPIO115, AF2, FLOAT)
#define GPIO116_KP_DKIN_3	MFP_CFG_LPM(GPIO116, AF2, FLOAT)
#define GPIO117_KP_DKIN_4	MFP_CFG_LPM(GPIO117, AF2, FLOAT)
#define GPIO118_KP_DKIN_5	MFP_CFG_LPM(GPIO118, AF2, FLOAT)
#define GPIO119_KP_DKIN_6	MFP_CFG_LPM(GPIO119, AF2, FLOAT)
#define GPIO120_KP_DKIN_7	MFP_CFG_LPM(GPIO120, AF2, FLOAT)

#define GPIO127_KP_DKIN_0	MFP_CFG_LPM(GPIO127, AF2, FLOAT)
#define GPIO126_KP_DKIN_1	MFP_CFG_LPM(GPIO126, AF2, FLOAT)

#define GPIO2_2_KP_DKIN_0	MFP_CFG_LPM(GPIO2_2, AF2, FLOAT)
#define GPIO3_2_KP_DKIN_1	MFP_CFG_LPM(GPIO3_2, AF2, FLOAT)
#define GPIO125_KP_DKIN_2	MFP_CFG_LPM(GPIO125, AF2, FLOAT)
#define GPIO124_KP_DKIN_3	MFP_CFG_LPM(GPIO124, AF2, FLOAT)
#define GPIO123_KP_DKIN_4	MFP_CFG_LPM(GPIO123, AF2, FLOAT)
#define GPIO122_KP_DKIN_5	MFP_CFG_LPM(GPIO122, AF2, FLOAT)
#define GPIO121_KP_DKIN_6	MFP_CFG_LPM(GPIO121, AF2, FLOAT)
#define GPIO4_2_KP_DKIN_7	MFP_CFG_LPM(GPIO4_2, AF2, FLOAT)

#define GPIO113_KP_MKIN_0	MFP_CFG_LPM(GPIO113, AF1, FLOAT)
#define GPIO114_KP_MKIN_1	MFP_CFG_LPM(GPIO114, AF1, FLOAT)
#define GPIO115_KP_MKIN_2	MFP_CFG_LPM(GPIO115, AF1, FLOAT)
#define GPIO116_KP_MKIN_3	MFP_CFG_LPM(GPIO116, AF1, FLOAT)
#define GPIO117_KP_MKIN_4	MFP_CFG_LPM(GPIO117, AF1, FLOAT)
#define GPIO118_KP_MKIN_5	MFP_CFG_LPM(GPIO118, AF1, FLOAT)
#define GPIO119_KP_MKIN_6	MFP_CFG_LPM(GPIO119, AF1, FLOAT)
#define GPIO120_KP_MKIN_7	MFP_CFG_LPM(GPIO120, AF1, FLOAT)

#define GPIO83_KP_MKOUT_0	MFP_CFG_LPM(GPIO83, AF2, DRIVE_HIGH)
#define GPIO84_KP_MKOUT_1	MFP_CFG_LPM(GPIO84, AF2, DRIVE_HIGH)
#define GPIO85_KP_MKOUT_2	MFP_CFG_LPM(GPIO85, AF2, DRIVE_HIGH)
#define GPIO86_KP_MKOUT_3	MFP_CFG_LPM(GPIO86, AF2, DRIVE_HIGH)
#define GPIO13_KP_MKOUT_4	MFP_CFG_LPM(GPIO13, AF3, DRIVE_HIGH)
#define GPIO14_KP_MKOUT_5	MFP_CFG_LPM(GPIO14, AF3, DRIVE_HIGH)

#define GPIO121_KP_MKOUT_0	MFP_CFG_LPM(GPIO121, AF1, DRIVE_HIGH)
#define GPIO122_KP_MKOUT_1	MFP_CFG_LPM(GPIO122, AF1, DRIVE_HIGH)
#define GPIO123_KP_MKOUT_2	MFP_CFG_LPM(GPIO123, AF1, DRIVE_HIGH)
#define GPIO124_KP_MKOUT_3	MFP_CFG_LPM(GPIO124, AF1, DRIVE_HIGH)
#define GPIO125_KP_MKOUT_4	MFP_CFG_LPM(GPIO125, AF1, DRIVE_HIGH)
#define GPIO126_KP_MKOUT_5	MFP_CFG_LPM(GPIO126, AF1, DRIVE_HIGH)
#define GPIO127_KP_MKOUT_6	MFP_CFG_LPM(GPIO127, AF1, DRIVE_HIGH)
#define GPIO5_2_KP_MKOUT_7	MFP_CFG_LPM(GPIO5_2, AF1, DRIVE_HIGH)

/* LCD */
#define GPIO6_2_LCD_LDD_0	MFP_CFG_DRV(GPIO6_2, AF1, DS01X)
#define GPIO7_2_LCD_LDD_1	MFP_CFG_DRV(GPIO7_2, AF1, DS01X)
#define GPIO8_2_LCD_LDD_2	MFP_CFG_DRV(GPIO8_2, AF1, DS01X)
#define GPIO9_2_LCD_LDD_3	MFP_CFG_DRV(GPIO9_2, AF1, DS01X)
#define GPIO10_2_LCD_LDD_4	MFP_CFG_DRV(GPIO10_2, AF1, DS01X)
#define GPIO11_2_LCD_LDD_5	MFP_CFG_DRV(GPIO11_2, AF1, DS01X)
#define GPIO12_2_LCD_LDD_6	MFP_CFG_DRV(GPIO12_2, AF1, DS01X)
#define GPIO13_2_LCD_LDD_7	MFP_CFG_DRV(GPIO13_2, AF1, DS01X)
#define GPIO63_LCD_LDD_8	MFP_CFG_DRV(GPIO63, AF1, DS01X)
#define GPIO64_LCD_LDD_9	MFP_CFG_DRV(GPIO64, AF1, DS01X)
#define GPIO65_LCD_LDD_10	MFP_CFG_DRV(GPIO65, AF1, DS01X)
#define GPIO66_LCD_LDD_11	MFP_CFG_DRV(GPIO66, AF1, DS01X)
#define GPIO67_LCD_LDD_12	MFP_CFG_DRV(GPIO67, AF1, DS01X)
#define GPIO68_LCD_LDD_13	MFP_CFG_DRV(GPIO68, AF1, DS01X)
#define GPIO69_LCD_LDD_14	MFP_CFG_DRV(GPIO69, AF1, DS01X)
#define GPIO70_LCD_LDD_15	MFP_CFG_DRV(GPIO70, AF1, DS01X)
#define GPIO71_LCD_LDD_16	MFP_CFG_DRV(GPIO71, AF1, DS01X)
#define GPIO72_LCD_LDD_17	MFP_CFG_DRV(GPIO72, AF1, DS01X)
#define GPIO73_LCD_CS_N		MFP_CFG_DRV(GPIO73, AF2, DS01X)
#define GPIO74_LCD_VSYNC	MFP_CFG_DRV(GPIO74, AF2, DS01X)
#define GPIO14_2_LCD_FCLK	MFP_CFG_DRV(GPIO14_2, AF1, DS01X)
#define GPIO15_2_LCD_LCLK	MFP_CFG_DRV(GPIO15_2, AF1, DS01X)
#define GPIO16_2_LCD_PCLK	MFP_CFG_DRV(GPIO16_2, AF1, DS01X)
#define GPIO17_2_LCD_BIAS	MFP_CFG_DRV(GPIO17_2, AF1, DS01X)
#define GPIO64_LCD_VSYNC	MFP_CFG_DRV(GPIO64, AF2, DS01X)
#define GPIO63_LCD_CS_N		MFP_CFG_DRV(GPIO63, AF2, DS01X)

#define GPIO6_2_MLCD_DD_0	MFP_CFG_DRV(GPIO6_2, AF7, DS08X)
#define GPIO7_2_MLCD_DD_1	MFP_CFG_DRV(GPIO7_2, AF7, DS08X)
#define GPIO8_2_MLCD_DD_2	MFP_CFG_DRV(GPIO8_2, AF7, DS08X)
#define GPIO9_2_MLCD_DD_3	MFP_CFG_DRV(GPIO9_2, AF7, DS08X)
#define GPIO10_2_MLCD_DD_4	MFP_CFG_DRV(GPIO10_2, AF7, DS08X)
#define GPIO11_2_MLCD_DD_5	MFP_CFG_DRV(GPIO11_2, AF7, DS08X)
#define GPIO12_2_MLCD_DD_6	MFP_CFG_DRV(GPIO12_2, AF7, DS08X)
#define GPIO13_2_MLCD_DD_7	MFP_CFG_DRV(GPIO13_2, AF7, DS08X)
#define GPIO63_MLCD_DD_8	MFP_CFG_DRV(GPIO63, AF7, DS08X)
#define GPIO64_MLCD_DD_9	MFP_CFG_DRV(GPIO64, AF7, DS08X)
#define GPIO65_MLCD_DD_10	MFP_CFG_DRV(GPIO65, AF7, DS08X)
#define GPIO66_MLCD_DD_11	MFP_CFG_DRV(GPIO66, AF7, DS08X)
#define GPIO67_MLCD_DD_12	MFP_CFG_DRV(GPIO67, AF7, DS08X)
#define GPIO68_MLCD_DD_13	MFP_CFG_DRV(GPIO68, AF7, DS08X)
#define GPIO69_MLCD_DD_14	MFP_CFG_DRV(GPIO69, AF7, DS08X)
#define GPIO70_MLCD_DD_15	MFP_CFG_DRV(GPIO70, AF7, DS08X)
#define GPIO71_MLCD_DD_16	MFP_CFG_DRV(GPIO71, AF7, DS08X)
#define GPIO72_MLCD_DD_17	MFP_CFG_DRV(GPIO72, AF7, DS08X)
#define GPIO73_MLCD_CS		MFP_CFG_DRV(GPIO73, AF7, DS08X)
#define GPIO74_MLCD_VSYNC	MFP_CFG_DRV(GPIO74, AF7, DS08X)
#define GPIO14_2_MLCD_FCLK	MFP_CFG_DRV(GPIO14_2, AF7, DS08X)
#define GPIO15_2_MLCD_LCLK	MFP_CFG_DRV(GPIO15_2, AF7, DS08X)
#define GPIO16_2_MLCD_PCLK	MFP_CFG_DRV(GPIO16_2, AF7, DS08X)
#define GPIO17_2_MLCD_BIAS	MFP_CFG_DRV(GPIO17_2, AF7, DS08X)

/* MMC1 */
#define GPIO9_MMC1_CMD		MFP_CFG_LPM(GPIO9,  AF4, DRIVE_HIGH)
#define GPIO22_MMC1_CLK		MFP_CFG_LPM(GPIO22, AF4, DRIVE_HIGH)
#define GPIO23_MMC1_CMD		MFP_CFG_LPM(GPIO23, AF4, DRIVE_HIGH)
#define GPIO30_MMC1_CLK		MFP_CFG_LPM(GPIO30, AF4, DRIVE_HIGH)
#define GPIO31_MMC1_CMD		MFP_CFG_LPM(GPIO31, AF4, DRIVE_HIGH)
#define GPIO5_MMC1_DAT0		MFP_CFG_LPM(GPIO5,  AF4, DRIVE_HIGH)
#define GPIO6_MMC1_DAT1		MFP_CFG_LPM(GPIO6,  AF4, DRIVE_HIGH)
#define GPIO7_MMC1_DAT2		MFP_CFG_LPM(GPIO7,  AF4, DRIVE_HIGH)
#define GPIO8_MMC1_DAT3		MFP_CFG_LPM(GPIO8,  AF4, DRIVE_HIGH)
#define GPIO18_MMC1_DAT0	MFP_CFG_LPM(GPIO18, AF4, DRIVE_HIGH)
#define GPIO19_MMC1_DAT1	MFP_CFG_LPM(GPIO19, AF4, DRIVE_HIGH)
#define GPIO20_MMC1_DAT2	MFP_CFG_LPM(GPIO20, AF4, DRIVE_HIGH)
#define GPIO21_MMC1_DAT3	MFP_CFG_LPM(GPIO21, AF4, DRIVE_HIGH)

#define GPIO28_MMC2_CLK		MFP_CFG_LPM(GPIO28, AF4, PULL_HIGH)
#define GPIO29_MMC2_CMD		MFP_CFG_LPM(GPIO29, AF4, PULL_HIGH)
#define GPIO30_MMC2_CLK		MFP_CFG_LPM(GPIO30, AF3, PULL_HIGH)
#define GPIO31_MMC2_CMD		MFP_CFG_LPM(GPIO31, AF3, PULL_HIGH)
#define GPIO79_MMC2_CLK		MFP_CFG_LPM(GPIO79, AF4, PULL_HIGH)
#define GPIO80_MMC2_CMD		MFP_CFG_LPM(GPIO80, AF4, PULL_HIGH)

#define GPIO5_MMC2_DAT0		MFP_CFG_LPM(GPIO5, AF2, PULL_HIGH)
#define GPIO6_MMC2_DAT1		MFP_CFG_LPM(GPIO6, AF2, PULL_HIGH)
#define GPIO7_MMC2_DAT2		MFP_CFG_LPM(GPIO7, AF2, PULL_HIGH)
#define GPIO8_MMC2_DAT3		MFP_CFG_LPM(GPIO8, AF2, PULL_HIGH)
#define GPIO24_MMC2_DAT0	MFP_CFG_LPM(GPIO24, AF4, PULL_HIGH)
#define GPIO75_MMC2_DAT0	MFP_CFG_LPM(GPIO75, AF4, PULL_HIGH)
#define GPIO25_MMC2_DAT1	MFP_CFG_LPM(GPIO25, AF4, PULL_HIGH)
#define GPIO76_MMC2_DAT1	MFP_CFG_LPM(GPIO76, AF4, PULL_HIGH)
#define GPIO26_MMC2_DAT2	MFP_CFG_LPM(GPIO26, AF4, PULL_HIGH)
#define GPIO77_MMC2_DAT2	MFP_CFG_LPM(GPIO77, AF4, PULL_HIGH)
#define GPIO27_MMC2_DAT3	MFP_CFG_LPM(GPIO27, AF4, PULL_HIGH)
#define GPIO78_MMC2_DAT3	MFP_CFG_LPM(GPIO78, AF4, PULL_HIGH)

/* 1-Wire */
#define GPIO14_ONE_WIRE		MFP_CFG_LPM(GPIO14,  AF5, FLOAT)
#define GPIO0_2_ONE_WIRE	MFP_CFG_LPM(GPIO0_2, AF2, FLOAT)

/* SSP1 */
#define GPIO87_SSP1_EXTCLK	MFP_CFG(GPIO87, AF1)
#define GPIO88_SSP1_SYSCLK	MFP_CFG(GPIO88, AF1)
#define GPIO83_SSP1_SCLK	MFP_CFG(GPIO83, AF1)
#define GPIO84_SSP1_SFRM	MFP_CFG(GPIO84, AF1)
#define GPIO85_SSP1_RXD		MFP_CFG(GPIO85, AF6)
#define GPIO85_SSP1_TXD		MFP_CFG(GPIO85, AF1)
#define GPIO86_SSP1_RXD		MFP_CFG(GPIO86, AF1)
#define GPIO86_SSP1_TXD		MFP_CFG(GPIO86, AF6)

/* SSP2 */
#define GPIO39_SSP2_EXTCLK	MFP_CFG(GPIO39, AF2)
#define GPIO40_SSP2_SYSCLK	MFP_CFG(GPIO40, AF2)
#define GPIO12_SSP2_SCLK	MFP_CFG(GPIO12, AF2)
#define GPIO35_SSP2_SCLK	MFP_CFG(GPIO35, AF2)
#define GPIO36_SSP2_SFRM	MFP_CFG(GPIO36, AF2)
#define GPIO37_SSP2_RXD		MFP_CFG(GPIO37, AF5)
#define GPIO37_SSP2_TXD		MFP_CFG(GPIO37, AF2)
#define GPIO38_SSP2_RXD		MFP_CFG(GPIO38, AF2)
#define GPIO38_SSP2_TXD		MFP_CFG(GPIO38, AF5)

#define GPIO69_SSP3_SCLK	MFP_CFG_X(GPIO69, AF2, DS08X, FLOAT)
#define GPIO70_SSP3_FRM		MFP_CFG_X(GPIO70, AF2, DS08X, DRIVE_LOW)
#define GPIO89_SSP3_SCLK	MFP_CFG_X(GPIO89, AF1, DS08X, FLOAT)
#define GPIO90_SSP3_FRM		MFP_CFG_X(GPIO90, AF1, DS08X, DRIVE_LOW)
#define GPIO71_SSP3_RXD		MFP_CFG_X(GPIO71, AF5, DS08X, FLOAT)
#define GPIO71_SSP3_TXD		MFP_CFG_X(GPIO71, AF2, DS08X, DRIVE_LOW)
#define GPIO72_SSP3_RXD		MFP_CFG_X(GPIO72, AF2, DS08X, FLOAT)
#define GPIO72_SSP3_TXD		MFP_CFG_X(GPIO72, AF5, DS08X, DRIVE_LOW)
#define GPIO91_SSP3_RXD		MFP_CFG_X(GPIO91, AF5, DS08X, FLOAT)
#define GPIO91_SSP3_TXD		MFP_CFG_X(GPIO91, AF1, DS08X, DRIVE_LOW)
#define GPIO92_SSP3_RXD		MFP_CFG_X(GPIO92, AF1, DS08X, FLOAT)
#define GPIO92_SSP3_TXD		MFP_CFG_X(GPIO92, AF5, DS08X, DRIVE_LOW)

#define GPIO93_SSP4_SCLK	MFP_CFG_LPM(GPIO93, AF1, PULL_HIGH)
#define GPIO94_SSP4_FRM		MFP_CFG_LPM(GPIO94, AF1, PULL_HIGH)
#define GPIO94_SSP4_RXD		MFP_CFG_LPM(GPIO94, AF5, PULL_HIGH)
#define GPIO95_SSP4_RXD		MFP_CFG_LPM(GPIO95, AF5, PULL_HIGH)
#define GPIO95_SSP4_TXD		MFP_CFG_LPM(GPIO95, AF1, PULL_HIGH)
#define GPIO96_SSP4_RXD		MFP_CFG_LPM(GPIO96, AF1, PULL_HIGH)
#define GPIO96_SSP4_TXD		MFP_CFG_LPM(GPIO96, AF5, PULL_HIGH)

/* UART1 */
#define GPIO41_UART1_RXD	MFP_CFG_LPM(GPIO41, AF2, FLOAT)
#define GPIO41_UART1_TXD	MFP_CFG_LPM(GPIO41, AF4, FLOAT)
#define GPIO42_UART1_RXD	MFP_CFG_LPM(GPIO42, AF4, FLOAT)
#define GPIO42_UART1_TXD	MFP_CFG_LPM(GPIO42, AF2, FLOAT)
#define GPIO97_UART1_RXD	MFP_CFG_LPM(GPIO97, AF1, FLOAT)
#define GPIO97_UART1_TXD	MFP_CFG_LPM(GPIO97, AF6, FLOAT)
#define GPIO98_UART1_RXD	MFP_CFG_LPM(GPIO98, AF6, FLOAT)
#define GPIO98_UART1_TXD	MFP_CFG_LPM(GPIO98, AF1, FLOAT)
#define GPIO43_UART1_CTS	MFP_CFG_LPM(GPIO43, AF2, FLOAT)
#define GPIO43_UART1_RTS	MFP_CFG_LPM(GPIO43, AF4, FLOAT)
#define GPIO48_UART1_CTS	MFP_CFG_LPM(GPIO48, AF4, FLOAT)
#define GPIO48_UART1_RTS	MFP_CFG_LPM(GPIO48, AF2, FLOAT)
#define GPIO99_UART1_CTS	MFP_CFG_LPM(GPIO99, AF1, FLOAT)
#define GPIO99_UART1_RTS	MFP_CFG_LPM(GPIO99, AF6, FLOAT)
#define GPIO104_UART1_CTS	MFP_CFG_LPM(GPIO104, AF6, FLOAT)
#define GPIO104_UART1_RTS	MFP_CFG_LPM(GPIO104, AF1, FLOAT)
#define GPIO45_UART1_DTR	MFP_CFG_LPM(GPIO45, AF4, FLOAT)
#define GPIO45_UART1_DSR	MFP_CFG_LPM(GPIO45, AF2, FLOAT)
#define GPIO47_UART1_DTR	MFP_CFG_LPM(GPIO47, AF2, FLOAT)
#define GPIO47_UART1_DSR	MFP_CFG_LPM(GPIO47, AF4, FLOAT)
#define GPIO101_UART1_DTR	MFP_CFG_LPM(GPIO101, AF6, FLOAT)
#define GPIO101_UART1_DSR	MFP_CFG_LPM(GPIO101, AF1, FLOAT)
#define GPIO103_UART1_DTR	MFP_CFG_LPM(GPIO103, AF1, FLOAT)
#define GPIO103_UART1_DSR	MFP_CFG_LPM(GPIO103, AF6, FLOAT)
#define GPIO44_UART1_DCD	MFP_CFG_LPM(GPIO44, AF2, FLOAT)
#define GPIO100_UART1_DCD	MFP_CFG_LPM(GPIO100, AF1, FLOAT)
#define GPIO46_UART1_RI		MFP_CFG_LPM(GPIO46, AF2, FLOAT)
#define GPIO102_UART1_RI	MFP_CFG_LPM(GPIO102, AF1, FLOAT)

/* UART2 */
#define GPIO109_UART2_CTS	MFP_CFG_LPM(GPIO109, AF3, FLOAT)
#define GPIO109_UART2_RTS	MFP_CFG_LPM(GPIO109, AF1, FLOAT)
#define GPIO112_UART2_CTS	MFP_CFG_LPM(GPIO112, AF1, FLOAT)
#define GPIO112_UART2_RTS	MFP_CFG_LPM(GPIO112, AF3, FLOAT)
#define GPIO110_UART2_RXD	MFP_CFG_LPM(GPIO110, AF1, FLOAT)
#define GPIO110_UART2_TXD	MFP_CFG_LPM(GPIO110, AF3, FLOAT)
#define GPIO111_UART2_RXD	MFP_CFG_LPM(GPIO111, AF3, FLOAT)
#define GPIO111_UART2_TXD	MFP_CFG_LPM(GPIO111, AF1, FLOAT)

/* UART3 */
#define GPIO89_UART3_CTS	MFP_CFG_LPM(GPIO89, AF2, FLOAT)
#define GPIO89_UART3_RTS	MFP_CFG_LPM(GPIO89, AF4, FLOAT)
#define GPIO90_UART3_CTS	MFP_CFG_LPM(GPIO90, AF4, FLOAT)
#define GPIO90_UART3_RTS	MFP_CFG_LPM(GPIO90, AF2, FLOAT)
#define GPIO105_UART3_CTS	MFP_CFG_LPM(GPIO105, AF1, FLOAT)
#define GPIO105_UART3_RTS	MFP_CFG_LPM(GPIO105, AF3, FLOAT)
#define GPIO106_UART3_CTS	MFP_CFG_LPM(GPIO106, AF3, FLOAT)
#define GPIO106_UART3_RTS	MFP_CFG_LPM(GPIO106, AF1, FLOAT)
#define GPIO30_UART3_RXD	MFP_CFG_LPM(GPIO30, AF2, FLOAT)
#define GPIO30_UART3_TXD	MFP_CFG_LPM(GPIO30, AF6, FLOAT)
#define GPIO31_UART3_RXD	MFP_CFG_LPM(GPIO31, AF6, FLOAT)
#define GPIO31_UART3_TXD	MFP_CFG_LPM(GPIO31, AF2, FLOAT)
#define GPIO91_UART3_RXD	MFP_CFG_LPM(GPIO91, AF4, FLOAT)
#define GPIO91_UART3_TXD	MFP_CFG_LPM(GPIO91, AF2, FLOAT)
#define GPIO92_UART3_RXD	MFP_CFG_LPM(GPIO92, AF2, FLOAT)
#define GPIO92_UART3_TXD	MFP_CFG_LPM(GPIO92, AF4, FLOAT)
#define GPIO107_UART3_RXD	MFP_CFG_LPM(GPIO107, AF3, FLOAT)
#define GPIO107_UART3_TXD	MFP_CFG_LPM(GPIO107, AF1, FLOAT)
#define GPIO108_UART3_RXD	MFP_CFG_LPM(GPIO108, AF1, FLOAT)
#define GPIO108_UART3_TXD	MFP_CFG_LPM(GPIO108, AF3, FLOAT)


/* USB 2.0 UTMI */
#define GPIO10_UTM_CLK		MFP_CFG(GPIO10, AF1)
#define GPIO36_U2D_RXERROR	MFP_CFG(GPIO36, AF3)
#define GPIO60_U2D_RXERROR	MFP_CFG(GPIO60, AF1)
#define GPIO87_U2D_RXERROR	MFP_CFG(GPIO87, AF5)
#define GPIO34_UTM_RXVALID	MFP_CFG(GPIO34, AF3)
#define GPIO58_UTM_RXVALID	MFP_CFG(GPIO58, AF2)
#define GPIO85_UTM_RXVALID	MFP_CFG(GPIO85, AF5)
#define GPIO35_UTM_RXACTIVE	MFP_CFG(GPIO35, AF3)
#define GPIO59_UTM_RXACTIVE	MFP_CFG(GPIO59, AF1)
#define GPIO86_UTM_RXACTIVE	MFP_CFG(GPIO86, AF5)
#define GPIO73_UTM_TXREADY	MFP_CFG(GPIO73, AF1)
#define GPIO68_UTM_LINESTATE_0	MFP_CFG(GPIO68, AF3)
#define GPIO90_UTM_LINESTATE_0	MFP_CFG(GPIO90, AF3)
#define GPIO102_UTM_LINESTATE_0	MFP_CFG(GPIO102, AF3)
#define GPIO107_UTM_LINESTATE_0	MFP_CFG(GPIO107, AF4)
#define GPIO69_UTM_LINESTATE_1	MFP_CFG(GPIO69, AF3)
#define GPIO91_UTM_LINESTATE_1	MFP_CFG(GPIO91, AF3)
#define GPIO103_UTM_LINESTATE_1	MFP_CFG(GPIO103, AF3)

#define GPIO41_U2D_PHYDATA_0	MFP_CFG(GPIO41, AF3)
#define GPIO42_U2D_PHYDATA_1	MFP_CFG(GPIO42, AF3)
#define GPIO43_U2D_PHYDATA_2	MFP_CFG(GPIO43, AF3)
#define GPIO44_U2D_PHYDATA_3	MFP_CFG(GPIO44, AF3)
#define GPIO45_U2D_PHYDATA_4	MFP_CFG(GPIO45, AF3)
#define GPIO46_U2D_PHYDATA_5	MFP_CFG(GPIO46, AF3)
#define GPIO47_U2D_PHYDATA_6	MFP_CFG(GPIO47, AF3)
#define GPIO48_U2D_PHYDATA_7	MFP_CFG(GPIO48, AF3)

#define GPIO49_U2D_PHYDATA_0	MFP_CFG(GPIO49, AF3)
#define GPIO50_U2D_PHYDATA_1	MFP_CFG(GPIO50, AF3)
#define GPIO51_U2D_PHYDATA_2	MFP_CFG(GPIO51, AF3)
#define GPIO52_U2D_PHYDATA_3	MFP_CFG(GPIO52, AF3)
#define GPIO53_U2D_PHYDATA_4	MFP_CFG(GPIO53, AF3)
#define GPIO54_U2D_PHYDATA_5	MFP_CFG(GPIO54, AF3)
#define GPIO55_U2D_PHYDATA_6	MFP_CFG(GPIO55, AF3)
#define GPIO56_U2D_PHYDATA_7	MFP_CFG(GPIO56, AF3)

#define GPIO37_U2D_OPMODE0	MFP_CFG(GPIO37, AF4)
#define GPIO61_U2D_OPMODE0	MFP_CFG(GPIO61, AF2)
#define GPIO88_U2D_OPMODE0	MFP_CFG(GPIO88, AF7)

#define GPIO38_U2D_OPMODE1	MFP_CFG(GPIO38, AF4)
#define GPIO62_U2D_OPMODE1	MFP_CFG(GPIO62, AF2)
#define GPIO104_U2D_OPMODE1	MFP_CFG(GPIO104, AF4)
#define GPIO108_U2D_OPMODE1	MFP_CFG(GPIO108, AF5)

#define GPIO74_U2D_RESET	MFP_CFG(GPIO74, AF1)
#define GPIO93_U2D_RESET	MFP_CFG(GPIO93, AF2)
#define GPIO98_U2D_RESET	MFP_CFG(GPIO98, AF3)

#define GPIO67_U2D_SUSPEND	MFP_CFG(GPIO67, AF3)
#define GPIO96_U2D_SUSPEND	MFP_CFG(GPIO96, AF2)
#define GPIO101_U2D_SUSPEND	MFP_CFG(GPIO101, AF3)

#define GPIO66_U2D_TERM_SEL	MFP_CFG(GPIO66, AF5)
#define GPIO95_U2D_TERM_SEL	MFP_CFG(GPIO95, AF3)
#define GPIO97_U2D_TERM_SEL	MFP_CFG(GPIO97, AF7)
#define GPIO100_U2D_TERM_SEL	MFP_CFG(GPIO100, AF5)

#define GPIO39_U2D_TXVALID	MFP_CFG(GPIO39, AF4)
#define GPIO70_U2D_TXVALID	MFP_CFG(GPIO70, AF5)
#define GPIO83_U2D_TXVALID	MFP_CFG(GPIO83, AF7)

#define GPIO65_U2D_XCVR_SEL	MFP_CFG(GPIO65, AF5)
#define GPIO94_U2D_XCVR_SEL	MFP_CFG(GPIO94, AF3)
#define GPIO99_U2D_XCVR_SEL	MFP_CFG(GPIO99, AF5)

/* USB Host 1.1 */
#define GPIO2_2_USBH_PEN	MFP_CFG(GPIO2_2, AF1)
#define GPIO3_2_USBH_PWR	MFP_CFG(GPIO3_2, AF1)

/* USB P2 */
#define GPIO97_USB_P2_2		MFP_CFG(GPIO97, AF2)
#define GPIO97_USB_P2_6		MFP_CFG(GPIO97, AF4)
#define GPIO98_USB_P2_2		MFP_CFG(GPIO98, AF4)
#define GPIO98_USB_P2_6		MFP_CFG(GPIO98, AF2)
#define GPIO99_USB_P2_1		MFP_CFG(GPIO99, AF2)
#define GPIO100_USB_P2_4	MFP_CFG(GPIO100, AF2)
#define GPIO101_USB_P2_8	MFP_CFG(GPIO101, AF2)
#define GPIO102_USB_P2_3	MFP_CFG(GPIO102, AF2)
#define GPIO103_USB_P2_5	MFP_CFG(GPIO103, AF2)
#define GPIO104_USB_P2_7	MFP_CFG(GPIO104, AF2)

/* USB P3 */
#define GPIO75_USB_P3_1		MFP_CFG(GPIO75, AF2)
#define GPIO76_USB_P3_2		MFP_CFG(GPIO76, AF2)
#define GPIO77_USB_P3_3		MFP_CFG(GPIO77, AF2)
#define GPIO78_USB_P3_4		MFP_CFG(GPIO78, AF2)
#define GPIO79_USB_P3_5		MFP_CFG(GPIO79, AF2)
#define GPIO80_USB_P3_6		MFP_CFG(GPIO80, AF2)

#define GPIO13_CHOUT0		MFP_CFG(GPIO13, AF6)
#define GPIO14_CHOUT1		MFP_CFG(GPIO14, AF6)

#define GPIO2_RDY		MFP_CFG(GPIO2, AF1)
#define GPIO5_NPIOR		MFP_CFG(GPIO5, AF3)

#define GPIO11_PWM0_OUT		MFP_CFG(GPIO11, AF1)
#define GPIO12_PWM1_OUT		MFP_CFG(GPIO12, AF1)
#define GPIO13_PWM2_OUT		MFP_CFG(GPIO13, AF1)
#define GPIO14_PWM3_OUT		MFP_CFG(GPIO14, AF1)

#endif /* __ASM_ARCH_MFP_PXA320_H */
