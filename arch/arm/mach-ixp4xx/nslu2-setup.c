/*
 * arch/arm/mach-ixp4xx/nslu2-setup.c
 *
 * NSLU2 board-setup
 *
 * based ixdp425-setup.c:
 *      Copyright (C) 2003-2004 MontaVista Software, Inc.
 *
 * Author: Mark Rakes <mrakes at mac.com>
 * Author: Rod Whitby <rod@whitby.id.au>
 * Maintainers: http://www.nslu2-linux.org/
 *
 * Fixed missing init_time in MACHINE_START kas11 10/22/04
 * Changed to conform to new style __init ixdp425 kas11 10/22/04
 */

#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>
#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/time.h>
#include <asm/io.h>

static struct flash_platform_data nslu2_flash_data = {
	.map_name		= "cfi_probe",
	.width			= 2,
};

static struct resource nslu2_flash_resource = {
	.flags			= IORESOURCE_MEM,
};

static struct platform_device nslu2_flash = {
	.name			= "IXP4XX-Flash",
	.id			= 0,
	.dev.platform_data	= &nslu2_flash_data,
	.num_resources		= 1,
	.resource		= &nslu2_flash_resource,
};

static struct i2c_gpio_platform_data nslu2_i2c_gpio_data = {
	.sda_pin		= NSLU2_SDA_PIN,
	.scl_pin		= NSLU2_SCL_PIN,
};

static struct i2c_board_info __initdata nslu2_i2c_board_info [] = {
	{
		I2C_BOARD_INFO("rtc-x1205", 0x6f),
	},
};

static struct gpio_led nslu2_led_pins[] = {
	{
		.name		= "ready",  /* green led */
		.gpio		= NSLU2_LED_GRN_GPIO,
	},
	{
		.name		= "status", /* red led */
		.gpio		= NSLU2_LED_RED_GPIO,
	},
	{
		.name		= "disk-1",
		.gpio		= NSLU2_LED_DISK1_GPIO,
		.active_low	= true,
	},
	{
		.name		= "disk-2",
		.gpio		= NSLU2_LED_DISK2_GPIO,
		.active_low	= true,
	},
};

static struct gpio_led_platform_data nslu2_led_data = {
	.num_leds		= ARRAY_SIZE(nslu2_led_pins),
	.leds			= nslu2_led_pins,
};

static struct platform_device nslu2_leds = {
	.name			= "leds-gpio",
	.id			= -1,
	.dev.platform_data	= &nslu2_led_data,
};

static struct platform_device nslu2_i2c_gpio = {
	.name			= "i2c-gpio",
	.id			= 0,
	.dev	 = {
		.platform_data	= &nslu2_i2c_gpio_data,
	},
};

static struct platform_device nslu2_beeper = {
	.name			= "ixp4xx-beeper",
	.id			= NSLU2_GPIO_BUZZ,
	.num_resources		= 0,
};

static struct resource nslu2_uart_resources[] = {
	{
		.start		= IXP4XX_UART1_BASE_PHYS,
		.end		= IXP4XX_UART1_BASE_PHYS + 0x0fff,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= IXP4XX_UART2_BASE_PHYS,
		.end		= IXP4XX_UART2_BASE_PHYS + 0x0fff,
		.flags		= IORESOURCE_MEM,
	}
};

static struct plat_serial8250_port nslu2_uart_data[] = {
	{
		.mapbase	= IXP4XX_UART1_BASE_PHYS,
		.membase	= (char *)IXP4XX_UART1_BASE_VIRT + REG_OFFSET,
		.irq		= IRQ_IXP4XX_UART1,
		.flags		= UPF_BOOT_AUTOCONF,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= IXP4XX_UART_XTAL,
	},
	{
		.mapbase	= IXP4XX_UART2_BASE_PHYS,
		.membase	= (char *)IXP4XX_UART2_BASE_VIRT + REG_OFFSET,
		.irq		= IRQ_IXP4XX_UART2,
		.flags		= UPF_BOOT_AUTOCONF,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= IXP4XX_UART_XTAL,
	},
	{ }
};

static struct platform_device nslu2_uart = {
	.name			= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev.platform_data	= nslu2_uart_data,
	.num_resources		= 2,
	.resource		= nslu2_uart_resources,
};

/* Built-in 10/100 Ethernet MAC interfaces */
static struct eth_plat_info nslu2_plat_eth[] = {
	{
		.phy		= 1,
		.rxq		= 3,
		.txreadyq	= 20,
	}
};

static struct platform_device nslu2_eth[] = {
	{
		.name			= "ixp4xx_eth",
		.id			= IXP4XX_ETH_NPEB,
		.dev.platform_data	= nslu2_plat_eth,
	}
};

static struct platform_device *nslu2_devices[] __initdata = {
	&nslu2_i2c_gpio,
	&nslu2_flash,
	&nslu2_beeper,
	&nslu2_leds,
	&nslu2_eth[0],
};

static void nslu2_power_off(void)
{
	/* This causes the box to drop the power and go dead. */

	/* enable the pwr cntl gpio */
	gpio_line_config(NSLU2_PO_GPIO, IXP4XX_GPIO_OUT);

	/* do the deed */
	gpio_line_set(NSLU2_PO_GPIO, IXP4XX_GPIO_HIGH);
}

static void __init nslu2_timer_init(void)
{
    /* The xtal on this machine is non-standard. */
    ixp4xx_timer_freq = NSLU2_FREQ;

    /* Call standard timer_init function. */
    ixp4xx_timer_init();
}

static struct sys_timer nslu2_timer = {
    .init   = nslu2_timer_init,
};

static void __init nslu2_init(void)
{
	DECLARE_MAC_BUF(mac_buf);
	uint8_t __iomem *f;
	int i;

	ixp4xx_sys_init();

	nslu2_flash_resource.start = IXP4XX_EXP_BUS_BASE(0);
	nslu2_flash_resource.end =
		IXP4XX_EXP_BUS_BASE(0) + ixp4xx_exp_bus_size - 1;

	pm_power_off = nslu2_power_off;

	i2c_register_board_info(0, nslu2_i2c_board_info,
				ARRAY_SIZE(nslu2_i2c_board_info));

	/*
	 * This is only useful on a modified machine, but it is valuable
	 * to have it first in order to see debug messages, and so that
	 * it does *not* get removed if platform_add_devices fails!
	 */
	(void)platform_device_register(&nslu2_uart);

	platform_add_devices(nslu2_devices, ARRAY_SIZE(nslu2_devices));


	/*
	 * Map in a portion of the flash and read the MAC address.
	 * Since it is stored in BE in the flash itself, we need to
	 * byteswap it if we're in LE mode.
	 */
	f = ioremap(IXP4XX_EXP_BUS_BASE(0), 0x40000);
	if (f) {
		for (i = 0; i < 6; i++)
#ifdef __ARMEB__
			nslu2_plat_eth[0].hwaddr[i] = readb(f + 0x3FFB0 + i);
#else
			nslu2_plat_eth[0].hwaddr[i] = readb(f + 0x3FFB0 + (i^3));
#endif
		iounmap(f);
	}
	printk(KERN_INFO "NSLU2: Using MAC address %s for port 0\n",
	       print_mac(mac_buf, nslu2_plat_eth[0].hwaddr));

}

MACHINE_START(NSLU2, "Linksys NSLU2")
	/* Maintainer: www.nslu2-linux.org */
	.phys_io	= IXP4XX_PERIPHERAL_BASE_PHYS,
	.io_pg_offst	= ((IXP4XX_PERIPHERAL_BASE_VIRT) >> 18) & 0xFFFC,
	.boot_params	= 0x00000100,
	.map_io		= ixp4xx_map_io,
	.init_irq	= ixp4xx_init_irq,
	.timer          = &nslu2_timer,
	.init_machine	= nslu2_init,
MACHINE_END
