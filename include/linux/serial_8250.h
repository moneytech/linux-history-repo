/*
 *  linux/include/linux/serial_8250.h
 *
 *  Copyright (C) 2004 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _LINUX_SERIAL_8250_H
#define _LINUX_SERIAL_8250_H

#include <linux/serial_core.h>
#include <linux/device.h>

/*
 * This is the platform device platform_data structure
 */
struct plat_serial8250_port {
	unsigned long	iobase;		/* io base address */
	void __iomem	*membase;	/* ioremap cookie or NULL */
	unsigned long	mapbase;	/* resource base */
	unsigned int	irq;		/* interrupt number */
	unsigned int	uartclk;	/* UART clock rate */
	unsigned char	regshift;	/* register shift */
	unsigned char	iotype;		/* UPIO_* */
	unsigned char	hub6;
	unsigned int	flags;		/* UPF_* flags */
};

/*
 * This should be used by drivers which want to register
 * their own 8250 ports without registering their own
 * platform device.  Using these will make your driver
 * dependent on the 8250 driver.
 */
struct uart_port;

int serial8250_register_port(struct uart_port *);
void serial8250_unregister_port(int line);
void serial8250_suspend_port(int line);
void serial8250_resume_port(int line);

#endif
