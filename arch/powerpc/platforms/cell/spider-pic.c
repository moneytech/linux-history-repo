/*
 * External Interrupt Controller on Spider South Bridge
 *
 * (C) Copyright IBM Deutschland Entwicklung GmbH 2005
 *
 * Author: Arnd Bergmann <arndb@de.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
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

#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/pgtable.h>
#include <asm/prom.h>
#include <asm/io.h>

#include "interrupt.h"

/* register layout taken from Spider spec, table 7.4-4 */
enum {
	TIR_DEN		= 0x004, /* Detection Enable Register */
	TIR_MSK		= 0x084, /* Mask Level Register */
	TIR_EDC		= 0x0c0, /* Edge Detection Clear Register */
	TIR_PNDA	= 0x100, /* Pending Register A */
	TIR_PNDB	= 0x104, /* Pending Register B */
	TIR_CS		= 0x144, /* Current Status Register */
	TIR_LCSA	= 0x150, /* Level Current Status Register A */
	TIR_LCSB	= 0x154, /* Level Current Status Register B */
	TIR_LCSC	= 0x158, /* Level Current Status Register C */
	TIR_LCSD	= 0x15c, /* Level Current Status Register D */
	TIR_CFGA	= 0x200, /* Setting Register A0 */
	TIR_CFGB	= 0x204, /* Setting Register B0 */
			/* 0x208 ... 0x3ff Setting Register An/Bn */
	TIR_PPNDA	= 0x400, /* Packet Pending Register A */
	TIR_PPNDB	= 0x404, /* Packet Pending Register B */
	TIR_PIERA	= 0x408, /* Packet Output Error Register A */
	TIR_PIERB	= 0x40c, /* Packet Output Error Register B */
	TIR_PIEN	= 0x444, /* Packet Output Enable Register */
	TIR_PIPND	= 0x454, /* Packet Output Pending Register */
	TIRDID		= 0x484, /* Spider Device ID Register */
	REISTIM		= 0x500, /* Reissue Command Timeout Time Setting */
	REISTIMEN	= 0x504, /* Reissue Command Timeout Setting */
	REISWAITEN	= 0x508, /* Reissue Wait Control*/
};

static void __iomem *spider_pics[4];

static void __iomem *spider_get_pic(int irq)
{
	int node = irq / IIC_NODE_STRIDE;
	irq %= IIC_NODE_STRIDE;

	if (irq >= IIC_EXT_OFFSET &&
	    irq < IIC_EXT_OFFSET + IIC_NUM_EXT &&
	    spider_pics)
		return spider_pics[node];
	return NULL;
}

static int spider_get_nr(unsigned int irq)
{
	return (irq % IIC_NODE_STRIDE) - IIC_EXT_OFFSET;
}

static void __iomem *spider_get_irq_config(int irq)
{
	void __iomem *pic;
	pic = spider_get_pic(irq);
	return pic + TIR_CFGA + 8 * spider_get_nr(irq);
}

static void spider_enable_irq(unsigned int irq)
{
	int nodeid = (irq / IIC_NODE_STRIDE) * 0x10;
	void __iomem *cfg = spider_get_irq_config(irq);
	irq = spider_get_nr(irq);

	out_be32(cfg, (in_be32(cfg) & ~0xf0)| 0x3107000eu | nodeid);
	out_be32(cfg + 4, in_be32(cfg + 4) | 0x00020000u | irq);
}

static void spider_disable_irq(unsigned int irq)
{
	void __iomem *cfg = spider_get_irq_config(irq);
	irq = spider_get_nr(irq);

	out_be32(cfg, in_be32(cfg) & ~0x30000000u);
}

static unsigned int spider_startup_irq(unsigned int irq)
{
	spider_enable_irq(irq);
	return 0;
}

static void spider_shutdown_irq(unsigned int irq)
{
	spider_disable_irq(irq);
}

static void spider_end_irq(unsigned int irq)
{
	spider_enable_irq(irq);
}

static void spider_ack_irq(unsigned int irq)
{
	spider_disable_irq(irq);
	iic_local_enable();
}

static struct hw_interrupt_type spider_pic = {
	.typename = " SPIDER   ",
	.startup = spider_startup_irq,
	.shutdown = spider_shutdown_irq,
	.enable = spider_enable_irq,
	.disable = spider_disable_irq,
	.ack = spider_ack_irq,
	.end = spider_end_irq,
};

int spider_get_irq(int node)
{
	unsigned long cs;
	void __iomem *regs = spider_pics[node];

	cs = in_be32(regs + TIR_CS) >> 24;

	if (cs == 63)
		return -1;
	else
		return cs;
}

/* hardcoded part to be compatible with older firmware */

void spider_init_IRQ_hardcoded(void)
{
	int node;
	long spiderpic;
	long pics[] = { 0x24000008000, 0x34000008000 };
	int n;

	pr_debug("%s(%d): Using hardcoded defaults\n", __FUNCTION__, __LINE__);

	for (node = 0; node < num_present_cpus()/2; node++) {
		spiderpic = pics[node];
		printk(KERN_DEBUG "SPIDER addr: %lx\n", spiderpic);
		spider_pics[node] = ioremap(spiderpic, 0x800);
		for (n = 0; n < IIC_NUM_EXT; n++) {
			int irq = n + IIC_EXT_OFFSET + node * IIC_NODE_STRIDE;
			get_irq_desc(irq)->chip = &spider_pic;
		}

 		/* do not mask any interrupts because of level */
 		out_be32(spider_pics[node] + TIR_MSK, 0x0);

 		/* disable edge detection clear */
 		/* out_be32(spider_pics[node] + TIR_EDC, 0x0); */

 		/* enable interrupt packets to be output */
 		out_be32(spider_pics[node] + TIR_PIEN,
			in_be32(spider_pics[node] + TIR_PIEN) | 0x1);

 		/* Enable the interrupt detection enable bit. Do this last! */
 		out_be32(spider_pics[node] + TIR_DEN,
			in_be32(spider_pics[node] + TIR_DEN) | 0x1);
	}
}

void spider_init_IRQ(void)
{
	long spider_reg;
	struct device_node *dn;
	char *compatible;
	int n, node = 0;

	for (dn = NULL; (dn = of_find_node_by_name(dn, "interrupt-controller"));) {
		compatible = (char *)get_property(dn, "compatible", NULL);

		if (!compatible)
			continue;

		if (strstr(compatible, "CBEA,platform-spider-pic"))
			spider_reg = *(long *)get_property(dn,"reg", NULL);
		else if (strstr(compatible, "sti,platform-spider-pic")) {
			spider_init_IRQ_hardcoded();
			return;
		} else
			continue;

		if (!spider_reg)
			printk("interrupt controller does not have reg property !\n");

		n = prom_n_addr_cells(dn);

		if ( n != 2)
			printk("reg property with invalid number of elements \n");

		spider_pics[node] = ioremap(spider_reg, 0x800);

		printk("SPIDER addr: %lx with %i addr_cells mapped to %p\n",
		       spider_reg, n, spider_pics[node]);

		for (n = 0; n < IIC_NUM_EXT; n++) {
			int irq = n + IIC_EXT_OFFSET + node * IIC_NODE_STRIDE;
			get_irq_desc(irq)->chip = &spider_pic;
		}

		/* do not mask any interrupts because of level */
		out_be32(spider_pics[node] + TIR_MSK, 0x0);

		/* disable edge detection clear */
		/* out_be32(spider_pics[node] + TIR_EDC, 0x0); */

		/* enable interrupt packets to be output */
		out_be32(spider_pics[node] + TIR_PIEN,
			in_be32(spider_pics[node] + TIR_PIEN) | 0x1);

		/* Enable the interrupt detection enable bit. Do this last! */
		out_be32(spider_pics[node] + TIR_DEN,
			in_be32(spider_pics[node] + TIR_DEN) | 0x1);

		node++;
	}
}
