/*
 * linux/drivers/input/keyboard/pxa27x_keypad.c
 *
 * Driver for the pxa27x matrix keyboard controller.
 *
 * Created:	Feb 22, 2007
 * Author:	Rodolfo Giometti <giometti@linux.it>
 *
 * Based on a previous implementations by Kevin O'Connor
 * <kevin_at_koconnor.net> and Alex Osborne <bobofdoom@gmail.com> and
 * on some suggestions by Nicolas Pitre <nico@cam.org>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/hardware.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/irqs.h>
#include <asm/arch/pxa27x_keypad.h>

#define DRIVER_NAME		"pxa27x-keypad"

#define KPASMKP(col)		(col/2 == 0 ? KPASMKP0 : \
				 col/2 == 1 ? KPASMKP1 : \
				 col/2 == 2 ? KPASMKP2 : KPASMKP3)
#define KPASMKPx_MKC(row, col)	(1 << (row + 16 * (col % 2)))

static struct clk *pxa27x_keypad_clk;

static irqreturn_t pxa27x_keypad_irq_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct pxa27x_keypad_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input_dev = platform_get_drvdata(pdev);
	unsigned long kpc = KPC;
	int p, row, col, rel;

	if (kpc & KPC_DI) {
		unsigned long kpdk = KPDK;

		if (!(kpdk & KPDK_DKP)) {
			/* better luck next time */
		} else if (kpc & KPC_REE0) {
			unsigned long kprec = KPREC;
			KPREC = 0x7f;

			if (kprec & KPREC_OF0)
				rel = (kprec & 0xff) + 0x7f;
			else if (kprec & KPREC_UF0)
				rel = (kprec & 0xff) - 0x7f - 0xff;
			else
				rel = (kprec & 0xff) - 0x7f;

			if (rel) {
				input_report_rel(input_dev, REL_WHEEL, rel);
				input_sync(input_dev);
			}
		}
	}

	if (kpc & KPC_MI) {
		/* report the status of every button */
		for (row = 0; row < pdata->nr_rows; row++) {
			for (col = 0; col < pdata->nr_cols; col++) {
				p = KPASMKP(col) & KPASMKPx_MKC(row, col) ?
					1 : 0;
				pr_debug("keycode %x - pressed %x\n",
						pdata->keycodes[row][col], p);
				input_report_key(input_dev,
						pdata->keycodes[row][col], p);
			}
		}
		input_sync(input_dev);
	}

	return IRQ_HANDLED;
}

static int pxa27x_keypad_open(struct input_dev *dev)
{
	/* Set keypad control register */
	KPC |= (KPC_ASACT |
		KPC_MS_ALL |
		(2 << 6) | KPC_REE0 | KPC_DK_DEB_SEL |
		KPC_ME | KPC_MIE | KPC_DE | KPC_DIE);

	KPC &= ~KPC_AS;         /* disable automatic scan */
	KPC &= ~KPC_IMKP;       /* do not ignore multiple keypresses */

	/* Set rotary count to mid-point value */
	KPREC = 0x7F;

	/* Enable unit clock */
	clk_enable(pxa27x_keypad_clk);

	return 0;
}

static void pxa27x_keypad_close(struct input_dev *dev)
{
	/* Disable clock unit */
	clk_disable(pxa27x_keypad_clk);
}

#ifdef CONFIG_PM
static int pxa27x_keypad_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct pxa27x_keypad_platform_data *pdata = pdev->dev.platform_data;

	/* Save controller status */
	pdata->reg_kpc = KPC;
	pdata->reg_kprec = KPREC;

	return 0;
}

static int pxa27x_keypad_resume(struct platform_device *pdev)
{
	struct pxa27x_keypad_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input_dev = platform_get_drvdata(pdev);

	mutex_lock(&input_dev->mutex);

	if (input_dev->users) {
		/* Restore controller status */
		KPC = pdata->reg_kpc;
		KPREC = pdata->reg_kprec;

		/* Enable unit clock */
		clk_disable(pxa27x_keypad_clk);
		clk_enable(pxa27x_keypad_clk);
	}

	mutex_unlock(&input_dev->mutex);

	return 0;
}
#else
#define pxa27x_keypad_suspend	NULL
#define pxa27x_keypad_resume	NULL
#endif

static int __devinit pxa27x_keypad_probe(struct platform_device *pdev)
{
	struct pxa27x_keypad_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input_dev;
	int i, row, col, error;

	pxa27x_keypad_clk = clk_get(&pdev->dev, "KBDCLK");
	if (IS_ERR(pxa27x_keypad_clk)) {
		error = PTR_ERR(pxa27x_keypad_clk);
		goto err_clk;
	}

	/* Create and register the input driver. */
	input_dev = input_allocate_device();
	if (!input_dev) {
		printk(KERN_ERR "Cannot request keypad device\n");
		error = -ENOMEM;
		goto err_alloc;
	}

	input_dev->name = DRIVER_NAME;
	input_dev->id.bustype = BUS_HOST;
	input_dev->open = pxa27x_keypad_open;
	input_dev->close = pxa27x_keypad_close;
	input_dev->dev.parent = &pdev->dev;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP) |
		BIT_MASK(EV_REL);
	input_dev->relbit[BIT_WORD(REL_WHEEL)] = BIT_MASK(REL_WHEEL);
	for (row = 0; row < pdata->nr_rows; row++) {
		for (col = 0; col < pdata->nr_cols; col++) {
			int code = pdata->keycodes[row][col];
			if (code > 0)
				set_bit(code, input_dev->keybit);
		}
	}

	error = request_irq(IRQ_KEYPAD, pxa27x_keypad_irq_handler, IRQF_DISABLED,
			    DRIVER_NAME, pdev);
	if (error) {
		printk(KERN_ERR "Cannot request keypad IRQ\n");
		goto err_free_dev;
	}

	platform_set_drvdata(pdev, input_dev);

	/* Register the input device */
	error = input_register_device(input_dev);
	if (error)
		goto err_free_irq;

	/*
	 * Store rows/cols info into keyboard registers.
	 */

	KPC |= (pdata->nr_rows - 1) << 26;
	KPC |= (pdata->nr_cols - 1) << 23;

	for (col = 0; col < pdata->nr_cols; col++)
		KPC |= KPC_MS0 << col;

	return 0;

 err_free_irq:
	platform_set_drvdata(pdev, NULL);
	free_irq(IRQ_KEYPAD, pdev);
 err_free_dev:
	input_free_device(input_dev);
 err_alloc:
	clk_put(pxa27x_keypad_clk);
 err_clk:
	return error;
}

static int __devexit pxa27x_keypad_remove(struct platform_device *pdev)
{
	struct input_dev *input_dev = platform_get_drvdata(pdev);

	input_unregister_device(input_dev);
	free_irq(IRQ_KEYPAD, pdev);
	clk_put(pxa27x_keypad_clk);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver pxa27x_keypad_driver = {
	.probe		= pxa27x_keypad_probe,
	.remove		= __devexit_p(pxa27x_keypad_remove),
	.suspend	= pxa27x_keypad_suspend,
	.resume		= pxa27x_keypad_resume,
	.driver		= {
		.name	= DRIVER_NAME,
	},
};

static int __init pxa27x_keypad_init(void)
{
	return platform_driver_register(&pxa27x_keypad_driver);
}

static void __exit pxa27x_keypad_exit(void)
{
	platform_driver_unregister(&pxa27x_keypad_driver);
}

module_init(pxa27x_keypad_init);
module_exit(pxa27x_keypad_exit);

MODULE_DESCRIPTION("PXA27x Keypad Controller Driver");
MODULE_LICENSE("GPL");
