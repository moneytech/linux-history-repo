/*
 * mv64x60_wdt.c - MV64X60 (Marvell Discovery) watchdog userspace interface
 *
 * Author: James Chapman <jchapman@katalix.com>
 *
 * Platform-specific setup code should configure the dog to generate
 * interrupt or reset as required.  This code only enables/disables
 * and services the watchdog.
 *
 * Derived from mpc8xx_wdt.c, with the following copyright.
 * 
 * 2002 (c) Florian Schirmer <jolt@tuxbox.org> This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/watchdog.h>
#include <linux/platform_device.h>

#include <linux/mv643xx.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define MV64x60_WDT_WDC_OFFSET	0

/* MV64x60 WDC (config) register access definitions */
#define MV64x60_WDC_CTL1_MASK	(3 << 24)
#define MV64x60_WDC_CTL1(val)	((val & 3) << 24)
#define MV64x60_WDC_CTL2_MASK	(3 << 26)
#define MV64x60_WDC_CTL2(val)	((val & 3) << 26)

/* Flags bits */
#define MV64x60_WDOG_FLAG_OPENED	0
#define MV64x60_WDOG_FLAG_ENABLED	1

static unsigned long wdt_flags;
static int wdt_status;
static void __iomem *mv64x60_wdt_regs;
static int mv64x60_wdt_timeout;
static unsigned int bus_clk;

static void mv64x60_wdt_reg_write(u32 val)
{
	/* Allow write only to CTL1 / CTL2 fields, retaining values in
	 * other fields.
	 */
	u32 data = readl(mv64x60_wdt_regs + MV64x60_WDT_WDC_OFFSET);
	data &= ~(MV64x60_WDC_CTL1_MASK | MV64x60_WDC_CTL2_MASK);
	data |= val;
	writel(data, mv64x60_wdt_regs + MV64x60_WDT_WDC_OFFSET);
}

static void mv64x60_wdt_service(void)
{
	/* Write 01 followed by 10 to CTL2 */
	mv64x60_wdt_reg_write(MV64x60_WDC_CTL2(0x01));
	mv64x60_wdt_reg_write(MV64x60_WDC_CTL2(0x02));
}

static void mv64x60_wdt_handler_disable(void)
{
	if (test_and_clear_bit(MV64x60_WDOG_FLAG_ENABLED, &wdt_flags)) {
		/* Write 01 followed by 10 to CTL1 */
		mv64x60_wdt_reg_write(MV64x60_WDC_CTL1(0x01));
		mv64x60_wdt_reg_write(MV64x60_WDC_CTL1(0x02));
		printk(KERN_NOTICE "mv64x60_wdt: watchdog deactivated\n");
	}
}

static void mv64x60_wdt_handler_enable(void)
{
	if (!test_and_set_bit(MV64x60_WDOG_FLAG_ENABLED, &wdt_flags)) {
		/* Write 01 followed by 10 to CTL1 */
		mv64x60_wdt_reg_write(MV64x60_WDC_CTL1(0x01));
		mv64x60_wdt_reg_write(MV64x60_WDC_CTL1(0x02));
		printk(KERN_NOTICE "mv64x60_wdt: watchdog activated\n");
	}
}

static void mv64x60_wdt_set_timeout(int timeout)
{
	/* maximum bus cycle count is 0xFFFFFFFF */
	if (timeout > 0xFFFFFFFF / bus_clk)
		timeout = 0xFFFFFFFF / bus_clk;

	mv64x60_wdt_timeout = timeout;
	writel((timeout * bus_clk) >> 8,
	       mv64x60_wdt_regs + MV64x60_WDT_WDC_OFFSET);
	mv64x60_wdt_service();
}

static int mv64x60_wdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(MV64x60_WDOG_FLAG_OPENED, &wdt_flags))
		return -EBUSY;

	mv64x60_wdt_service();
	mv64x60_wdt_handler_enable();

	return nonseekable_open(inode, file);
}

static int mv64x60_wdt_release(struct inode *inode, struct file *file)
{
	mv64x60_wdt_service();

#if !defined(CONFIG_WATCHDOG_NOWAYOUT)
	mv64x60_wdt_handler_disable();
#endif

	clear_bit(MV64x60_WDOG_FLAG_OPENED, &wdt_flags);

	return 0;
}

static ssize_t mv64x60_wdt_write(struct file *file, const char __user *data,
				 size_t len, loff_t * ppos)
{
	if (len)
		mv64x60_wdt_service();

	return len;
}

static int mv64x60_wdt_ioctl(struct inode *inode, struct file *file,
			     unsigned int cmd, unsigned long arg)
{
	int timeout;
	void __user *argp = (void __user *)arg;
	static struct watchdog_info info = {
		.options =	WDIOF_SETTIMEOUT	|
				WDIOF_KEEPALIVEPING,
		.firmware_version = 0,
		.identity = "MV64x60 watchdog",
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		if (copy_to_user(argp, &info, sizeof(info)))
			return -EFAULT;
		break;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		if (put_user(wdt_status, (int __user *)argp))
			return -EFAULT;
		wdt_status &= ~WDIOF_KEEPALIVEPING;
		break;

	case WDIOC_GETTEMP:
		return -EOPNOTSUPP;

	case WDIOC_SETOPTIONS:
		return -EOPNOTSUPP;

	case WDIOC_KEEPALIVE:
		mv64x60_wdt_service();
		wdt_status |= WDIOF_KEEPALIVEPING;
		break;

	case WDIOC_SETTIMEOUT:
		if (get_user(timeout, (int __user *)argp))
			return -EFAULT;
		mv64x60_wdt_set_timeout(timeout);
		/* Fall through */

	case WDIOC_GETTIMEOUT:
		if (put_user(mv64x60_wdt_timeout, (int __user *)argp))
			return -EFAULT;
		break;

	default:
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations mv64x60_wdt_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.write = mv64x60_wdt_write,
	.ioctl = mv64x60_wdt_ioctl,
	.open = mv64x60_wdt_open,
	.release = mv64x60_wdt_release,
};

static struct miscdevice mv64x60_wdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &mv64x60_wdt_fops,
};

static int __devinit mv64x60_wdt_probe(struct platform_device *dev)
{
	struct mv64x60_wdt_pdata *pdata = dev->dev.platform_data;
	struct resource *r;
	int timeout = 10;

	bus_clk = 133;			/* in MHz */
	if (pdata) {
		timeout = pdata->timeout;
		bus_clk = pdata->bus_clk;
	}

	/* Since bus_clk is truncated MHz, actual frequency could be
	 * up to 1MHz higher.  Round up, since it's better to time out
	 * too late than too soon.
	 */
	bus_clk++;
	bus_clk *= 1000000;		/* convert to Hz */

	r = platform_get_resource(dev, IORESOURCE_MEM, 0);
	if (!r)
		return -ENODEV;

	mv64x60_wdt_regs = ioremap(r->start, r->end - r->start + 1);
	if (mv64x60_wdt_regs == NULL)
		return -ENOMEM;

	mv64x60_wdt_set_timeout(timeout);

	return misc_register(&mv64x60_wdt_miscdev);
}

static int __devexit mv64x60_wdt_remove(struct platform_device *dev)
{
	misc_deregister(&mv64x60_wdt_miscdev);

	mv64x60_wdt_service();
	mv64x60_wdt_handler_disable();

	iounmap(mv64x60_wdt_regs);

	return 0;
}

static struct platform_driver mv64x60_wdt_driver = {
	.probe = mv64x60_wdt_probe,
	.remove = __devexit_p(mv64x60_wdt_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = MV64x60_WDT_NAME,
	},
};

static int __init mv64x60_wdt_init(void)
{
	printk(KERN_INFO "MV64x60 watchdog driver\n");

	return platform_driver_register(&mv64x60_wdt_driver);
}

static void __exit mv64x60_wdt_exit(void)
{
	platform_driver_unregister(&mv64x60_wdt_driver);
}

module_init(mv64x60_wdt_init);
module_exit(mv64x60_wdt_exit);

MODULE_AUTHOR("James Chapman <jchapman@katalix.com>");
MODULE_DESCRIPTION("MV64x60 watchdog driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
