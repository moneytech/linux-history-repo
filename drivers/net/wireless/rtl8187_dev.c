/*
 * Linux device driver for RTL8187
 *
 * Copyright 2007 Michael Wu <flamingice@sourmilk.net>
 * Copyright 2007 Andrea Merello <andreamrl@tiscali.it>
 *
 * Based on the r8187 driver, which is:
 * Copyright 2005 Andrea Merello <andreamrl@tiscali.it>, et al.
 *
 * Magic delays and register offsets below are taken from the original
 * r8187 driver sources.  Thanks to Realtek for their support!
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/usb.h>
#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/eeprom_93cx6.h>
#include <net/mac80211.h>

#include "rtl8187.h"
#include "rtl8187_rtl8225.h"

MODULE_AUTHOR("Michael Wu <flamingice@sourmilk.net>");
MODULE_AUTHOR("Andrea Merello <andreamrl@tiscali.it>");
MODULE_DESCRIPTION("RTL8187/RTL8187B USB wireless driver");
MODULE_LICENSE("GPL");

static struct usb_device_id rtl8187_table[] __devinitdata = {
	/* Realtek */
	{USB_DEVICE(0x0bda, 0x8187), .driver_info = DEVICE_RTL8187},
	{USB_DEVICE(0x0bda, 0x8189), .driver_info = DEVICE_RTL8187B},
	{USB_DEVICE(0x0bda, 0x8197), .driver_info = DEVICE_RTL8187B},
	/* Netgear */
	{USB_DEVICE(0x0846, 0x6100), .driver_info = DEVICE_RTL8187},
	{USB_DEVICE(0x0846, 0x6a00), .driver_info = DEVICE_RTL8187},
	/* HP */
	{USB_DEVICE(0x03f0, 0xca02), .driver_info = DEVICE_RTL8187},
	/* Sitecom */
	{USB_DEVICE(0x0df6, 0x000d), .driver_info = DEVICE_RTL8187},
	{}
};

MODULE_DEVICE_TABLE(usb, rtl8187_table);

static const struct ieee80211_rate rtl818x_rates[] = {
	{ .bitrate = 10, .hw_value = 0, },
	{ .bitrate = 20, .hw_value = 1, },
	{ .bitrate = 55, .hw_value = 2, },
	{ .bitrate = 110, .hw_value = 3, },
	{ .bitrate = 60, .hw_value = 4, },
	{ .bitrate = 90, .hw_value = 5, },
	{ .bitrate = 120, .hw_value = 6, },
	{ .bitrate = 180, .hw_value = 7, },
	{ .bitrate = 240, .hw_value = 8, },
	{ .bitrate = 360, .hw_value = 9, },
	{ .bitrate = 480, .hw_value = 10, },
	{ .bitrate = 540, .hw_value = 11, },
};

static const struct ieee80211_channel rtl818x_channels[] = {
	{ .center_freq = 2412 },
	{ .center_freq = 2417 },
	{ .center_freq = 2422 },
	{ .center_freq = 2427 },
	{ .center_freq = 2432 },
	{ .center_freq = 2437 },
	{ .center_freq = 2442 },
	{ .center_freq = 2447 },
	{ .center_freq = 2452 },
	{ .center_freq = 2457 },
	{ .center_freq = 2462 },
	{ .center_freq = 2467 },
	{ .center_freq = 2472 },
	{ .center_freq = 2484 },
};

static void rtl8187_iowrite_async_cb(struct urb *urb)
{
	kfree(urb->context);
	usb_free_urb(urb);
}

static void rtl8187_iowrite_async(struct rtl8187_priv *priv, __le16 addr,
				  void *data, u16 len)
{
	struct usb_ctrlrequest *dr;
	struct urb *urb;
	struct rtl8187_async_write_data {
		u8 data[4];
		struct usb_ctrlrequest dr;
	} *buf;
	int rc;

	buf = kmalloc(sizeof(*buf), GFP_ATOMIC);
	if (!buf)
		return;

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		kfree(buf);
		return;
	}

	dr = &buf->dr;

	dr->bRequestType = RTL8187_REQT_WRITE;
	dr->bRequest = RTL8187_REQ_SET_REG;
	dr->wValue = addr;
	dr->wIndex = 0;
	dr->wLength = cpu_to_le16(len);

	memcpy(buf, data, len);

	usb_fill_control_urb(urb, priv->udev, usb_sndctrlpipe(priv->udev, 0),
			     (unsigned char *)dr, buf, len,
			     rtl8187_iowrite_async_cb, buf);
	rc = usb_submit_urb(urb, GFP_ATOMIC);
	if (rc < 0) {
		kfree(buf);
		usb_free_urb(urb);
	}
}

static inline void rtl818x_iowrite32_async(struct rtl8187_priv *priv,
					   __le32 *addr, u32 val)
{
	__le32 buf = cpu_to_le32(val);

	rtl8187_iowrite_async(priv, cpu_to_le16((unsigned long)addr),
			      &buf, sizeof(buf));
}

void rtl8187_write_phy(struct ieee80211_hw *dev, u8 addr, u32 data)
{
	struct rtl8187_priv *priv = dev->priv;

	data <<= 8;
	data |= addr | 0x80;

	rtl818x_iowrite8(priv, &priv->map->PHY[3], (data >> 24) & 0xFF);
	rtl818x_iowrite8(priv, &priv->map->PHY[2], (data >> 16) & 0xFF);
	rtl818x_iowrite8(priv, &priv->map->PHY[1], (data >> 8) & 0xFF);
	rtl818x_iowrite8(priv, &priv->map->PHY[0], data & 0xFF);

	msleep(1);
}

static void rtl8187_tx_cb(struct urb *urb)
{
	struct sk_buff *skb = (struct sk_buff *)urb->context;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_hw *hw = info->driver_data[0];
	struct rtl8187_priv *priv = hw->priv;

	usb_free_urb(info->driver_data[1]);
	skb_pull(skb, priv->is_rtl8187b ? sizeof(struct rtl8187b_tx_hdr) :
					  sizeof(struct rtl8187_tx_hdr));
	memset(&info->status, 0, sizeof(info->status));
	info->flags |= IEEE80211_TX_STAT_ACK;
	ieee80211_tx_status_irqsafe(hw, skb);
}

static int rtl8187_tx(struct ieee80211_hw *dev, struct sk_buff *skb)
{
	struct rtl8187_priv *priv = dev->priv;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	unsigned int ep;
	void *buf;
	struct urb *urb;
	__le16 rts_dur = 0;
	u32 flags;
	int rc;

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		kfree_skb(skb);
		return 0;
	}

	flags = skb->len;
	flags |= RTL8187_TX_FLAG_NO_ENCRYPT;

	flags |= ieee80211_get_tx_rate(dev, info)->hw_value << 24;
	if (ieee80211_has_morefrags(((struct ieee80211_hdr *)skb->data)->frame_control))
		flags |= RTL8187_TX_FLAG_MORE_FRAG;
	if (info->flags & IEEE80211_TX_CTL_USE_RTS_CTS) {
		flags |= RTL8187_TX_FLAG_RTS;
		flags |= ieee80211_get_rts_cts_rate(dev, info)->hw_value << 19;
		rts_dur = ieee80211_rts_duration(dev, priv->vif,
						 skb->len, info);
	} else if (info->flags & IEEE80211_TX_CTL_USE_CTS_PROTECT) {
		flags |= RTL8187_TX_FLAG_CTS;
		flags |= ieee80211_get_rts_cts_rate(dev, info)->hw_value << 19;
	}

	if (!priv->is_rtl8187b) {
		struct rtl8187_tx_hdr *hdr =
			(struct rtl8187_tx_hdr *)skb_push(skb, sizeof(*hdr));
		hdr->flags = cpu_to_le32(flags);
		hdr->len = 0;
		hdr->rts_duration = rts_dur;
		hdr->retry = cpu_to_le32(info->control.retry_limit << 8);
		buf = hdr;

		ep = 2;
	} else {
		/* fc needs to be calculated before skb_push() */
		unsigned int epmap[4] = { 6, 7, 5, 4 };
		struct ieee80211_hdr *tx_hdr =
			(struct ieee80211_hdr *)(skb->data);
		u16 fc = le16_to_cpu(tx_hdr->frame_control);

		struct rtl8187b_tx_hdr *hdr =
			(struct rtl8187b_tx_hdr *)skb_push(skb, sizeof(*hdr));
		struct ieee80211_rate *txrate =
			ieee80211_get_tx_rate(dev, info);
		memset(hdr, 0, sizeof(*hdr));
		hdr->flags = cpu_to_le32(flags);
		hdr->rts_duration = rts_dur;
		hdr->retry = cpu_to_le32(info->control.retry_limit << 8);
		hdr->tx_duration =
			ieee80211_generic_frame_duration(dev, priv->vif,
							 skb->len, txrate);
		buf = hdr;

		if ((fc & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_MGMT)
			ep = 12;
		else
			ep = epmap[skb_get_queue_mapping(skb)];
	}

	info->driver_data[0] = dev;
	info->driver_data[1] = urb;

	usb_fill_bulk_urb(urb, priv->udev, usb_sndbulkpipe(priv->udev, ep),
			  buf, skb->len, rtl8187_tx_cb, skb);
	rc = usb_submit_urb(urb, GFP_ATOMIC);
	if (rc < 0) {
		usb_free_urb(urb);
		kfree_skb(skb);
	}

	return 0;
}

static void rtl8187_rx_cb(struct urb *urb)
{
	struct sk_buff *skb = (struct sk_buff *)urb->context;
	struct rtl8187_rx_info *info = (struct rtl8187_rx_info *)skb->cb;
	struct ieee80211_hw *dev = info->dev;
	struct rtl8187_priv *priv = dev->priv;
	struct ieee80211_rx_status rx_status = { 0 };
	int rate, signal;
	u32 flags;

	spin_lock(&priv->rx_queue.lock);
	if (skb->next)
		__skb_unlink(skb, &priv->rx_queue);
	else {
		spin_unlock(&priv->rx_queue.lock);
		return;
	}
	spin_unlock(&priv->rx_queue.lock);

	if (unlikely(urb->status)) {
		usb_free_urb(urb);
		dev_kfree_skb_irq(skb);
		return;
	}

	skb_put(skb, urb->actual_length);
	if (!priv->is_rtl8187b) {
		struct rtl8187_rx_hdr *hdr =
			(typeof(hdr))(skb_tail_pointer(skb) - sizeof(*hdr));
		flags = le32_to_cpu(hdr->flags);
		signal = hdr->signal & 0x7f;
		rx_status.antenna = (hdr->signal >> 7) & 1;
		rx_status.signal = signal;
		rx_status.noise = hdr->noise;
		rx_status.mactime = le64_to_cpu(hdr->mac_time);
		priv->signal = signal;
		priv->quality = signal;
		priv->noise = hdr->noise;
	} else {
		struct rtl8187b_rx_hdr *hdr =
			(typeof(hdr))(skb_tail_pointer(skb) - sizeof(*hdr));
		flags = le32_to_cpu(hdr->flags);
		signal = hdr->agc >> 1;
		rx_status.antenna = (hdr->signal >> 7) & 1;
		rx_status.signal = 64 - min(hdr->noise, (u8)64);
		rx_status.noise = hdr->noise;
		rx_status.mactime = le64_to_cpu(hdr->mac_time);
		priv->signal = hdr->signal;
		priv->quality = hdr->agc >> 1;
		priv->noise = hdr->noise;
	}

	skb_trim(skb, flags & 0x0FFF);
	rate = (flags >> 20) & 0xF;
	if (rate > 3) {	/* OFDM rate */
		if (signal > 90)
			signal = 90;
		else if (signal < 25)
			signal = 25;
		signal = 90 - signal;
	} else {	/* CCK rate */
		if (signal > 95)
			signal = 95;
		else if (signal < 30)
			signal = 30;
		signal = 95 - signal;
	}

	rx_status.qual = priv->quality;
	rx_status.signal = signal;
	rx_status.rate_idx = rate;
	rx_status.freq = dev->conf.channel->center_freq;
	rx_status.band = dev->conf.channel->band;
	rx_status.flag |= RX_FLAG_TSFT;
	if (flags & (1 << 13))
		rx_status.flag |= RX_FLAG_FAILED_FCS_CRC;
	ieee80211_rx_irqsafe(dev, skb, &rx_status);

	skb = dev_alloc_skb(RTL8187_MAX_RX);
	if (unlikely(!skb)) {
		usb_free_urb(urb);
		/* TODO check rx queue length and refill *somewhere* */
		return;
	}

	info = (struct rtl8187_rx_info *)skb->cb;
	info->urb = urb;
	info->dev = dev;
	urb->transfer_buffer = skb_tail_pointer(skb);
	urb->context = skb;
	skb_queue_tail(&priv->rx_queue, skb);

	usb_submit_urb(urb, GFP_ATOMIC);
}

static int rtl8187_init_urbs(struct ieee80211_hw *dev)
{
	struct rtl8187_priv *priv = dev->priv;
	struct urb *entry;
	struct sk_buff *skb;
	struct rtl8187_rx_info *info;

	while (skb_queue_len(&priv->rx_queue) < 8) {
		skb = __dev_alloc_skb(RTL8187_MAX_RX, GFP_KERNEL);
		if (!skb)
			break;
		entry = usb_alloc_urb(0, GFP_KERNEL);
		if (!entry) {
			kfree_skb(skb);
			break;
		}
		usb_fill_bulk_urb(entry, priv->udev,
				  usb_rcvbulkpipe(priv->udev,
				  priv->is_rtl8187b ? 3 : 1),
				  skb_tail_pointer(skb),
				  RTL8187_MAX_RX, rtl8187_rx_cb, skb);
		info = (struct rtl8187_rx_info *)skb->cb;
		info->urb = entry;
		info->dev = dev;
		skb_queue_tail(&priv->rx_queue, skb);
		usb_submit_urb(entry, GFP_KERNEL);
	}

	return 0;
}

static int rtl8187_cmd_reset(struct ieee80211_hw *dev)
{
	struct rtl8187_priv *priv = dev->priv;
	u8 reg;
	int i;

	reg = rtl818x_ioread8(priv, &priv->map->CMD);
	reg &= (1 << 1);
	reg |= RTL818X_CMD_RESET;
	rtl818x_iowrite8(priv, &priv->map->CMD, reg);

	i = 10;
	do {
		msleep(2);
		if (!(rtl818x_ioread8(priv, &priv->map->CMD) &
		      RTL818X_CMD_RESET))
			break;
	} while (--i);

	if (!i) {
		printk(KERN_ERR "%s: Reset timeout!\n", wiphy_name(dev->wiphy));
		return -ETIMEDOUT;
	}

	/* reload registers from eeprom */
	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, RTL818X_EEPROM_CMD_LOAD);

	i = 10;
	do {
		msleep(4);
		if (!(rtl818x_ioread8(priv, &priv->map->EEPROM_CMD) &
		      RTL818X_EEPROM_CMD_CONFIG))
			break;
	} while (--i);

	if (!i) {
		printk(KERN_ERR "%s: eeprom reset timeout!\n",
		       wiphy_name(dev->wiphy));
		return -ETIMEDOUT;
	}

	return 0;
}

static int rtl8187_init_hw(struct ieee80211_hw *dev)
{
	struct rtl8187_priv *priv = dev->priv;
	u8 reg;
	int res;

	/* reset */
	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD,
			 RTL818X_EEPROM_CMD_CONFIG);
	reg = rtl818x_ioread8(priv, &priv->map->CONFIG3);
	rtl818x_iowrite8(priv, &priv->map->CONFIG3, reg |
			 RTL818X_CONFIG3_ANAPARAM_WRITE);
	rtl818x_iowrite32(priv, &priv->map->ANAPARAM, RTL8225_ANAPARAM_ON);
	rtl818x_iowrite32(priv, &priv->map->ANAPARAM2, RTL8225_ANAPARAM2_ON);
	rtl818x_iowrite8(priv, &priv->map->CONFIG3, reg &
			 ~RTL818X_CONFIG3_ANAPARAM_WRITE);
	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD,
			 RTL818X_EEPROM_CMD_NORMAL);

	rtl818x_iowrite16(priv, &priv->map->INT_MASK, 0);

	msleep(200);
	rtl818x_iowrite8(priv, (u8 *)0xFE18, 0x10);
	rtl818x_iowrite8(priv, (u8 *)0xFE18, 0x11);
	rtl818x_iowrite8(priv, (u8 *)0xFE18, 0x00);
	msleep(200);

	res = rtl8187_cmd_reset(dev);
	if (res)
		return res;

	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, RTL818X_EEPROM_CMD_CONFIG);
	reg = rtl818x_ioread8(priv, &priv->map->CONFIG3);
	rtl818x_iowrite8(priv, &priv->map->CONFIG3,
			reg | RTL818X_CONFIG3_ANAPARAM_WRITE);
	rtl818x_iowrite32(priv, &priv->map->ANAPARAM, RTL8225_ANAPARAM_ON);
	rtl818x_iowrite32(priv, &priv->map->ANAPARAM2, RTL8225_ANAPARAM2_ON);
	rtl818x_iowrite8(priv, &priv->map->CONFIG3,
			reg & ~RTL818X_CONFIG3_ANAPARAM_WRITE);
	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, RTL818X_EEPROM_CMD_NORMAL);

	/* setup card */
	rtl818x_iowrite16(priv, &priv->map->RFPinsSelect, 0);
	rtl818x_iowrite8(priv, &priv->map->GPIO, 0);

	rtl818x_iowrite16(priv, &priv->map->RFPinsSelect, (4 << 8));
	rtl818x_iowrite8(priv, &priv->map->GPIO, 1);
	rtl818x_iowrite8(priv, &priv->map->GP_ENABLE, 0);

	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, RTL818X_EEPROM_CMD_CONFIG);

	rtl818x_iowrite16(priv, (__le16 *)0xFFF4, 0xFFFF);
	reg = rtl818x_ioread8(priv, &priv->map->CONFIG1);
	reg &= 0x3F;
	reg |= 0x80;
	rtl818x_iowrite8(priv, &priv->map->CONFIG1, reg);

	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, RTL818X_EEPROM_CMD_NORMAL);

	rtl818x_iowrite32(priv, &priv->map->INT_TIMEOUT, 0);
	rtl818x_iowrite8(priv, &priv->map->WPA_CONF, 0);
	rtl818x_iowrite8(priv, &priv->map->RATE_FALLBACK, 0x81);

	// TODO: set RESP_RATE and BRSR properly
	rtl818x_iowrite8(priv, &priv->map->RESP_RATE, (8 << 4) | 0);
	rtl818x_iowrite16(priv, &priv->map->BRSR, 0x01F3);

	/* host_usb_init */
	rtl818x_iowrite16(priv, &priv->map->RFPinsSelect, 0);
	rtl818x_iowrite8(priv, &priv->map->GPIO, 0);
	reg = rtl818x_ioread8(priv, (u8 *)0xFE53);
	rtl818x_iowrite8(priv, (u8 *)0xFE53, reg | (1 << 7));
	rtl818x_iowrite16(priv, &priv->map->RFPinsSelect, (4 << 8));
	rtl818x_iowrite8(priv, &priv->map->GPIO, 0x20);
	rtl818x_iowrite8(priv, &priv->map->GP_ENABLE, 0);
	rtl818x_iowrite16(priv, &priv->map->RFPinsOutput, 0x80);
	rtl818x_iowrite16(priv, &priv->map->RFPinsSelect, 0x80);
	rtl818x_iowrite16(priv, &priv->map->RFPinsEnable, 0x80);
	msleep(100);

	rtl818x_iowrite32(priv, &priv->map->RF_TIMING, 0x000a8008);
	rtl818x_iowrite16(priv, &priv->map->BRSR, 0xFFFF);
	rtl818x_iowrite32(priv, &priv->map->RF_PARA, 0x00100044);
	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD,
			 RTL818X_EEPROM_CMD_CONFIG);
	rtl818x_iowrite8(priv, &priv->map->CONFIG3, 0x44);
	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD,
			 RTL818X_EEPROM_CMD_NORMAL);
	rtl818x_iowrite16(priv, &priv->map->RFPinsEnable, 0x1FF7);
	msleep(100);

	priv->rf->init(dev);

	rtl818x_iowrite16(priv, &priv->map->BRSR, 0x01F3);
	reg = rtl818x_ioread8(priv, &priv->map->PGSELECT) & ~1;
	rtl818x_iowrite8(priv, &priv->map->PGSELECT, reg | 1);
	rtl818x_iowrite16(priv, (__le16 *)0xFFFE, 0x10);
	rtl818x_iowrite8(priv, &priv->map->TALLY_SEL, 0x80);
	rtl818x_iowrite8(priv, (u8 *)0xFFFF, 0x60);
	rtl818x_iowrite8(priv, &priv->map->PGSELECT, reg);

	return 0;
}

static const u8 rtl8187b_reg_table[][3] = {
	{0xF0, 0x32, 0}, {0xF1, 0x32, 0}, {0xF2, 0x00, 0}, {0xF3, 0x00, 0},
	{0xF4, 0x32, 0}, {0xF5, 0x43, 0}, {0xF6, 0x00, 0}, {0xF7, 0x00, 0},
	{0xF8, 0x46, 0}, {0xF9, 0xA4, 0}, {0xFA, 0x00, 0}, {0xFB, 0x00, 0},
	{0xFC, 0x96, 0}, {0xFD, 0xA4, 0}, {0xFE, 0x00, 0}, {0xFF, 0x00, 0},

	{0x58, 0x4B, 1}, {0x59, 0x00, 1}, {0x5A, 0x4B, 1}, {0x5B, 0x00, 1},
	{0x60, 0x4B, 1}, {0x61, 0x09, 1}, {0x62, 0x4B, 1}, {0x63, 0x09, 1},
	{0xCE, 0x0F, 1}, {0xCF, 0x00, 1}, {0xE0, 0xFF, 1}, {0xE1, 0x0F, 1},
	{0xE2, 0x00, 1}, {0xF0, 0x4E, 1}, {0xF1, 0x01, 1}, {0xF2, 0x02, 1},
	{0xF3, 0x03, 1}, {0xF4, 0x04, 1}, {0xF5, 0x05, 1}, {0xF6, 0x06, 1},
	{0xF7, 0x07, 1}, {0xF8, 0x08, 1},

	{0x4E, 0x00, 2}, {0x0C, 0x04, 2}, {0x21, 0x61, 2}, {0x22, 0x68, 2},
	{0x23, 0x6F, 2}, {0x24, 0x76, 2}, {0x25, 0x7D, 2}, {0x26, 0x84, 2},
	{0x27, 0x8D, 2}, {0x4D, 0x08, 2}, {0x50, 0x05, 2}, {0x51, 0xF5, 2},
	{0x52, 0x04, 2}, {0x53, 0xA0, 2}, {0x54, 0x1F, 2}, {0x55, 0x23, 2},
	{0x56, 0x45, 2}, {0x57, 0x67, 2}, {0x58, 0x08, 2}, {0x59, 0x08, 2},
	{0x5A, 0x08, 2}, {0x5B, 0x08, 2}, {0x60, 0x08, 2}, {0x61, 0x08, 2},
	{0x62, 0x08, 2}, {0x63, 0x08, 2}, {0x64, 0xCF, 2}, {0x72, 0x56, 2},
	{0x73, 0x9A, 2},

	{0x34, 0xF0, 0}, {0x35, 0x0F, 0}, {0x5B, 0x40, 0}, {0x84, 0x88, 0},
	{0x85, 0x24, 0}, {0x88, 0x54, 0}, {0x8B, 0xB8, 0}, {0x8C, 0x07, 0},
	{0x8D, 0x00, 0}, {0x94, 0x1B, 0}, {0x95, 0x12, 0}, {0x96, 0x00, 0},
	{0x97, 0x06, 0}, {0x9D, 0x1A, 0}, {0x9F, 0x10, 0}, {0xB4, 0x22, 0},
	{0xBE, 0x80, 0}, {0xDB, 0x00, 0}, {0xEE, 0x00, 0}, {0x91, 0x03, 0},

	{0x4C, 0x00, 2}, {0x9F, 0x00, 3}, {0x8C, 0x01, 0}, {0x8D, 0x10, 0},
	{0x8E, 0x08, 0}, {0x8F, 0x00, 0}
};

static int rtl8187b_init_hw(struct ieee80211_hw *dev)
{
	struct rtl8187_priv *priv = dev->priv;
	int res, i;
	u8 reg;

	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD,
			 RTL818X_EEPROM_CMD_CONFIG);

	reg = rtl818x_ioread8(priv, &priv->map->CONFIG3);
	reg |= RTL818X_CONFIG3_ANAPARAM_WRITE | RTL818X_CONFIG3_GNT_SELECT;
	rtl818x_iowrite8(priv, &priv->map->CONFIG3, reg);
	rtl818x_iowrite32(priv, &priv->map->ANAPARAM2, 0x727f3f52);
	rtl818x_iowrite32(priv, &priv->map->ANAPARAM, 0x45090658);
	rtl818x_iowrite8(priv, &priv->map->ANAPARAM3, 0);

	rtl818x_iowrite8(priv, (u8 *)0xFF61, 0x10);
	reg = rtl818x_ioread8(priv, (u8 *)0xFF62);
	rtl818x_iowrite8(priv, (u8 *)0xFF62, reg & ~(1 << 5));
	rtl818x_iowrite8(priv, (u8 *)0xFF62, reg | (1 << 5));

	reg = rtl818x_ioread8(priv, &priv->map->CONFIG3);
	reg &= ~RTL818X_CONFIG3_ANAPARAM_WRITE;
	rtl818x_iowrite8(priv, &priv->map->CONFIG3, reg);

	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD,
			 RTL818X_EEPROM_CMD_NORMAL);

	res = rtl8187_cmd_reset(dev);
	if (res)
		return res;

	rtl818x_iowrite16(priv, (__le16 *)0xFF2D, 0x0FFF);
	reg = rtl818x_ioread8(priv, &priv->map->CW_CONF);
	reg |= RTL818X_CW_CONF_PERPACKET_RETRY_SHIFT;
	rtl818x_iowrite8(priv, &priv->map->CW_CONF, reg);
	reg = rtl818x_ioread8(priv, &priv->map->TX_AGC_CTL);
	reg |= RTL818X_TX_AGC_CTL_PERPACKET_GAIN_SHIFT |
	       RTL818X_TX_AGC_CTL_PERPACKET_ANTSEL_SHIFT;
	rtl818x_iowrite8(priv, &priv->map->TX_AGC_CTL, reg);

	rtl818x_iowrite16_idx(priv, (__le16 *)0xFFE0, 0x0FFF, 1);
	reg = rtl818x_ioread8(priv, &priv->map->RATE_FALLBACK);
	reg |= RTL818X_RATE_FALLBACK_ENABLE;
	rtl818x_iowrite8(priv, &priv->map->RATE_FALLBACK, reg);

	rtl818x_iowrite16(priv, &priv->map->BEACON_INTERVAL, 100);
	rtl818x_iowrite16(priv, &priv->map->ATIM_WND, 2);
	rtl818x_iowrite16_idx(priv, (__le16 *)0xFFD4, 0xFFFF, 1);

	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD,
			 RTL818X_EEPROM_CMD_CONFIG);
	reg = rtl818x_ioread8(priv, &priv->map->CONFIG1);
	rtl818x_iowrite8(priv, &priv->map->CONFIG1, (reg & 0x3F) | 0x80);
	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD,
			 RTL818X_EEPROM_CMD_NORMAL);

	rtl818x_iowrite8(priv, &priv->map->WPA_CONF, 0);
	for (i = 0; i < ARRAY_SIZE(rtl8187b_reg_table); i++) {
		rtl818x_iowrite8_idx(priv,
				     (u8 *)(uintptr_t)
				     (rtl8187b_reg_table[i][0] | 0xFF00),
				     rtl8187b_reg_table[i][1],
				     rtl8187b_reg_table[i][2]);
	}

	rtl818x_iowrite16(priv, &priv->map->TID_AC_MAP, 0xFA50);
	rtl818x_iowrite16(priv, &priv->map->INT_MIG, 0);

	rtl818x_iowrite32_idx(priv, (__le32 *)0xFFF0, 0, 1);
	rtl818x_iowrite32_idx(priv, (__le32 *)0xFFF4, 0, 1);
	rtl818x_iowrite8_idx(priv, (u8 *)0xFFF8, 0, 1);

	rtl818x_iowrite32(priv, &priv->map->RF_TIMING, 0x00004001);

	rtl818x_iowrite16_idx(priv, (__le16 *)0xFF72, 0x569A, 2);

	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD,
			 RTL818X_EEPROM_CMD_CONFIG);
	reg = rtl818x_ioread8(priv, &priv->map->CONFIG3);
	reg |= RTL818X_CONFIG3_ANAPARAM_WRITE;
	rtl818x_iowrite8(priv, &priv->map->CONFIG3, reg);
	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD,
			 RTL818X_EEPROM_CMD_NORMAL);

	rtl818x_iowrite16(priv, &priv->map->RFPinsOutput, 0x0480);
	rtl818x_iowrite16(priv, &priv->map->RFPinsSelect, 0x2488);
	rtl818x_iowrite16(priv, &priv->map->RFPinsEnable, 0x1FFF);
	msleep(1100);

	priv->rf->init(dev);

	reg = RTL818X_CMD_TX_ENABLE | RTL818X_CMD_RX_ENABLE;
	rtl818x_iowrite8(priv, &priv->map->CMD, reg);
	rtl818x_iowrite16(priv, &priv->map->INT_MASK, 0xFFFF);

	rtl818x_iowrite8(priv, (u8 *)0xFE41, 0xF4);
	rtl818x_iowrite8(priv, (u8 *)0xFE40, 0x00);
	rtl818x_iowrite8(priv, (u8 *)0xFE42, 0x00);
	rtl818x_iowrite8(priv, (u8 *)0xFE42, 0x01);
	rtl818x_iowrite8(priv, (u8 *)0xFE40, 0x0F);
	rtl818x_iowrite8(priv, (u8 *)0xFE42, 0x00);
	rtl818x_iowrite8(priv, (u8 *)0xFE42, 0x01);

	reg = rtl818x_ioread8(priv, (u8 *)0xFFDB);
	rtl818x_iowrite8(priv, (u8 *)0xFFDB, reg | (1 << 2));
	rtl818x_iowrite16_idx(priv, (__le16 *)0xFF72, 0x59FA, 3);
	rtl818x_iowrite16_idx(priv, (__le16 *)0xFF74, 0x59D2, 3);
	rtl818x_iowrite16_idx(priv, (__le16 *)0xFF76, 0x59D2, 3);
	rtl818x_iowrite16_idx(priv, (__le16 *)0xFF78, 0x19FA, 3);
	rtl818x_iowrite16_idx(priv, (__le16 *)0xFF7A, 0x19FA, 3);
	rtl818x_iowrite16_idx(priv, (__le16 *)0xFF7C, 0x00D0, 3);
	rtl818x_iowrite8(priv, (u8 *)0xFF61, 0);
	rtl818x_iowrite8_idx(priv, (u8 *)0xFF80, 0x0F, 1);
	rtl818x_iowrite8_idx(priv, (u8 *)0xFF83, 0x03, 1);
	rtl818x_iowrite8(priv, (u8 *)0xFFDA, 0x10);
	rtl818x_iowrite8_idx(priv, (u8 *)0xFF4D, 0x08, 2);

	rtl818x_iowrite32(priv, &priv->map->HSSI_PARA, 0x0600321B);

	rtl818x_iowrite16_idx(priv, (__le16 *)0xFFEC, 0x0800, 1);

	return 0;
}

static int rtl8187_start(struct ieee80211_hw *dev)
{
	struct rtl8187_priv *priv = dev->priv;
	u32 reg;
	int ret;

	ret = (!priv->is_rtl8187b) ? rtl8187_init_hw(dev) :
				     rtl8187b_init_hw(dev);
	if (ret)
		return ret;

	if (priv->is_rtl8187b) {
		reg = RTL818X_RX_CONF_MGMT |
		      RTL818X_RX_CONF_DATA |
		      RTL818X_RX_CONF_BROADCAST |
		      RTL818X_RX_CONF_NICMAC |
		      RTL818X_RX_CONF_BSSID |
		      (7 << 13 /* RX FIFO threshold NONE */) |
		      (7 << 10 /* MAX RX DMA */) |
		      RTL818X_RX_CONF_RX_AUTORESETPHY |
		      RTL818X_RX_CONF_ONLYERLPKT |
		      RTL818X_RX_CONF_MULTICAST;
		priv->rx_conf = reg;
		rtl818x_iowrite32(priv, &priv->map->RX_CONF, reg);

		rtl818x_iowrite32(priv, &priv->map->TX_CONF,
				  RTL818X_TX_CONF_HW_SEQNUM |
				  RTL818X_TX_CONF_DISREQQSIZE |
				  (7 << 8  /* short retry limit */) |
				  (7 << 0  /* long retry limit */) |
				  (7 << 21 /* MAX TX DMA */));
		rtl8187_init_urbs(dev);
		return 0;
	}

	rtl818x_iowrite16(priv, &priv->map->INT_MASK, 0xFFFF);

	rtl818x_iowrite32(priv, &priv->map->MAR[0], ~0);
	rtl818x_iowrite32(priv, &priv->map->MAR[1], ~0);

	rtl8187_init_urbs(dev);

	reg = RTL818X_RX_CONF_ONLYERLPKT |
	      RTL818X_RX_CONF_RX_AUTORESETPHY |
	      RTL818X_RX_CONF_BSSID |
	      RTL818X_RX_CONF_MGMT |
	      RTL818X_RX_CONF_DATA |
	      (7 << 13 /* RX FIFO threshold NONE */) |
	      (7 << 10 /* MAX RX DMA */) |
	      RTL818X_RX_CONF_BROADCAST |
	      RTL818X_RX_CONF_NICMAC;

	priv->rx_conf = reg;
	rtl818x_iowrite32(priv, &priv->map->RX_CONF, reg);

	reg = rtl818x_ioread8(priv, &priv->map->CW_CONF);
	reg &= ~RTL818X_CW_CONF_PERPACKET_CW_SHIFT;
	reg |= RTL818X_CW_CONF_PERPACKET_RETRY_SHIFT;
	rtl818x_iowrite8(priv, &priv->map->CW_CONF, reg);

	reg = rtl818x_ioread8(priv, &priv->map->TX_AGC_CTL);
	reg &= ~RTL818X_TX_AGC_CTL_PERPACKET_GAIN_SHIFT;
	reg &= ~RTL818X_TX_AGC_CTL_PERPACKET_ANTSEL_SHIFT;
	reg &= ~RTL818X_TX_AGC_CTL_FEEDBACK_ANT;
	rtl818x_iowrite8(priv, &priv->map->TX_AGC_CTL, reg);

	reg  = RTL818X_TX_CONF_CW_MIN |
	       (7 << 21 /* MAX TX DMA */) |
	       RTL818X_TX_CONF_NO_ICV;
	rtl818x_iowrite32(priv, &priv->map->TX_CONF, reg);

	reg = rtl818x_ioread8(priv, &priv->map->CMD);
	reg |= RTL818X_CMD_TX_ENABLE;
	reg |= RTL818X_CMD_RX_ENABLE;
	rtl818x_iowrite8(priv, &priv->map->CMD, reg);

	return 0;
}

static void rtl8187_stop(struct ieee80211_hw *dev)
{
	struct rtl8187_priv *priv = dev->priv;
	struct rtl8187_rx_info *info;
	struct sk_buff *skb;
	u32 reg;

	rtl818x_iowrite16(priv, &priv->map->INT_MASK, 0);

	reg = rtl818x_ioread8(priv, &priv->map->CMD);
	reg &= ~RTL818X_CMD_TX_ENABLE;
	reg &= ~RTL818X_CMD_RX_ENABLE;
	rtl818x_iowrite8(priv, &priv->map->CMD, reg);

	priv->rf->stop(dev);

	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, RTL818X_EEPROM_CMD_CONFIG);
	reg = rtl818x_ioread8(priv, &priv->map->CONFIG4);
	rtl818x_iowrite8(priv, &priv->map->CONFIG4, reg | RTL818X_CONFIG4_VCOOFF);
	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, RTL818X_EEPROM_CMD_NORMAL);

	while ((skb = skb_dequeue(&priv->rx_queue))) {
		info = (struct rtl8187_rx_info *)skb->cb;
		usb_kill_urb(info->urb);
		kfree_skb(skb);
	}
	return;
}

static int rtl8187_add_interface(struct ieee80211_hw *dev,
				 struct ieee80211_if_init_conf *conf)
{
	struct rtl8187_priv *priv = dev->priv;
	int i;

	if (priv->mode != IEEE80211_IF_TYPE_MNTR)
		return -EOPNOTSUPP;

	switch (conf->type) {
	case IEEE80211_IF_TYPE_STA:
		priv->mode = conf->type;
		break;
	default:
		return -EOPNOTSUPP;
	}

	priv->vif = conf->vif;

	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, RTL818X_EEPROM_CMD_CONFIG);
	for (i = 0; i < ETH_ALEN; i++)
		rtl818x_iowrite8(priv, &priv->map->MAC[i],
				 ((u8 *)conf->mac_addr)[i]);
	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, RTL818X_EEPROM_CMD_NORMAL);

	return 0;
}

static void rtl8187_remove_interface(struct ieee80211_hw *dev,
				     struct ieee80211_if_init_conf *conf)
{
	struct rtl8187_priv *priv = dev->priv;
	priv->mode = IEEE80211_IF_TYPE_MNTR;
	priv->vif = NULL;
}

static int rtl8187_config(struct ieee80211_hw *dev, struct ieee80211_conf *conf)
{
	struct rtl8187_priv *priv = dev->priv;
	u32 reg;

	reg = rtl818x_ioread32(priv, &priv->map->TX_CONF);
	/* Enable TX loopback on MAC level to avoid TX during channel
	 * changes, as this has be seen to causes problems and the
	 * card will stop work until next reset
	 */
	rtl818x_iowrite32(priv, &priv->map->TX_CONF,
			  reg | RTL818X_TX_CONF_LOOPBACK_MAC);
	msleep(10);
	priv->rf->set_chan(dev, conf);
	msleep(10);
	rtl818x_iowrite32(priv, &priv->map->TX_CONF, reg);

	if (!priv->is_rtl8187b) {
		rtl818x_iowrite8(priv, &priv->map->SIFS, 0x22);

		if (conf->flags & IEEE80211_CONF_SHORT_SLOT_TIME) {
			rtl818x_iowrite8(priv, &priv->map->SLOT, 0x9);
			rtl818x_iowrite8(priv, &priv->map->DIFS, 0x14);
			rtl818x_iowrite8(priv, &priv->map->EIFS, 91 - 0x14);
			rtl818x_iowrite8(priv, &priv->map->CW_VAL, 0x73);
		} else {
			rtl818x_iowrite8(priv, &priv->map->SLOT, 0x14);
			rtl818x_iowrite8(priv, &priv->map->DIFS, 0x24);
			rtl818x_iowrite8(priv, &priv->map->EIFS, 91 - 0x24);
			rtl818x_iowrite8(priv, &priv->map->CW_VAL, 0xa5);
		}
	}

	rtl818x_iowrite16(priv, &priv->map->ATIM_WND, 2);
	rtl818x_iowrite16(priv, &priv->map->ATIMTR_INTERVAL, 100);
	rtl818x_iowrite16(priv, &priv->map->BEACON_INTERVAL, 100);
	rtl818x_iowrite16(priv, &priv->map->BEACON_INTERVAL_TIME, 100);
	return 0;
}

static int rtl8187_config_interface(struct ieee80211_hw *dev,
				    struct ieee80211_vif *vif,
				    struct ieee80211_if_conf *conf)
{
	struct rtl8187_priv *priv = dev->priv;
	int i;
	u8 reg;

	for (i = 0; i < ETH_ALEN; i++)
		rtl818x_iowrite8(priv, &priv->map->BSSID[i], conf->bssid[i]);

	if (is_valid_ether_addr(conf->bssid)) {
		reg = RTL818X_MSR_INFRA;
		if (priv->is_rtl8187b)
			reg |= RTL818X_MSR_ENEDCA;
		rtl818x_iowrite8(priv, &priv->map->MSR, reg);
	} else {
		reg = RTL818X_MSR_NO_LINK;
		rtl818x_iowrite8(priv, &priv->map->MSR, reg);
	}

	return 0;
}

static void rtl8187_configure_filter(struct ieee80211_hw *dev,
				     unsigned int changed_flags,
				     unsigned int *total_flags,
				     int mc_count, struct dev_addr_list *mclist)
{
	struct rtl8187_priv *priv = dev->priv;

	if (changed_flags & FIF_FCSFAIL)
		priv->rx_conf ^= RTL818X_RX_CONF_FCS;
	if (changed_flags & FIF_CONTROL)
		priv->rx_conf ^= RTL818X_RX_CONF_CTRL;
	if (changed_flags & FIF_OTHER_BSS)
		priv->rx_conf ^= RTL818X_RX_CONF_MONITOR;
	if (*total_flags & FIF_ALLMULTI || mc_count > 0)
		priv->rx_conf |= RTL818X_RX_CONF_MULTICAST;
	else
		priv->rx_conf &= ~RTL818X_RX_CONF_MULTICAST;

	*total_flags = 0;

	if (priv->rx_conf & RTL818X_RX_CONF_FCS)
		*total_flags |= FIF_FCSFAIL;
	if (priv->rx_conf & RTL818X_RX_CONF_CTRL)
		*total_flags |= FIF_CONTROL;
	if (priv->rx_conf & RTL818X_RX_CONF_MONITOR)
		*total_flags |= FIF_OTHER_BSS;
	if (priv->rx_conf & RTL818X_RX_CONF_MULTICAST)
		*total_flags |= FIF_ALLMULTI;

	rtl818x_iowrite32_async(priv, &priv->map->RX_CONF, priv->rx_conf);
}

static const struct ieee80211_ops rtl8187_ops = {
	.tx			= rtl8187_tx,
	.start			= rtl8187_start,
	.stop			= rtl8187_stop,
	.add_interface		= rtl8187_add_interface,
	.remove_interface	= rtl8187_remove_interface,
	.config			= rtl8187_config,
	.config_interface	= rtl8187_config_interface,
	.configure_filter	= rtl8187_configure_filter,
};

static void rtl8187_eeprom_register_read(struct eeprom_93cx6 *eeprom)
{
	struct ieee80211_hw *dev = eeprom->data;
	struct rtl8187_priv *priv = dev->priv;
	u8 reg = rtl818x_ioread8(priv, &priv->map->EEPROM_CMD);

	eeprom->reg_data_in = reg & RTL818X_EEPROM_CMD_WRITE;
	eeprom->reg_data_out = reg & RTL818X_EEPROM_CMD_READ;
	eeprom->reg_data_clock = reg & RTL818X_EEPROM_CMD_CK;
	eeprom->reg_chip_select = reg & RTL818X_EEPROM_CMD_CS;
}

static void rtl8187_eeprom_register_write(struct eeprom_93cx6 *eeprom)
{
	struct ieee80211_hw *dev = eeprom->data;
	struct rtl8187_priv *priv = dev->priv;
	u8 reg = RTL818X_EEPROM_CMD_PROGRAM;

	if (eeprom->reg_data_in)
		reg |= RTL818X_EEPROM_CMD_WRITE;
	if (eeprom->reg_data_out)
		reg |= RTL818X_EEPROM_CMD_READ;
	if (eeprom->reg_data_clock)
		reg |= RTL818X_EEPROM_CMD_CK;
	if (eeprom->reg_chip_select)
		reg |= RTL818X_EEPROM_CMD_CS;

	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, reg);
	udelay(10);
}

static int __devinit rtl8187_probe(struct usb_interface *intf,
				   const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	struct ieee80211_hw *dev;
	struct rtl8187_priv *priv;
	struct eeprom_93cx6 eeprom;
	struct ieee80211_channel *channel;
	const char *chip_name;
	u16 txpwr, reg;
	int err, i;
	DECLARE_MAC_BUF(mac);

	dev = ieee80211_alloc_hw(sizeof(*priv), &rtl8187_ops);
	if (!dev) {
		printk(KERN_ERR "rtl8187: ieee80211 alloc failed\n");
		return -ENOMEM;
	}

	priv = dev->priv;
	priv->is_rtl8187b = (id->driver_info == DEVICE_RTL8187B) ||
			    !memcmp(udev->product, "RTL8187B", 8);

	SET_IEEE80211_DEV(dev, &intf->dev);
	usb_set_intfdata(intf, dev);
	priv->udev = udev;

	usb_get_dev(udev);

	skb_queue_head_init(&priv->rx_queue);

	BUILD_BUG_ON(sizeof(priv->channels) != sizeof(rtl818x_channels));
	BUILD_BUG_ON(sizeof(priv->rates) != sizeof(rtl818x_rates));

	memcpy(priv->channels, rtl818x_channels, sizeof(rtl818x_channels));
	memcpy(priv->rates, rtl818x_rates, sizeof(rtl818x_rates));
	priv->map = (struct rtl818x_csr *)0xFF00;

	priv->band.band = IEEE80211_BAND_2GHZ;
	priv->band.channels = priv->channels;
	priv->band.n_channels = ARRAY_SIZE(rtl818x_channels);
	priv->band.bitrates = priv->rates;
	priv->band.n_bitrates = ARRAY_SIZE(rtl818x_rates);
	dev->wiphy->bands[IEEE80211_BAND_2GHZ] = &priv->band;


	priv->mode = IEEE80211_IF_TYPE_MNTR;
	dev->flags = IEEE80211_HW_HOST_BROADCAST_PS_BUFFERING |
		     IEEE80211_HW_RX_INCLUDES_FCS |
		     IEEE80211_HW_SIGNAL_UNSPEC;
	dev->extra_tx_headroom = (!priv->is_rtl8187b) ?
				  sizeof(struct rtl8187_tx_hdr) :
				  sizeof(struct rtl8187b_tx_hdr);
	if (!priv->is_rtl8187b)
		dev->queues = 1;
	else
		dev->queues = 4;
	dev->max_signal = 65;

	eeprom.data = dev;
	eeprom.register_read = rtl8187_eeprom_register_read;
	eeprom.register_write = rtl8187_eeprom_register_write;
	if (rtl818x_ioread32(priv, &priv->map->RX_CONF) & (1 << 6))
		eeprom.width = PCI_EEPROM_WIDTH_93C66;
	else
		eeprom.width = PCI_EEPROM_WIDTH_93C46;

	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, RTL818X_EEPROM_CMD_CONFIG);
	udelay(10);

	eeprom_93cx6_multiread(&eeprom, RTL8187_EEPROM_MAC_ADDR,
			       (__le16 __force *)dev->wiphy->perm_addr, 3);
	if (!is_valid_ether_addr(dev->wiphy->perm_addr)) {
		printk(KERN_WARNING "rtl8187: Invalid hwaddr! Using randomly "
		       "generated MAC address\n");
		random_ether_addr(dev->wiphy->perm_addr);
	}

	channel = priv->channels;
	for (i = 0; i < 3; i++) {
		eeprom_93cx6_read(&eeprom, RTL8187_EEPROM_TXPWR_CHAN_1 + i,
				  &txpwr);
		(*channel++).hw_value = txpwr & 0xFF;
		(*channel++).hw_value = txpwr >> 8;
	}
	for (i = 0; i < 2; i++) {
		eeprom_93cx6_read(&eeprom, RTL8187_EEPROM_TXPWR_CHAN_4 + i,
				  &txpwr);
		(*channel++).hw_value = txpwr & 0xFF;
		(*channel++).hw_value = txpwr >> 8;
	}
	if (!priv->is_rtl8187b) {
		for (i = 0; i < 2; i++) {
			eeprom_93cx6_read(&eeprom,
					  RTL8187_EEPROM_TXPWR_CHAN_6 + i,
					  &txpwr);
			(*channel++).hw_value = txpwr & 0xFF;
			(*channel++).hw_value = txpwr >> 8;
		}
	} else {
		eeprom_93cx6_read(&eeprom, RTL8187_EEPROM_TXPWR_CHAN_6,
				  &txpwr);
		(*channel++).hw_value = txpwr & 0xFF;

		eeprom_93cx6_read(&eeprom, 0x0A, &txpwr);
		(*channel++).hw_value = txpwr & 0xFF;

		eeprom_93cx6_read(&eeprom, 0x1C, &txpwr);
		(*channel++).hw_value = txpwr & 0xFF;
		(*channel++).hw_value = txpwr >> 8;
	}

	eeprom_93cx6_read(&eeprom, RTL8187_EEPROM_TXPWR_BASE,
			  &priv->txpwr_base);

	reg = rtl818x_ioread8(priv, &priv->map->PGSELECT) & ~1;
	rtl818x_iowrite8(priv, &priv->map->PGSELECT, reg | 1);
	/* 0 means asic B-cut, we should use SW 3 wire
	 * bit-by-bit banging for radio. 1 means we can use
	 * USB specific request to write radio registers */
	priv->asic_rev = rtl818x_ioread8(priv, (u8 *)0xFFFE) & 0x3;
	rtl818x_iowrite8(priv, &priv->map->PGSELECT, reg);
	rtl818x_iowrite8(priv, &priv->map->EEPROM_CMD, RTL818X_EEPROM_CMD_NORMAL);

	if (!priv->is_rtl8187b) {
		u32 reg32;
		reg32 = rtl818x_ioread32(priv, &priv->map->TX_CONF);
		reg32 &= RTL818X_TX_CONF_HWVER_MASK;
		switch (reg32) {
		case RTL818X_TX_CONF_R8187vD_1:
		case RTL818X_TX_CONF_R8187vD_2:
			chip_name = "RTL8187vD";
			break;
		default:
			chip_name = "RTL8187vB (default)";
		}
       } else {
		printk(KERN_WARNING "rtl8187: 8187B chip detected. Support "
			"is EXPERIMENTAL, and could damage your\n"
			"         hardware, use at your own risk\n");
		/*
		 * Force USB request to write radio registers for 8187B, Realtek
		 * only uses it in their sources
		 */
		/*if (priv->asic_rev == 0) {
			printk(KERN_WARNING "rtl8187: Forcing use of USB "
			       "requests to write to radio registers\n");
			priv->asic_rev = 1;
		}*/
		switch (rtl818x_ioread8(priv, (u8 *)0xFFE1)) {
		case RTL818X_R8187B_B:
			chip_name = "RTL8187BvB";
			priv->hw_rev = RTL8187BvB;
			break;
		case RTL818X_R8187B_D:
			chip_name = "RTL8187BvD";
			priv->hw_rev = RTL8187BvD;
			break;
		case RTL818X_R8187B_E:
			chip_name = "RTL8187BvE";
			priv->hw_rev = RTL8187BvE;
			break;
		default:
			chip_name = "RTL8187BvB (default)";
			priv->hw_rev = RTL8187BvB;
		}
	}

	priv->rf = rtl8187_detect_rf(dev);

	err = ieee80211_register_hw(dev);
	if (err) {
		printk(KERN_ERR "rtl8187: Cannot register device\n");
		goto err_free_dev;
	}

	printk(KERN_INFO "%s: hwaddr %s, %s V%d + %s\n",
	       wiphy_name(dev->wiphy), print_mac(mac, dev->wiphy->perm_addr),
	       chip_name, priv->asic_rev, priv->rf->name);

	return 0;

 err_free_dev:
	ieee80211_free_hw(dev);
	usb_set_intfdata(intf, NULL);
	usb_put_dev(udev);
	return err;
}

static void __devexit rtl8187_disconnect(struct usb_interface *intf)
{
	struct ieee80211_hw *dev = usb_get_intfdata(intf);
	struct rtl8187_priv *priv;

	if (!dev)
		return;

	ieee80211_unregister_hw(dev);

	priv = dev->priv;
	usb_put_dev(interface_to_usbdev(intf));
	ieee80211_free_hw(dev);
}

static struct usb_driver rtl8187_driver = {
	.name		= KBUILD_MODNAME,
	.id_table	= rtl8187_table,
	.probe		= rtl8187_probe,
	.disconnect	= rtl8187_disconnect,
};

static int __init rtl8187_init(void)
{
	return usb_register(&rtl8187_driver);
}

static void __exit rtl8187_exit(void)
{
	usb_deregister(&rtl8187_driver);
}

module_init(rtl8187_init);
module_exit(rtl8187_exit);
