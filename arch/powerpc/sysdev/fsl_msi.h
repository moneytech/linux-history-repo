/*
 * Copyright (C) 2007-2008 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Tony Li <tony.li@freescale.com>
 *	   Jason Jin <Jason.jin@freescale.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
 * License.
 *
 */
#ifndef _POWERPC_SYSDEV_FSL_MSI_H
#define _POWERPC_SYSDEV_FSL_MSI_H

#define NR_MSI_REG		8
#define IRQS_PER_MSI_REG	32
#define NR_MSI_IRQS	(NR_MSI_REG * IRQS_PER_MSI_REG)

#define FSL_PIC_IP_MASK	0x0000000F
#define FSL_PIC_IP_MPIC	0x00000001
#define FSL_PIC_IP_IPIC	0x00000002

struct fsl_msi {
	struct irq_host *irqhost;

	unsigned long cascade_irq;

	u32 msi_addr_lo;
	u32 msi_addr_hi;
	void __iomem *msi_regs;
	u32 feature;

	unsigned long *fsl_msi_bitmap;
	spinlock_t bitmap_lock;
};

#endif /* _POWERPC_SYSDEV_FSL_MSI_H */

