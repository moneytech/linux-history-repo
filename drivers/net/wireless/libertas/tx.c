/**
  * This file contains the handling of TX in wlan driver.
  */
#include <linux/netdevice.h>

#include "hostcmd.h"
#include "radiotap.h"
#include "decl.h"
#include "defs.h"
#include "dev.h"
#include "wext.h"

/**
 *  @brief This function converts Tx/Rx rates from IEEE80211_RADIOTAP_RATE
 *  units (500 Kb/s) into Marvell WLAN format (see Table 8 in Section 3.2.1)
 *
 *  @param rate    Input rate
 *  @return      Output Rate (0 if invalid)
 */
static u32 convert_radiotap_rate_to_mv(u8 rate)
{
	switch (rate) {
	case 2:		/*   1 Mbps */
		return 0 | (1 << 4);
	case 4:		/*   2 Mbps */
		return 1 | (1 << 4);
	case 11:		/* 5.5 Mbps */
		return 2 | (1 << 4);
	case 22:		/*  11 Mbps */
		return 3 | (1 << 4);
	case 12:		/*   6 Mbps */
		return 4 | (1 << 4);
	case 18:		/*   9 Mbps */
		return 5 | (1 << 4);
	case 24:		/*  12 Mbps */
		return 6 | (1 << 4);
	case 36:		/*  18 Mbps */
		return 7 | (1 << 4);
	case 48:		/*  24 Mbps */
		return 8 | (1 << 4);
	case 72:		/*  36 Mbps */
		return 9 | (1 << 4);
	case 96:		/*  48 Mbps */
		return 10 | (1 << 4);
	case 108:		/*  54 Mbps */
		return 11 | (1 << 4);
	}
	return 0;
}

/**
 *  @brief This function processes a single packet and sends
 *  to IF layer
 *
 *  @param priv    A pointer to struct lbs_private structure
 *  @param skb     A pointer to skb which includes TX packet
 *  @return 	   0 or -1
 */
static int SendSinglePacket(struct lbs_private *priv, struct sk_buff *skb)
{
	int ret = 0;
	struct txpd localtxpd;
	struct txpd *plocaltxpd = &localtxpd;
	u8 *p802x_hdr;
	struct tx_radiotap_hdr *pradiotap_hdr;
	u32 new_rate;
	u8 *ptr = priv->tmptxbuf;

	lbs_deb_enter(LBS_DEB_TX);

	if (priv->surpriseremoved)
		return -1;

	if (!skb->len || (skb->len > MRVDRV_ETH_TX_PACKET_BUFFER_SIZE)) {
		lbs_deb_tx("tx err: skb length %d 0 or > %zd\n",
		       skb->len, MRVDRV_ETH_TX_PACKET_BUFFER_SIZE);
		ret = -1;
		goto done;
	}

	memset(plocaltxpd, 0, sizeof(struct txpd));

	plocaltxpd->tx_packet_length = cpu_to_le16(skb->len);

	/* offset of actual data */
	plocaltxpd->tx_packet_location = cpu_to_le32(sizeof(struct txpd));

	p802x_hdr = skb->data;
	if (priv->monitormode != LBS_MONITOR_OFF) {

		/* locate radiotap header */
		pradiotap_hdr = (struct tx_radiotap_hdr *)skb->data;

		/* set txpd fields from the radiotap header */
		new_rate = convert_radiotap_rate_to_mv(pradiotap_hdr->rate);
		if (new_rate != 0) {
			/* use new tx_control[4:0] */
			plocaltxpd->tx_control = cpu_to_le32(new_rate);
		}

		/* skip the radiotap header */
		p802x_hdr += sizeof(struct tx_radiotap_hdr);
		plocaltxpd->tx_packet_length =
			cpu_to_le16(le16_to_cpu(plocaltxpd->tx_packet_length)
				    - sizeof(struct tx_radiotap_hdr));

	}
	/* copy destination address from 802.3 or 802.11 header */
	if (priv->monitormode != LBS_MONITOR_OFF)
		memcpy(plocaltxpd->tx_dest_addr_high, p802x_hdr + 4, ETH_ALEN);
	else
		memcpy(plocaltxpd->tx_dest_addr_high, p802x_hdr, ETH_ALEN);

	lbs_deb_hex(LBS_DEB_TX, "txpd", (u8 *) plocaltxpd, sizeof(struct txpd));

	if (IS_MESH_FRAME(skb)) {
		plocaltxpd->tx_control |= cpu_to_le32(TxPD_MESH_FRAME);
	}

	memcpy(ptr, plocaltxpd, sizeof(struct txpd));

	ptr += sizeof(struct txpd);

	lbs_deb_hex(LBS_DEB_TX, "Tx Data", (u8 *) p802x_hdr, le16_to_cpu(plocaltxpd->tx_packet_length));
	memcpy(ptr, p802x_hdr, le16_to_cpu(plocaltxpd->tx_packet_length));
	ret = priv->hw_host_to_card(priv, MVMS_DAT,
				    priv->tmptxbuf,
				    le16_to_cpu(plocaltxpd->tx_packet_length) +
				    sizeof(struct txpd));

	if (ret) {
		lbs_deb_tx("tx err: hw_host_to_card returned 0x%X\n", ret);
		goto done;
	}

	lbs_deb_tx("SendSinglePacket succeeds\n");

done:
	if (!ret) {
		priv->stats.tx_packets++;
		priv->stats.tx_bytes += skb->len;
	} else {
		priv->stats.tx_dropped++;
		priv->stats.tx_errors++;
	}

	if (!ret && priv->monitormode != LBS_MONITOR_OFF) {
		/* Keep the skb to echo it back once Tx feedback is
		   received from FW */
		skb_orphan(skb);
		/* stop processing outgoing pkts */
		netif_stop_queue(priv->dev);
		if (priv->mesh_dev)
			netif_stop_queue(priv->mesh_dev);

		/* Keep the skb around for when we get feedback */
		priv->currenttxskb = skb;
	} else {
		dev_kfree_skb_any(skb);
	}

	lbs_deb_leave_args(LBS_DEB_TX, "ret %d", ret);
	return ret;
}


void lbs_tx_runqueue(struct lbs_private *priv)
{
	int i;

	spin_lock(&priv->txqueue_lock);
	for (i = 0; i < priv->tx_queue_idx; i++) {
		struct sk_buff *skb = priv->tx_queue_ps[i];
		spin_unlock(&priv->txqueue_lock);
		SendSinglePacket(priv, skb);
		spin_lock(&priv->txqueue_lock);
	}
	priv->tx_queue_idx = 0;
	spin_unlock(&priv->txqueue_lock);
}

static void lbs_tx_queue(struct lbs_private *priv, struct sk_buff *skb)
{

	spin_lock(&priv->txqueue_lock);

	WARN_ON(priv->tx_queue_idx >= NR_TX_QUEUE);
	priv->tx_queue_ps[priv->tx_queue_idx++] = skb;
	if (priv->tx_queue_idx == NR_TX_QUEUE) {
		netif_stop_queue(priv->dev);
		if (priv->mesh_dev)
			netif_stop_queue(priv->mesh_dev);
	} else {
		netif_start_queue(priv->dev);
		if (priv->mesh_dev)
			netif_start_queue(priv->mesh_dev);
	}

	spin_unlock(&priv->txqueue_lock);
}

/**
 *  @brief This function checks the conditions and sends packet to IF
 *  layer if everything is ok.
 *
 *  @param priv    A pointer to struct lbs_private structure
 *  @return 	   n/a
 */
int lbs_process_tx(struct lbs_private *priv, struct sk_buff *skb)
{
	int ret = -1;

	lbs_deb_enter(LBS_DEB_TX);
	lbs_deb_hex(LBS_DEB_TX, "TX Data", skb->data, min_t(unsigned int, skb->len, 100));

	if (priv->dnld_sent) {
		lbs_pr_alert( "TX error: dnld_sent = %d, not sending\n",
		       priv->dnld_sent);
		goto done;
	}

	if ((priv->psstate == PS_STATE_SLEEP) ||
	    (priv->psstate == PS_STATE_PRE_SLEEP)) {
		lbs_tx_queue(priv, skb);
		return ret;
	}

	ret = SendSinglePacket(priv, skb);
done:
	lbs_deb_leave_args(LBS_DEB_TX, "ret %d", ret);
	return ret;
}

/**
 *  @brief This function sends to the host the last transmitted packet,
 *  filling the radiotap headers with transmission information.
 *
 *  @param priv     A pointer to struct lbs_private structure
 *  @param status   A 32 bit value containing transmission status.
 *
 *  @returns void
 */
void lbs_send_tx_feedback(struct lbs_private *priv)
{
	struct tx_radiotap_hdr *radiotap_hdr;
	u32 status = priv->eventcause;
	int txfail;
	int try_count;

	if (priv->monitormode == LBS_MONITOR_OFF ||
	    priv->currenttxskb == NULL)
		return;

	radiotap_hdr = (struct tx_radiotap_hdr *)priv->currenttxskb->data;

	txfail = (status >> 24);

#if 0
	/* The version of roofnet that we've tested does not use this yet
	 * But it may be used in the future.
	 */
	if (txfail)
		radiotap_hdr->flags &= IEEE80211_RADIOTAP_F_TX_FAIL;
#endif
	try_count = (status >> 16) & 0xff;
	radiotap_hdr->data_retries = (try_count) ?
	    (1 + priv->txretrycount - try_count) : 0;
	lbs_upload_rx_packet(priv, priv->currenttxskb);
	priv->currenttxskb = NULL;

	if (priv->connect_status == LBS_CONNECTED)
		netif_wake_queue(priv->dev);

	if (priv->mesh_dev && (priv->mesh_connect_status == LBS_CONNECTED))
		netif_wake_queue(priv->mesh_dev);
}
EXPORT_SYMBOL_GPL(lbs_send_tx_feedback);
