/*
 * Renesas System Solutions Asia Pte. Ltd - Migo-R
 *
 * Copyright (C) 2008 Magnus Damm
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/nand.h>
#include <linux/i2c.h>
#include <linux/smc91x.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <video/sh_mobile_lcdc.h>
#include <media/sh_mobile_ceu.h>
#include <media/ov772x.h>
#include <media/tw9910.h>
#include <asm/clock.h>
#include <asm/machvec.h>
#include <asm/io.h>
#include <asm/sh_keysc.h>
#include <mach/migor.h>
#include <cpu/sh7722.h>

/* Address     IRQ  Size  Bus  Description
 * 0x00000000       64MB  16   NOR Flash (SP29PL256N)
 * 0x0c000000       64MB  64   SDRAM (2xK4M563233G)
 * 0x10000000  IRQ0       16   Ethernet (SMC91C111)
 * 0x14000000  IRQ4       16   USB 2.0 Host Controller (M66596)
 * 0x18000000       8GB    8   NAND Flash (K9K8G08U0A)
 */

static struct smc91x_platdata smc91x_info = {
	.flags = SMC91X_USE_16BIT | SMC91X_NOWAIT,
};

static struct resource smc91x_eth_resources[] = {
	[0] = {
		.name   = "SMC91C111" ,
		.start  = 0x10000300,
		.end    = 0x1000030f,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = 32, /* IRQ0 */
		.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct platform_device smc91x_eth_device = {
	.name           = "smc91x",
	.num_resources  = ARRAY_SIZE(smc91x_eth_resources),
	.resource       = smc91x_eth_resources,
	.dev	= {
		.platform_data	= &smc91x_info,
	},
};

static struct sh_keysc_info sh_keysc_info = {
	.mode = SH_KEYSC_MODE_2, /* KEYOUT0->4, KEYIN1->5 */
	.scan_timing = 3,
	.delay = 5,
	.keycodes = {
		0, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER,
		0, KEY_F, KEY_C, KEY_D,	KEY_H, KEY_1,
		0, KEY_2, KEY_3, KEY_4,	KEY_5, KEY_6,
		0, KEY_7, KEY_8, KEY_9, KEY_S, KEY_0,
		0, KEY_P, KEY_STOP, KEY_REWIND, KEY_PLAY, KEY_FASTFORWARD,
	},
};

static struct resource sh_keysc_resources[] = {
	[0] = {
		.start  = 0x044b0000,
		.end    = 0x044b000f,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = 79,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device sh_keysc_device = {
	.name           = "sh_keysc",
	.id             = 0, /* "keysc0" clock */
	.num_resources  = ARRAY_SIZE(sh_keysc_resources),
	.resource       = sh_keysc_resources,
	.dev	= {
		.platform_data	= &sh_keysc_info,
	},
};

static struct mtd_partition migor_nor_flash_partitions[] =
{
	{
		.name = "uboot",
		.offset = 0,
		.size = (1 * 1024 * 1024),
		.mask_flags = MTD_WRITEABLE,	/* Read-only */
	},
	{
		.name = "rootfs",
		.offset = MTDPART_OFS_APPEND,
		.size = (15 * 1024 * 1024),
	},
	{
		.name = "other",
		.offset = MTDPART_OFS_APPEND,
		.size = MTDPART_SIZ_FULL,
	},
};

static struct physmap_flash_data migor_nor_flash_data = {
	.width		= 2,
	.parts		= migor_nor_flash_partitions,
	.nr_parts	= ARRAY_SIZE(migor_nor_flash_partitions),
};

static struct resource migor_nor_flash_resources[] = {
	[0] = {
		.name		= "NOR Flash",
		.start		= 0x00000000,
		.end		= 0x03ffffff,
		.flags		= IORESOURCE_MEM,
	}
};

static struct platform_device migor_nor_flash_device = {
	.name		= "physmap-flash",
	.resource	= migor_nor_flash_resources,
	.num_resources	= ARRAY_SIZE(migor_nor_flash_resources),
	.dev		= {
		.platform_data = &migor_nor_flash_data,
	},
};

static struct mtd_partition migor_nand_flash_partitions[] = {
	{
		.name		= "nanddata1",
		.offset		= 0x0,
		.size		= 512 * 1024 * 1024,
	},
	{
		.name		= "nanddata2",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 512 * 1024 * 1024,
	},
};

static void migor_nand_flash_cmd_ctl(struct mtd_info *mtd, int cmd,
				     unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;

	if (cmd == NAND_CMD_NONE)
		return;

	if (ctrl & NAND_CLE)
		writeb(cmd, chip->IO_ADDR_W + 0x00400000);
	else if (ctrl & NAND_ALE)
		writeb(cmd, chip->IO_ADDR_W + 0x00800000);
	else
		writeb(cmd, chip->IO_ADDR_W);
}

static int migor_nand_flash_ready(struct mtd_info *mtd)
{
	return gpio_get_value(GPIO_PTA1); /* NAND_RBn */
}

struct platform_nand_data migor_nand_flash_data = {
	.chip = {
		.nr_chips = 1,
		.partitions = migor_nand_flash_partitions,
		.nr_partitions = ARRAY_SIZE(migor_nand_flash_partitions),
		.chip_delay = 20,
		.part_probe_types = (const char *[]) { "cmdlinepart", NULL },
	},
	.ctrl = {
		.dev_ready = migor_nand_flash_ready,
		.cmd_ctrl = migor_nand_flash_cmd_ctl,
	},
};

static struct resource migor_nand_flash_resources[] = {
	[0] = {
		.name		= "NAND Flash",
		.start		= 0x18000000,
		.end		= 0x18ffffff,
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device migor_nand_flash_device = {
	.name		= "gen_nand",
	.resource	= migor_nand_flash_resources,
	.num_resources	= ARRAY_SIZE(migor_nand_flash_resources),
	.dev		= {
		.platform_data = &migor_nand_flash_data,
	}
};

static struct sh_mobile_lcdc_info sh_mobile_lcdc_info = {
#ifdef CONFIG_SH_MIGOR_RTA_WVGA
	.clock_source = LCDC_CLK_BUS,
	.ch[0] = {
		.chan = LCDC_CHAN_MAINLCD,
		.bpp = 16,
		.interface_type = RGB16,
		.clock_divider = 2,
		.lcd_cfg = {
			.name = "LB070WV1",
			.xres = 800,
			.yres = 480,
			.left_margin = 64,
			.right_margin = 16,
			.hsync_len = 120,
			.upper_margin = 1,
			.lower_margin = 17,
			.vsync_len = 2,
			.sync = 0,
		},
		.lcd_size_cfg = { /* 7.0 inch */
			.width = 152,
			.height = 91,
		},
	}
#endif
#ifdef CONFIG_SH_MIGOR_QVGA
	.clock_source = LCDC_CLK_PERIPHERAL,
	.ch[0] = {
		.chan = LCDC_CHAN_MAINLCD,
		.bpp = 16,
		.interface_type = SYS16A,
		.clock_divider = 10,
		.lcd_cfg = {
			.name = "PH240320T",
			.xres = 320,
			.yres = 240,
			.left_margin = 0,
			.right_margin = 16,
			.hsync_len = 8,
			.upper_margin = 1,
			.lower_margin = 17,
			.vsync_len = 2,
			.sync = FB_SYNC_HOR_HIGH_ACT,
		},
		.lcd_size_cfg = { /* 2.4 inch */
			.width = 49,
			.height = 37,
		},
		.board_cfg = {
			.setup_sys = migor_lcd_qvga_setup,
		},
		.sys_bus_cfg = {
			.ldmt2r = 0x06000a09,
			.ldmt3r = 0x180e3418,
			/* set 1s delay to encourage fsync() */
			.deferred_io_msec = 1000,
		},
	}
#endif
};

static struct resource migor_lcdc_resources[] = {
	[0] = {
		.name	= "LCDC",
		.start	= 0xfe940000, /* P4-only space */
		.end	= 0xfe941fff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 28,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device migor_lcdc_device = {
	.name		= "sh_mobile_lcdc_fb",
	.num_resources	= ARRAY_SIZE(migor_lcdc_resources),
	.resource	= migor_lcdc_resources,
	.dev	= {
		.platform_data	= &sh_mobile_lcdc_info,
	},
};

static struct clk *camera_clk;
static DEFINE_MUTEX(camera_lock);

static void camera_power_on(int is_tw)
{
	mutex_lock(&camera_lock);

	/* Use 10 MHz VIO_CKO instead of 24 MHz to work
	 * around signal quality issues on Panel Board V2.1.
	 */
	camera_clk = clk_get(NULL, "video_clk");
	clk_set_rate(camera_clk, 10000000);
	clk_enable(camera_clk);	/* start VIO_CKO */

	/* use VIO_RST to take camera out of reset */
	mdelay(10);
	if (is_tw) {
		gpio_set_value(GPIO_PTT2, 0);
		gpio_set_value(GPIO_PTT0, 0);
	} else {
		gpio_set_value(GPIO_PTT0, 1);
	}
	gpio_set_value(GPIO_PTT3, 0);
	mdelay(10);
	gpio_set_value(GPIO_PTT3, 1);
	mdelay(10); /* wait to let chip come out of reset */
}

static void camera_power_off(void)
{
	clk_disable(camera_clk); /* stop VIO_CKO */
	clk_put(camera_clk);

	gpio_set_value(GPIO_PTT3, 0);
	mutex_unlock(&camera_lock);
}

static int ov7725_power(struct device *dev, int mode)
{
	if (mode)
		camera_power_on(0);
	else
		camera_power_off();

	return 0;
}

static int tw9910_power(struct device *dev, int mode)
{
	if (mode)
		camera_power_on(1);
	else
		camera_power_off();

	return 0;
}

static struct sh_mobile_ceu_info sh_mobile_ceu_info = {
	.flags = SH_CEU_FLAG_USE_8BIT_BUS,
};

static struct resource migor_ceu_resources[] = {
	[0] = {
		.name	= "CEU",
		.start	= 0xfe910000,
		.end	= 0xfe91009f,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start  = 52,
		.flags  = IORESOURCE_IRQ,
	},
	[2] = {
		/* place holder for contiguous memory */
	},
};

static struct platform_device migor_ceu_device = {
	.name		= "sh_mobile_ceu",
	.id             = 0, /* "ceu0" clock */
	.num_resources	= ARRAY_SIZE(migor_ceu_resources),
	.resource	= migor_ceu_resources,
	.dev	= {
		.platform_data	= &sh_mobile_ceu_info,
	},
};

static struct ov772x_camera_info ov7725_info = {
	.buswidth  = SOCAM_DATAWIDTH_8,
	.link = {
		.power  = ov7725_power,
	},
};

static struct tw9910_video_info tw9910_info = {
	.buswidth = SOCAM_DATAWIDTH_8,
	.mpout    = TW9910_MPO_FIELD,
	.link = {
		.power  = tw9910_power,
	}
};

struct spi_gpio_platform_data sdcard_cn9_platform_data = {
	.sck = GPIO_PTD0,
	.mosi = GPIO_PTD1,
	.miso = GPIO_PTD2,
	.num_chipselect = 1,
};

static struct platform_device sdcard_cn9_device = {
	.name		= "spi_gpio",
	.dev	= {
		.platform_data	= &sdcard_cn9_platform_data,
	},
};

static struct platform_device *migor_devices[] __initdata = {
	&smc91x_eth_device,
	&sh_keysc_device,
	&migor_lcdc_device,
	&migor_ceu_device,
	&migor_nor_flash_device,
	&migor_nand_flash_device,
	&sdcard_cn9_device,
};

static struct i2c_board_info migor_i2c_devices[] = {
	{
		I2C_BOARD_INFO("rs5c372b", 0x32),
	},
	{
		I2C_BOARD_INFO("migor_ts", 0x51),
		.irq = 38, /* IRQ6 */
	},
	{
		I2C_BOARD_INFO("ov772x", 0x21),
		.platform_data = &ov7725_info,
	},
	{
		I2C_BOARD_INFO("tw9910", 0x45),
		.platform_data = &tw9910_info,
	},
};

static struct spi_board_info migor_spi_devices[] = {
	{
		.modalias = "mmc_spi",
		.max_speed_hz = 5000000,
		.chip_select = 0,
		.controller_data = (void *) GPIO_PTD5,
	},
};

static int __init migor_devices_setup(void)
{

#ifdef CONFIG_PM
	/* Let D11 LED show STATUS0 */
	gpio_request(GPIO_FN_STATUS0, NULL);

	/* Lit D12 LED show PDSTATUS */
	gpio_request(GPIO_FN_PDSTATUS, NULL);
#else
	/* Lit D11 LED */
	gpio_request(GPIO_PTJ7, NULL);
	gpio_direction_output(GPIO_PTJ7, 1);
	gpio_export(GPIO_PTJ7, 0);

	/* Lit D12 LED */
	gpio_request(GPIO_PTJ5, NULL);
	gpio_direction_output(GPIO_PTJ5, 1);
	gpio_export(GPIO_PTJ5, 0);
#endif

	/* SMC91C111 - Enable IRQ0, Setup CS4 for 16-bit fast access */
	gpio_request(GPIO_FN_IRQ0, NULL);
	ctrl_outl(0x00003400, BSC_CS4BCR);
	ctrl_outl(0x00110080, BSC_CS4WCR);

	/* KEYSC */
	gpio_request(GPIO_FN_KEYOUT0, NULL);
	gpio_request(GPIO_FN_KEYOUT1, NULL);
	gpio_request(GPIO_FN_KEYOUT2, NULL);
	gpio_request(GPIO_FN_KEYOUT3, NULL);
	gpio_request(GPIO_FN_KEYOUT4_IN6, NULL);
	gpio_request(GPIO_FN_KEYIN1, NULL);
	gpio_request(GPIO_FN_KEYIN2, NULL);
	gpio_request(GPIO_FN_KEYIN3, NULL);
	gpio_request(GPIO_FN_KEYIN4, NULL);
	gpio_request(GPIO_FN_KEYOUT5_IN5, NULL);

	/* NAND Flash */
	gpio_request(GPIO_FN_CS6A_CE2B, NULL);
	ctrl_outl((ctrl_inl(BSC_CS6ABCR) & ~0x0600) | 0x0200, BSC_CS6ABCR);
	gpio_request(GPIO_PTA1, NULL);
	gpio_direction_input(GPIO_PTA1);

	/* Touch Panel */
	gpio_request(GPIO_FN_IRQ6, NULL);

	/* LCD Panel */
#ifdef CONFIG_SH_MIGOR_QVGA /* LCDC - QVGA - Enable SYS Interface signals */
	gpio_request(GPIO_FN_LCDD17, NULL);
	gpio_request(GPIO_FN_LCDD16, NULL);
	gpio_request(GPIO_FN_LCDD15, NULL);
	gpio_request(GPIO_FN_LCDD14, NULL);
	gpio_request(GPIO_FN_LCDD13, NULL);
	gpio_request(GPIO_FN_LCDD12, NULL);
	gpio_request(GPIO_FN_LCDD11, NULL);
	gpio_request(GPIO_FN_LCDD10, NULL);
	gpio_request(GPIO_FN_LCDD8, NULL);
	gpio_request(GPIO_FN_LCDD7, NULL);
	gpio_request(GPIO_FN_LCDD6, NULL);
	gpio_request(GPIO_FN_LCDD5, NULL);
	gpio_request(GPIO_FN_LCDD4, NULL);
	gpio_request(GPIO_FN_LCDD3, NULL);
	gpio_request(GPIO_FN_LCDD2, NULL);
	gpio_request(GPIO_FN_LCDD1, NULL);
	gpio_request(GPIO_FN_LCDRS, NULL);
	gpio_request(GPIO_FN_LCDCS, NULL);
	gpio_request(GPIO_FN_LCDRD, NULL);
	gpio_request(GPIO_FN_LCDWR, NULL);
	gpio_request(GPIO_PTH2, NULL); /* LCD_DON */
	gpio_direction_output(GPIO_PTH2, 1);
#endif
#ifdef CONFIG_SH_MIGOR_RTA_WVGA /* LCDC - WVGA - Enable RGB Interface signals */
	gpio_request(GPIO_FN_LCDD15, NULL);
	gpio_request(GPIO_FN_LCDD14, NULL);
	gpio_request(GPIO_FN_LCDD13, NULL);
	gpio_request(GPIO_FN_LCDD12, NULL);
	gpio_request(GPIO_FN_LCDD11, NULL);
	gpio_request(GPIO_FN_LCDD10, NULL);
	gpio_request(GPIO_FN_LCDD9, NULL);
	gpio_request(GPIO_FN_LCDD8, NULL);
	gpio_request(GPIO_FN_LCDD7, NULL);
	gpio_request(GPIO_FN_LCDD6, NULL);
	gpio_request(GPIO_FN_LCDD5, NULL);
	gpio_request(GPIO_FN_LCDD4, NULL);
	gpio_request(GPIO_FN_LCDD3, NULL);
	gpio_request(GPIO_FN_LCDD2, NULL);
	gpio_request(GPIO_FN_LCDD1, NULL);
	gpio_request(GPIO_FN_LCDD0, NULL);
	gpio_request(GPIO_FN_LCDLCLK, NULL);
	gpio_request(GPIO_FN_LCDDCK, NULL);
	gpio_request(GPIO_FN_LCDVEPWC, NULL);
	gpio_request(GPIO_FN_LCDVCPWC, NULL);
	gpio_request(GPIO_FN_LCDVSYN, NULL);
	gpio_request(GPIO_FN_LCDHSYN, NULL);
	gpio_request(GPIO_FN_LCDDISP, NULL);
	gpio_request(GPIO_FN_LCDDON, NULL);
#endif

	/* CEU */
	gpio_request(GPIO_FN_VIO_CLK2, NULL);
	gpio_request(GPIO_FN_VIO_VD2, NULL);
	gpio_request(GPIO_FN_VIO_HD2, NULL);
	gpio_request(GPIO_FN_VIO_FLD, NULL);
	gpio_request(GPIO_FN_VIO_CKO, NULL);
	gpio_request(GPIO_FN_VIO_D15, NULL);
	gpio_request(GPIO_FN_VIO_D14, NULL);
	gpio_request(GPIO_FN_VIO_D13, NULL);
	gpio_request(GPIO_FN_VIO_D12, NULL);
	gpio_request(GPIO_FN_VIO_D11, NULL);
	gpio_request(GPIO_FN_VIO_D10, NULL);
	gpio_request(GPIO_FN_VIO_D9, NULL);
	gpio_request(GPIO_FN_VIO_D8, NULL);

	gpio_request(GPIO_PTT3, NULL); /* VIO_RST */
	gpio_direction_output(GPIO_PTT3, 0);
	gpio_request(GPIO_PTT2, NULL); /* TV_IN_EN */
	gpio_direction_output(GPIO_PTT2, 1);
	gpio_request(GPIO_PTT0, NULL); /* CAM_EN */
#ifdef CONFIG_SH_MIGOR_RTA_WVGA
	gpio_direction_output(GPIO_PTT0, 0);
#else
	gpio_direction_output(GPIO_PTT0, 1);
#endif
	ctrl_outw(ctrl_inw(PORT_MSELCRB) | 0x2000, PORT_MSELCRB); /* D15->D8 */

	platform_resource_setup_memory(&migor_ceu_device, "ceu", 4 << 20);

	i2c_register_board_info(0, migor_i2c_devices,
				ARRAY_SIZE(migor_i2c_devices));

	spi_register_board_info(migor_spi_devices,
				ARRAY_SIZE(migor_spi_devices));

	return platform_add_devices(migor_devices, ARRAY_SIZE(migor_devices));
}
__initcall(migor_devices_setup);
