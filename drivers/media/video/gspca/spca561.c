/*
 * Sunplus spca561 subdriver
 *
 * Copyright (C) 2004 Michel Xhaard mxhaard@magic.fr
 *
 * V4L2 by Jean-Francois Moine <http://moinejf.free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define MODULE_NAME "spca561"

#include "gspca.h"

#define DRIVER_VERSION_NUMBER	KERNEL_VERSION(2, 1, 4)
static const char version[] = "2.1.4";

MODULE_AUTHOR("Michel Xhaard <mxhaard@users.sourceforge.net>");
MODULE_DESCRIPTION("GSPCA/SPCA561 USB Camera Driver");
MODULE_LICENSE("GPL");

/* specific webcam descriptor */
struct sd {
	struct gspca_dev gspca_dev;	/* !! must be the first item */

	unsigned short contrast;
	__u8 brightness;
	__u8 autogain;

	__u8 chip_revision;
	signed char ag_cnt;
#define AG_CNT_START 13
};

/* V4L2 controls supported by the driver */
static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightness(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setcontrast(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getcontrast(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val);

static struct ctrl sd_ctrls[] = {
#define SD_BRIGHTNESS 0
	{
	 {
	  .id = V4L2_CID_BRIGHTNESS,
	  .type = V4L2_CTRL_TYPE_INTEGER,
	  .name = "Brightness",
	  .minimum = 0,
	  .maximum = 63,
	  .step = 1,
	  .default_value = 32,
	  },
	 .set = sd_setbrightness,
	 .get = sd_getbrightness,
	 },
#define SD_CONTRAST 1
	{
	 {
	  .id = V4L2_CID_CONTRAST,
	  .type = V4L2_CTRL_TYPE_INTEGER,
	  .name = "Contrast",
	  .minimum = 0,
	  .maximum = 0x3fff,
	  .step = 1,
	  .default_value = 0x2000,
	  },
	 .set = sd_setcontrast,
	 .get = sd_getcontrast,
	 },
#define SD_AUTOGAIN 2
	{
	 {
	  .id = V4L2_CID_AUTOGAIN,
	  .type = V4L2_CTRL_TYPE_BOOLEAN,
	  .name = "Auto Gain",
	  .minimum = 0,
	  .maximum = 1,
	  .step = 1,
	  .default_value = 1,
	  },
	 .set = sd_setautogain,
	 .get = sd_getautogain,
	 },
};

static struct cam_mode sif_mode[] = {
	{V4L2_PIX_FMT_SGBRG8, 160, 120, 3},
	{V4L2_PIX_FMT_SGBRG8, 176, 144, 2},
	{V4L2_PIX_FMT_SPCA561, 320, 240, 1},
	{V4L2_PIX_FMT_SPCA561, 352, 288, 0},
};

/*
 * Initialization data
 * I'm not very sure how to split initialization from open data
 * chunks. For now, we'll consider everything as initialization
 */
/* Frame packet header offsets for the spca561 */
#define SPCA561_OFFSET_SNAP 1
#define SPCA561_OFFSET_TYPE 2
#define SPCA561_OFFSET_COMPRESS 3
#define SPCA561_OFFSET_FRAMSEQ   4
#define SPCA561_OFFSET_GPIO 5
#define SPCA561_OFFSET_USBBUFF 6
#define SPCA561_OFFSET_WIN2GRAVE 7
#define SPCA561_OFFSET_WIN2RAVE 8
#define SPCA561_OFFSET_WIN2BAVE 9
#define SPCA561_OFFSET_WIN2GBAVE 10
#define SPCA561_OFFSET_WIN1GRAVE 11
#define SPCA561_OFFSET_WIN1RAVE 12
#define SPCA561_OFFSET_WIN1BAVE 13
#define SPCA561_OFFSET_WIN1GBAVE 14
#define SPCA561_OFFSET_FREQ 15
#define SPCA561_OFFSET_VSYNC 16
#define SPCA561_OFFSET_DATA 1
#define SPCA561_INDEX_I2C_BASE 0x8800
#define SPCA561_SNAPBIT 0x20
#define SPCA561_SNAPCTRL 0x40
enum {
	Rev072A = 0,
	Rev012A,
};

static void reg_w_val(struct usb_device *dev, __u16 index, __u16 value)
{
	int ret;

	ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
			      0,		/* request */
			      USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      value, index, NULL, 0, 500);
	PDEBUG(D_USBO, "reg write: 0x%02x:0x%02x", index, value);
	if (ret < 0)
		PDEBUG(D_ERR, "reg write: error %d", ret);
}

static void write_vector(struct gspca_dev *gspca_dev,
			const __u16 data[][2])
{
	struct usb_device *dev = gspca_dev->dev;
	int i;

	i = 0;
	while (data[i][1] != 0) {
		reg_w_val(dev, data[i][1], data[i][0]);
		i++;
	}
}

static void reg_r(struct usb_device *dev,
		  __u16 index, __u8 *buffer, __u16 length)
{
	usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			0,			/* request */
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0,			/* value */
			index, buffer, length, 500);
}

static void reg_w_buf(struct usb_device *dev,
		      __u16 index, const __u8 *buffer, __u16 len)
{
	__u8 tmpbuf[8];

	memcpy(tmpbuf, buffer, len);
	usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
			0,			/* request */
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0,			/* value */
			index, tmpbuf, len, 500);
}

static void i2c_init(struct gspca_dev *gspca_dev, __u8 mode)
{
	reg_w_val(gspca_dev->dev, 0x92, 0x8804);
	reg_w_val(gspca_dev->dev, mode, 0x8802);
}

static void i2c_write(struct gspca_dev *gspca_dev, __u16 valeur, __u16 reg)
{
	int retry = 60;
	__u8 DataLow;
	__u8 DataHight;
	__u8 Data;

	DataLow = valeur;
	DataHight = valeur >> 8;
	reg_w_val(gspca_dev->dev, reg, 0x8801);
	reg_w_val(gspca_dev->dev, DataLow, 0x8805);
	reg_w_val(gspca_dev->dev, DataHight, 0x8800);
	while (retry--) {
		reg_r(gspca_dev->dev, 0x8803, &Data, 1);
		if (!Data)
			break;
	}
}

static int i2c_read(struct gspca_dev *gspca_dev, __u16 reg, __u8 mode)
{
	int retry = 60;
	__u8 value;
	__u8 vallsb;
	__u8 Data;

	reg_w_val(gspca_dev->dev, 0x92, 0x8804);
	reg_w_val(gspca_dev->dev, reg, 0x8801);
	reg_w_val(gspca_dev->dev, (mode | 0x01), 0x8802);
	while (retry--) {
		reg_r(gspca_dev->dev, 0x8803, &Data, 1);
		if (!Data)
			break;
	}
	if (retry == 0)
		return -1;
	reg_r(gspca_dev->dev, 0x8800, &value, 1);
	reg_r(gspca_dev->dev, 0x8805, &vallsb, 1);
	return ((int) value << 8) | vallsb;
}

static const __u16 spca561_init_data[][2] = {
	{0x0000, 0x8114},	/* Software GPIO output data */
	{0x0001, 0x8114},	/* Software GPIO output data */
	{0x0000, 0x8112},	/* Some kind of reset */
	{0x0003, 0x8701},	/* PCLK clock delay adjustment */
	{0x0001, 0x8703},	/* HSYNC from cmos inverted */
	{0x0011, 0x8118},	/* Enable and conf sensor */
	{0x0001, 0x8118},	/* Conf sensor */
	{0x0092, 0x8804},	/* I know nothing about these */
	{0x0010, 0x8802},	/* 0x88xx registers, so I won't */
	/***************/
	{0x000d, 0x8805},	/* sensor default setting */
	{0x0001, 0x8801},	/* 1 <- 0x0d */
	{0x0000, 0x8800},
	{0x0018, 0x8805},
	{0x0002, 0x8801},	/* 2 <- 0x18 */
	{0x0000, 0x8800},
	{0x0065, 0x8805},
	{0x0004, 0x8801},	/* 4 <- 0x01 0x65 */
	{0x0001, 0x8800},
	{0x0021, 0x8805},
	{0x0005, 0x8801},	/* 5 <- 0x21 */
	{0x0000, 0x8800},
	{0x00aa, 0x8805},
	{0x0007, 0x8801},	/* 7 <- 0xaa */
	{0x0000, 0x8800},
	{0x0004, 0x8805},
	{0x0020, 0x8801},	/* 0x20 <- 0x15 0x04 */
	{0x0015, 0x8800},
	{0x0002, 0x8805},
	{0x0039, 0x8801},	/* 0x39 <- 0x02 */
	{0x0000, 0x8800},
	{0x0010, 0x8805},
	{0x0035, 0x8801},	/* 0x35 <- 0x10 */
	{0x0000, 0x8800},
	{0x0049, 0x8805},
	{0x0009, 0x8801},	/* 0x09 <- 0x10 0x49 */
	{0x0010, 0x8800},
	{0x000b, 0x8805},
	{0x0028, 0x8801},	/* 0x28 <- 0x0b */
	{0x0000, 0x8800},
	{0x000f, 0x8805},
	{0x003b, 0x8801},	/* 0x3b <- 0x0f */
	{0x0000, 0x8800},
	{0x0000, 0x8805},
	{0x003c, 0x8801},	/* 0x3c <- 0x00 */
	{0x0000, 0x8800},
	/***************/
	{0x0018, 0x8601},	/* Pixel/line selection for color separation */
	{0x0000, 0x8602},	/* Optical black level for user setting */
	{0x0060, 0x8604},	/* Optical black horizontal offset */
	{0x0002, 0x8605},	/* Optical black vertical offset */
	{0x0000, 0x8603},	/* Non-automatic optical black level */
	{0x0002, 0x865b},	/* Horizontal offset for valid pixels */
	{0x0000, 0x865f},	/* Vertical valid pixels window (x2) */
	{0x00b0, 0x865d},	/* Horizontal valid pixels window (x2) */
	{0x0090, 0x865e},	/* Vertical valid lines window (x2) */
	{0x00e0, 0x8406},	/* Memory buffer threshold */
	{0x0000, 0x8660},	/* Compensation memory stuff */
	{0x0002, 0x8201},	/* Output address for r/w serial EEPROM */
	{0x0008, 0x8200},	/* Clear valid bit for serial EEPROM */
	{0x0001, 0x8200},	/* OprMode to be executed by hardware */
	{0x0007, 0x8201},	/* Output address for r/w serial EEPROM */
	{0x0008, 0x8200},	/* Clear valid bit for serial EEPROM */
	{0x0001, 0x8200},	/* OprMode to be executed by hardware */
	{0x0010, 0x8660},	/* Compensation memory stuff */
	{0x0018, 0x8660},	/* Compensation memory stuff */

	{0x0004, 0x8611},	/* R offset for white balance */
	{0x0004, 0x8612},	/* Gr offset for white balance */
	{0x0007, 0x8613},	/* B offset for white balance */
	{0x0000, 0x8614},	/* Gb offset for white balance */
	{0x008c, 0x8651},	/* R gain for white balance */
	{0x008c, 0x8652},	/* Gr gain for white balance */
	{0x00b5, 0x8653},	/* B gain for white balance */
	{0x008c, 0x8654},	/* Gb gain for white balance */
	{0x0002, 0x8502},	/* Maximum average bit rate stuff */

	{0x0011, 0x8802},
	{0x0087, 0x8700},	/* Set master clock (96Mhz????) */
	{0x0081, 0x8702},	/* Master clock output enable */

	{0x0000, 0x8500},	/* Set image type (352x288 no compression) */
	/* Originally was 0x0010 (352x288 compression) */

	{0x0002, 0x865b},	/* Horizontal offset for valid pixels */
	{0x0003, 0x865c},	/* Vertical offset for valid lines */
	/***************//* sensor active */
	{0x0003, 0x8801},	/* 0x03 <- 0x01 0x21 //289 */
	{0x0021, 0x8805},
	{0x0001, 0x8800},
	{0x0004, 0x8801},	/* 0x04 <- 0x01 0x65 //357 */
	{0x0065, 0x8805},
	{0x0001, 0x8800},
	{0x0005, 0x8801},	/* 0x05 <- 0x2f */
	{0x002f, 0x8805},
	{0x0000, 0x8800},
	{0x0006, 0x8801},	/* 0x06 <- 0 */
	{0x0000, 0x8805},
	{0x0000, 0x8800},
	{0x000a, 0x8801},	/* 0x0a <- 2 */
	{0x0002, 0x8805},
	{0x0000, 0x8800},
	{0x0009, 0x8801},	/* 0x09 <- 0x1061 */
	{0x0061, 0x8805},
	{0x0010, 0x8800},
	{0x0035, 0x8801},	/* 0x35 <-0x14 */
	{0x0014, 0x8805},
	{0x0000, 0x8800},
	{0x0030, 0x8112},	/* ISO and drop packet enable */
	{0x0000, 0x8112},	/* Some kind of reset ???? */
	{0x0009, 0x8118},	/* Enable sensor and set standby */
	{0x0000, 0x8114},	/* Software GPIO output data */
	{0x0000, 0x8114},	/* Software GPIO output data */
	{0x0001, 0x8114},	/* Software GPIO output data */
	{0x0000, 0x8112},	/* Some kind of reset ??? */
	{0x0003, 0x8701},
	{0x0001, 0x8703},
	{0x0011, 0x8118},
	{0x0001, 0x8118},
	/***************/
	{0x0092, 0x8804},
	{0x0010, 0x8802},
	{0x000d, 0x8805},
	{0x0001, 0x8801},
	{0x0000, 0x8800},
	{0x0018, 0x8805},
	{0x0002, 0x8801},
	{0x0000, 0x8800},
	{0x0065, 0x8805},
	{0x0004, 0x8801},
	{0x0001, 0x8800},
	{0x0021, 0x8805},
	{0x0005, 0x8801},
	{0x0000, 0x8800},
	{0x00aa, 0x8805},
	{0x0007, 0x8801},	/* mode 0xaa */
	{0x0000, 0x8800},
	{0x0004, 0x8805},
	{0x0020, 0x8801},
	{0x0015, 0x8800},	/* mode 0x0415 */
	{0x0002, 0x8805},
	{0x0039, 0x8801},
	{0x0000, 0x8800},
	{0x0010, 0x8805},
	{0x0035, 0x8801},
	{0x0000, 0x8800},
	{0x0049, 0x8805},
	{0x0009, 0x8801},
	{0x0010, 0x8800},
	{0x000b, 0x8805},
	{0x0028, 0x8801},
	{0x0000, 0x8800},
	{0x000f, 0x8805},
	{0x003b, 0x8801},
	{0x0000, 0x8800},
	{0x0000, 0x8805},
	{0x003c, 0x8801},
	{0x0000, 0x8800},
	{0x0002, 0x8502},
	{0x0039, 0x8801},
	{0x0000, 0x8805},
	{0x0000, 0x8800},

	{0x0087, 0x8700},	/* overwrite by start */
	{0x0081, 0x8702},
	{0x0000, 0x8500},
/*	{0x0010, 0x8500},  -- Previous line was this */
	{0x0002, 0x865b},
	{0x0003, 0x865c},
	/***************/
	{0x0003, 0x8801},	/* 0x121-> 289 */
	{0x0021, 0x8805},
	{0x0001, 0x8800},
	{0x0004, 0x8801},	/* 0x165 -> 357 */
	{0x0065, 0x8805},
	{0x0001, 0x8800},
	{0x0005, 0x8801},	/* 0x2f //blanking control colonne */
	{0x002f, 0x8805},
	{0x0000, 0x8800},
	{0x0006, 0x8801},	/* 0x00 //blanking mode row */
	{0x0000, 0x8805},
	{0x0000, 0x8800},
	{0x000a, 0x8801},	/* 0x01 //0x02 */
	{0x0001, 0x8805},
	{0x0000, 0x8800},
	{0x0009, 0x8801},	/* 0x1061 - setexposure times && pixel clock
				 * 0001 0 | 000 0110 0001 */
	{0x0061, 0x8805},	/* 61 31 */
	{0x0008, 0x8800},	/* 08 */
	{0x0035, 0x8801},	/* 0x14 - set gain general */
	{0x001f, 0x8805},	/* 0x14 */
	{0x0000, 0x8800},
	{0x0030, 0x8112},
	{}
};

static void sensor_reset(struct gspca_dev *gspca_dev)
{
	reg_w_val(gspca_dev->dev, 0x8631, 0xc8);
	reg_w_val(gspca_dev->dev, 0x8634, 0xc8);
	reg_w_val(gspca_dev->dev, 0x8112, 0x00);
	reg_w_val(gspca_dev->dev, 0x8114, 0x00);
	reg_w_val(gspca_dev->dev, 0x8118, 0x21);
	i2c_init(gspca_dev, 0x14);
	i2c_write(gspca_dev, 1, 0x0d);
	i2c_write(gspca_dev, 0, 0x0d);
}

/******************** QC Express etch2 stuff ********************/
static const __u16 Pb100_1map8300[][2] = {
	/* reg, value */
	{0x8320, 0x3304},

	{0x8303, 0x0125},	/* image area */
	{0x8304, 0x0169},
	{0x8328, 0x000b},
	{0x833c, 0x0001},

	{0x832f, 0x0419},
	{0x8307, 0x00aa},
	{0x8301, 0x0003},
	{0x8302, 0x000e},
	{}
};
static const __u16 Pb100_2map8300[][2] = {
	/* reg, value */
	{0x8339, 0x0000},
	{0x8307, 0x00aa},
	{}
};

static const __u16 spca561_161rev12A_data1[][2] = {
	{0x21, 0x8118},
	{0x01, 0x8114},
	{0x00, 0x8112},
	{0x92, 0x8804},
	{0x04, 0x8802},		/* windows uses 08 */
	{}
};
static const __u16 spca561_161rev12A_data2[][2] = {
	{0x21, 0x8118},
	{0x10, 0x8500},
	{0x07, 0x8601},
	{0x07, 0x8602},
	{0x04, 0x8501},
	{0x21, 0x8118},

	{0x07, 0x8201},		/* windows uses 02 */
	{0x08, 0x8200},
	{0x01, 0x8200},

	{0x00, 0x8114},
	{0x01, 0x8114},		/* windows uses 00 */

	{0x90, 0x8604},
	{0x00, 0x8605},
	{0xb0, 0x8603},

	/* sensor gains */
	{0x00, 0x8610},		/* *red */
	{0x00, 0x8611},		/* 3f   *green */
	{0x00, 0x8612},		/* green *blue */
	{0x00, 0x8613},		/* blue *green */
	{0x35, 0x8614},		/* green *red */
	{0x35, 0x8615},		/* 40   *green */
	{0x35, 0x8616},		/* 7a   *blue */
	{0x35, 0x8617},		/* 40   *green */

	{0x0c, 0x8620},		/* 0c */
	{0xc8, 0x8631},		/* c8 */
	{0xc8, 0x8634},		/* c8 */
	{0x23, 0x8635},		/* 23 */
	{0x1f, 0x8636},		/* 1f */
	{0xdd, 0x8637},		/* dd */
	{0xe1, 0x8638},		/* e1 */
	{0x1d, 0x8639},		/* 1d */
	{0x21, 0x863a},		/* 21 */
	{0xe3, 0x863b},		/* e3 */
	{0xdf, 0x863c},		/* df */
	{0xf0, 0x8505},
	{0x32, 0x850a},
	{}
};

static void sensor_mapwrite(struct gspca_dev *gspca_dev,
			    const __u16 sensormap[][2])
{
	int i = 0;
	__u8 usbval[2];

	while (sensormap[i][0]) {
		usbval[0] = sensormap[i][1];
		usbval[1] = sensormap[i][1] >> 8;
		reg_w_buf(gspca_dev->dev, sensormap[i][0], usbval, 2);
		i++;
	}
}
static void init_161rev12A(struct gspca_dev *gspca_dev)
{
	sensor_reset(gspca_dev);
	write_vector(gspca_dev, spca561_161rev12A_data1);
	sensor_mapwrite(gspca_dev, Pb100_1map8300);
	write_vector(gspca_dev, spca561_161rev12A_data2);
	sensor_mapwrite(gspca_dev, Pb100_2map8300);
}

/* this function is called at probe time */
static int sd_config(struct gspca_dev *gspca_dev,
		     const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct usb_device *dev = gspca_dev->dev;
	struct cam *cam;
	__u16 vendor, product;
	__u8 data1, data2;

	/* Read frm global register the USB product and vendor IDs, just to
	 * prove that we can communicate with the device.  This works, which
	 * confirms at we are communicating properly and that the device
	 * is a 561. */
	reg_r(dev, 0x8104, &data1, 1);
	reg_r(dev, 0x8105, &data2, 1);
	vendor = (data2 << 8) | data1;
	reg_r(dev, 0x8106, &data1, 1);
	reg_r(dev, 0x8107, &data2, 1);
	product = (data2 << 8) | data1;
	if (vendor != id->idVendor || product != id->idProduct) {
		PDEBUG(D_PROBE, "Bad vendor / product from device");
		return -EINVAL;
	}
	switch (product) {
	case 0x0928:
	case 0x0929:
	case 0x092a:
	case 0x092b:
	case 0x092c:
	case 0x092d:
	case 0x092e:
	case 0x092f:
	case 0x403b:
		sd->chip_revision = Rev012A;
		break;
	default:
/*	case 0x0561:
	case 0x0815:			* ?? in spca508.c
	case 0x401a:
	case 0x7004:
	case 0x7e50:
	case 0xa001:
	case 0xcdee: */
		sd->chip_revision = Rev072A;
		break;
	}
	cam = &gspca_dev->cam;
	cam->dev_name = (char *) id->driver_info;
	cam->epaddr = 0x01;
	gspca_dev->nbalt = 7 + 1;	/* choose alternate 7 first */
	cam->cam_mode = sif_mode;
	cam->nmodes = sizeof sif_mode / sizeof sif_mode[0];
	sd->brightness = sd_ctrls[SD_BRIGHTNESS].qctrl.default_value;
	sd->contrast = sd_ctrls[SD_CONTRAST].qctrl.default_value;
	sd->autogain = sd_ctrls[SD_AUTOGAIN].qctrl.default_value;
	return 0;
}

/* this function is called at open time */
static int sd_open(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (sd->chip_revision) {
	case Rev072A:
		PDEBUG(D_STREAM, "Chip revision id: 072a");
		write_vector(gspca_dev, spca561_init_data);
		break;
	default:
/*	case Rev012A: */
		PDEBUG(D_STREAM, "Chip revision id: 012a");
		init_161rev12A(gspca_dev);
		break;
	}
	return 0;
}

static void setcontrast(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct usb_device *dev = gspca_dev->dev;
	__u8 lowb;
	int expotimes;

	switch (sd->chip_revision) {
	case Rev072A:
		lowb = sd->contrast >> 8;
		reg_w_val(dev, lowb, 0x8651);
		reg_w_val(dev, lowb, 0x8652);
		reg_w_val(dev, lowb, 0x8653);
		reg_w_val(dev, lowb, 0x8654);
		break;
	case Rev012A: {
		__u8 Reg8391[] =
			{ 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00 };

		/* Write camera sensor settings */
		expotimes = (sd->contrast >> 5) & 0x07ff;
		Reg8391[0] = expotimes & 0xff;	/* exposure */
		Reg8391[1] = 0x18 | (expotimes >> 8);
		Reg8391[2] = sd->brightness;	/* gain */
		reg_w_buf(dev, 0x8391, Reg8391, 8);
		reg_w_buf(dev, 0x8390, Reg8391, 8);
		break;
	    }
	}
}

static void sd_start(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct usb_device *dev = gspca_dev->dev;
	int Clck;
	__u8 Reg8307[] = { 0xaa, 0x00 };
	int mode;

	mode = gspca_dev->cam.cam_mode[(int) gspca_dev->curr_mode].mode;
	switch (sd->chip_revision) {
	case Rev072A:
		switch (mode) {
		default:
/*		case 0:
		case 1: */
			Clck = 0x25;
			break;
		case 2:
			Clck = 0x22;
			break;
		case 3:
			Clck = 0x21;
			break;
		}
		reg_w_val(dev, 0x8500, mode);	/* mode */
		reg_w_val(dev, 0x8700, Clck);	/* 0x27 clock */
		reg_w_val(dev, 0x8112, 0x10 | 0x20);
		break;
	default:
/*	case Rev012A: */
		switch (mode) {
		case 0:
		case 1:
			Clck = 0x8a;
			break;
		case 2:
			Clck = 0x85;
			break;
		default:
			Clck = 0x83;
			break;
		}
		if (mode <= 1) {
			/* Use compression on 320x240 and above */
			reg_w_val(dev, 0x8500, 0x10 | mode);
		} else {
			/* I couldn't get the compression to work below 320x240
			 * Fortunately at these resolutions the bandwidth
			 * is sufficient to push raw frames at ~20fps */
			reg_w_val(dev, 0x8500, mode);
		}		/* -- qq@kuku.eu.org */
		reg_w_buf(dev, 0x8307, Reg8307, 2);
		reg_w_val(dev, 0x8700, Clck);	/* 0x8f 0x85 0x27 clock */
		reg_w_val(dev, 0x8112, 0x1e | 0x20);
		reg_w_val(dev, 0x850b, 0x03);
		setcontrast(gspca_dev);
		break;
	}
}

static void sd_stopN(struct gspca_dev *gspca_dev)
{
	reg_w_val(gspca_dev->dev, 0x8112, 0x20);
}

static void sd_stop0(struct gspca_dev *gspca_dev)
{
}

/* this function is called at close time */
static void sd_close(struct gspca_dev *gspca_dev)
{
	reg_w_val(gspca_dev->dev, 0x8114, 0);
}

static void setautogain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int expotimes = 0;
	int pixelclk = 0;
	int gainG = 0;
	__u8 R, Gr, Gb, B;
	int y;
	__u8 luma_mean = 110;
	__u8 luma_delta = 20;
	__u8 spring = 4;

	switch (sd->chip_revision) {
	case Rev072A:
		reg_r(gspca_dev->dev, 0x8621, &Gr, 1);
		reg_r(gspca_dev->dev, 0x8622, &R, 1);
		reg_r(gspca_dev->dev, 0x8623, &B, 1);
		reg_r(gspca_dev->dev, 0x8624, &Gb, 1);
		y = (77 * R + 75 * (Gr + Gb) + 29 * B) >> 8;
		/* u= (128*B-(43*(Gr+Gb+R))) >> 8; */
		/* v= (128*R-(53*(Gr+Gb))-21*B) >> 8; */
		/* PDEBUG(D_CONF,"reading Y %d U %d V %d ",y,u,v); */

		if (y < luma_mean - luma_delta ||
		    y > luma_mean + luma_delta) {
			expotimes = i2c_read(gspca_dev, 0x09, 0x10);
			pixelclk = 0x0800;
			expotimes = expotimes & 0x07ff;
			/* PDEBUG(D_PACK,
				"Exposition Times 0x%03X Clock 0x%04X ",
				expotimes,pixelclk); */
			gainG = i2c_read(gspca_dev, 0x35, 0x10);
			/* PDEBUG(D_PACK,
				"reading Gain register %d", gainG); */

			expotimes += (luma_mean - y) >> spring;
			gainG += (luma_mean - y) / 50;
			/* PDEBUG(D_PACK,
				"compute expotimes %d gain %d",
				expotimes,gainG); */

			if (gainG > 0x3f)
				gainG = 0x3f;
			else if (gainG < 4)
				gainG = 3;
			i2c_write(gspca_dev, gainG, 0x35);

			if (expotimes >= 0x0256)
				expotimes = 0x0256;
			else if (expotimes < 4)
				expotimes = 3;
			i2c_write(gspca_dev, expotimes | pixelclk, 0x09);
		}
		break;
	case Rev012A:
		/* sensor registers is access and memory mapped to 0x8300 */
		/* readind all 0x83xx block the sensor */
		/*
		 * The data from the header seem wrong where is the luma
		 * and chroma mean value
		 * at the moment set exposure in contrast set
		 */
		break;
	}
}

static void sd_pkt_scan(struct gspca_dev *gspca_dev,
			struct gspca_frame *frame, /* target */
			__u8 *data,		/* isoc packet */
			int len)		/* iso packet length */
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (data[0]) {
	case 0:		/* start of frame */
		frame = gspca_frame_add(gspca_dev, LAST_PACKET, frame,
					data, 0);
		if (sd->ag_cnt >= 0) {
			if (--sd->ag_cnt < 0) {
				sd->ag_cnt = AG_CNT_START;
				setautogain(gspca_dev);
			}
		}
		data += SPCA561_OFFSET_DATA;
		len -= SPCA561_OFFSET_DATA;
		if (data[1] & 0x10) {
			/* compressed bayer */
			gspca_frame_add(gspca_dev, FIRST_PACKET,
					frame, data, len);
		} else {
			/* raw bayer (with a header, which we skip) */
			data += 20;
			len -= 20;
			gspca_frame_add(gspca_dev, FIRST_PACKET,
						frame, data, len);
		}
		return;
	case 0xff:		/* drop */
/*		gspca_dev->last_packet_type = DISCARD_PACKET; */
		return;
	}
	data++;
	len--;
	gspca_frame_add(gspca_dev, INTER_PACKET, frame, data, len);
}

static void setbrightness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u8 value;

	switch (sd->chip_revision) {
	case Rev072A:
		value = sd->brightness;
		reg_w_val(gspca_dev->dev, value, 0x8611);
		reg_w_val(gspca_dev->dev, value, 0x8612);
		reg_w_val(gspca_dev->dev, value, 0x8613);
		reg_w_val(gspca_dev->dev, value, 0x8614);
		break;
	default:
/*	case Rev012A: */
		setcontrast(gspca_dev);
		break;
	}
}

static void getbrightness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u8 value;
	__u16 tot;

	switch (sd->chip_revision) {
	case Rev072A:
		tot = 0;
		reg_r(gspca_dev->dev, 0x8611, &value, 1);
		tot += value;
		reg_r(gspca_dev->dev, 0x8612, &value, 1);
		tot += value;
		reg_r(gspca_dev->dev, 0x8613, &value, 1);
		tot += value;
		reg_r(gspca_dev->dev, 0x8614, &value, 1);
		tot += value;
		sd->brightness = tot >> 2;
		break;
	default:
/*	case Rev012A: */
		/* no way to read sensor settings */
		break;
	}
}

static void getcontrast(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u8 value;
	__u16 tot;

	switch (sd->chip_revision) {
	case Rev072A:
		tot = 0;
		reg_r(gspca_dev->dev, 0x8651, &value, 1);
		tot += value;
		reg_r(gspca_dev->dev, 0x8652, &value, 1);
		tot += value;
		reg_r(gspca_dev->dev, 0x8653, &value, 1);
		tot += value;
		reg_r(gspca_dev->dev, 0x8654, &value, 1);
		tot += value;
		sd->contrast = tot << 6;
		break;
	default:
/*	case Rev012A: */
		/* no way to read sensor settings */
		break;
	}
	PDEBUG(D_CONF, "get contrast %d", sd->contrast);
}

static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->brightness = val;
	if (gspca_dev->streaming)
		setbrightness(gspca_dev);
	return 0;
}

static int sd_getbrightness(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	getbrightness(gspca_dev);
	*val = sd->brightness;
	return 0;
}

static int sd_setcontrast(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->contrast = val;
	if (gspca_dev->streaming)
		setcontrast(gspca_dev);
	return 0;
}

static int sd_getcontrast(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	getcontrast(gspca_dev);
	*val = sd->contrast;
	return 0;
}

static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->autogain = val;
	if (val)
		sd->ag_cnt = AG_CNT_START;
	else
		sd->ag_cnt = -1;
	return 0;
}

static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->autogain;
	return 0;
}

/* sub-driver description */
static const struct sd_desc sd_desc = {
	.name = MODULE_NAME,
	.ctrls = sd_ctrls,
	.nctrls = ARRAY_SIZE(sd_ctrls),
	.config = sd_config,
	.open = sd_open,
	.start = sd_start,
	.stopN = sd_stopN,
	.stop0 = sd_stop0,
	.close = sd_close,
	.pkt_scan = sd_pkt_scan,
};

/* -- module initialisation -- */
#define DVNM(name) .driver_info = (kernel_ulong_t) name
static const __devinitdata struct usb_device_id device_table[] = {
	{USB_DEVICE(0x041e, 0x401a), DVNM("Creative Webcam Vista (PD1100)")},
	{USB_DEVICE(0x041e, 0x403b),  DVNM("Creative Webcam Vista (VF0010)")},
	{USB_DEVICE(0x0458, 0x7004), DVNM("Genius VideoCAM Express V2")},
	{USB_DEVICE(0x046d, 0x0928), DVNM("Logitech QC Express Etch2")},
	{USB_DEVICE(0x046d, 0x0929), DVNM("Labtec Webcam Elch2")},
	{USB_DEVICE(0x046d, 0x092a), DVNM("Logitech QC for Notebook")},
	{USB_DEVICE(0x046d, 0x092b), DVNM("Labtec Webcam Plus")},
	{USB_DEVICE(0x046d, 0x092c), DVNM("Logitech QC chat Elch2")},
	{USB_DEVICE(0x046d, 0x092d), DVNM("Logitech QC Elch2")},
	{USB_DEVICE(0x046d, 0x092e), DVNM("Logitech QC Elch2")},
	{USB_DEVICE(0x046d, 0x092f), DVNM("Logitech QC Elch2")},
	{USB_DEVICE(0x04fc, 0x0561), DVNM("Flexcam 100")},
	{USB_DEVICE(0x060b, 0xa001), DVNM("Maxell Compact Pc PM3")},
	{USB_DEVICE(0x10fd, 0x7e50), DVNM("FlyCam Usb 100")},
	{USB_DEVICE(0xabcd, 0xcdee), DVNM("Petcam")},
	{}
};

MODULE_DEVICE_TABLE(usb, device_table);

/* -- device connect -- */
static int sd_probe(struct usb_interface *intf,
		    const struct usb_device_id *id)
{
	return gspca_dev_probe(intf, id, &sd_desc, sizeof(struct sd),
			       THIS_MODULE);
}

static struct usb_driver sd_driver = {
	.name = MODULE_NAME,
	.id_table = device_table,
	.probe = sd_probe,
	.disconnect = gspca_disconnect,
};

/* -- module insert / remove -- */
static int __init sd_mod_init(void)
{
	if (usb_register(&sd_driver) < 0)
		return -1;
	PDEBUG(D_PROBE, "v%s registered", version);
	return 0;
}
static void __exit sd_mod_exit(void)
{
	usb_deregister(&sd_driver);
	PDEBUG(D_PROBE, "deregistered");
}

module_init(sd_mod_init);
module_exit(sd_mod_exit);
