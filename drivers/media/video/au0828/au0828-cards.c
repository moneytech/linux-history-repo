/*
 *  Driver for the Auvitek USB bridge
 *
 *  Copyright (c) 2008 Steven Toth <stoth@hauppauge.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "au0828.h"
#include "au0828-cards.h"

#define _dbg(level, fmt, arg...)\
	do {\
		if (debug >= level) \
			printk(KERN_DEBUG DRIVER_NAME "/0: " fmt, ## arg);\
	} while (0)

struct au0828_board au0828_boards[] = {
	[AU0828_BOARD_UNKNOWN] = {
		.name	= "Unknown board",
	},
	[AU0828_BOARD_HAUPPAUGE_HVR850] = {
		.name	= "Hauppauge HVR850",
	},
	[AU0828_BOARD_HAUPPAUGE_HVR950Q] = {
		.name	= "Hauppauge HVR950Q",
	},
	[AU0828_BOARD_DVICO_FUSIONHDTV7] = {
		.name	= "DViCO FusionHDTV USB",
	},
};
const unsigned int au0828_bcount = ARRAY_SIZE(au0828_boards);

/* Tuner callback function for au0828 boards. Currently only needed
 * for HVR1500Q, which has an xc5000 tuner.
 */
int au0828_tuner_callback(void *priv, int command, int arg)
{
	struct au0828_dev *dev = priv;

	switch(dev->board) {
	case AU0828_BOARD_HAUPPAUGE_HVR850:
	case AU0828_BOARD_HAUPPAUGE_HVR950Q:
	case AU0828_BOARD_DVICO_FUSIONHDTV7:
		if(command == 0) {
			/* Tuner Reset Command from xc5000 */
			/* Drive the tuner into reset and out */
			au0828_clear(dev, REG_001, 2);
			mdelay(200);
			au0828_set(dev, REG_001, 2);
			mdelay(50);
			return 0;
		}
		else {
			printk(KERN_ERR
				"%s(): Unknown command.\n", __FUNCTION__);
			return -EINVAL;
		}
		break;
	}

	return 0; /* Should never be here */
}

/*
 * The bridge has between 8 and 12 gpios.
 * Regs 1 and 0 deal with output enables.
 * Regs 3 and 2 * deal with direction.
 */
void au0828_gpio_setup(struct au0828_dev *dev)
{
	switch(dev->board) {
	case AU0828_BOARD_HAUPPAUGE_HVR850:
	case AU0828_BOARD_HAUPPAUGE_HVR950Q:
		/* GPIO's
		 * 4 - CS5340
		 * 5 - AU8522 Demodulator
		 * 6 - eeprom W/P
		 * 9 - XC5000 Tuner
		 */

		/* Into reset */
		au0828_write(dev, REG_003, 0x02);
		au0828_write(dev, REG_002, 0x88 | 0x20);
		au0828_write(dev, REG_001, 0x0);
		au0828_write(dev, REG_000, 0x0);
		msleep(100);

		/* Out of reset */
		au0828_write(dev, REG_003, 0x02);
		au0828_write(dev, REG_001, 0x02);
		au0828_write(dev, REG_002, 0x88 | 0x20);
		au0828_write(dev, REG_000, 0x88 | 0x20 | 0x40);
		msleep(250);
		break;
	case AU0828_BOARD_DVICO_FUSIONHDTV7:
		/* GPIO's
		 * 6 - ?
		 * 8 - AU8522 Demodulator
		 * 9 - XC5000 Tuner
		 */

		/* Into reset */
		au0828_write(dev, REG_003, 0x02);
		au0828_write(dev, REG_002, 0xa0);
		au0828_write(dev, REG_001, 0x0);
		au0828_write(dev, REG_000, 0x0);
		msleep(100);

		/* Out of reset */
		au0828_write(dev, REG_003, 0x02);
		au0828_write(dev, REG_002, 0xa0);
		au0828_write(dev, REG_001, 0x02);
		au0828_write(dev, REG_000, 0xa0);
		msleep(250);
		break;
	}
}

/* table of devices that work with this driver */
struct usb_device_id au0828_usb_id_table [] = {
	{ USB_DEVICE(0x2040, 0x7200),
		.driver_info = AU0828_BOARD_HAUPPAUGE_HVR950Q },
	{ USB_DEVICE(0x2040, 0x7240),
		.driver_info = AU0828_BOARD_HAUPPAUGE_HVR850 },
	{ USB_DEVICE(0x0fe9, 0xd620),
		.driver_info = AU0828_BOARD_DVICO_FUSIONHDTV7 },
	{ },
};

MODULE_DEVICE_TABLE(usb, au0828_usb_id_table);
