/*
 * linux/sound/wm8993.h -- Platform data for WM8993
 *
 * Copyright 2009 Wolfson Microelectronics. PLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_SND_WM8993_H
#define __LINUX_SND_WM8993_H

/* Note that EQ1 only contains the enable/disable bit so will be
   ignored but is included for simplicity.
 */
struct wm8993_retune_mobile_setting {
	const char *name;
	unsigned int rate;
	u16 config[24];
};

struct wm8993_platform_data {
	struct wm8993_retune_mobile_setting *retune_configs;
	int num_retune_configs;

	/* LINEOUT can be differential or single ended */
	unsigned int lineout1_diff:1;
	unsigned int lineout2_diff:1;

	/* Common mode feedback */
	unsigned int lineout1fb:1;
	unsigned int lineout2fb:1;

	/* Microphone biases: 0=0.9*AVDD1 1=0.65*AVVD1 */
	unsigned int micbias1_lvl:1;
	unsigned int micbias2_lvl:1;

	/* Jack detect threashold levels, see datasheet for values */
	unsigned int jd_scthr:2;
	unsigned int jd_thr:2;
};

#endif
