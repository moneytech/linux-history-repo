/*
 *  bt819 - BT819A VideoStream Decoder (Rockwell Part)
 *
 * Copyright (C) 1999 Mike Bernson <mike@mlb.org>
 * Copyright (C) 1998 Dave Perks <dperks@ibm.net>
 *
 * Modifications for LML33/DC10plus unified driver
 * Copyright (C) 2000 Serguei Miridonov <mirsev@cicese.mx>
 *
 * Changes by Ronald Bultje <rbultje@ronald.bitfreak.net>
 *    - moved over to linux>=2.4.x i2c protocol (9/9/2002)
 *
 * This code was modify/ported from the saa7111 driver written
 * by Dave Perks.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/videodev.h>
#include <linux/video_decoder.h>
#include <media/v4l2-common.h>
#include <media/v4l2-i2c-drv-legacy.h>

MODULE_DESCRIPTION("Brooktree-819 video decoder driver");
MODULE_AUTHOR("Mike Bernson & Dave Perks");
MODULE_LICENSE("GPL");

static int debug;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

/* ----------------------------------------------------------------------- */

struct bt819 {
	unsigned char reg[32];

	int initialized;
	v4l2_std_id norm;
	int input;
	int enable;
	int bright;
	int contrast;
	int hue;
	int sat;
};

struct timing {
	int hactive;
	int hdelay;
	int vactive;
	int vdelay;
	int hscale;
	int vscale;
};

/* for values, see the bt819 datasheet */
static struct timing timing_data[] = {
	{864 - 24, 20, 625 - 2, 1, 0x0504, 0x0000},
	{858 - 24, 20, 525 - 2, 1, 0x00f8, 0x0000},
};

/* ----------------------------------------------------------------------- */

static inline int bt819_write(struct i2c_client *client, u8 reg, u8 value)
{
	struct bt819 *decoder = i2c_get_clientdata(client);

	decoder->reg[reg] = value;
	return i2c_smbus_write_byte_data(client, reg, value);
}

static inline int bt819_setbit(struct i2c_client *client, u8 reg, u8 bit, u8 value)
{
	struct bt819 *decoder = i2c_get_clientdata(client);

	return bt819_write(client, reg,
		(decoder->reg[reg] & ~(1 << bit)) | (value ? (1 << bit) : 0));
}

static int bt819_write_block(struct i2c_client *client, const u8 *data, unsigned int len)
{
	int ret = -1;
	u8 reg;

	/* the bt819 has an autoincrement function, use it if
	 * the adapter understands raw I2C */
	if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		/* do raw I2C, not smbus compatible */
		struct bt819 *decoder = i2c_get_clientdata(client);
		u8 block_data[32];
		int block_len;

		while (len >= 2) {
			block_len = 0;
			block_data[block_len++] = reg = data[0];
			do {
				block_data[block_len++] =
				    decoder->reg[reg++] = data[1];
				len -= 2;
				data += 2;
			} while (len >= 2 && data[0] == reg && block_len < 32);
			ret = i2c_master_send(client, block_data, block_len);
			if (ret < 0)
				break;
		}
	} else {
		/* do some slow I2C emulation kind of thing */
		while (len >= 2) {
			reg = *data++;
			if ((ret = bt819_write(client, reg, *data++)) < 0)
				break;
			len -= 2;
		}
	}

	return ret;
}

static inline int bt819_read(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int bt819_init(struct i2c_client *client)
{
	struct bt819 *decoder = i2c_get_clientdata(client);

	static unsigned char init[] = {
		/*0x1f, 0x00,*/     /* Reset */
		0x01, 0x59,	/* 0x01 input format */
		0x02, 0x00,	/* 0x02 temporal decimation */
		0x03, 0x12,	/* 0x03 Cropping msb */
		0x04, 0x16,	/* 0x04 Vertical Delay, lsb */
		0x05, 0xe0,	/* 0x05 Vertical Active lsb */
		0x06, 0x80,	/* 0x06 Horizontal Delay lsb */
		0x07, 0xd0,	/* 0x07 Horizontal Active lsb */
		0x08, 0x00,	/* 0x08 Horizontal Scaling msb */
		0x09, 0xf8,	/* 0x09 Horizontal Scaling lsb */
		0x0a, 0x00,	/* 0x0a Brightness control */
		0x0b, 0x30,	/* 0x0b Miscellaneous control */
		0x0c, 0xd8,	/* 0x0c Luma Gain lsb */
		0x0d, 0xfe,	/* 0x0d Chroma Gain (U) lsb */
		0x0e, 0xb4,	/* 0x0e Chroma Gain (V) msb */
		0x0f, 0x00,	/* 0x0f Hue control */
		0x12, 0x04,	/* 0x12 Output Format */
		0x13, 0x20,	/* 0x13 Vertial Scaling msb 0x00
					   chroma comb OFF, line drop scaling, interlace scaling
					   BUG? Why does turning the chroma comb on fuck up color?
					   Bug in the bt819 stepping on my board?
					*/
		0x14, 0x00,	/* 0x14 Vertial Scaling lsb */
		0x16, 0x07,	/* 0x16 Video Timing Polarity
					   ACTIVE=active low
					   FIELD: high=odd,
					   vreset=active high,
					   hreset=active high */
		0x18, 0x68,	/* 0x18 AGC Delay */
		0x19, 0x5d,	/* 0x19 Burst Gate Delay */
		0x1a, 0x80,	/* 0x1a ADC Interface */
	};

	struct timing *timing = &timing_data[(decoder->norm & V4L2_STD_525_60) ? 1 : 0];

	init[0x03 * 2 - 1] =
	    (((timing->vdelay >> 8) & 0x03) << 6) |
	    (((timing->vactive >> 8) & 0x03) << 4) |
	    (((timing->hdelay >> 8) & 0x03) << 2) |
	    ((timing->hactive >> 8) & 0x03);
	init[0x04 * 2 - 1] = timing->vdelay & 0xff;
	init[0x05 * 2 - 1] = timing->vactive & 0xff;
	init[0x06 * 2 - 1] = timing->hdelay & 0xff;
	init[0x07 * 2 - 1] = timing->hactive & 0xff;
	init[0x08 * 2 - 1] = timing->hscale >> 8;
	init[0x09 * 2 - 1] = timing->hscale & 0xff;
	/* 0x15 in array is address 0x19 */
	init[0x15 * 2 - 1] = (decoder->norm & V4L2_STD_625_50) ? 115 : 93;	/* Chroma burst delay */
	/* reset */
	bt819_write(client, 0x1f, 0x00);
	mdelay(1);

	/* init */
	return bt819_write_block(client, init, sizeof(init));
}

/* ----------------------------------------------------------------------- */

static int bt819_command(struct i2c_client *client, unsigned cmd, void *arg)
{
	int temp;

	struct bt819 *decoder = i2c_get_clientdata(client);

	if (!decoder->initialized) {	/* First call to bt819_init could be */
		bt819_init(client);	/* without #FRST = 0 */
		decoder->initialized = 1;
	}

	switch (cmd) {
	case VIDIOC_INT_INIT:
		/* This is just for testing!!! */
		bt819_init(client);
		break;

	case VIDIOC_QUERYSTD:
	case VIDIOC_INT_G_INPUT_STATUS: {
		int *iarg = arg;
		v4l2_std_id *istd = arg;
		int status;
		int res = V4L2_IN_ST_NO_SIGNAL;
		v4l2_std_id std;

		status = bt819_read(client, 0x00);
		if ((status & 0x80))
			res = 0;

		if ((status & 0x10))
			std = V4L2_STD_PAL;
		else
			std = V4L2_STD_NTSC;
		if (cmd == VIDIOC_QUERYSTD)
			*istd = std;
		else
			*iarg = res;

		v4l_dbg(1, debug, client, "get status %x\n", *iarg);
		break;
	}

	case VIDIOC_S_STD:
	{
		v4l2_std_id *iarg = arg;
		struct timing *timing = NULL;

		v4l_dbg(1, debug, client, "set norm %llx\n", *iarg);

		if (*iarg & V4L2_STD_NTSC) {
			bt819_setbit(client, 0x01, 0, 1);
			bt819_setbit(client, 0x01, 1, 0);
			bt819_setbit(client, 0x01, 5, 0);
			bt819_write(client, 0x18, 0x68);
			bt819_write(client, 0x19, 0x5d);
			/* bt819_setbit(client, 0x1a,  5, 1); */
			timing = &timing_data[1];
		} else if (*iarg & V4L2_STD_PAL) {
			bt819_setbit(client, 0x01, 0, 1);
			bt819_setbit(client, 0x01, 1, 1);
			bt819_setbit(client, 0x01, 5, 1);
			bt819_write(client, 0x18, 0x7f);
			bt819_write(client, 0x19, 0x72);
			/* bt819_setbit(client, 0x1a,  5, 0); */
			timing = &timing_data[0];
		} else {
			v4l_dbg(1, debug, client, "unsupported norm %llx\n", *iarg);
			return -EINVAL;
		}
/*		case VIDEO_MODE_AUTO:
			bt819_setbit(client, 0x01, 0, 0);
			bt819_setbit(client, 0x01, 1, 0);*/

		bt819_write(client, 0x03,
			    (((timing->vdelay >> 8) & 0x03) << 6) |
			    (((timing->vactive >> 8) & 0x03) << 4) |
			    (((timing->hdelay >> 8) & 0x03) << 2) |
			     ((timing->hactive >> 8) & 0x03));
		bt819_write(client, 0x04, timing->vdelay & 0xff);
		bt819_write(client, 0x05, timing->vactive & 0xff);
		bt819_write(client, 0x06, timing->hdelay & 0xff);
		bt819_write(client, 0x07, timing->hactive & 0xff);
		bt819_write(client, 0x08, (timing->hscale >> 8) & 0xff);
		bt819_write(client, 0x09, timing->hscale & 0xff);
		decoder->norm = *iarg;
		break;
	}

	case VIDIOC_INT_S_VIDEO_ROUTING:
	{
		struct v4l2_routing *route = arg;

		v4l_dbg(1, debug, client, "set input %x\n", route->input);

		if (route->input < 0 || route->input > 7)
			return -EINVAL;

		if (decoder->input != route->input) {
			decoder->input = route->input;
			/* select mode */
			if (decoder->input == 0) {
				bt819_setbit(client, 0x0b, 6, 0);
				bt819_setbit(client, 0x1a, 1, 1);
			} else {
				bt819_setbit(client, 0x0b, 6, 1);
				bt819_setbit(client, 0x1a, 1, 0);
			}
		}
		break;
	}

	case VIDIOC_STREAMON:
	case VIDIOC_STREAMOFF:
	{
		int enable = cmd == VIDIOC_STREAMON;

		v4l_dbg(1, debug, client, "enable output %x\n", enable);

		if (decoder->enable != enable) {
			decoder->enable = enable;
			bt819_setbit(client, 0x16, 7, !enable);
		}
		break;
	}

	case VIDIOC_QUERYCTRL:
	{
		struct v4l2_queryctrl *qc = arg;

		switch (qc->id) {
		case V4L2_CID_BRIGHTNESS:
			v4l2_ctrl_query_fill(qc, -128, 127, 1, 0);
			break;

		case V4L2_CID_CONTRAST:
			v4l2_ctrl_query_fill(qc, 0, 511, 1, 256);
			break;

		case V4L2_CID_SATURATION:
			v4l2_ctrl_query_fill(qc, 0, 511, 1, 256);
			break;

		case V4L2_CID_HUE:
			v4l2_ctrl_query_fill(qc, -128, 127, 1, 0);
			break;

		default:
			return -EINVAL;
		}
		break;
	}

	case VIDIOC_S_CTRL:
	{
		struct v4l2_control *ctrl = arg;

		switch (ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			if (decoder->bright != ctrl->value) {
				decoder->bright = ctrl->value;
				bt819_write(client, 0x0a, decoder->bright);
			}
			break;

		case V4L2_CID_CONTRAST:
			if (decoder->contrast != ctrl->value) {
				decoder->contrast = ctrl->value;
				bt819_write(client, 0x0c,
						decoder->contrast & 0xff);
				bt819_setbit(client, 0x0b, 2,
						((decoder->contrast >> 8) & 0x01));
			}
			break;

		case V4L2_CID_SATURATION:
			if (decoder->sat != ctrl->value) {
				decoder->sat = ctrl->value;
				bt819_write(client, 0x0d,
						(decoder->sat >> 7) & 0xff);
				bt819_setbit(client, 0x0b, 1,
						((decoder->sat >> 15) & 0x01));

				/* Ratio between U gain and V gain must stay the same as
				   the ratio between the default U and V gain values. */
				temp = (decoder->sat * 180) / 254;
				bt819_write(client, 0x0e, (temp >> 7) & 0xff);
				bt819_setbit(client, 0x0b, 0, (temp >> 15) & 0x01);
			}
			break;

		case V4L2_CID_HUE:
			if (decoder->hue != ctrl->value) {
				decoder->hue = ctrl->value;
				bt819_write(client, 0x0f, decoder->hue);
			}
			break;
		default:
			return -EINVAL;
		}
		break;
	}

	case VIDIOC_G_CTRL:
	{
		struct v4l2_control *ctrl = arg;

		switch (ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			ctrl->value = decoder->bright;
			break;
		case V4L2_CID_CONTRAST:
			ctrl->value = decoder->contrast;
			break;
		case V4L2_CID_SATURATION:
			ctrl->value = decoder->sat;
			break;
		case V4L2_CID_HUE:
			ctrl->value = decoder->hue;
			break;
		default:
			return -EINVAL;
		}
		break;
	}

	default:
		return -EINVAL;
	}

	return 0;
}

/* ----------------------------------------------------------------------- */

static unsigned short normal_i2c[] = { 0x8a >> 1, I2C_CLIENT_END };

I2C_CLIENT_INSMOD;

static int bt819_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int i, ver;
	struct bt819 *decoder;
	const char *name;

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	ver = bt819_read(client, 0x17);
	switch (ver & 0xf0) {
	case 0x70:
		name = "bt819a";
		break;
	case 0x60:
		name = "bt817a";
		break;
	case 0x20:
		name = "bt815a";
		break;
	default:
		v4l_dbg(1, debug, client,
			"unknown chip version 0x%02x\n", ver);
		return -ENODEV;
	}

	v4l_info(client, "%s found @ 0x%x (%s)\n", name,
			client->addr << 1, client->adapter->name);

	decoder = kzalloc(sizeof(struct bt819), GFP_KERNEL);
	if (decoder == NULL)
		return -ENOMEM;
	decoder->norm = V4L2_STD_NTSC;
	decoder->input = 0;
	decoder->enable = 1;
	decoder->bright = 0;
	decoder->contrast = 0xd8;	/* 100% of original signal */
	decoder->hue = 0;
	decoder->sat = 0xfe;	/* 100% of original signal */
	decoder->initialized = 0;
	i2c_set_clientdata(client, decoder);

	i = bt819_init(client);
	if (i < 0)
		v4l_dbg(1, debug, client, "init status %d\n", i);
	return 0;
}

static int bt819_remove(struct i2c_client *client)
{
	kfree(i2c_get_clientdata(client));
	return 0;
}

/* ----------------------------------------------------------------------- */

static const struct i2c_device_id bt819_id[] = {
	{ "bt819a", 0 },
	{ "bt817a", 0 },
	{ "bt815a", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bt819_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = "bt819",
	.driverid = I2C_DRIVERID_BT819,
	.command = bt819_command,
	.probe = bt819_probe,
	.remove = bt819_remove,
	.id_table = bt819_id,
};
