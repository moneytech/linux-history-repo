/*
 * USB Driver for ALi m5602 based webcams
 *
 * Copyright (C) 2008 Erik Andrén
 * Copyright (C) 2007 Ilyes Gouta. Based on the m5603x Linux Driver Project.
 * Copyright (C) 2005 m5603x Linux Driver Project <m5602@x3ng.com.br>
 *
 * Portions of code to USB interface and ALi driver software,
 * Copyright (c) 2006 Willem Duinker
 * v4l2 interface modeled after the V4L2 driver
 * for SN9C10x PC Camera Controllers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 */

#ifndef M5602_BRIDGE_H_
#define M5602_BRIDGE_H_

#include "gspca.h"

#define MODULE_NAME "ALi m5602"

/*****************************************************************************/

#undef PDEBUG
#undef info
#undef err

#define err(format, arg...) printk(KERN_ERR KBUILD_MODNAME ": " \
	format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO KBUILD_MODNAME ": " \
	format "\n" , ## arg)

/* Debug parameters */
#define DBG_INIT 0x1
#define DBG_PROBE 0x2
#define DBG_V4L2 0x4
#define DBG_TRACE 0x8
#define DBG_DATA 0x10
#define DBG_V4L2_CID 0x20
#define DBG_GSPCA 0x40

#define PDEBUG(level, fmt, args...) \
	do { \
		if (m5602_debug & level)     \
			info("[%s:%d] " fmt, __func__, __LINE__ , \
			## args); \
	} while (0)

/*****************************************************************************/

#define M5602_XB_SENSOR_TYPE 0x00
#define M5602_XB_SENSOR_CTRL 0x01
#define M5602_XB_LINE_OF_FRAME_H 0x02
#define M5602_XB_LINE_OF_FRAME_L 0x03
#define M5602_XB_PIX_OF_LINE_H 0x04
#define M5602_XB_PIX_OF_LINE_L 0x05
#define M5602_XB_VSYNC_PARA 0x06
#define M5602_XB_HSYNC_PARA 0x07
#define M5602_XB_TEST_MODE_1 0x08
#define M5602_XB_TEST_MODE_2 0x09
#define M5602_XB_SIG_INI 0x0a
#define M5602_XB_DS_PARA 0x0e
#define M5602_XB_TRIG_PARA 0x0f
#define M5602_XB_CLK_PD 0x10
#define M5602_XB_MCU_CLK_CTRL 0x12
#define M5602_XB_MCU_CLK_DIV 0x13
#define M5602_XB_SEN_CLK_CTRL 0x14
#define M5602_XB_SEN_CLK_DIV 0x15
#define M5602_XB_AUD_CLK_CTRL 0x16
#define M5602_XB_AUD_CLK_DIV 0x17
#define M5602_XB_DEVCTR1 0x41
#define M5602_XB_EPSETR0 0x42
#define M5602_XB_EPAFCTR 0x47
#define M5602_XB_EPBFCTR 0x49
#define M5602_XB_EPEFCTR 0x4f
#define M5602_XB_TEST_REG 0x53
#define M5602_XB_ALT2SIZE 0x54
#define M5602_XB_ALT3SIZE 0x55
#define M5602_XB_OBSFRAME 0x56
#define M5602_XB_PWR_CTL 0x59
#define M5602_XB_ADC_CTRL 0x60
#define M5602_XB_ADC_DATA 0x61
#define M5602_XB_MISC_CTRL 0x62
#define M5602_XB_SNAPSHOT 0x63
#define M5602_XB_SCRATCH_1 0x64
#define M5602_XB_SCRATCH_2 0x65
#define M5602_XB_SCRATCH_3 0x66
#define M5602_XB_SCRATCH_4 0x67
#define M5602_XB_I2C_CTRL 0x68
#define M5602_XB_I2C_CLK_DIV 0x69
#define M5602_XB_I2C_DEV_ADDR 0x6a
#define M5602_XB_I2C_REG_ADDR 0x6b
#define M5602_XB_I2C_DATA 0x6c
#define M5602_XB_I2C_STATUS 0x6d
#define M5602_XB_GPIO_DAT_H 0x70
#define M5602_XB_GPIO_DAT_L 0x71
#define M5602_XB_GPIO_DIR_H 0x72
#define M5602_XB_GPIO_DIR_L 0x73
#define M5602_XB_GPIO_EN_H 0x74
#define M5602_XB_GPIO_EN_L 0x75
#define M5602_XB_GPIO_DAT 0x76
#define M5602_XB_GPIO_DIR 0x77
#define M5602_XB_MISC_CTL 0x70

#define I2C_BUSY 0x80

/*****************************************************************************/

/* Driver info */
#define DRIVER_AUTHOR "ALi m5602 Linux Driver Project"
#define DRIVER_DESC "ALi m5602 webcam driver"

#define M5602_ISOC_ENDPOINT_ADDR 0x81
#define M5602_INTR_ENDPOINT_ADDR 0x82

#define M5602_MAX_FRAMES	32
#define M5602_URBS		2
#define M5602_ISOC_PACKETS	14

#define M5602_URB_TIMEOUT	msecs_to_jiffies(2 * M5602_ISOC_PACKETS)
#define M5602_URB_MSG_TIMEOUT   5000
#define M5602_FRAME_TIMEOUT	2

/*****************************************************************************/

/* A skeleton used for sending messages to the m5602 bridge */
static const unsigned char bridge_urb_skeleton[] = {
	0x13, 0x00, 0x81, 0x00
};

/* A skeleton used for sending messages to the sensor */
static const unsigned char sensor_urb_skeleton[] = {
	0x23, M5602_XB_GPIO_EN_H, 0x81, 0x06,
	0x23, M5602_XB_MISC_CTRL, 0x81, 0x80,
	0x13, M5602_XB_I2C_DEV_ADDR, 0x81, 0x00,
	0x13, M5602_XB_I2C_REG_ADDR, 0x81, 0x00,
	0x13, M5602_XB_I2C_DATA, 0x81, 0x00,
	0x13, M5602_XB_I2C_CTRL, 0x81, 0x11
};

/* m5602 device descriptor, currently it just wraps the m5602_camera struct */
struct sd {
	struct gspca_dev gspca_dev;

	/* The name of the m5602 camera */
	char *name;

	/* A pointer to the currently connected sensor */
	struct m5602_sensor *sensor;

	struct sd_desc *desc;

	/* The current frame's id, used to detect frame boundaries */
	u8 frame_id;

	/* The current frame count */
	u32 frame_count;
};

int m5602_read_bridge(
	struct sd *sd, u8 address, u8 *i2c_data);

int m5602_write_bridge(
	struct sd *sd, u8 address, u8 i2c_data);

#endif
