/*
 * slip.c	This module implements the SLIP protocol for kernel-based
 *		devices like TTY.  It interfaces between a raw TTY, and the
 *		kernel's INET protocol layers.
 *
 * Version:	@(#)slip.c	0.8.3	12/24/94
 *
 * Authors:	Laurence Culhane, <loz@holmes.demon.co.uk>
 *		Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *
 * Fixes:
 *		Alan Cox	: 	Sanity checks and avoid tx overruns.
 *					Has a new sl->mtu field.
 *		Alan Cox	: 	Found cause of overrun. ifconfig sl0 mtu upwards.
 *					Driver now spots this and grows/shrinks its buffers(hack!).
 *					Memory leak if you run out of memory setting up a slip driver fixed.
 *		Matt Dillon	:	Printable slip (borrowed from NET2E)
 *	Pauline Middelink	:	Slip driver fixes.
 *		Alan Cox	:	Honours the old SL_COMPRESSED flag
 *		Alan Cox	:	KISS AX.25 and AXUI IP support
 *		Michael Riepe	:	Automatic CSLIP recognition added
 *		Charles Hedrick :	CSLIP header length problem fix.
 *		Alan Cox	:	Corrected non-IP cases of the above.
 *		Alan Cox	:	Now uses hardware type as per FvK.
 *		Alan Cox	:	Default to 192.168.0.0 (RFC 1597)
 *		A.N.Kuznetsov	:	dev_tint() recursion fix.
 *	Dmitry Gorodchanin	:	SLIP memory leaks
 *      Dmitry Gorodchanin      :       Code cleanup. Reduce tty driver
 *                                      buffering from 4096 to 256 bytes.
 *                                      Improving SLIP response time.
 *                                      CONFIG_SLIP_MODE_SLIP6.
 *                                      ifconfig sl? up & down now works correctly.
 *					Modularization.
 *              Alan Cox        :       Oops - fix AX.25 buffer lengths
 *      Dmitry Gorodchanin      :       Even more cleanups. Preserve CSLIP
 *                                      statistics. Include CSLIP code only
 *                                      if it really needed.
 *		Alan Cox	:	Free slhc buffers in the right place.
 *		Alan Cox	:	Allow for digipeated IP over AX.25
 *		Matti Aarnio	:	Dynamic SLIP devices, with ideas taken
 *					from Jim Freeman's <jfree@caldera.com>
 *					dynamic PPP devices.  We do NOT kfree()
 *					device entries, just reg./unreg. them
 *					as they are needed.  We kfree() them
 *					at module cleanup.
 *					With MODULE-loading ``insmod'', user can
 *					issue parameter:   slip_maxdev=1024
 *					(Or how much he/she wants.. Default is 256)
 * *	Stanislav Voronyi	:	Slip line checking, with ideas taken
 *					from multislip BSDI driver which was written
 *					by Igor Chechik, RELCOM Corp. Only algorithms
 * 					have been ported to Linux SLIP driver.
 *	Vitaly E. Lavrov	:	Sane behaviour on tty hangup.
 *	Alexey Kuznetsov	:	Cleanup interfaces to tty&netdevice modules.
 */

#define SL_CHECK_TRANSMIT
#include <linux/config.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>
#include <linux/if_slip.h>
#include <linux/init.h>
#include "slip.h"
#ifdef CONFIG_INET
#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/slhc_vj.h>
#endif

#define SLIP_VERSION	"0.8.4-NET3.019-NEWTTY"

static struct net_device **slip_devs;

static int slip_maxdev = SL_NRUNIT;
module_param(slip_maxdev, int, 0);
MODULE_PARM_DESC(slip_maxdev, "Maximum number of slip devices");

static int slip_esc(unsigned char *p, unsigned char *d, int len);
static void slip_unesc(struct slip *sl, unsigned char c);
#ifdef CONFIG_SLIP_MODE_SLIP6
static int slip_esc6(unsigned char *p, unsigned char *d, int len);
static void slip_unesc6(struct slip *sl, unsigned char c);
#endif
#ifdef CONFIG_SLIP_SMART
static void sl_keepalive(unsigned long sls);
static void sl_outfill(unsigned long sls);
static int sl_ioctl(struct net_device *dev,struct ifreq *rq,int cmd);
#endif

/********************************
*  Buffer administration routines:
*	sl_alloc_bufs()
*	sl_free_bufs()
*	sl_realloc_bufs()
*
* NOTE: sl_realloc_bufs != sl_free_bufs + sl_alloc_bufs, because
*	sl_realloc_bufs provides strong atomicity and reallocation
*	on actively running device.
*********************************/

/* 
   Allocate channel buffers.
 */

static int
sl_alloc_bufs(struct slip *sl, int mtu)
{
	int err = -ENOBUFS;
	unsigned long len;
	char * rbuff = NULL;
	char * xbuff = NULL;
#ifdef SL_INCLUDE_CSLIP
	char * cbuff = NULL;
	struct slcompress *slcomp = NULL;
#endif

	/*
	 * Allocate the SLIP frame buffers:
	 *
	 * rbuff	Receive buffer.
	 * xbuff	Transmit buffer.
	 * cbuff        Temporary compression buffer.
	 */
	len = mtu * 2;

	/*
	 * allow for arrival of larger UDP packets, even if we say not to
	 * also fixes a bug in which SunOS sends 512-byte packets even with
	 * an MSS of 128
	 */
	if (len < 576 * 2)
		len = 576 * 2;
	rbuff = kmalloc(len + 4, GFP_KERNEL);
	if (rbuff == NULL)
		goto err_exit;
	xbuff = kmalloc(len + 4, GFP_KERNEL);
	if (xbuff == NULL)
		goto err_exit;
#ifdef SL_INCLUDE_CSLIP
	cbuff = kmalloc(len + 4, GFP_KERNEL);
	if (cbuff == NULL)
		goto err_exit;
	slcomp = slhc_init(16, 16);
	if (slcomp == NULL)
		goto err_exit;
#endif
	spin_lock_bh(&sl->lock);
	if (sl->tty == NULL) {
		spin_unlock_bh(&sl->lock);
		err = -ENODEV;
		goto err_exit;
	}
	sl->mtu	     = mtu;
	sl->buffsize = len;
	sl->rcount   = 0;
	sl->xleft    = 0;
	rbuff = xchg(&sl->rbuff, rbuff);
	xbuff = xchg(&sl->xbuff, xbuff);
#ifdef SL_INCLUDE_CSLIP
	cbuff = xchg(&sl->cbuff, cbuff);
	slcomp = xchg(&sl->slcomp, slcomp);
#ifdef CONFIG_SLIP_MODE_SLIP6
	sl->xdata    = 0;
	sl->xbits    = 0;
#endif
#endif
	spin_unlock_bh(&sl->lock);
	err = 0;

	/* Cleanup */
err_exit:
#ifdef SL_INCLUDE_CSLIP
	kfree(cbuff);
	if (slcomp)
		slhc_free(slcomp);
#endif
	kfree(xbuff);
	kfree(rbuff);
	return err;
}

/* Free a SLIP channel buffers. */
static void
sl_free_bufs(struct slip *sl)
{
	void * tmp;

	/* Free all SLIP frame buffers. */
	tmp = xchg(&sl->rbuff, NULL);
	kfree(tmp);
	tmp = xchg(&sl->xbuff, NULL);
	kfree(tmp);
#ifdef SL_INCLUDE_CSLIP
	tmp = xchg(&sl->cbuff, NULL);
	kfree(tmp);
	if ((tmp = xchg(&sl->slcomp, NULL)) != NULL)
		slhc_free(tmp);
#endif
}

/* 
   Reallocate slip channel buffers.
 */

static int sl_realloc_bufs(struct slip *sl, int mtu)
{
	int err = 0;
	struct net_device *dev = sl->dev;
	unsigned char *xbuff, *rbuff;
#ifdef SL_INCLUDE_CSLIP
	unsigned char *cbuff;
#endif
	int len = mtu * 2;

/*
 * allow for arrival of larger UDP packets, even if we say not to
 * also fixes a bug in which SunOS sends 512-byte packets even with
 * an MSS of 128
 */
	if (len < 576 * 2)
		len = 576 * 2;

	xbuff = (unsigned char *) kmalloc (len + 4, GFP_ATOMIC);
	rbuff = (unsigned char *) kmalloc (len + 4, GFP_ATOMIC);
#ifdef SL_INCLUDE_CSLIP
	cbuff = (unsigned char *) kmalloc (len + 4, GFP_ATOMIC);
#endif


#ifdef SL_INCLUDE_CSLIP
	if (xbuff == NULL || rbuff == NULL || cbuff == NULL)  {
#else
	if (xbuff == NULL || rbuff == NULL)  {
#endif
		if (mtu >= sl->mtu) {
			printk(KERN_WARNING "%s: unable to grow slip buffers, MTU change cancelled.\n",
			       dev->name);
			err = -ENOBUFS;
		}
		goto done;
	}

	spin_lock_bh(&sl->lock);

	err = -ENODEV;
	if (sl->tty == NULL)
		goto done_on_bh;

	xbuff    = xchg(&sl->xbuff, xbuff);
	rbuff    = xchg(&sl->rbuff, rbuff);
#ifdef SL_INCLUDE_CSLIP
	cbuff    = xchg(&sl->cbuff, cbuff);
#endif
	if (sl->xleft)  {
		if (sl->xleft <= len)  {
			memcpy(sl->xbuff, sl->xhead, sl->xleft);
		} else  {
			sl->xleft = 0;
			sl->tx_dropped++;
		}
	}
	sl->xhead = sl->xbuff;

	if (sl->rcount)  {
		if (sl->rcount <= len) {
			memcpy(sl->rbuff, rbuff, sl->rcount);
		} else  {
			sl->rcount = 0;
			sl->rx_over_errors++;
			set_bit(SLF_ERROR, &sl->flags);
		}
	}
	sl->mtu      = mtu;
	dev->mtu      = mtu;
	sl->buffsize = len;
	err = 0;

done_on_bh:
	spin_unlock_bh(&sl->lock);

done:
	kfree(xbuff);
	kfree(rbuff);
#ifdef SL_INCLUDE_CSLIP
	kfree(cbuff);
#endif
	return err;
}


/* Set the "sending" flag.  This must be atomic hence the set_bit. */
static inline void
sl_lock(struct slip *sl)
{
	netif_stop_queue(sl->dev);
}


/* Clear the "sending" flag.  This must be atomic, hence the ASM. */
static inline void
sl_unlock(struct slip *sl)
{
	netif_wake_queue(sl->dev);
}

/* Send one completely decapsulated IP datagram to the IP layer. */
static void
sl_bump(struct slip *sl)
{
	struct sk_buff *skb;
	int count;

	count = sl->rcount;
#ifdef SL_INCLUDE_CSLIP
	if (sl->mode & (SL_MODE_ADAPTIVE | SL_MODE_CSLIP)) {
		unsigned char c;
		if ((c = sl->rbuff[0]) & SL_TYPE_COMPRESSED_TCP) {
			/* ignore compressed packets when CSLIP is off */
			if (!(sl->mode & SL_MODE_CSLIP)) {
				printk(KERN_WARNING "%s: compressed packet ignored\n", sl->dev->name);
				return;
			}
			/* make sure we've reserved enough space for uncompress to use */
			if (count + 80 > sl->buffsize) {
				sl->rx_over_errors++;
				return;
			}
			count = slhc_uncompress(sl->slcomp, sl->rbuff, count);
			if (count <= 0) {
				return;
			}
		} else if (c >= SL_TYPE_UNCOMPRESSED_TCP) {
			if (!(sl->mode & SL_MODE_CSLIP)) {
				/* turn on header compression */
				sl->mode |= SL_MODE_CSLIP;
				sl->mode &= ~SL_MODE_ADAPTIVE;
				printk(KERN_INFO "%s: header compression turned on\n", sl->dev->name);
			}
			sl->rbuff[0] &= 0x4f;
			if (slhc_remember(sl->slcomp, sl->rbuff, count) <= 0) {
				return;
			}
		}
	}
#endif  /* SL_INCLUDE_CSLIP */

	sl->rx_bytes+=count;
	
	skb = dev_alloc_skb(count);
	if (skb == NULL)  {
		printk(KERN_WARNING "%s: memory squeeze, dropping packet.\n", sl->dev->name);
		sl->rx_dropped++;
		return;
	}
	skb->dev = sl->dev;
	memcpy(skb_put(skb,count), sl->rbuff, count);
	skb->mac.raw=skb->data;
	skb->protocol=htons(ETH_P_IP);
	netif_rx(skb);
	sl->dev->last_rx = jiffies;
	sl->rx_packets++;
}

/* Encapsulate one IP datagram and stuff into a TTY queue. */
static void
sl_encaps(struct slip *sl, unsigned char *icp, int len)
{
	unsigned char *p;
	int actual, count;

	if (len > sl->mtu) {		/* Sigh, shouldn't occur BUT ... */
		printk(KERN_WARNING "%s: truncating oversized transmit packet!\n", sl->dev->name);
		sl->tx_dropped++;
		sl_unlock(sl);
		return;
	}

	p = icp;
#ifdef SL_INCLUDE_CSLIP
	if (sl->mode & SL_MODE_CSLIP)  {
		len = slhc_compress(sl->slcomp, p, len, sl->cbuff, &p, 1);
	}
#endif
#ifdef CONFIG_SLIP_MODE_SLIP6
	if(sl->mode & SL_MODE_SLIP6)
		count = slip_esc6(p, (unsigned char *) sl->xbuff, len);
	else
#endif
		count = slip_esc(p, (unsigned char *) sl->xbuff, len);

	/* Order of next two lines is *very* important.
	 * When we are sending a little amount of data,
	 * the transfer may be completed inside driver.write()
	 * routine, because it's running with interrupts enabled.
	 * In this case we *never* got WRITE_WAKEUP event,
	 * if we did not request it before write operation.
	 *       14 Oct 1994  Dmitry Gorodchanin.
	 */
	sl->tty->flags |= (1 << TTY_DO_WRITE_WAKEUP);
	actual = sl->tty->driver->write(sl->tty, sl->xbuff, count);
#ifdef SL_CHECK_TRANSMIT
	sl->dev->trans_start = jiffies;
#endif
	sl->xleft = count - actual;
	sl->xhead = sl->xbuff + actual;
#ifdef CONFIG_SLIP_SMART
	/* VSV */
	clear_bit(SLF_OUTWAIT, &sl->flags);	/* reset outfill flag */
#endif
}

/*
 * Called by the driver when there's room for more data.  If we have
 * more packets to send, we send them here.
 */
static void slip_write_wakeup(struct tty_struct *tty)
{
	int actual;
	struct slip *sl = (struct slip *) tty->disc_data;

	/* First make sure we're connected. */
	if (!sl || sl->magic != SLIP_MAGIC || !netif_running(sl->dev)) {
		return;
	}
	if (sl->xleft <= 0)  {
		/* Now serial buffer is almost free & we can start
		 * transmission of another packet */
		sl->tx_packets++;
		tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
		sl_unlock(sl);
		return;
	}

	actual = tty->driver->write(tty, sl->xhead, sl->xleft);
	sl->xleft -= actual;
	sl->xhead += actual;
}

static void sl_tx_timeout(struct net_device *dev)
{
	struct slip *sl = netdev_priv(dev);

	spin_lock(&sl->lock);

	if (netif_queue_stopped(dev)) {
		if (!netif_running(dev))
			goto out;

		/* May be we must check transmitter timeout here ?
		 *      14 Oct 1994 Dmitry Gorodchanin.
		 */
#ifdef SL_CHECK_TRANSMIT
		if (time_before(jiffies, dev->trans_start + 20 * HZ))  {
			/* 20 sec timeout not reached */
			goto out;
		}
		printk(KERN_WARNING "%s: transmit timed out, %s?\n", dev->name,
		       (sl->tty->driver->chars_in_buffer(sl->tty) || sl->xleft) ?
		       "bad line quality" : "driver error");
		sl->xleft = 0;
		sl->tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
		sl_unlock(sl);
#endif
	}

out:
	spin_unlock(&sl->lock);
}


/* Encapsulate an IP datagram and kick it into a TTY queue. */
static int
sl_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct slip *sl = netdev_priv(dev);

	spin_lock(&sl->lock);
	if (!netif_running(dev))  {
		spin_unlock(&sl->lock);
		printk(KERN_WARNING "%s: xmit call when iface is down\n", dev->name);
		dev_kfree_skb(skb);
		return 0;
	}
	if (sl->tty == NULL) {
		spin_unlock(&sl->lock);
		dev_kfree_skb(skb);
		return 0;
	}

	sl_lock(sl);
	sl->tx_bytes+=skb->len;
	sl_encaps(sl, skb->data, skb->len);
	spin_unlock(&sl->lock);

	dev_kfree_skb(skb);
	return 0;
}


/******************************************
 *   Routines looking at netdevice side.
 ******************************************/

/* Netdevice UP -> DOWN routine */

static int
sl_close(struct net_device *dev)
{
	struct slip *sl = netdev_priv(dev);

	spin_lock_bh(&sl->lock);
	if (sl->tty) {
		/* TTY discipline is running. */
		sl->tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
	}
	netif_stop_queue(dev);
	sl->rcount   = 0;
	sl->xleft    = 0;
	spin_unlock_bh(&sl->lock);

	return 0;
}

/* Netdevice DOWN -> UP routine */

static int sl_open(struct net_device *dev)
{
	struct slip *sl = netdev_priv(dev);

	if (sl->tty==NULL)
		return -ENODEV;

	sl->flags &= (1 << SLF_INUSE);
	netif_start_queue(dev);
	return 0;
}

/* Netdevice change MTU request */

static int sl_change_mtu(struct net_device *dev, int new_mtu)
{
	struct slip *sl = netdev_priv(dev);

	if (new_mtu < 68 || new_mtu > 65534)
		return -EINVAL;

	if (new_mtu != dev->mtu)
		return sl_realloc_bufs(sl, new_mtu);
	return 0;
}

/* Netdevice get statistics request */

static struct net_device_stats *
sl_get_stats(struct net_device *dev)
{
	static struct net_device_stats stats;
	struct slip *sl = netdev_priv(dev);
#ifdef SL_INCLUDE_CSLIP
	struct slcompress *comp;
#endif

	memset(&stats, 0, sizeof(struct net_device_stats));

	stats.rx_packets     = sl->rx_packets;
	stats.tx_packets     = sl->tx_packets;
	stats.rx_bytes	     = sl->rx_bytes;
	stats.tx_bytes	     = sl->tx_bytes;
	stats.rx_dropped     = sl->rx_dropped;
	stats.tx_dropped     = sl->tx_dropped;
	stats.tx_errors      = sl->tx_errors;
	stats.rx_errors      = sl->rx_errors;
	stats.rx_over_errors = sl->rx_over_errors;
#ifdef SL_INCLUDE_CSLIP
	stats.rx_fifo_errors = sl->rx_compressed;
	stats.tx_fifo_errors = sl->tx_compressed;
	stats.collisions     = sl->tx_misses;
	comp = sl->slcomp;
	if (comp) {
		stats.rx_fifo_errors += comp->sls_i_compressed;
		stats.rx_dropped     += comp->sls_i_tossed;
		stats.tx_fifo_errors += comp->sls_o_compressed;
		stats.collisions     += comp->sls_o_misses;
	}
#endif /* CONFIG_INET */
	return (&stats);
}

/* Netdevice register callback */

static int sl_init(struct net_device *dev)
{
	struct slip *sl = netdev_priv(dev);

	/*
	 *	Finish setting up the DEVICE info. 
	 */

	dev->mtu		= sl->mtu;
	dev->type		= ARPHRD_SLIP + sl->mode;
#ifdef SL_CHECK_TRANSMIT
	dev->tx_timeout		= sl_tx_timeout;
	dev->watchdog_timeo	= 20*HZ;
#endif
	return 0;
}


static void sl_uninit(struct net_device *dev)
{
	struct slip *sl = netdev_priv(dev);

	sl_free_bufs(sl);
}

static void sl_setup(struct net_device *dev)
{
	dev->init		= sl_init;
	dev->uninit	  	= sl_uninit;
	dev->open		= sl_open;
	dev->destructor		= free_netdev;
	dev->stop		= sl_close;
	dev->get_stats	        = sl_get_stats;
	dev->change_mtu		= sl_change_mtu;
	dev->hard_start_xmit	= sl_xmit;
#ifdef CONFIG_SLIP_SMART
	dev->do_ioctl		= sl_ioctl;
#endif
	dev->hard_header_len	= 0;
	dev->addr_len		= 0;
	dev->tx_queue_len	= 10;

	SET_MODULE_OWNER(dev);

	/* New-style flags. */
	dev->flags		= IFF_NOARP|IFF_POINTOPOINT|IFF_MULTICAST;
}

/******************************************
  Routines looking at TTY side.
 ******************************************/


static int slip_receive_room(struct tty_struct *tty)
{
	return 65536;  /* We can handle an infinite amount of data. :-) */
}

/*
 * Handle the 'receiver data ready' interrupt.
 * This function is called by the 'tty_io' module in the kernel when
 * a block of SLIP data has been received, which can now be decapsulated
 * and sent on to some IP layer for further processing. This will not
 * be re-entered while running but other ldisc functions may be called
 * in parallel
 */
 
static void slip_receive_buf(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	struct slip *sl = (struct slip *) tty->disc_data;

	if (!sl || sl->magic != SLIP_MAGIC ||
	    !netif_running(sl->dev))
		return;

	/* Read the characters out of the buffer */
	while (count--) {
		if (fp && *fp++) {
			if (!test_and_set_bit(SLF_ERROR, &sl->flags))  {
				sl->rx_errors++;
			}
			cp++;
			continue;
		}
#ifdef CONFIG_SLIP_MODE_SLIP6
		if (sl->mode & SL_MODE_SLIP6)
			slip_unesc6(sl, *cp++);
		else
#endif
			slip_unesc(sl, *cp++);
	}
}

/************************************
 *  slip_open helper routines.
 ************************************/

/* Collect hanged up channels */

static void sl_sync(void)
{
	int i;
	struct net_device *dev;
	struct slip	  *sl;

	for (i = 0; i < slip_maxdev; i++) {
		if ((dev = slip_devs[i]) == NULL)
			break;

		sl = netdev_priv(dev);
		if (sl->tty || sl->leased)
			continue;
		if (dev->flags&IFF_UP)
			dev_close(dev);
	}
}


/* Find a free SLIP channel, and link in this `tty' line. */
static struct slip *
sl_alloc(dev_t line)
{
	int i;
	int sel = -1;
	int score = -1;
	struct net_device *dev = NULL;
	struct slip       *sl;

	if (slip_devs == NULL) 
		return NULL;	/* Master array missing ! */

	for (i = 0; i < slip_maxdev; i++) {
		dev = slip_devs[i];
		if (dev == NULL)
			break;

		sl = netdev_priv(dev);
		if (sl->leased) {
			if (sl->line != line)
				continue;
			if (sl->tty)
				return NULL;

			/* Clear ESCAPE & ERROR flags */
			sl->flags &= (1 << SLF_INUSE);
			return sl;
		}

		if (sl->tty)
			continue;

		if (current->pid == sl->pid) {
			if (sl->line == line && score < 3) {
				sel = i;
				score = 3;
				continue;
			}
			if (score < 2) {
				sel = i;
				score = 2;
			}
			continue;
		}
		if (sl->line == line && score < 1) {
			sel = i;
			score = 1;
			continue;
		}
		if (score < 0) {
			sel = i;
			score = 0;
		}
	}

	if (sel >= 0) {
		i = sel;
		dev = slip_devs[i];
		if (score > 1) {
			sl = netdev_priv(dev);
			sl->flags &= (1 << SLF_INUSE);
			return sl;
		}
	}

	/* Sorry, too many, all slots in use */
	if (i >= slip_maxdev)
		return NULL;

	if (dev) {
		sl = netdev_priv(dev);
		if (test_bit(SLF_INUSE, &sl->flags)) {
			unregister_netdevice(dev);
			dev = NULL;
			slip_devs[i] = NULL;
		}
	}
	
	if (!dev) {
		char name[IFNAMSIZ];
		sprintf(name, "sl%d", i);

		dev = alloc_netdev(sizeof(*sl), name, sl_setup);
		if (!dev)
			return NULL;
		dev->base_addr  = i;
	}

	sl = netdev_priv(dev);

	/* Initialize channel control data */
	sl->magic       = SLIP_MAGIC;
	sl->dev	      	= dev;
	spin_lock_init(&sl->lock);
	sl->mode        = SL_MODE_DEFAULT;
#ifdef CONFIG_SLIP_SMART
	init_timer(&sl->keepalive_timer);	/* initialize timer_list struct */
	sl->keepalive_timer.data=(unsigned long)sl;
	sl->keepalive_timer.function=sl_keepalive;
	init_timer(&sl->outfill_timer);
	sl->outfill_timer.data=(unsigned long)sl;
	sl->outfill_timer.function=sl_outfill;
#endif
	slip_devs[i] = dev;
				   
	return sl;
}

/*
 * Open the high-level part of the SLIP channel.
 * This function is called by the TTY module when the
 * SLIP line discipline is called for.  Because we are
 * sure the tty line exists, we only have to link it to
 * a free SLIP channel...
 *
 * Called in process context serialized from other ldisc calls.
 */

static int slip_open(struct tty_struct *tty)
{
	struct slip *sl;
	int err;

	if(!capable(CAP_NET_ADMIN))
		return -EPERM;
		
	/* RTnetlink lock is misused here to serialize concurrent
	   opens of slip channels. There are better ways, but it is
	   the simplest one.
	 */
	rtnl_lock();

	/* Collect hanged up channels. */
	sl_sync();

	sl = (struct slip *) tty->disc_data;

	err = -EEXIST;
	/* First make sure we're not already connected. */
	if (sl && sl->magic == SLIP_MAGIC)
		goto err_exit;

	/* OK.  Find a free SLIP channel to use. */
	err = -ENFILE;
	if ((sl = sl_alloc(tty_devnum(tty))) == NULL)
		goto err_exit;

	sl->tty = tty;
	tty->disc_data = sl;
	sl->line = tty_devnum(tty);
	sl->pid = current->pid;
	
	/* FIXME: already done before we were called - seems this can go */
	if (tty->driver->flush_buffer)
		tty->driver->flush_buffer(tty);
		
	if (!test_bit(SLF_INUSE, &sl->flags)) {
		/* Perform the low-level SLIP initialization. */
		if ((err = sl_alloc_bufs(sl, SL_MTU)) != 0)
			goto err_free_chan;

		set_bit(SLF_INUSE, &sl->flags);

		if ((err = register_netdevice(sl->dev)))
			goto err_free_bufs;
	}

#ifdef CONFIG_SLIP_SMART
	if (sl->keepalive) {
		sl->keepalive_timer.expires=jiffies+sl->keepalive*HZ;
		add_timer (&sl->keepalive_timer);
	}
	if (sl->outfill) {
		sl->outfill_timer.expires=jiffies+sl->outfill*HZ;
		add_timer (&sl->outfill_timer);
	}
#endif

	/* Done.  We have linked the TTY line to a channel. */
	rtnl_unlock();
	return sl->dev->base_addr;

err_free_bufs:
	sl_free_bufs(sl);

err_free_chan:
	sl->tty = NULL;
	tty->disc_data = NULL;
	clear_bit(SLF_INUSE, &sl->flags);

err_exit:
	rtnl_unlock();

	/* Count references from TTY module */
	return err;
}

/*

  FIXME: 1,2 are fixed 3 was never true anyway.
  
   Let me to blame a bit.
   1. TTY module calls this funstion on soft interrupt.
   2. TTY module calls this function WITH MASKED INTERRUPTS!
   3. TTY module does not notify us about line discipline
      shutdown,

   Seems, now it is clean. The solution is to consider netdevice and
   line discipline sides as two independent threads.

   By-product (not desired): sl? does not feel hangups and remains open.
   It is supposed, that user level program (dip, diald, slattach...)
   will catch SIGHUP and make the rest of work. 

   I see no way to make more with current tty code. --ANK
 */

/*
 * Close down a SLIP channel.
 * This means flushing out any pending queues, and then returning. This
 * call is serialized against other ldisc functions.
 */
static void
slip_close(struct tty_struct *tty)
{
	struct slip *sl = (struct slip *) tty->disc_data;

	/* First make sure we're connected. */
	if (!sl || sl->magic != SLIP_MAGIC || sl->tty != tty)
		return;

	tty->disc_data = NULL;
	sl->tty = NULL;
	if (!sl->leased)
		sl->line = 0;

	/* VSV = very important to remove timers */
#ifdef CONFIG_SLIP_SMART
	del_timer_sync(&sl->keepalive_timer);
	del_timer_sync(&sl->outfill_timer);
#endif

	/* Count references from TTY module */
}

 /************************************************************************
  *			STANDARD SLIP ENCAPSULATION		  	 *
  ************************************************************************/

int
slip_esc(unsigned char *s, unsigned char *d, int len)
{
	unsigned char *ptr = d;
	unsigned char c;

	/*
	 * Send an initial END character to flush out any
	 * data that may have accumulated in the receiver
	 * due to line noise.
	 */

	*ptr++ = END;

	/*
	 * For each byte in the packet, send the appropriate
	 * character sequence, according to the SLIP protocol.
	 */

	while (len-- > 0) {
		switch(c = *s++) {
		 case END:
			*ptr++ = ESC;
			*ptr++ = ESC_END;
			break;
		 case ESC:
			*ptr++ = ESC;
			*ptr++ = ESC_ESC;
			break;
		 default:
			*ptr++ = c;
			break;
		}
	}
	*ptr++ = END;
	return (ptr - d);
}

static void slip_unesc(struct slip *sl, unsigned char s)
{

	switch(s) {
	 case END:
#ifdef CONFIG_SLIP_SMART
		/* drop keeptest bit = VSV */
		if (test_bit(SLF_KEEPTEST, &sl->flags))
			clear_bit(SLF_KEEPTEST, &sl->flags);
#endif

		if (!test_and_clear_bit(SLF_ERROR, &sl->flags) && (sl->rcount > 2))  {
			sl_bump(sl);
		}
		clear_bit(SLF_ESCAPE, &sl->flags);
		sl->rcount = 0;
		return;

	 case ESC:
		set_bit(SLF_ESCAPE, &sl->flags);
		return;
	 case ESC_ESC:
		if (test_and_clear_bit(SLF_ESCAPE, &sl->flags))  {
			s = ESC;
		}
		break;
	 case ESC_END:
		if (test_and_clear_bit(SLF_ESCAPE, &sl->flags))  {
			s = END;
		}
		break;
	}
	if (!test_bit(SLF_ERROR, &sl->flags))  {
		if (sl->rcount < sl->buffsize)  {
			sl->rbuff[sl->rcount++] = s;
			return;
		}
		sl->rx_over_errors++;
		set_bit(SLF_ERROR, &sl->flags);
	}
}


#ifdef CONFIG_SLIP_MODE_SLIP6
/************************************************************************
 *			 6 BIT SLIP ENCAPSULATION			*
 ************************************************************************/

int
slip_esc6(unsigned char *s, unsigned char *d, int len)
{
	unsigned char *ptr = d;
	unsigned char c;
	int i;
	unsigned short v = 0;
	short bits = 0;

	/*
	 * Send an initial END character to flush out any
	 * data that may have accumulated in the receiver
	 * due to line noise.
	 */

	*ptr++ = 0x70;

	/*
	 * Encode the packet into printable ascii characters
	 */

	for (i = 0; i < len; ++i) {
		v = (v << 8) | s[i];
		bits += 8;
		while (bits >= 6) {
			bits -= 6;
			c = 0x30 + ((v >> bits) & 0x3F);
			*ptr++ = c;
		}
	}
	if (bits) {
		c = 0x30 + ((v << (6 - bits)) & 0x3F);
		*ptr++ = c;
	}
	*ptr++ = 0x70;
	return ptr - d;
}

void
slip_unesc6(struct slip *sl, unsigned char s)
{
	unsigned char c;

	if (s == 0x70) {
#ifdef CONFIG_SLIP_SMART
		/* drop keeptest bit = VSV */
		if (test_bit(SLF_KEEPTEST, &sl->flags))
			clear_bit(SLF_KEEPTEST, &sl->flags);
#endif

		if (!test_and_clear_bit(SLF_ERROR, &sl->flags) && (sl->rcount > 2))  {
			sl_bump(sl);
		}
		sl->rcount = 0;
		sl->xbits = 0;
		sl->xdata = 0;
 	} else if (s >= 0x30 && s < 0x70) {
		sl->xdata = (sl->xdata << 6) | ((s - 0x30) & 0x3F);
		sl->xbits += 6;
		if (sl->xbits >= 8) {
			sl->xbits -= 8;
			c = (unsigned char)(sl->xdata >> sl->xbits);
			if (!test_bit(SLF_ERROR, &sl->flags))  {
				if (sl->rcount < sl->buffsize)  {
					sl->rbuff[sl->rcount++] = c;
					return;
				}
				sl->rx_over_errors++;
				set_bit(SLF_ERROR, &sl->flags);
			}
		}
 	}
}
#endif /* CONFIG_SLIP_MODE_SLIP6 */

/* Perform I/O control on an active SLIP channel. */
static int slip_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct slip *sl = (struct slip *) tty->disc_data;
	unsigned int tmp;
	int __user *p = (int __user *)arg;

	/* First make sure we're connected. */
	if (!sl || sl->magic != SLIP_MAGIC) {
		return -EINVAL;
	}

	switch(cmd) {
	 case SIOCGIFNAME:
		tmp = strlen(sl->dev->name) + 1;
		if (copy_to_user((void __user *)arg, sl->dev->name, tmp))
			return -EFAULT;
		return 0;

	case SIOCGIFENCAP:
		if (put_user(sl->mode, p))
			return -EFAULT;
		return 0;

	case SIOCSIFENCAP:
		if (get_user(tmp, p))
			return -EFAULT;
#ifndef SL_INCLUDE_CSLIP
		if (tmp & (SL_MODE_CSLIP|SL_MODE_ADAPTIVE))  {
			return -EINVAL;
		}
#else
		if ((tmp & (SL_MODE_ADAPTIVE | SL_MODE_CSLIP)) ==
		    (SL_MODE_ADAPTIVE | SL_MODE_CSLIP))  {
			/* return -EINVAL; */
			tmp &= ~SL_MODE_ADAPTIVE;
		}
#endif
#ifndef CONFIG_SLIP_MODE_SLIP6
		if (tmp & SL_MODE_SLIP6)  {
			return -EINVAL;
		}
#endif
		sl->mode = tmp;
		sl->dev->type = ARPHRD_SLIP+sl->mode;
		return 0;

	 case SIOCSIFHWADDR:
		return -EINVAL;

#ifdef CONFIG_SLIP_SMART
	/* VSV changes start here */
        case SIOCSKEEPALIVE:
		if (get_user(tmp, p))
			return -EFAULT;
                if (tmp > 255) /* max for unchar */
			return -EINVAL;

		spin_lock_bh(&sl->lock);
		if (!sl->tty) {
			spin_unlock_bh(&sl->lock);
			return -ENODEV;
		}
		if ((sl->keepalive = (unchar) tmp) != 0) {
			mod_timer(&sl->keepalive_timer, jiffies+sl->keepalive*HZ);
			set_bit(SLF_KEEPTEST, &sl->flags);
                } else {
                        del_timer (&sl->keepalive_timer);
		}
		spin_unlock_bh(&sl->lock);
		return 0;

        case SIOCGKEEPALIVE:
		if (put_user(sl->keepalive, p))
			return -EFAULT;
		return 0;

        case SIOCSOUTFILL:
		if (get_user(tmp, p))
			return -EFAULT;
                if (tmp > 255) /* max for unchar */
			return -EINVAL;
		spin_lock_bh(&sl->lock);
		if (!sl->tty) {
			spin_unlock_bh(&sl->lock);
			return -ENODEV;
		}
                if ((sl->outfill = (unchar) tmp) != 0){
			mod_timer(&sl->outfill_timer, jiffies+sl->outfill*HZ);
			set_bit(SLF_OUTWAIT, &sl->flags);
		} else {
                        del_timer (&sl->outfill_timer);
		}
		spin_unlock_bh(&sl->lock);
                return 0;

        case SIOCGOUTFILL:
		if (put_user(sl->outfill, p))
			return -EFAULT;
		return 0;
	/* VSV changes end */
#endif

	/* Allow stty to read, but not set, the serial port */
	case TCGETS:
	case TCGETA:
		return n_tty_ioctl(tty, file, cmd, arg);

	default:
		return -ENOIOCTLCMD;
	}
}

/* VSV changes start here */
#ifdef CONFIG_SLIP_SMART
/* function do_ioctl called from net/core/dev.c
   to allow get/set outfill/keepalive parameter
   by ifconfig                                 */

static int sl_ioctl(struct net_device *dev,struct ifreq *rq,int cmd)
{
	struct slip *sl = netdev_priv(dev);
	unsigned long *p = (unsigned long *)&rq->ifr_ifru;

	if (sl == NULL)		/* Allocation failed ?? */
		return -ENODEV;

	spin_lock_bh(&sl->lock);

	if (!sl->tty) {
		spin_unlock_bh(&sl->lock);
		return -ENODEV;
	}

	switch(cmd){
        case SIOCSKEEPALIVE:
		/* max for unchar */
                if ((unsigned)*p > 255) {
			spin_unlock_bh(&sl->lock);
			return -EINVAL;
		}
		sl->keepalive = (unchar) *p;
		if (sl->keepalive != 0) {
			sl->keepalive_timer.expires=jiffies+sl->keepalive*HZ;
			mod_timer(&sl->keepalive_timer, jiffies+sl->keepalive*HZ);
			set_bit(SLF_KEEPTEST, &sl->flags);
                } else {
                        del_timer(&sl->keepalive_timer);
		}
		break;

        case SIOCGKEEPALIVE:
		*p = sl->keepalive;
		break;

        case SIOCSOUTFILL:
                if ((unsigned)*p > 255) { /* max for unchar */
			spin_unlock_bh(&sl->lock);
			return -EINVAL;
		}
                if ((sl->outfill = (unchar)*p) != 0){
			mod_timer(&sl->outfill_timer, jiffies+sl->outfill*HZ);
			set_bit(SLF_OUTWAIT, &sl->flags);
		} else {
                        del_timer (&sl->outfill_timer);
		}
                break;

        case SIOCGOUTFILL:
		*p = sl->outfill;
		break;

        case SIOCSLEASE:
		/* Resolve race condition, when ioctl'ing hanged up 
		   and opened by another process device.
		 */
		if (sl->tty != current->signal->tty && sl->pid != current->pid) {
			spin_unlock_bh(&sl->lock);
			return -EPERM;
		}
		sl->leased = 0;
                if (*p)
			sl->leased = 1;
                break;

        case SIOCGLEASE:
		*p = sl->leased;
	};
	spin_unlock_bh(&sl->lock);
	return 0;
}
#endif
/* VSV changes end */

static struct tty_ldisc	sl_ldisc = {
	.owner 		= THIS_MODULE,
	.magic 		= TTY_LDISC_MAGIC,
	.name 		= "slip",
	.open 		= slip_open,
	.close	 	= slip_close,
	.ioctl		= slip_ioctl,
	.receive_buf	= slip_receive_buf,
	.receive_room	= slip_receive_room,
	.write_wakeup	= slip_write_wakeup,
};

static int __init slip_init(void)
{
	int status;

	if (slip_maxdev < 4)
		slip_maxdev = 4; /* Sanity */

	printk(KERN_INFO "SLIP: version %s (dynamic channels, max=%d)"
#ifdef CONFIG_SLIP_MODE_SLIP6
	       " (6 bit encapsulation enabled)"
#endif
	       ".\n",
	       SLIP_VERSION, slip_maxdev );
#if defined(SL_INCLUDE_CSLIP)
	printk(KERN_INFO "CSLIP: code copyright 1989 Regents of the University of California.\n");
#endif
#ifdef CONFIG_SLIP_SMART
	printk(KERN_INFO "SLIP linefill/keepalive option.\n");
#endif

	slip_devs = kmalloc(sizeof(struct net_device *)*slip_maxdev, GFP_KERNEL);
	if (!slip_devs) {
		printk(KERN_ERR "SLIP: Can't allocate slip devices array!  Uaargh! (-> No SLIP available)\n");
		return -ENOMEM;
	}

	/* Clear the pointer array, we allocate devices when we need them */
	memset(slip_devs, 0, sizeof(struct net_device *)*slip_maxdev); 

	/* Fill in our line protocol discipline, and register it */
	if ((status = tty_register_ldisc(N_SLIP, &sl_ldisc)) != 0)  {
		printk(KERN_ERR "SLIP: can't register line discipline (err = %d)\n", status);
		kfree(slip_devs);
	}
	return status;
}

static void __exit slip_exit(void)
{
	int i;
	struct net_device *dev;
	struct slip *sl;
	unsigned long timeout = jiffies + HZ;
	int busy = 0;

	if (slip_devs == NULL) 
		return;

	/* First of all: check for active disciplines and hangup them.
	 */
	do {
		if (busy) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ / 10);
		}

		busy = 0;
		for (i = 0; i < slip_maxdev; i++) {
			dev = slip_devs[i];
			if (!dev)
				continue;
			sl = netdev_priv(dev);
			spin_lock_bh(&sl->lock);
			if (sl->tty) {
				busy++;
				tty_hangup(sl->tty);
			}
			spin_unlock_bh(&sl->lock);
		}
	} while (busy && time_before(jiffies, timeout));


	for (i = 0; i < slip_maxdev; i++) {
		dev = slip_devs[i];
		if (!dev)
			continue;
		slip_devs[i] = NULL;

		sl = netdev_priv(dev);
		if (sl->tty) {
			printk(KERN_ERR "%s: tty discipline still running\n",
			       dev->name);
			/* Intentionally leak the control block. */
			dev->destructor = NULL;
		} 

		unregister_netdev(dev);
	}

	kfree(slip_devs);
	slip_devs = NULL;

	if ((i = tty_register_ldisc(N_SLIP, NULL)))
	{
		printk(KERN_ERR "SLIP: can't unregister line discipline (err = %d)\n", i);
	}
}

module_init(slip_init);
module_exit(slip_exit);

#ifdef CONFIG_SLIP_SMART
/*
 * This is start of the code for multislip style line checking
 * added by Stanislav Voronyi. All changes before marked VSV
 */

static void sl_outfill(unsigned long sls)
{
	struct slip *sl=(struct slip *)sls;

	spin_lock(&sl->lock);

	if (sl->tty == NULL)
		goto out;

	if(sl->outfill)
	{
		if( test_bit(SLF_OUTWAIT, &sl->flags) )
		{
			/* no packets were transmitted, do outfill */
#ifdef CONFIG_SLIP_MODE_SLIP6
			unsigned char s = (sl->mode & SL_MODE_SLIP6)?0x70:END;
#else
			unsigned char s = END;
#endif
			/* put END into tty queue. Is it right ??? */
			if (!netif_queue_stopped(sl->dev))
			{
				/* if device busy no outfill */
				sl->tty->driver->write(sl->tty, &s, 1);
			}
		}
		else
			set_bit(SLF_OUTWAIT, &sl->flags);

		mod_timer(&sl->outfill_timer, jiffies+sl->outfill*HZ);
	}
out:
	spin_unlock(&sl->lock);
}

static void sl_keepalive(unsigned long sls)
{
	struct slip *sl=(struct slip *)sls;

	spin_lock(&sl->lock);

	if (sl->tty == NULL)
		goto out;

	if( sl->keepalive)
	{
		if(test_bit(SLF_KEEPTEST, &sl->flags))
		{
			/* keepalive still high :(, we must hangup */
			if( sl->outfill ) /* outfill timer must be deleted too */
				(void)del_timer(&sl->outfill_timer);
			printk(KERN_DEBUG "%s: no packets received during keepalive timeout, hangup.\n", sl->dev->name);
			tty_hangup(sl->tty); /* this must hangup tty & close slip */
			/* I think we need not something else */
			goto out;
		}
		else
			set_bit(SLF_KEEPTEST, &sl->flags);

		mod_timer(&sl->keepalive_timer, jiffies+sl->keepalive*HZ);
	}

out:
	spin_unlock(&sl->lock);
}

#endif
MODULE_LICENSE("GPL");
MODULE_ALIAS_LDISC(N_SLIP);
