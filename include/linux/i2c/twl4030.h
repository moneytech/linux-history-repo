/*
 * twl4030.h - header for TWL4030 PM and audio CODEC device
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * Based on tlv320aic23.c:
 * Copyright (c) by Kai Svahn <kai.svahn@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __TWL4030_H_
#define __TWL4030_H_

#include <linux/types.h>
#include <linux/input/matrix_keypad.h>

/*
 * Using the twl4030 core we address registers using a pair
 *	{ module id, relative register offset }
 * which that core then maps to the relevant
 *	{ i2c slave, absolute register address }
 *
 * The module IDs are meaningful only to the twl4030 core code,
 * which uses them as array indices to look up the first register
 * address each module uses within a given i2c slave.
 */

/* Slave 0 (i2c address 0x48) */
#define TWL4030_MODULE_USB		0x00

/* Slave 1 (i2c address 0x49) */
#define TWL4030_MODULE_AUDIO_VOICE	0x01
#define TWL4030_MODULE_GPIO		0x02
#define TWL4030_MODULE_INTBR		0x03
#define TWL4030_MODULE_PIH		0x04
#define TWL4030_MODULE_TEST		0x05

/* Slave 2 (i2c address 0x4a) */
#define TWL4030_MODULE_KEYPAD		0x06
#define TWL4030_MODULE_MADC		0x07
#define TWL4030_MODULE_INTERRUPTS	0x08
#define TWL4030_MODULE_LED		0x09
#define TWL4030_MODULE_MAIN_CHARGE	0x0A
#define TWL4030_MODULE_PRECHARGE	0x0B
#define TWL4030_MODULE_PWM0		0x0C
#define TWL4030_MODULE_PWM1		0x0D
#define TWL4030_MODULE_PWMA		0x0E
#define TWL4030_MODULE_PWMB		0x0F

/* Slave 3 (i2c address 0x4b) */
#define TWL4030_MODULE_BACKUP		0x10
#define TWL4030_MODULE_INT		0x11
#define TWL4030_MODULE_PM_MASTER	0x12
#define TWL4030_MODULE_PM_RECEIVER	0x13
#define TWL4030_MODULE_RTC		0x14
#define TWL4030_MODULE_SECURED_REG	0x15

/*
 * Read and write single 8-bit registers
 */
int twl4030_i2c_write_u8(u8 mod_no, u8 val, u8 reg);
int twl4030_i2c_read_u8(u8 mod_no, u8 *val, u8 reg);

/*
 * Read and write several 8-bit registers at once.
 *
 * IMPORTANT:  For twl4030_i2c_write(), allocate num_bytes + 1
 * for the value, and populate your data starting at offset 1.
 */
int twl4030_i2c_write(u8 mod_no, u8 *value, u8 reg, unsigned num_bytes);
int twl4030_i2c_read(u8 mod_no, u8 *value, u8 reg, unsigned num_bytes);

/*----------------------------------------------------------------------*/

/*
 * NOTE:  at up to 1024 registers, this is a big chip.
 *
 * Avoid putting register declarations in this file, instead of into
 * a driver-private file, unless some of the registers in a block
 * need to be shared with other drivers.  One example is blocks that
 * have Secondary IRQ Handler (SIH) registers.
 */

#define TWL4030_SIH_CTRL_EXCLEN_MASK	BIT(0)
#define TWL4030_SIH_CTRL_PENDDIS_MASK	BIT(1)
#define TWL4030_SIH_CTRL_COR_MASK	BIT(2)

/*----------------------------------------------------------------------*/

/*
 * GPIO Block Register offsets (use TWL4030_MODULE_GPIO)
 */

#define REG_GPIODATAIN1			0x0
#define REG_GPIODATAIN2			0x1
#define REG_GPIODATAIN3			0x2
#define REG_GPIODATADIR1		0x3
#define REG_GPIODATADIR2		0x4
#define REG_GPIODATADIR3		0x5
#define REG_GPIODATAOUT1		0x6
#define REG_GPIODATAOUT2		0x7
#define REG_GPIODATAOUT3		0x8
#define REG_CLEARGPIODATAOUT1		0x9
#define REG_CLEARGPIODATAOUT2		0xA
#define REG_CLEARGPIODATAOUT3		0xB
#define REG_SETGPIODATAOUT1		0xC
#define REG_SETGPIODATAOUT2		0xD
#define REG_SETGPIODATAOUT3		0xE
#define REG_GPIO_DEBEN1			0xF
#define REG_GPIO_DEBEN2			0x10
#define REG_GPIO_DEBEN3			0x11
#define REG_GPIO_CTRL			0x12
#define REG_GPIOPUPDCTR1		0x13
#define REG_GPIOPUPDCTR2		0x14
#define REG_GPIOPUPDCTR3		0x15
#define REG_GPIOPUPDCTR4		0x16
#define REG_GPIOPUPDCTR5		0x17
#define REG_GPIO_ISR1A			0x19
#define REG_GPIO_ISR2A			0x1A
#define REG_GPIO_ISR3A			0x1B
#define REG_GPIO_IMR1A			0x1C
#define REG_GPIO_IMR2A			0x1D
#define REG_GPIO_IMR3A			0x1E
#define REG_GPIO_ISR1B			0x1F
#define REG_GPIO_ISR2B			0x20
#define REG_GPIO_ISR3B			0x21
#define REG_GPIO_IMR1B			0x22
#define REG_GPIO_IMR2B			0x23
#define REG_GPIO_IMR3B			0x24
#define REG_GPIO_EDR1			0x28
#define REG_GPIO_EDR2			0x29
#define REG_GPIO_EDR3			0x2A
#define REG_GPIO_EDR4			0x2B
#define REG_GPIO_EDR5			0x2C
#define REG_GPIO_SIH_CTRL		0x2D

/* Up to 18 signals are available as GPIOs, when their
 * pins are not assigned to another use (such as ULPI/USB).
 */
#define TWL4030_GPIO_MAX		18

/*----------------------------------------------------------------------*/

/*
 * Keypad register offsets (use TWL4030_MODULE_KEYPAD)
 * ... SIH/interrupt only
 */

#define TWL4030_KEYPAD_KEYP_ISR1	0x11
#define TWL4030_KEYPAD_KEYP_IMR1	0x12
#define TWL4030_KEYPAD_KEYP_ISR2	0x13
#define TWL4030_KEYPAD_KEYP_IMR2	0x14
#define TWL4030_KEYPAD_KEYP_SIR		0x15	/* test register */
#define TWL4030_KEYPAD_KEYP_EDR		0x16
#define TWL4030_KEYPAD_KEYP_SIH_CTRL	0x17

/*----------------------------------------------------------------------*/

/*
 * Multichannel ADC register offsets (use TWL4030_MODULE_MADC)
 * ... SIH/interrupt only
 */

#define TWL4030_MADC_ISR1		0x61
#define TWL4030_MADC_IMR1		0x62
#define TWL4030_MADC_ISR2		0x63
#define TWL4030_MADC_IMR2		0x64
#define TWL4030_MADC_SIR		0x65	/* test register */
#define TWL4030_MADC_EDR		0x66
#define TWL4030_MADC_SIH_CTRL		0x67

/*----------------------------------------------------------------------*/

/*
 * Battery charger register offsets (use TWL4030_MODULE_INTERRUPTS)
 */

#define TWL4030_INTERRUPTS_BCIISR1A	0x0
#define TWL4030_INTERRUPTS_BCIISR2A	0x1
#define TWL4030_INTERRUPTS_BCIIMR1A	0x2
#define TWL4030_INTERRUPTS_BCIIMR2A	0x3
#define TWL4030_INTERRUPTS_BCIISR1B	0x4
#define TWL4030_INTERRUPTS_BCIISR2B	0x5
#define TWL4030_INTERRUPTS_BCIIMR1B	0x6
#define TWL4030_INTERRUPTS_BCIIMR2B	0x7
#define TWL4030_INTERRUPTS_BCISIR1	0x8	/* test register */
#define TWL4030_INTERRUPTS_BCISIR2	0x9	/* test register */
#define TWL4030_INTERRUPTS_BCIEDR1	0xa
#define TWL4030_INTERRUPTS_BCIEDR2	0xb
#define TWL4030_INTERRUPTS_BCIEDR3	0xc
#define TWL4030_INTERRUPTS_BCISIHCTRL	0xd

/*----------------------------------------------------------------------*/

/*
 * Power Interrupt block register offsets (use TWL4030_MODULE_INT)
 */

#define TWL4030_INT_PWR_ISR1		0x0
#define TWL4030_INT_PWR_IMR1		0x1
#define TWL4030_INT_PWR_ISR2		0x2
#define TWL4030_INT_PWR_IMR2		0x3
#define TWL4030_INT_PWR_SIR		0x4	/* test register */
#define TWL4030_INT_PWR_EDR1		0x5
#define TWL4030_INT_PWR_EDR2		0x6
#define TWL4030_INT_PWR_SIH_CTRL	0x7

/*----------------------------------------------------------------------*/

/* Power bus message definitions */

/* The TWL4030/5030 splits its power-management resources (the various
 * regulators, clock and reset lines) into 3 processor groups - P1, P2 and
 * P3. These groups can then be configured to transition between sleep, wait-on
 * and active states by sending messages to the power bus.  See Section 5.4.2
 * Power Resources of TWL4030 TRM
 */

/* Processor groups */
#define DEV_GRP_NULL		0x0
#define DEV_GRP_P1		0x1	/* P1: all OMAP devices */
#define DEV_GRP_P2		0x2	/* P2: all Modem devices */
#define DEV_GRP_P3		0x4	/* P3: all peripheral devices */

/* Resource groups */
#define RES_GRP_RES		0x0	/* Reserved */
#define RES_GRP_PP		0x1	/* Power providers */
#define RES_GRP_RC		0x2	/* Reset and control */
#define RES_GRP_PP_RC		0x3
#define RES_GRP_PR		0x4	/* Power references */
#define RES_GRP_PP_PR		0x5
#define RES_GRP_RC_PR		0x6
#define RES_GRP_ALL		0x7	/* All resource groups */

#define RES_TYPE2_R0		0x0

#define RES_TYPE_ALL		0x7

#define RES_STATE_WRST		0xF
#define RES_STATE_ACTIVE	0xE
#define RES_STATE_SLEEP		0x8
#define RES_STATE_OFF		0x0

/* Power resources */

/* Power providers */
#define RES_VAUX1               1
#define RES_VAUX2               2
#define RES_VAUX3               3
#define RES_VAUX4               4
#define RES_VMMC1               5
#define RES_VMMC2               6
#define RES_VPLL1               7
#define RES_VPLL2               8
#define RES_VSIM                9
#define RES_VDAC                10
#define RES_VINTANA1            11
#define RES_VINTANA2            12
#define RES_VINTDIG             13
#define RES_VIO                 14
#define RES_VDD1                15
#define RES_VDD2                16
#define RES_VUSB_1V5            17
#define RES_VUSB_1V8            18
#define RES_VUSB_3V1            19
#define RES_VUSBCP              20
#define RES_REGEN               21
/* Reset and control */
#define RES_NRES_PWRON          22
#define RES_CLKEN               23
#define RES_SYSEN               24
#define RES_HFCLKOUT            25
#define RES_32KCLKOUT           26
#define RES_RESET               27
/* Power Reference */
#define RES_Main_Ref            28

#define TOTAL_RESOURCES		28
/*
 * Power Bus Message Format ... these can be sent individually by Linux,
 * but are usually part of downloaded scripts that are run when various
 * power events are triggered.
 *
 *  Broadcast Message (16 Bits):
 *    DEV_GRP[15:13] MT[12]  RES_GRP[11:9]  RES_TYPE2[8:7] RES_TYPE[6:4]
 *    RES_STATE[3:0]
 *
 *  Singular Message (16 Bits):
 *    DEV_GRP[15:13] MT[12]  RES_ID[11:4]  RES_STATE[3:0]
 */

#define MSG_BROADCAST(devgrp, grp, type, type2, state) \
	( (devgrp) << 13 | 1 << 12 | (grp) << 9 | (type2) << 7 \
	| (type) << 4 | (state))

#define MSG_SINGULAR(devgrp, id, state) \
	((devgrp) << 13 | 0 << 12 | (id) << 4 | (state))

/*----------------------------------------------------------------------*/

struct twl4030_bci_platform_data {
	int *battery_tmp_tbl;
	unsigned int tblsize;
};

/* TWL4030_GPIO_MAX (18) GPIOs, with interrupts */
struct twl4030_gpio_platform_data {
	int		gpio_base;
	unsigned	irq_base, irq_end;

	/* package the two LED signals as output-only GPIOs? */
	bool		use_leds;

	/* gpio-n should control VMMC(n+1) if BIT(n) in mmc_cd is set */
	u8		mmc_cd;

	/* if BIT(N) is set, or VMMC(n+1) is linked, debounce GPIO-N */
	u32		debounce;

	/* For gpio-N, bit (1 << N) in "pullups" is set if that pullup
	 * should be enabled.  Else, if that bit is set in "pulldowns",
	 * that pulldown is enabled.  Don't waste power by letting any
	 * digital inputs float...
	 */
	u32		pullups;
	u32		pulldowns;

	int		(*setup)(struct device *dev,
				unsigned gpio, unsigned ngpio);
	int		(*teardown)(struct device *dev,
				unsigned gpio, unsigned ngpio);
};

struct twl4030_madc_platform_data {
	int		irq_line;
};

/* Boards have uniqe mappings of {row, col} --> keycode.
 * Column and row are 8 bits each, but range only from 0..7.
 * a PERSISTENT_KEY is "always on" and never reported.
 */
#define PERSISTENT_KEY(r, c)	KEY((r), (c), KEY_RESERVED)

struct twl4030_keypad_data {
	const struct matrix_keymap_data *keymap_data;
	unsigned rows;
	unsigned cols;
	bool rep;
};

enum twl4030_usb_mode {
	T2_USB_MODE_ULPI = 1,
	T2_USB_MODE_CEA2011_3PIN = 2,
};

struct twl4030_usb_data {
	enum twl4030_usb_mode	usb_mode;
};

struct twl4030_ins {
	u16 pmb_message;
	u8 delay;
};

struct twl4030_script {
	struct twl4030_ins *script;
	unsigned size;
	u8 flags;
#define TWL4030_WRST_SCRIPT	(1<<0)
#define TWL4030_WAKEUP12_SCRIPT	(1<<1)
#define TWL4030_WAKEUP3_SCRIPT	(1<<2)
#define TWL4030_SLEEP_SCRIPT	(1<<3)
};

struct twl4030_resconfig {
	u8 resource;
	u8 devgroup;	/* Processor group that Power resource belongs to */
	u8 type;	/* Power resource addressed, 6 / broadcast message */
	u8 type2;	/* Power resource addressed, 3 / broadcast message */
};

struct twl4030_power_data {
	struct twl4030_script **scripts;
	unsigned num;
	struct twl4030_resconfig *resource_config;
};

extern void twl4030_power_init(struct twl4030_power_data *triton2_scripts);

struct twl4030_codec_audio_data {
	unsigned int	audio_mclk;
	unsigned int ramp_delay_value;
	unsigned int hs_extmute:1;
	void (*set_hs_extmute)(int mute);
};

struct twl4030_codec_vibra_data {
	unsigned int	audio_mclk;
	unsigned int	coexist;
};

struct twl4030_codec_data {
	struct twl4030_codec_audio_data		*audio;
	struct twl4030_codec_vibra_data		*vibra;
};

struct twl4030_platform_data {
	unsigned				irq_base, irq_end;
	struct twl4030_bci_platform_data	*bci;
	struct twl4030_gpio_platform_data	*gpio;
	struct twl4030_madc_platform_data	*madc;
	struct twl4030_keypad_data		*keypad;
	struct twl4030_usb_data			*usb;
	struct twl4030_power_data		*power;
	struct twl4030_codec_data		*codec;

	/* LDO regulators */
	struct regulator_init_data		*vdac;
	struct regulator_init_data		*vpll1;
	struct regulator_init_data		*vpll2;
	struct regulator_init_data		*vmmc1;
	struct regulator_init_data		*vmmc2;
	struct regulator_init_data		*vsim;
	struct regulator_init_data		*vaux1;
	struct regulator_init_data		*vaux2;
	struct regulator_init_data		*vaux3;
	struct regulator_init_data		*vaux4;

	/* REVISIT more to come ... _nothing_ should be hard-wired */
};

/*----------------------------------------------------------------------*/

int twl4030_sih_setup(int module);

/* Offsets to Power Registers */
#define TWL4030_VDAC_DEV_GRP		0x3B
#define TWL4030_VDAC_DEDICATED		0x3E
#define TWL4030_VAUX1_DEV_GRP		0x17
#define TWL4030_VAUX1_DEDICATED		0x1A
#define TWL4030_VAUX2_DEV_GRP		0x1B
#define TWL4030_VAUX2_DEDICATED		0x1E
#define TWL4030_VAUX3_DEV_GRP		0x1F
#define TWL4030_VAUX3_DEDICATED		0x22

#if defined(CONFIG_TWL4030_BCI_BATTERY) || \
	defined(CONFIG_TWL4030_BCI_BATTERY_MODULE)
	extern int twl4030charger_usb_en(int enable);
#else
	static inline int twl4030charger_usb_en(int enable) { return 0; }
#endif

/*----------------------------------------------------------------------*/

/* Linux-specific regulator identifiers ... for now, we only support
 * the LDOs, and leave the three buck converters alone.  VDD1 and VDD2
 * need to tie into hardware based voltage scaling (cpufreq etc), while
 * VIO is generally fixed.
 */

/* EXTERNAL dc-to-dc buck converters */
#define TWL4030_REG_VDD1	0
#define TWL4030_REG_VDD2	1
#define TWL4030_REG_VIO		2

/* EXTERNAL LDOs */
#define TWL4030_REG_VDAC	3
#define TWL4030_REG_VPLL1	4
#define TWL4030_REG_VPLL2	5	/* not on all chips */
#define TWL4030_REG_VMMC1	6
#define TWL4030_REG_VMMC2	7	/* not on all chips */
#define TWL4030_REG_VSIM	8	/* not on all chips */
#define TWL4030_REG_VAUX1	9	/* not on all chips */
#define TWL4030_REG_VAUX2_4030	10	/* (twl4030-specific) */
#define TWL4030_REG_VAUX2	11	/* (twl5030 and newer) */
#define TWL4030_REG_VAUX3	12	/* not on all chips */
#define TWL4030_REG_VAUX4	13	/* not on all chips */

/* INTERNAL LDOs */
#define TWL4030_REG_VINTANA1	14
#define TWL4030_REG_VINTANA2	15
#define TWL4030_REG_VINTDIG	16
#define TWL4030_REG_VUSB1V5	17
#define TWL4030_REG_VUSB1V8	18
#define TWL4030_REG_VUSB3V1	19

#endif /* End of __TWL4030_H */
