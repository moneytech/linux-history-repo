/*
 * linux/arch/arm/mach-pxa/poodle.c
 *
 *  Support for the SHARP Poodle Board.
 *
 * Based on:
 *  linux/arch/arm/mach-pxa/lubbock.c Author:	Nicolas Pitre
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 * Change Log
 *  12-Dec-2002 Sharp Corporation for Poodle
 *  John Lenz <lenz@cs.wisc.edu> updates to 2.6
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/mtd/physmap.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/ads7846.h>
#include <linux/mtd/sharpsl.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/system.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/pxa25x.h>
#include <mach/mmc.h>
#include <mach/udc.h>
#include <mach/i2c.h>
#include <mach/irda.h>
#include <mach/poodle.h>
#include <mach/pxafb.h>
#include <mach/sharpsl.h>
#include <mach/ssp.h>
#include <mach/pxa2xx_spi.h>

#include <asm/hardware/scoop.h>
#include <asm/hardware/locomo.h>
#include <asm/mach/sharpsl_param.h>

#include "generic.h"
#include "devices.h"
#include "sharpsl.h"

static unsigned long poodle_pin_config[] __initdata = {
	/* I/O */
	GPIO79_nCS_3,
	GPIO80_nCS_4,
	GPIO18_RDY,

	/* Clock */
	GPIO12_32KHz,

	/* SSP1 */
	GPIO23_SSP1_SCLK,
	GPIO25_SSP1_TXD,
	GPIO26_SSP1_RXD,
	GPIO24_GPIO,	/* POODLE_GPIO_TP_CS - SFRM as chip select */

	/* I2S */
	GPIO28_I2S_BITCLK_OUT,
	GPIO29_I2S_SDATA_IN,
	GPIO30_I2S_SDATA_OUT,
	GPIO31_I2S_SYNC,
	GPIO32_I2S_SYSCLK,

	/* Infra-Red */
	GPIO47_FICP_TXD,
	GPIO46_FICP_RXD,

	/* FFUART */
	GPIO40_FFUART_DTR,
	GPIO41_FFUART_RTS,
	GPIO39_FFUART_TXD,
	GPIO37_FFUART_DSR,
	GPIO34_FFUART_RXD,
	GPIO35_FFUART_CTS,

	/* LCD */
	GPIO58_LCD_LDD_0,
	GPIO59_LCD_LDD_1,
	GPIO60_LCD_LDD_2,
	GPIO61_LCD_LDD_3,
	GPIO62_LCD_LDD_4,
	GPIO63_LCD_LDD_5,
	GPIO64_LCD_LDD_6,
	GPIO65_LCD_LDD_7,
	GPIO66_LCD_LDD_8,
	GPIO67_LCD_LDD_9,
	GPIO68_LCD_LDD_10,
	GPIO69_LCD_LDD_11,
	GPIO70_LCD_LDD_12,
	GPIO71_LCD_LDD_13,
	GPIO72_LCD_LDD_14,
	GPIO73_LCD_LDD_15,
	GPIO74_LCD_FCLK,
	GPIO75_LCD_LCLK,
	GPIO76_LCD_PCLK,
	GPIO77_LCD_BIAS,

	/* PC Card */
	GPIO48_nPOE,
	GPIO49_nPWE,
	GPIO50_nPIOR,
	GPIO51_nPIOW,
	GPIO52_nPCE_1,
	GPIO53_nPCE_2,
	GPIO54_nPSKTSEL,
	GPIO55_nPREG,
	GPIO56_nPWAIT,
	GPIO57_nIOIS16,

	/* MMC */
	GPIO6_MMC_CLK,
	GPIO8_MMC_CS0,

	/* GPIO */
	GPIO9_GPIO,	/* POODLE_GPIO_nSD_DETECT */
	GPIO7_GPIO,	/* POODLE_GPIO_nSD_WP */
	GPIO3_GPIO,	/* POODLE_GPIO_SD_PWR */
	GPIO33_GPIO,	/* POODLE_GPIO_SD_PWR1 */

	GPIO20_GPIO,	/* POODLE_GPIO_USB_PULLUP */
	GPIO22_GPIO,	/* POODLE_GPIO_IR_ON */
};

static struct resource poodle_scoop_resources[] = {
	[0] = {
		.start		= 0x10800000,
		.end		= 0x10800fff,
		.flags		= IORESOURCE_MEM,
	},
};

static struct scoop_config poodle_scoop_setup = {
	.io_dir		= POODLE_SCOOP_IO_DIR,
	.io_out		= POODLE_SCOOP_IO_OUT,
	.gpio_base	= POODLE_SCOOP_GPIO_BASE,
};

struct platform_device poodle_scoop_device = {
	.name		= "sharp-scoop",
	.id		= -1,
	.dev		= {
		.platform_data	= &poodle_scoop_setup,
	},
	.num_resources	= ARRAY_SIZE(poodle_scoop_resources),
	.resource	= poodle_scoop_resources,
};

static struct scoop_pcmcia_dev poodle_pcmcia_scoop[] = {
{
	.dev        = &poodle_scoop_device.dev,
	.irq        = POODLE_IRQ_GPIO_CF_IRQ,
	.cd_irq     = POODLE_IRQ_GPIO_CF_CD,
	.cd_irq_str = "PCMCIA0 CD",
},
};

static struct scoop_pcmcia_config poodle_pcmcia_config = {
	.devs         = &poodle_pcmcia_scoop[0],
	.num_devs     = 1,
};

EXPORT_SYMBOL(poodle_scoop_device);


/* LoCoMo device */
static struct resource locomo_resources[] = {
	[0] = {
		.start		= 0x10000000,
		.end		= 0x10001fff,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= IRQ_GPIO(10),
		.end		= IRQ_GPIO(10),
		.flags		= IORESOURCE_IRQ,
	},
};

struct platform_device poodle_locomo_device = {
	.name		= "locomo",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(locomo_resources),
	.resource	= locomo_resources,
};

EXPORT_SYMBOL(poodle_locomo_device);

#if defined(CONFIG_SPI_PXA2XX) || defined(CONFIG_SPI_PXA2XX_MODULE)
static struct pxa2xx_spi_master poodle_spi_info = {
	.num_chipselect	= 1,
};

static struct ads7846_platform_data poodle_ads7846_info = {
	.model			= 7846,
	.vref_delay_usecs	= 100,
	.x_plate_ohms		= 419,
	.y_plate_ohms		= 486,
	.gpio_pendown		= POODLE_GPIO_TP_INT,
};

static void ads7846_cs(u32 command)
{
	gpio_set_value(POODLE_GPIO_TP_CS, !(command == PXA2XX_CS_ASSERT));
}

static struct pxa2xx_spi_chip poodle_ads7846_chip = {
	.cs_control		= ads7846_cs,
};

static struct spi_board_info poodle_spi_devices[] = {
	{
		.modalias	= "ads7846",
		.max_speed_hz	= 10000,
		.bus_num	= 1,
		.platform_data	= &poodle_ads7846_info,
		.controller_data= &poodle_ads7846_chip,
		.irq		= gpio_to_irq(POODLE_GPIO_TP_INT),
	},
};

static void __init poodle_init_spi(void)
{
	int err;

	err = gpio_request(POODLE_GPIO_TP_CS, "ADS7846_CS");
	if (err)
		return;

	gpio_direction_output(POODLE_GPIO_TP_CS, 1);

	pxa2xx_set_spi_info(1, &poodle_spi_info);
	spi_register_board_info(ARRAY_AND_SIZE(poodle_spi_devices));
}
#else
static inline void poodle_init_spi(void) {}
#endif

/*
 * MMC/SD Device
 *
 * The card detect interrupt isn't debounced so we delay it by 250ms
 * to give the card a chance to fully insert/eject.
 */
static struct pxamci_platform_data poodle_mci_platform_data;

static int poodle_mci_init(struct device *dev, irq_handler_t poodle_detect_int, void *data)
{
	int err;

	err = gpio_request(POODLE_GPIO_nSD_DETECT, "nSD_DETECT");
	if (err)
		goto err_out;

	err = gpio_request(POODLE_GPIO_nSD_WP, "nSD_WP");
	if (err)
		goto err_free_1;

	err = gpio_request(POODLE_GPIO_SD_PWR, "SD_PWR");
	if (err)
		goto err_free_2;

	err = gpio_request(POODLE_GPIO_SD_PWR1, "SD_PWR1");
	if (err)
		goto err_free_3;

	gpio_direction_input(POODLE_GPIO_nSD_DETECT);
	gpio_direction_input(POODLE_GPIO_nSD_WP);

	gpio_direction_output(POODLE_GPIO_SD_PWR, 0);
	gpio_direction_output(POODLE_GPIO_SD_PWR1, 0);

	poodle_mci_platform_data.detect_delay = msecs_to_jiffies(250);

	err = request_irq(POODLE_IRQ_GPIO_nSD_DETECT, poodle_detect_int,
			  IRQF_DISABLED | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			  "MMC card detect", data);
	if (err) {
		pr_err("%s: MMC/SD: can't request MMC card detect IRQ\n",
				__func__);
		goto err_free_4;
	}

	return 0;

err_free_4:
	gpio_free(POODLE_GPIO_SD_PWR1);
err_free_3:
	gpio_free(POODLE_GPIO_SD_PWR);
err_free_2:
	gpio_free(POODLE_GPIO_nSD_WP);
err_free_1:
	gpio_free(POODLE_GPIO_nSD_DETECT);
err_out:
	return err;
}

static void poodle_mci_setpower(struct device *dev, unsigned int vdd)
{
	struct pxamci_platform_data* p_d = dev->platform_data;

	if ((1 << vdd) & p_d->ocr_mask) {
		gpio_set_value(POODLE_GPIO_SD_PWR, 1);
		mdelay(2);
		gpio_set_value(POODLE_GPIO_SD_PWR1, 1);
	} else {
		gpio_set_value(POODLE_GPIO_SD_PWR1, 0);
		gpio_set_value(POODLE_GPIO_SD_PWR, 0);
	}
}

static int poodle_mci_get_ro(struct device *dev)
{
	return !!gpio_get_value(POODLE_GPIO_nSD_WP);
	return GPLR(POODLE_GPIO_nSD_WP) & GPIO_bit(POODLE_GPIO_nSD_WP);
}


static void poodle_mci_exit(struct device *dev, void *data)
{
	free_irq(POODLE_IRQ_GPIO_nSD_DETECT, data);
	gpio_free(POODLE_GPIO_SD_PWR1);
	gpio_free(POODLE_GPIO_SD_PWR);
	gpio_free(POODLE_GPIO_nSD_WP);
	gpio_free(POODLE_GPIO_nSD_DETECT);
}

static struct pxamci_platform_data poodle_mci_platform_data = {
	.ocr_mask	= MMC_VDD_32_33|MMC_VDD_33_34,
	.init 		= poodle_mci_init,
	.get_ro		= poodle_mci_get_ro,
	.setpower 	= poodle_mci_setpower,
	.exit		= poodle_mci_exit,
};


/*
 * Irda
 */
static void poodle_irda_transceiver_mode(struct device *dev, int mode)
{
	gpio_set_value(POODLE_GPIO_IR_ON, mode & IR_OFF);
	pxa2xx_transceiver_mode(dev, mode);
}

static int poodle_irda_startup(struct device *dev)
{
	int err;

	err = gpio_request(POODLE_GPIO_IR_ON, "IR_ON");
	if (err)
		return err;

	gpio_direction_output(POODLE_GPIO_IR_ON, 1);
	return 0;
}

static void poodle_irda_shutdown(struct device *dev)
{
	gpio_free(POODLE_GPIO_IR_ON);
}

static struct pxaficp_platform_data poodle_ficp_platform_data = {
	.transceiver_cap	= IR_SIRMODE | IR_OFF,
	.transceiver_mode	= poodle_irda_transceiver_mode,
	.startup		= poodle_irda_startup,
	.shutdown		= poodle_irda_shutdown,
};


/*
 * USB Device Controller
 */
static struct pxa2xx_udc_mach_info udc_info __initdata = {
	/* no connect GPIO; poodle can't tell connection status */
	.gpio_pullup	= POODLE_GPIO_USB_PULLUP,
};


/* PXAFB device */
static struct pxafb_mode_info poodle_fb_mode = {
	.pixclock	= 144700,
	.xres		= 320,
	.yres		= 240,
	.bpp		= 16,
	.hsync_len	= 7,
	.left_margin	= 11,
	.right_margin	= 30,
	.vsync_len	= 2,
	.upper_margin	= 2,
	.lower_margin	= 0,
	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
};

static struct pxafb_mach_info poodle_fb_info = {
	.modes		= &poodle_fb_mode,
	.num_modes	= 1,
	.lcd_conn	= LCD_COLOR_TFT_16BPP,
};

static struct mtd_partition sharpsl_nand_partitions[] = {
	{
		.name = "System Area",
		.offset = 0,
		.size = 7 * 1024 * 1024,
	},
	{
		.name = "Root Filesystem",
		.offset = 7 * 1024 * 1024,
		.size = 22 * 1024 * 1024,
	},
	{
		.name = "Home Filesystem",
		.offset = MTDPART_OFS_APPEND,
		.size = MTDPART_SIZ_FULL,
	},
};

static uint8_t scan_ff_pattern[] = { 0xff, 0xff };

static struct nand_bbt_descr sharpsl_bbt = {
	.options = 0,
	.offs = 4,
	.len = 2,
	.pattern = scan_ff_pattern
};

static struct sharpsl_nand_platform_data sharpsl_nand_platform_data = {
	.badblock_pattern	= &sharpsl_bbt,
	.partitions		= sharpsl_nand_partitions,
	.nr_partitions		= ARRAY_SIZE(sharpsl_nand_partitions),
};

static struct resource sharpsl_nand_resources[] = {
	{
		.start	= 0x0C000000,
		.end	= 0x0C000FFF,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device sharpsl_nand_device = {
	.name		= "sharpsl-nand",
	.id		= -1,
	.resource	= sharpsl_nand_resources,
	.num_resources	= ARRAY_SIZE(sharpsl_nand_resources),
	.dev.platform_data	= &sharpsl_nand_platform_data,
};

static struct mtd_partition sharpsl_rom_parts[] = {
	{
		.name	="Boot PROM Filesystem",
		.offset	= 0x00120000,
		.size	= MTDPART_SIZ_FULL,
	},
};

static struct physmap_flash_data sharpsl_rom_data = {
	.width		= 2,
	.nr_parts	= ARRAY_SIZE(sharpsl_rom_parts),
	.parts		= sharpsl_rom_parts,
};

static struct resource sharpsl_rom_resources[] = {
	{
		.start	= 0x00000000,
		.end	= 0x007fffff,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device sharpsl_rom_device = {
	.name	= "physmap-flash",
	.id	= -1,
	.resource = sharpsl_rom_resources,
	.num_resources = ARRAY_SIZE(sharpsl_rom_resources),
	.dev.platform_data = &sharpsl_rom_data,
};

static struct platform_device *devices[] __initdata = {
	&poodle_locomo_device,
	&poodle_scoop_device,
	&sharpsl_nand_device,
	&sharpsl_rom_device,
};

static void poodle_poweroff(void)
{
	arm_machine_restart('h');
}

static void poodle_restart(char mode)
{
	arm_machine_restart('h');
}

static void __init poodle_init(void)
{
	int ret = 0;

	pm_power_off = poodle_poweroff;
	arm_pm_restart = poodle_restart;

	PCFR |= PCFR_OPDE;

	pxa2xx_mfp_config(ARRAY_AND_SIZE(poodle_pin_config));

	platform_scoop_config = &poodle_pcmcia_config;

	ret = platform_add_devices(devices, ARRAY_SIZE(devices));
	if (ret)
		pr_warning("poodle: Unable to register LoCoMo device\n");

	set_pxa_fb_parent(&poodle_locomo_device.dev);
	set_pxa_fb_info(&poodle_fb_info);
	pxa_set_udc_info(&udc_info);
	pxa_set_mci_info(&poodle_mci_platform_data);
	pxa_set_ficp_info(&poodle_ficp_platform_data);
	pxa_set_i2c_info(NULL);
	poodle_init_spi();
}

static void __init fixup_poodle(struct machine_desc *desc,
		struct tag *tags, char **cmdline, struct meminfo *mi)
{
	sharpsl_save_param();
	mi->nr_banks=1;
	mi->bank[0].start = 0xa0000000;
	mi->bank[0].node = 0;
	mi->bank[0].size = (32*1024*1024);
}

MACHINE_START(POODLE, "SHARP Poodle")
	.phys_io	= 0x40000000,
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.fixup		= fixup_poodle,
	.map_io		= pxa_map_io,
	.init_irq	= pxa25x_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= poodle_init,
MACHINE_END
