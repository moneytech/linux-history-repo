/*
   sis190.c: Silicon Integrated Systems SiS190 ethernet driver

   Copyright (c) 2003 K.M. Liu <kmliu@sis.com>
   Copyright (c) 2003, 2004 Jeff Garzik <jgarzik@pobox.com>
   Copyright (c) 2003, 2004, 2005 Francois Romieu <romieu@fr.zoreil.com>

   Based on r8169.c, tg3.c, 8139cp.c, skge.c and probably even epic100.c.

   This software may be used and distributed according to the terms of
   the GNU General Public License (GPL), incorporated herein by reference.
   Drivers based on or derived from this code fall under the GPL and must
   retain the authorship, copyright and license notice.  This file is not
   a complete program and may only be used when the entire operating
   system is licensed under the GPL.

   See the file COPYING in this distribution for more information.

 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/pci.h>
#include <linux/mii.h>
#include <linux/delay.h>
#include <linux/crc32.h>
#include <linux/dma-mapping.h>
#include <asm/irq.h>

#define net_drv(p, arg...)	if (netif_msg_drv(p)) \
					printk(arg)
#define net_probe(p, arg...)	if (netif_msg_probe(p)) \
					printk(arg)
#define net_link(p, arg...)	if (netif_msg_link(p)) \
					printk(arg)
#define net_intr(p, arg...)	if (netif_msg_intr(p)) \
					printk(arg)
#define net_tx_err(p, arg...)	if (netif_msg_tx_err(p)) \
					printk(arg)

#ifdef CONFIG_SIS190_NAPI
#define NAPI_SUFFIX	"-NAPI"
#else
#define NAPI_SUFFIX	""
#endif

#define DRV_VERSION		"1.2" NAPI_SUFFIX
#define DRV_NAME		"sis190"
#define SIS190_DRIVER_NAME	DRV_NAME " Gigabit Ethernet driver " DRV_VERSION
#define PFX DRV_NAME ": "

#ifdef CONFIG_SIS190_NAPI
#define sis190_rx_skb			netif_receive_skb
#define sis190_rx_quota(count, quota)	min(count, quota)
#else
#define sis190_rx_skb			netif_rx
#define sis190_rx_quota(count, quota)	count
#endif

#define MAC_ADDR_LEN		6

#define NUM_TX_DESC		64
#define NUM_RX_DESC		64
#define TX_RING_BYTES		(NUM_TX_DESC * sizeof(struct TxDesc))
#define RX_RING_BYTES		(NUM_RX_DESC * sizeof(struct RxDesc))
#define RX_BUF_SIZE		1536

#define SIS190_REGS_SIZE	0x80
#define SIS190_TX_TIMEOUT	(6*HZ)
#define SIS190_PHY_TIMEOUT	(10*HZ)
#define SIS190_MSG_DEFAULT	(NETIF_MSG_DRV | NETIF_MSG_PROBE | \
				 NETIF_MSG_LINK | NETIF_MSG_IFUP | \
				 NETIF_MSG_IFDOWN)

/* Enhanced PHY access register bit definitions */
#define EhnMIIread		0x0000
#define EhnMIIwrite		0x0020
#define EhnMIIdataShift		16
#define EhnMIIpmdShift		6	/* 7016 only */
#define EhnMIIregShift		11
#define EhnMIIreq		0x0010
#define EhnMIInotDone		0x0010

/* Write/read MMIO register */
#define SIS_W8(reg, val)	writeb ((val), ioaddr + (reg))
#define SIS_W16(reg, val)	writew ((val), ioaddr + (reg))
#define SIS_W32(reg, val)	writel ((val), ioaddr + (reg))
#define SIS_R8(reg)		readb (ioaddr + (reg))
#define SIS_R16(reg)		readw (ioaddr + (reg))
#define SIS_R32(reg)		readl (ioaddr + (reg))

#define SIS_PCI_COMMIT()	SIS_R32(IntrControl)

enum sis190_registers {
	TxControl		= 0x00,
	TxDescStartAddr		= 0x04,
	TxNextDescAddr		= 0x0c,	// unused
	RxControl		= 0x10,
	RxDescStartAddr		= 0x14,
	RxNextDescAddr		= 0x1c,	// unused
	IntrStatus		= 0x20,
	IntrMask		= 0x24,
	IntrControl		= 0x28,
	IntrTimer		= 0x2c,	// unused
	PMControl		= 0x30,	// unused
	ROMControl		= 0x38,
	ROMInterface		= 0x3c,
	StationControl		= 0x40,
	GMIIControl		= 0x44,
	TxMacControl		= 0x50,
	RxMacControl		= 0x60,
	RxMacAddr		= 0x62,
	RxHashTable		= 0x68,
	// Undocumented		= 0x6c,
	RxWakeOnLan		= 0x70,
	// Undocumented		= 0x74,
	RxMPSControl		= 0x78,	// unused
};

enum sis190_register_content {
	/* IntrStatus */
	SoftInt			= 0x40000000,	// unused
	Timeup			= 0x20000000,	// unused
	PauseFrame		= 0x00080000,	// unused
	MagicPacket		= 0x00040000,	// unused
	WakeupFrame		= 0x00020000,	// unused
	LinkChange		= 0x00010000,
	RxQEmpty		= 0x00000080,
	RxQInt			= 0x00000040,
	TxQ1Empty		= 0x00000020,	// unused
	TxQ1Int			= 0x00000010,
	TxQ0Empty		= 0x00000008,	// unused
	TxQ0Int			= 0x00000004,
	RxHalt			= 0x00000002,
	TxHalt			= 0x00000001,

	/* RxStatusDesc */
	RxRES			= 0x00200000,	// unused
	RxCRC			= 0x00080000,
	RxRUNT			= 0x00100000,	// unused
	RxRWT			= 0x00400000,	// unused

	/* {Rx/Tx}CmdBits */
	CmdReset		= 0x10,
	CmdRxEnb		= 0x08,		// unused
	CmdTxEnb		= 0x01,
	RxBufEmpty		= 0x01,		// unused

	/* Cfg9346Bits */
	Cfg9346_Lock		= 0x00,		// unused
	Cfg9346_Unlock		= 0xc0,		// unused

	/* RxMacControl */
	AcceptErr		= 0x20,		// unused
	AcceptRunt		= 0x10,		// unused
	AcceptBroadcast		= 0x0800,
	AcceptMulticast		= 0x0400,
	AcceptMyPhys		= 0x0200,
	AcceptAllPhys		= 0x0100,

	/* RxConfigBits */
	RxCfgFIFOShift		= 13,
	RxCfgDMAShift		= 8,		// 0x1a in RxControl ?

	/* TxConfigBits */
	TxInterFrameGapShift	= 24,
	TxDMAShift		= 8, /* DMA burst value (0-7) is shift this many bits */

	/* StationControl */
	_1000bpsF		= 0x1c00,
	_1000bpsH		= 0x0c00,
	_100bpsF		= 0x1800,
	_100bpsH		= 0x0800,
	_10bpsF			= 0x1400,
	_10bpsH			= 0x0400,

	LinkStatus		= 0x02,		// unused
	FullDup			= 0x01,		// unused

	/* TBICSRBit */
	TBILinkOK		= 0x02000000,	// unused
};

struct TxDesc {
	u32 PSize;
	u32 status;
	u32 addr;
	u32 size;
};

struct RxDesc {
	u32 PSize;
	u32 status;
	u32 addr;
	u32 size;
};

enum _DescStatusBit {
	/* _Desc.status */
	OWNbit		= 0x80000000,
	INTbit		= 0x40000000,
	DEFbit		= 0x00200000,
	CRCbit		= 0x00020000,
	PADbit		= 0x00010000,
	/* _Desc.size */
	RingEnd		= (1 << 31),
	/* _Desc.PSize */
	RxSizeMask	= 0x0000ffff
};

struct sis190_private {
	void __iomem *mmio_addr;
	struct pci_dev *pci_dev;
	struct net_device_stats stats;
	spinlock_t lock;
	u32 rx_buf_sz;
	u32 cur_rx;
	u32 cur_tx;
	u32 dirty_rx;
	u32 dirty_tx;
	dma_addr_t rx_dma;
	dma_addr_t tx_dma;
	struct RxDesc *RxDescRing;
	struct TxDesc *TxDescRing;
	struct sk_buff *Rx_skbuff[NUM_RX_DESC];
	struct sk_buff *Tx_skbuff[NUM_TX_DESC];
	struct work_struct phy_task;
	struct timer_list timer;
	u32 msg_enable;
	struct mii_if_info mii_if;
};

const static struct {
	const char *name;
	u8 version;		/* depend on docs */
	u32 RxConfigMask;	/* clear the bits supported by this chip */
} sis_chip_info[] = {
	{ DRV_NAME, 0x00, 0xff7e1880, },
};

static struct pci_device_id sis190_pci_tbl[] __devinitdata = {
	{ PCI_DEVICE(PCI_VENDOR_ID_SI, 0x0190), 0, 0, 0 },
	{ 0, },
};

MODULE_DEVICE_TABLE(pci, sis190_pci_tbl);

static int rx_copybreak = 200;

static struct {
	u32 msg_enable;
} debug = { -1 };

MODULE_DESCRIPTION("SiS sis190 Gigabit Ethernet driver");
module_param(rx_copybreak, int, 0);
MODULE_PARM_DESC(rx_copybreak, "Copy breakpoint for copy-only-tiny-frames");
module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level (0=none, ..., 16=all)");
MODULE_AUTHOR("K.M. Liu <kmliu@sis.com>, Ueimor <romieu@fr.zoreil.com>");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");

static const u32 sis190_intr_mask =
	RxQEmpty | RxQInt | TxQ1Int | TxQ0Int | RxHalt | TxHalt;

/*
 * Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
 * The chips use a 64 element hash table based on the Ethernet CRC.
 */
static int multicast_filter_limit = 32;

static void __mdio_cmd(void __iomem *ioaddr, u32 ctl)
{
	unsigned int i;

	SIS_W32(GMIIControl, ctl);

	msleep(1);

	for (i = 0; i < 100; i++) {
		if (!(SIS_R32(GMIIControl) & EhnMIInotDone))
			break;
		msleep(1);
	}

	if (i > 999)
		printk(KERN_ERR PFX "PHY command failed !\n");
}

static void mdio_write(void __iomem *ioaddr, int reg, int val)
{
	u32 pmd = 1;

	__mdio_cmd(ioaddr, EhnMIIreq | EhnMIIwrite |
		(((u32) reg) << EhnMIIregShift) | (pmd << EhnMIIpmdShift) |
		(((u32) val) << EhnMIIdataShift));
}

static int mdio_read(void __iomem *ioaddr, int reg)
{
	u32 pmd = 1;

	__mdio_cmd(ioaddr, EhnMIIreq | EhnMIIread |
		(((u32) reg) << EhnMIIregShift) | (pmd << EhnMIIpmdShift));

	return (u16) (SIS_R32(GMIIControl) >> EhnMIIdataShift);
}

static void __mdio_write(struct net_device *dev, int phy_id, int reg, int val)
{
	struct sis190_private *tp = netdev_priv(dev);

	mdio_write(tp->mmio_addr, reg, val);
}

static int __mdio_read(struct net_device *dev, int phy_id, int reg)
{
	struct sis190_private *tp = netdev_priv(dev);

	return mdio_read(tp->mmio_addr, reg);
}

static int sis190_read_eeprom(void __iomem *ioaddr, u32 reg)
{
	unsigned int i;
	u16 data;
	u32 val;

	if (!(SIS_R32(ROMControl) & 0x0002))
		return 0;

	val = (0x0080 | (0x2 << 8) | (reg << 10));

	SIS_W32(ROMInterface, val);

	for (i = 0; i < 200; i++) {
		if (!(SIS_R32(ROMInterface) & 0x0080))
			break;
		msleep(1);
	}

	data = (u16) ((SIS_R32(ROMInterface) & 0xffff0000) >> 16);

	return data;
}

static void sis190_irq_mask_and_ack(void __iomem *ioaddr)
{
	SIS_W32(IntrMask, 0x00);
	SIS_W32(IntrStatus, 0xffffffff);
	SIS_PCI_COMMIT();
}

static void sis190_asic_down(void __iomem *ioaddr)
{
	/* Stop the chip's Tx and Rx DMA processes. */

	SIS_W32(TxControl, 0x1a00);
	SIS_W32(RxControl, 0x1a00);

	sis190_irq_mask_and_ack(ioaddr);
}

static void sis190_mark_as_last_descriptor(struct RxDesc *desc)
{
	desc->size |= cpu_to_le32(RingEnd);
}

static inline void sis190_give_to_asic(struct RxDesc *desc, u32 rx_buf_sz)
{
	u32 eor = le32_to_cpu(desc->size) & RingEnd;

	desc->PSize = 0x0;
	desc->size = cpu_to_le32(rx_buf_sz | eor);
	wmb();
	desc->status = cpu_to_le32(OWNbit | INTbit);
}

static inline void sis190_map_to_asic(struct RxDesc *desc, dma_addr_t mapping,
				      u32 rx_buf_sz)
{
	desc->addr = cpu_to_le32(mapping);
	sis190_give_to_asic(desc, rx_buf_sz);
}

static inline void sis190_make_unusable_by_asic(struct RxDesc *desc)
{
	desc->PSize = 0x0;
	desc->addr = 0xdeadbeef;
	desc->size &= cpu_to_le32(RingEnd);
	wmb();
	desc->status = 0x0;
}

static int sis190_alloc_rx_skb(struct pci_dev *pdev, struct sk_buff **sk_buff,
			       struct RxDesc *desc, u32 rx_buf_sz)
{
	struct sk_buff *skb;
	dma_addr_t mapping;
	int ret = 0;

	skb = dev_alloc_skb(rx_buf_sz);
	if (!skb)
		goto err_out;

	*sk_buff = skb;

	mapping = pci_map_single(pdev, skb->data, rx_buf_sz,
				 PCI_DMA_FROMDEVICE);

	sis190_map_to_asic(desc, mapping, rx_buf_sz);
out:
	return ret;

err_out:
	ret = -ENOMEM;
	sis190_make_unusable_by_asic(desc);
	goto out;
}

static u32 sis190_rx_fill(struct sis190_private *tp, struct net_device *dev,
			  u32 start, u32 end)
{
	u32 cur;

	for (cur = start; cur < end; cur++) {
		int ret, i = cur % NUM_RX_DESC;

		if (tp->Rx_skbuff[i])
			continue;

		ret = sis190_alloc_rx_skb(tp->pci_dev, tp->Rx_skbuff + i,
					  tp->RxDescRing + i, tp->rx_buf_sz);
		if (ret < 0)
			break;
	}
	return cur - start;
}

static inline int sis190_try_rx_copy(struct sk_buff **sk_buff, int pkt_size,
				     struct RxDesc *desc, int rx_buf_sz)
{
	int ret = -1;

	if (pkt_size < rx_copybreak) {
		struct sk_buff *skb;

		skb = dev_alloc_skb(pkt_size + NET_IP_ALIGN);
		if (skb) {
			skb_reserve(skb, NET_IP_ALIGN);
			eth_copy_and_sum(skb, sk_buff[0]->data, pkt_size, 0);
			*sk_buff = skb;
			sis190_give_to_asic(desc, rx_buf_sz);
			ret = 0;
		}
	}
	return ret;
}

static int sis190_rx_interrupt(struct net_device *dev,
			       struct sis190_private *tp, void __iomem *ioaddr)
{
	struct net_device_stats *stats = &tp->stats;
	u32 rx_left, cur_rx = tp->cur_rx;
	u32 delta, count;

	rx_left = NUM_RX_DESC + tp->dirty_rx - cur_rx;
	rx_left = sis190_rx_quota(rx_left, (u32) dev->quota);

	for (; rx_left > 0; rx_left--, cur_rx++) {
		unsigned int entry = cur_rx % NUM_RX_DESC;
		struct RxDesc *desc = tp->RxDescRing + entry;
		u32 status;

		if (desc->status & OWNbit)
			break;

		status = le32_to_cpu(desc->PSize);

		// net_intr(tp, KERN_INFO "%s: Rx PSize = %08x.\n", dev->name,
		//	 status);

		if (status & RxCRC) {
			net_intr(tp, KERN_INFO "%s: bad crc. status = %08x.\n",
				 dev->name, status);
			stats->rx_errors++;
			stats->rx_crc_errors++;
			sis190_give_to_asic(desc, tp->rx_buf_sz);
		} else if (!(status & PADbit)) {
			net_intr(tp, KERN_INFO "%s: bad pad. status = %08x.\n",
				 dev->name, status);
			stats->rx_errors++;
			stats->rx_length_errors++;
			sis190_give_to_asic(desc, tp->rx_buf_sz);
		} else {
			struct sk_buff *skb = tp->Rx_skbuff[entry];
			int pkt_size = (status & RxSizeMask) - 4;
			void (*pci_action)(struct pci_dev *, dma_addr_t,
				size_t, int) = pci_dma_sync_single_for_device;

			if (unlikely(pkt_size > tp->rx_buf_sz)) {
				net_intr(tp, KERN_INFO
					 "%s: (frag) status = %08x.\n",
					 dev->name, status);
				stats->rx_dropped++;
				stats->rx_length_errors++;
				sis190_give_to_asic(desc, tp->rx_buf_sz);
				continue;
			}

			pci_dma_sync_single_for_cpu(tp->pci_dev,
				le32_to_cpu(desc->addr), tp->rx_buf_sz,
				PCI_DMA_FROMDEVICE);

			if (sis190_try_rx_copy(&skb, pkt_size, desc,
					       tp->rx_buf_sz)) {
				pci_action = pci_unmap_single;
				tp->Rx_skbuff[entry] = NULL;
				sis190_make_unusable_by_asic(desc);
			}

			pci_action(tp->pci_dev, le32_to_cpu(desc->addr),
				   tp->rx_buf_sz, PCI_DMA_FROMDEVICE);

			skb->dev = dev;
			skb_put(skb, pkt_size);
			skb->protocol = eth_type_trans(skb, dev);

			sis190_rx_skb(skb);

			dev->last_rx = jiffies;
			stats->rx_bytes += pkt_size;
			stats->rx_packets++;
		}
	}
	count = cur_rx - tp->cur_rx;
	tp->cur_rx = cur_rx;

	delta = sis190_rx_fill(tp, dev, tp->dirty_rx, tp->cur_rx);
	if (!delta && count && netif_msg_intr(tp))
		printk(KERN_INFO "%s: no Rx buffer allocated.\n", dev->name);
	tp->dirty_rx += delta;

	if (((tp->dirty_rx + NUM_RX_DESC) == tp->cur_rx) && netif_msg_intr(tp))
		printk(KERN_EMERG "%s: Rx buffers exhausted.\n", dev->name);

	return count;
}

static void sis190_unmap_tx_skb(struct pci_dev *pdev, struct sk_buff *skb,
				struct TxDesc *desc)
{
	unsigned int len;

	len = skb->len < ETH_ZLEN ? ETH_ZLEN : skb->len;

	pci_unmap_single(pdev, le32_to_cpu(desc->addr), len, PCI_DMA_TODEVICE);

	memset(desc, 0x00, sizeof(*desc));
}

static void sis190_tx_interrupt(struct net_device *dev,
				struct sis190_private *tp, void __iomem *ioaddr)
{
	u32 pending, dirty_tx = tp->dirty_tx;
	/*
	 * It would not be needed if queueing was allowed to be enabled
	 * again too early (hint: think preempt and unclocked smp systems).
	 */
	unsigned int queue_stopped;

	smp_rmb();
	pending = tp->cur_tx - dirty_tx;
	queue_stopped = (pending == NUM_TX_DESC);

	for (; pending; pending--, dirty_tx++) {
		unsigned int entry = dirty_tx % NUM_TX_DESC;
		struct TxDesc *txd = tp->TxDescRing + entry;
		struct sk_buff *skb;

		if (le32_to_cpu(txd->status) & OWNbit)
			break;

		skb = tp->Tx_skbuff[entry];

		tp->stats.tx_packets++;
		tp->stats.tx_bytes += skb->len;

		sis190_unmap_tx_skb(tp->pci_dev, skb, txd);
		tp->Tx_skbuff[entry] = NULL;
		dev_kfree_skb_irq(skb);
	}

	if (tp->dirty_tx != dirty_tx) {
		tp->dirty_tx = dirty_tx;
		smp_wmb();
		if (queue_stopped)
			netif_wake_queue(dev);
	}
}

/*
 * The interrupt handler does all of the Rx thread work and cleans up after
 * the Tx thread.
 */
static irqreturn_t sis190_interrupt(int irq, void *__dev, struct pt_regs *regs)
{
	struct net_device *dev = __dev;
	struct sis190_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned int handled = 0;
	u32 status;

	status = SIS_R32(IntrStatus);

	if ((status == 0xffffffff) || !status)
		goto out;

	handled = 1;

	if (unlikely(!netif_running(dev))) {
		sis190_asic_down(ioaddr);
		goto out;
	}

	SIS_W32(IntrStatus, status);

	// net_intr(tp, KERN_INFO "%s: status = %08x.\n", dev->name, status);

	if (status & LinkChange) {
		net_intr(tp, KERN_INFO "%s: link change.\n", dev->name);
		schedule_work(&tp->phy_task);
	}

	if (status & RxQInt)
		sis190_rx_interrupt(dev, tp, ioaddr);

	if (status & TxQ0Int)
		sis190_tx_interrupt(dev, tp, ioaddr);
out:
	return IRQ_RETVAL(handled);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void sis190_netpoll(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);
	struct pci_dev *pdev = tp->pci_dev;

	disable_irq(pdev->irq);
	sis190_interrupt(pdev->irq, dev, NULL);
	enable_irq(pdev->irq);
}
#endif

static void sis190_free_rx_skb(struct sis190_private *tp,
			       struct sk_buff **sk_buff, struct RxDesc *desc)
{
	struct pci_dev *pdev = tp->pci_dev;

	pci_unmap_single(pdev, le32_to_cpu(desc->addr), tp->rx_buf_sz,
			 PCI_DMA_FROMDEVICE);
	dev_kfree_skb(*sk_buff);
	*sk_buff = NULL;
	sis190_make_unusable_by_asic(desc);
}

static void sis190_rx_clear(struct sis190_private *tp)
{
	unsigned int i;

	for (i = 0; i < NUM_RX_DESC; i++) {
		if (!tp->Rx_skbuff[i])
			continue;
		sis190_free_rx_skb(tp, tp->Rx_skbuff + i, tp->RxDescRing + i);
	}
}

static void sis190_init_ring_indexes(struct sis190_private *tp)
{
	tp->dirty_tx = tp->dirty_rx = tp->cur_tx = tp->cur_rx = 0;
}

static int sis190_init_ring(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);

	sis190_init_ring_indexes(tp);

	memset(tp->Tx_skbuff, 0x0, NUM_TX_DESC * sizeof(struct sk_buff *));
	memset(tp->Rx_skbuff, 0x0, NUM_RX_DESC * sizeof(struct sk_buff *));

	if (sis190_rx_fill(tp, dev, 0, NUM_RX_DESC) != NUM_RX_DESC)
		goto err_rx_clear;

	sis190_mark_as_last_descriptor(tp->RxDescRing + NUM_RX_DESC - 1);

	return 0;

err_rx_clear:
	sis190_rx_clear(tp);
	return -ENOMEM;
}

static void sis190_set_rx_mode(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned long flags;
	u32 mc_filter[2];	/* Multicast hash filter */
	u16 rx_mode;

	if (dev->flags & IFF_PROMISC) {
		/* Unconditionally log net taps. */
		net_drv(tp, KERN_NOTICE "%s: Promiscuous mode enabled.\n",
			dev->name);
		rx_mode =
			AcceptBroadcast | AcceptMulticast | AcceptMyPhys |
			AcceptAllPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else if ((dev->mc_count > multicast_filter_limit) ||
		   (dev->flags & IFF_ALLMULTI)) {
		/* Too many to filter perfectly -- accept all multicasts. */
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else {
		struct dev_mc_list *mclist;
		unsigned int i;

		rx_mode = AcceptBroadcast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0;
		for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
		     i++, mclist = mclist->next) {
			int bit_nr =
				ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26;
			mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
			rx_mode |= AcceptMulticast;
		}
	}

	spin_lock_irqsave(&tp->lock, flags);

	SIS_W16(RxMacControl, rx_mode | 0x2);
	SIS_W32(RxHashTable, mc_filter[0]);
	SIS_W32(RxHashTable + 4, mc_filter[1]);

	spin_unlock_irqrestore(&tp->lock, flags);
}

static void sis190_soft_reset(void __iomem *ioaddr)
{
	SIS_W32(IntrControl, 0x8000);
	SIS_PCI_COMMIT();
	msleep(1);
	SIS_W32(IntrControl, 0x0);
	sis190_asic_down(ioaddr);
	msleep(1);
}

static void sis190_hw_start(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;

	sis190_soft_reset(ioaddr);

	SIS_W32(TxDescStartAddr, tp->tx_dma);
	SIS_W32(RxDescStartAddr, tp->rx_dma);

	SIS_W32(IntrStatus, 0xffffffff);
	SIS_W32(IntrMask, 0x0);
	/*
	 * Default is 100Mbps.
	 * A bit strange: 100Mbps is 0x1801 elsewhere -- FR 2005/06/09
	 */
	SIS_W16(StationControl, 0x1901);
	SIS_W32(GMIIControl, 0x0);
	SIS_W32(TxMacControl, 0x60);
	SIS_W16(RxMacControl, 0x02);
	SIS_W32(RxHashTable, 0x0);
	SIS_W32(0x6c, 0x0);
	SIS_W32(RxWakeOnLan, 0x0);
	SIS_W32(0x74, 0x0);

	SIS_PCI_COMMIT();

	sis190_set_rx_mode(dev);

	/* Enable all known interrupts by setting the interrupt mask. */
	SIS_W32(IntrMask, sis190_intr_mask);

	SIS_W32(TxControl, 0x1a00 | CmdTxEnb);
	SIS_W32(RxControl, 0x1a1d);

	netif_start_queue(dev);
}

static void sis190_phy_task(void * data)
{
	struct net_device *dev = data;
	struct sis190_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	u16 val;

	rtnl_lock();

	val = mdio_read(ioaddr, MII_BMCR);
	if (val & BMCR_RESET) {
		// FIXME: needlessly high ?  -- FR 02/07/2005
		mod_timer(&tp->timer, jiffies + HZ/10);
	} else if (!(mdio_read(ioaddr, MII_BMSR) & BMSR_ANEGCOMPLETE)) {
		net_link(tp, KERN_WARNING "%s: PHY reset until link up.\n",
			 dev->name);
		mdio_write(ioaddr, MII_BMCR, val | BMCR_RESET);
		mod_timer(&tp->timer, jiffies + SIS190_PHY_TIMEOUT);
	} else {
		/* Rejoice ! */
		struct {
			int val;
			const char *msg;
			u16 ctl;
		} reg31[] = {
			{ LPA_1000XFULL | LPA_SLCT,
				"1000 Mbps Full Duplex",
				0x01 | _1000bpsF },
			{ LPA_1000XHALF | LPA_SLCT,
				"1000 Mbps Half Duplex",
				0x01 | _1000bpsH },
			{ LPA_100FULL,
				"100 Mbps Full Duplex",
				0x01 | _100bpsF },
			{ LPA_100HALF,
				"100 Mbps Half Duplex",
				0x01 | _100bpsH },
			{ LPA_10FULL,
				"10 Mbps Full Duplex",
				0x01 | _10bpsF },
			{ LPA_10HALF,
				"10 Mbps Half Duplex",
				0x01 | _10bpsH },
			{ 0, "unknown", 0x0000 }
		}, *p;

		val = mdio_read(ioaddr, 0x1f);
		net_link(tp, KERN_INFO "%s: mii ext = %04x.\n", dev->name, val);

		val = mdio_read(ioaddr, MII_LPA);
		net_link(tp, KERN_INFO "%s: mii lpa = %04x.\n", dev->name, val);

		for (p = reg31; p->ctl; p++) {
			if ((val & p->val) == p->val)
				break;
		}
		if (p->ctl)
			SIS_W16(StationControl, p->ctl);
		net_link(tp, KERN_INFO "%s: link on %s mode.\n", dev->name,
			 p->msg);
		netif_carrier_on(dev);
	}

	rtnl_unlock();
}

static void sis190_phy_timer(unsigned long __opaque)
{
	struct net_device *dev = (struct net_device *)__opaque;
	struct sis190_private *tp = netdev_priv(dev);

	if (likely(netif_running(dev)))
		schedule_work(&tp->phy_task);
}

static inline void sis190_delete_timer(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);

	del_timer_sync(&tp->timer);
}

static inline void sis190_request_timer(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);
	struct timer_list *timer = &tp->timer;

	init_timer(timer);
	timer->expires = jiffies + SIS190_PHY_TIMEOUT;
	timer->data = (unsigned long)dev;
	timer->function = sis190_phy_timer;
	add_timer(timer);
}

static void sis190_set_rxbufsize(struct sis190_private *tp,
				 struct net_device *dev)
{
	unsigned int mtu = dev->mtu;

	tp->rx_buf_sz = (mtu > RX_BUF_SIZE) ? mtu + ETH_HLEN + 8 : RX_BUF_SIZE;
}

static int sis190_open(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);
	struct pci_dev *pdev = tp->pci_dev;
	int rc = -ENOMEM;

	sis190_set_rxbufsize(tp, dev);

	/*
	 * Rx and Tx descriptors need 256 bytes alignment.
	 * pci_alloc_consistent() guarantees a stronger alignment.
	 */
	tp->TxDescRing = pci_alloc_consistent(pdev, TX_RING_BYTES, &tp->tx_dma);
	if (!tp->TxDescRing)
		goto out;

	tp->RxDescRing = pci_alloc_consistent(pdev, RX_RING_BYTES, &tp->rx_dma);
	if (!tp->RxDescRing)
		goto err_free_tx_0;

	rc = sis190_init_ring(dev);
	if (rc < 0)
		goto err_free_rx_1;

	INIT_WORK(&tp->phy_task, sis190_phy_task, dev);

	sis190_request_timer(dev);

	rc = request_irq(dev->irq, sis190_interrupt, SA_SHIRQ, dev->name, dev);
	if (rc < 0)
		goto err_release_timer_2;

	sis190_hw_start(dev);
out:
	return rc;

err_release_timer_2:
	sis190_delete_timer(dev);
	sis190_rx_clear(tp);
err_free_rx_1:
	pci_free_consistent(tp->pci_dev, RX_RING_BYTES, tp->RxDescRing,
		tp->rx_dma);
err_free_tx_0:
	pci_free_consistent(tp->pci_dev, TX_RING_BYTES, tp->TxDescRing,
		tp->tx_dma);
	goto out;
}

static void sis190_tx_clear(struct sis190_private *tp)
{
	unsigned int i;

	for (i = 0; i < NUM_TX_DESC; i++) {
		struct sk_buff *skb = tp->Tx_skbuff[i];

		if (!skb)
			continue;

		sis190_unmap_tx_skb(tp->pci_dev, skb, tp->TxDescRing + i);
		tp->Tx_skbuff[i] = NULL;
		dev_kfree_skb(skb);

		tp->stats.tx_dropped++;
	}
	tp->cur_tx = tp->dirty_tx = 0;
}

static void sis190_down(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	unsigned int poll_locked = 0;

	sis190_delete_timer(dev);

	netif_stop_queue(dev);

	flush_scheduled_work();

	do {
		spin_lock_irq(&tp->lock);

		sis190_asic_down(ioaddr);

		spin_unlock_irq(&tp->lock);

		synchronize_irq(dev->irq);

		if (!poll_locked) {
			netif_poll_disable(dev);
			poll_locked++;
		}

		synchronize_sched();

	} while (SIS_R32(IntrMask));

	sis190_tx_clear(tp);
	sis190_rx_clear(tp);
}

static int sis190_close(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);
	struct pci_dev *pdev = tp->pci_dev;

	sis190_down(dev);

	free_irq(dev->irq, dev);

	netif_poll_enable(dev);

	pci_free_consistent(pdev, TX_RING_BYTES, tp->TxDescRing, tp->tx_dma);
	pci_free_consistent(pdev, RX_RING_BYTES, tp->RxDescRing, tp->rx_dma);

	tp->TxDescRing = NULL;
	tp->RxDescRing = NULL;

	return 0;
}

static int sis190_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	u32 len, entry, dirty_tx;
	struct TxDesc *desc;
	dma_addr_t mapping;

	if (unlikely(skb->len < ETH_ZLEN)) {
		skb = skb_padto(skb, ETH_ZLEN);
		if (!skb) {
			tp->stats.tx_dropped++;
			goto out;
		}
		len = ETH_ZLEN;
	} else {
		len = skb->len;
	}

	entry = tp->cur_tx % NUM_TX_DESC;
	desc = tp->TxDescRing + entry;

	if (unlikely(le32_to_cpu(desc->status) & OWNbit)) {
		netif_stop_queue(dev);
		net_tx_err(tp, KERN_ERR PFX
			   "%s: BUG! Tx Ring full when queue awake!\n",
			   dev->name);
		return NETDEV_TX_BUSY;
	}

	mapping = pci_map_single(tp->pci_dev, skb->data, len, PCI_DMA_TODEVICE);

	tp->Tx_skbuff[entry] = skb;

	desc->PSize = cpu_to_le32(len);
	desc->addr = cpu_to_le32(mapping);

	desc->size = cpu_to_le32(len);
	if (entry == (NUM_TX_DESC - 1))
		desc->size |= cpu_to_le32(RingEnd);

	wmb();

	desc->status = cpu_to_le32(OWNbit | INTbit | DEFbit | CRCbit | PADbit);

	tp->cur_tx++;

	smp_wmb();

	SIS_W32(TxControl, 0x1a00 | CmdReset | CmdTxEnb);

	dev->trans_start = jiffies;

	dirty_tx = tp->dirty_tx;
	if ((tp->cur_tx - NUM_TX_DESC) == dirty_tx) {
		netif_stop_queue(dev);
		smp_rmb();
		if (dirty_tx != tp->dirty_tx)
			netif_wake_queue(dev);
	}
out:
	return NETDEV_TX_OK;
}

static struct net_device_stats *sis190_get_stats(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);

	return &tp->stats;
}

static void sis190_release_board(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct sis190_private *tp = netdev_priv(dev);

	iounmap(tp->mmio_addr);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	free_netdev(dev);
}

static struct net_device * __devinit sis190_init_board(struct pci_dev *pdev)
{
	struct sis190_private *tp;
	struct net_device *dev;
	void __iomem *ioaddr;
	int rc;

	dev = alloc_etherdev(sizeof(*tp));
	if (!dev) {
		net_drv(&debug, KERN_ERR PFX "unable to alloc new ethernet\n");
		rc = -ENOMEM;
		goto err_out_0;
	}

	SET_MODULE_OWNER(dev);
	SET_NETDEV_DEV(dev, &pdev->dev);

	tp = netdev_priv(dev);
	tp->msg_enable = netif_msg_init(debug.msg_enable, SIS190_MSG_DEFAULT);

	rc = pci_enable_device(pdev);
	if (rc < 0) {
		net_probe(tp, KERN_ERR "%s: enable failure\n", pci_name(pdev));
		goto err_free_dev_1;
	}

	rc = -ENODEV;

	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		net_probe(tp, KERN_ERR "%s: region #0 is no MMIO resource.\n",
			  pci_name(pdev));
		goto err_pci_disable_2;
	}
	if (pci_resource_len(pdev, 0) < SIS190_REGS_SIZE) {
		net_probe(tp, KERN_ERR "%s: invalid PCI region size(s).\n",
			  pci_name(pdev));
		goto err_pci_disable_2;
	}

	rc = pci_request_regions(pdev, DRV_NAME);
	if (rc < 0) {
		net_probe(tp, KERN_ERR PFX "%s: could not request regions.\n",
			  pci_name(pdev));
		goto err_pci_disable_2;
	}

	rc = pci_set_dma_mask(pdev, DMA_32BIT_MASK);
	if (rc < 0) {
		net_probe(tp, KERN_ERR "%s: DMA configuration failed.\n",
			  pci_name(pdev));
		goto err_free_res_3;
	}

	pci_set_master(pdev);

	ioaddr = ioremap(pci_resource_start(pdev, 0), SIS190_REGS_SIZE);
	if (!ioaddr) {
		net_probe(tp, KERN_ERR "%s: cannot remap MMIO, aborting\n",
			  pci_name(pdev));
		rc = -EIO;
		goto err_free_res_3;
	}

	tp->pci_dev = pdev;
	tp->mmio_addr = ioaddr;

	tp->mii_if.dev = dev;
	tp->mii_if.mdio_read = __mdio_read;
	tp->mii_if.mdio_write = __mdio_write;
	// tp->mii_if.phy_id = XXX;
	tp->mii_if.phy_id_mask = 0x1f;
	tp->mii_if.reg_num_mask = 0x1f;

	sis190_irq_mask_and_ack(ioaddr);

	sis190_soft_reset(ioaddr);
out:
	return dev;

err_free_res_3:
	pci_release_regions(pdev);
err_pci_disable_2:
	pci_disable_device(pdev);
err_free_dev_1:
	free_netdev(dev);
err_out_0:
	dev = ERR_PTR(rc);
	goto out;
}

static void sis190_tx_timeout(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	u8 tmp8;

	/* Disable Tx, if not already */
	tmp8 = SIS_R8(TxControl);
	if (tmp8 & CmdTxEnb)
		SIS_W8(TxControl, tmp8 & ~CmdTxEnb);

	/* Disable interrupts by clearing the interrupt mask. */
	SIS_W32(IntrMask, 0x0000);

	/* Stop a shared interrupt from scavenging while we are. */
	spin_lock_irq(&tp->lock);
	sis190_tx_clear(tp);
	spin_unlock_irq(&tp->lock);

	/* ...and finally, reset everything. */
	sis190_hw_start(dev);

	netif_wake_queue(dev);
}

static void sis190_set_speed_auto(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);
	void __iomem *ioaddr = tp->mmio_addr;
	int val;

	net_link(tp, KERN_INFO "%s: Enabling Auto-negotiation.\n", dev->name);

	val = mdio_read(ioaddr, MII_ADVERTISE);

	// Enable 10/100 Full/Half Mode, leave MII_ADVERTISE bit4:0
	// unchanged.
	mdio_write(ioaddr, MII_ADVERTISE, (val & ADVERTISE_SLCT) |
		   ADVERTISE_100FULL | ADVERTISE_10FULL |
		   ADVERTISE_100HALF | ADVERTISE_10HALF);

	// Enable 1000 Full Mode.
	mdio_write(ioaddr, MII_CTRL1000, ADVERTISE_1000FULL);

	// Enable auto-negotiation and restart auto-negotiation.
	mdio_write(ioaddr, MII_BMCR,
		   BMCR_ANENABLE | BMCR_ANRESTART | BMCR_RESET);
}

static int sis190_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct sis190_private *tp = netdev_priv(dev);

	return mii_ethtool_gset(&tp->mii_if, cmd);
}

static int sis190_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct sis190_private *tp = netdev_priv(dev);

	return mii_ethtool_sset(&tp->mii_if, cmd);
}

static void sis190_get_drvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	struct sis190_private *tp = netdev_priv(dev);

	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	strcpy(info->bus_info, pci_name(tp->pci_dev));
}

static int sis190_get_regs_len(struct net_device *dev)
{
	return SIS190_REGS_SIZE;
}

static void sis190_get_regs(struct net_device *dev, struct ethtool_regs *regs,
			    void *p)
{
	struct sis190_private *tp = netdev_priv(dev);
	unsigned long flags;

	if (regs->len > SIS190_REGS_SIZE)
		regs->len = SIS190_REGS_SIZE;

	spin_lock_irqsave(&tp->lock, flags);
	memcpy_fromio(p, tp->mmio_addr, regs->len);
	spin_unlock_irqrestore(&tp->lock, flags);
}

static int sis190_nway_reset(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);

	return mii_nway_restart(&tp->mii_if);
}

static u32 sis190_get_msglevel(struct net_device *dev)
{
	struct sis190_private *tp = netdev_priv(dev);

	return tp->msg_enable;
}

static void sis190_set_msglevel(struct net_device *dev, u32 value)
{
	struct sis190_private *tp = netdev_priv(dev);

	tp->msg_enable = value;
}

static struct ethtool_ops sis190_ethtool_ops = {
	.get_settings	= sis190_get_settings,
	.set_settings	= sis190_set_settings,
	.get_drvinfo	= sis190_get_drvinfo,
	.get_regs_len	= sis190_get_regs_len,
	.get_regs	= sis190_get_regs,
	.get_link	= ethtool_op_get_link,
	.get_msglevel	= sis190_get_msglevel,
	.set_msglevel	= sis190_set_msglevel,
	.nway_reset	= sis190_nway_reset,
};

static int sis190_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct sis190_private *tp = netdev_priv(dev);

	return !netif_running(dev) ? -EINVAL :
		generic_mii_ioctl(&tp->mii_if, if_mii(ifr), cmd, NULL);
}

static int __devinit sis190_init_one(struct pci_dev *pdev,
				     const struct pci_device_id *ent)
{
	static int printed_version = 0;
	struct sis190_private *tp;
	struct net_device *dev;
	void __iomem *ioaddr;
	int i, rc;

	if (!printed_version) {
		net_drv(&debug, KERN_INFO SIS190_DRIVER_NAME " loaded.\n");
		printed_version = 1;
	}

	dev = sis190_init_board(pdev);
	if (IS_ERR(dev)) {
		rc = PTR_ERR(dev);
		goto out;
	}

	tp = netdev_priv(dev);
	ioaddr = tp->mmio_addr;

	/* Get MAC address */
	/* Read node address from the EEPROM */

	if (SIS_R32(ROMControl) & 0x4) {
		for (i = 0; i < 3; i++) {
			SIS_W16(RxMacAddr + 2*i,
				sis190_read_eeprom(ioaddr, 3 + i));
		}
	}

	for (i = 0; i < MAC_ADDR_LEN; i++)
		dev->dev_addr[i] = SIS_R8(RxMacAddr + i);

	INIT_WORK(&tp->phy_task, sis190_phy_task, dev);

	dev->open = sis190_open;
	dev->stop = sis190_close;
	dev->do_ioctl = sis190_ioctl;
	dev->get_stats = sis190_get_stats;
	dev->tx_timeout = sis190_tx_timeout;
	dev->watchdog_timeo = SIS190_TX_TIMEOUT;
	dev->hard_start_xmit = sis190_start_xmit;
#ifdef CONFIG_NET_POLL_CONTROLLER
	dev->poll_controller = sis190_netpoll;
#endif
	dev->set_multicast_list = sis190_set_rx_mode;
	SET_ETHTOOL_OPS(dev, &sis190_ethtool_ops);
	dev->irq = pdev->irq;
	dev->base_addr = (unsigned long) 0xdead;

	spin_lock_init(&tp->lock);
	rc = register_netdev(dev);
	if (rc < 0) {
		sis190_release_board(pdev);
		goto out;
	}

	pci_set_drvdata(pdev, dev);

	net_probe(tp, KERN_INFO "%s: %s at %p (IRQ: %d), "
	       "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
	       pci_name(pdev), sis_chip_info[ent->driver_data].name,
	       ioaddr, dev->irq,
	       dev->dev_addr[0], dev->dev_addr[1],
	       dev->dev_addr[2], dev->dev_addr[3],
	       dev->dev_addr[4], dev->dev_addr[5]);

	netif_carrier_off(dev);

	sis190_set_speed_auto(dev);
out:
	return rc;
}

static void __devexit sis190_remove_one(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	unregister_netdev(dev);
	sis190_release_board(pdev);
	pci_set_drvdata(pdev, NULL);
}

static struct pci_driver sis190_pci_driver = {
	.name		= DRV_NAME,
	.id_table	= sis190_pci_tbl,
	.probe		= sis190_init_one,
	.remove		= __devexit_p(sis190_remove_one),
};

static int __init sis190_init_module(void)
{
	return pci_module_init(&sis190_pci_driver);
}

static void __exit sis190_cleanup_module(void)
{
	pci_unregister_driver(&sis190_pci_driver);
}

module_init(sis190_init_module);
module_exit(sis190_cleanup_module);
