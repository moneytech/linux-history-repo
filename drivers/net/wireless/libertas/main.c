/**
  * This file contains the major functions in WLAN
  * driver. It includes init, exit, open, close and main
  * thread etc..
  */

#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/kthread.h>

#include <net/iw_handler.h>
#include <net/ieee80211.h>

#include "host.h"
#include "decl.h"
#include "dev.h"
#include "wext.h"
#include "debugfs.h"
#include "assoc.h"

#define DRIVER_RELEASE_VERSION "322.p1"
const char libertas_driver_version[] = "COMM-USB8388-" DRIVER_RELEASE_VERSION
#ifdef  DEBUG
    "-dbg"
#endif
    "";


/* Module parameters */
unsigned int libertas_debug = 0;
module_param(libertas_debug, int, 0644);
EXPORT_SYMBOL_GPL(libertas_debug);


#define WLAN_TX_PWR_DEFAULT		20	/*100mW */
#define WLAN_TX_PWR_US_DEFAULT		20	/*100mW */
#define WLAN_TX_PWR_JP_DEFAULT		16	/*50mW */
#define WLAN_TX_PWR_FR_DEFAULT		20	/*100mW */
#define WLAN_TX_PWR_EMEA_DEFAULT	20	/*100mW */

/* Format { channel, frequency (MHz), maxtxpower } */
/* band: 'B/G', region: USA FCC/Canada IC */
static struct chan_freq_power channel_freq_power_US_BG[] = {
	{1, 2412, WLAN_TX_PWR_US_DEFAULT},
	{2, 2417, WLAN_TX_PWR_US_DEFAULT},
	{3, 2422, WLAN_TX_PWR_US_DEFAULT},
	{4, 2427, WLAN_TX_PWR_US_DEFAULT},
	{5, 2432, WLAN_TX_PWR_US_DEFAULT},
	{6, 2437, WLAN_TX_PWR_US_DEFAULT},
	{7, 2442, WLAN_TX_PWR_US_DEFAULT},
	{8, 2447, WLAN_TX_PWR_US_DEFAULT},
	{9, 2452, WLAN_TX_PWR_US_DEFAULT},
	{10, 2457, WLAN_TX_PWR_US_DEFAULT},
	{11, 2462, WLAN_TX_PWR_US_DEFAULT}
};

/* band: 'B/G', region: Europe ETSI */
static struct chan_freq_power channel_freq_power_EU_BG[] = {
	{1, 2412, WLAN_TX_PWR_EMEA_DEFAULT},
	{2, 2417, WLAN_TX_PWR_EMEA_DEFAULT},
	{3, 2422, WLAN_TX_PWR_EMEA_DEFAULT},
	{4, 2427, WLAN_TX_PWR_EMEA_DEFAULT},
	{5, 2432, WLAN_TX_PWR_EMEA_DEFAULT},
	{6, 2437, WLAN_TX_PWR_EMEA_DEFAULT},
	{7, 2442, WLAN_TX_PWR_EMEA_DEFAULT},
	{8, 2447, WLAN_TX_PWR_EMEA_DEFAULT},
	{9, 2452, WLAN_TX_PWR_EMEA_DEFAULT},
	{10, 2457, WLAN_TX_PWR_EMEA_DEFAULT},
	{11, 2462, WLAN_TX_PWR_EMEA_DEFAULT},
	{12, 2467, WLAN_TX_PWR_EMEA_DEFAULT},
	{13, 2472, WLAN_TX_PWR_EMEA_DEFAULT}
};

/* band: 'B/G', region: Spain */
static struct chan_freq_power channel_freq_power_SPN_BG[] = {
	{10, 2457, WLAN_TX_PWR_DEFAULT},
	{11, 2462, WLAN_TX_PWR_DEFAULT}
};

/* band: 'B/G', region: France */
static struct chan_freq_power channel_freq_power_FR_BG[] = {
	{10, 2457, WLAN_TX_PWR_FR_DEFAULT},
	{11, 2462, WLAN_TX_PWR_FR_DEFAULT},
	{12, 2467, WLAN_TX_PWR_FR_DEFAULT},
	{13, 2472, WLAN_TX_PWR_FR_DEFAULT}
};

/* band: 'B/G', region: Japan */
static struct chan_freq_power channel_freq_power_JPN_BG[] = {
	{1, 2412, WLAN_TX_PWR_JP_DEFAULT},
	{2, 2417, WLAN_TX_PWR_JP_DEFAULT},
	{3, 2422, WLAN_TX_PWR_JP_DEFAULT},
	{4, 2427, WLAN_TX_PWR_JP_DEFAULT},
	{5, 2432, WLAN_TX_PWR_JP_DEFAULT},
	{6, 2437, WLAN_TX_PWR_JP_DEFAULT},
	{7, 2442, WLAN_TX_PWR_JP_DEFAULT},
	{8, 2447, WLAN_TX_PWR_JP_DEFAULT},
	{9, 2452, WLAN_TX_PWR_JP_DEFAULT},
	{10, 2457, WLAN_TX_PWR_JP_DEFAULT},
	{11, 2462, WLAN_TX_PWR_JP_DEFAULT},
	{12, 2467, WLAN_TX_PWR_JP_DEFAULT},
	{13, 2472, WLAN_TX_PWR_JP_DEFAULT},
	{14, 2484, WLAN_TX_PWR_JP_DEFAULT}
};

/**
 * the structure for channel, frequency and power
 */
struct region_cfp_table {
	u8 region;
	struct chan_freq_power *cfp_BG;
	int cfp_no_BG;
};

/**
 * the structure for the mapping between region and CFP
 */
static struct region_cfp_table region_cfp_table[] = {
	{0x10,			/*US FCC */
	 channel_freq_power_US_BG,
	 sizeof(channel_freq_power_US_BG) / sizeof(struct chan_freq_power),
	 }
	,
	{0x20,			/*CANADA IC */
	 channel_freq_power_US_BG,
	 sizeof(channel_freq_power_US_BG) / sizeof(struct chan_freq_power),
	 }
	,
	{0x30, /*EU*/ channel_freq_power_EU_BG,
	 sizeof(channel_freq_power_EU_BG) / sizeof(struct chan_freq_power),
	 }
	,
	{0x31, /*SPAIN*/ channel_freq_power_SPN_BG,
	 sizeof(channel_freq_power_SPN_BG) / sizeof(struct chan_freq_power),
	 }
	,
	{0x32, /*FRANCE*/ channel_freq_power_FR_BG,
	 sizeof(channel_freq_power_FR_BG) / sizeof(struct chan_freq_power),
	 }
	,
	{0x40, /*JAPAN*/ channel_freq_power_JPN_BG,
	 sizeof(channel_freq_power_JPN_BG) / sizeof(struct chan_freq_power),
	 }
	,
/*Add new region here */
};

/**
 * the table to keep region code
 */
u16 libertas_region_code_to_index[MRVDRV_MAX_REGION_CODE] =
    { 0x10, 0x20, 0x30, 0x31, 0x32, 0x40 };

/**
 * 802.11b/g supported bitrates (in 500Kb/s units)
 */
u8 libertas_bg_rates[MAX_RATES] =
    { 0x02, 0x04, 0x0b, 0x16, 0x0c, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c,
0x00, 0x00 };

/**
 * FW rate table.  FW refers to rates by their index in this table, not by the
 * rate value itself.  Values of 0x00 are
 * reserved positions.
 */
static u8 fw_data_rates[MAX_RATES] =
    { 0x02, 0x04, 0x0B, 0x16, 0x00, 0x0C, 0x12,
      0x18, 0x24, 0x30, 0x48, 0x60, 0x6C, 0x00
};

/**
 *  @brief use index to get the data rate
 *
 *  @param idx                The index of data rate
 *  @return 	   		data rate or 0
 */
u32 libertas_fw_index_to_data_rate(u8 idx)
{
	if (idx >= sizeof(fw_data_rates))
		idx = 0;
	return fw_data_rates[idx];
}

/**
 *  @brief use rate to get the index
 *
 *  @param rate                 data rate
 *  @return 	   		index or 0
 */
u8 libertas_data_rate_to_fw_index(u32 rate)
{
	u8 i;

	if (!rate)
		return 0;

	for (i = 0; i < sizeof(fw_data_rates); i++) {
		if (rate == fw_data_rates[i])
			return i;
	}
	return 0;
}

/**
 * Attributes exported through sysfs
 */

/**
 * @brief Get function for sysfs attribute anycast_mask
 */
static ssize_t libertas_anycast_get(struct device * dev,
		struct device_attribute *attr, char * buf)
{
	struct cmd_ds_mesh_access mesh_access;

	memset(&mesh_access, 0, sizeof(mesh_access));
	libertas_prepare_and_send_command(to_net_dev(dev)->priv,
			CMD_MESH_ACCESS,
			CMD_ACT_MESH_GET_ANYCAST,
			CMD_OPTION_WAITFORRSP, 0, (void *)&mesh_access);

	return snprintf(buf, 12, "0x%X\n", le32_to_cpu(mesh_access.data[0]));
}

/**
 * @brief Set function for sysfs attribute anycast_mask
 */
static ssize_t libertas_anycast_set(struct device * dev,
		struct device_attribute *attr, const char * buf, size_t count)
{
	struct cmd_ds_mesh_access mesh_access;
	uint32_t datum;

	memset(&mesh_access, 0, sizeof(mesh_access));
	sscanf(buf, "%x", &datum);
	mesh_access.data[0] = cpu_to_le32(datum);

	libertas_prepare_and_send_command((to_net_dev(dev))->priv,
			CMD_MESH_ACCESS,
			CMD_ACT_MESH_SET_ANYCAST,
			CMD_OPTION_WAITFORRSP, 0, (void *)&mesh_access);
	return strlen(buf);
}

/**
 * anycast_mask attribute to be exported per mshX interface
 * through sysfs (/sys/class/net/mshX/anycast_mask)
 */
static DEVICE_ATTR(anycast_mask, 0644, libertas_anycast_get, libertas_anycast_set);

/**
 *  @brief Check if the device can be open and wait if necessary.
 *
 *  @param dev     A pointer to net_device structure
 *  @return 	   0
 *
 * For USB adapter, on some systems the device open handler will be
 * called before FW ready. Use the following flag check and wait
 * function to work around the issue.
 *
 */
static int pre_open_check(struct net_device *dev)
{
	wlan_private *priv = (wlan_private *) dev->priv;
	wlan_adapter *adapter = priv->adapter;
	int i = 0;

	while (!adapter->fw_ready && i < 20) {
		i++;
		msleep_interruptible(100);
	}
	if (!adapter->fw_ready) {
		lbs_pr_err("firmware not ready\n");
		return -1;
	}

	return 0;
}

/**
 *  @brief This function opens the device
 *
 *  @param dev     A pointer to net_device structure
 *  @return 	   0
 */
static int libertas_dev_open(struct net_device *dev)
{
	wlan_private *priv = (wlan_private *) dev->priv;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_NET);

	priv->open = 1;

	if (adapter->connect_status == LIBERTAS_CONNECTED) {
		netif_carrier_on(priv->dev);
		netif_carrier_on(priv->mesh_dev);
	} else {
		netif_carrier_off(priv->dev);
		netif_carrier_off(priv->mesh_dev);
	}

	lbs_deb_leave(LBS_DEB_NET);
	return 0;
}
/**
 *  @brief This function opens the mshX interface
 *
 *  @param dev     A pointer to net_device structure
 *  @return 	   0
 */
static int libertas_mesh_open(struct net_device *dev)
{
	wlan_private *priv = (wlan_private *) dev->priv ;

	if (pre_open_check(dev) == -1)
		return -1;
	priv->mesh_open = 1 ;
	netif_wake_queue(priv->mesh_dev);
	if (priv->infra_open == 0)
		return libertas_dev_open(priv->dev) ;
	return 0;
}

/**
 *  @brief This function opens the ethX interface
 *
 *  @param dev     A pointer to net_device structure
 *  @return 	   0
 */
static int libertas_open(struct net_device *dev)
{
	wlan_private *priv = (wlan_private *) dev->priv ;

	if(pre_open_check(dev) == -1)
		return -1;
	priv->infra_open = 1 ;
	netif_wake_queue(priv->dev);
	if (priv->open == 0)
		return libertas_dev_open(priv->dev) ;
	return 0;
}

static int libertas_dev_close(struct net_device *dev)
{
	wlan_private *priv = dev->priv;

	lbs_deb_enter(LBS_DEB_NET);

	netif_carrier_off(priv->dev);
	priv->open = 0;

	lbs_deb_leave(LBS_DEB_NET);
	return 0;
}

/**
 *  @brief This function closes the mshX interface
 *
 *  @param dev     A pointer to net_device structure
 *  @return 	   0
 */
static int libertas_mesh_close(struct net_device *dev)
{
	wlan_private *priv = (wlan_private *) (dev->priv);

	priv->mesh_open = 0;
	netif_stop_queue(priv->mesh_dev);
	if (priv->infra_open == 0)
		return libertas_dev_close(dev);
	else
		return 0;
}

/**
 *  @brief This function closes the ethX interface
 *
 *  @param dev     A pointer to net_device structure
 *  @return 	   0
 */
static int libertas_close(struct net_device *dev)
{
	wlan_private *priv = (wlan_private *) dev->priv;

	netif_stop_queue(dev);
	priv->infra_open = 0;
	if (priv->mesh_open == 0)
		return libertas_dev_close(dev);
	else
		return 0;
}


static int libertas_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret = 0;
	wlan_private *priv = dev->priv;

	lbs_deb_enter(LBS_DEB_NET);

	if (priv->dnld_sent || priv->adapter->TxLockFlag) {
		priv->stats.tx_dropped++;
		goto done;
	}

	netif_stop_queue(priv->dev);
	netif_stop_queue(priv->mesh_dev);

	if (libertas_process_tx(priv, skb) == 0)
		dev->trans_start = jiffies;
done:
	lbs_deb_leave_args(LBS_DEB_NET, "ret %d", ret);
	return ret;
}

/**
 * @brief Mark mesh packets and handover them to libertas_hard_start_xmit
 *
 */
static int libertas_mesh_pre_start_xmit(struct sk_buff *skb,
		struct net_device *dev)
{
	wlan_private *priv = dev->priv;
	int ret;

	lbs_deb_enter(LBS_DEB_MESH);

	SET_MESH_FRAME(skb);

	ret = libertas_hard_start_xmit(skb, priv->dev);
	lbs_deb_leave_args(LBS_DEB_MESH, "ret %d", ret);
	return ret;
}

/**
 * @brief Mark non-mesh packets and handover them to libertas_hard_start_xmit
 *
 */
static int libertas_pre_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret;

	lbs_deb_enter(LBS_DEB_NET);

	UNSET_MESH_FRAME(skb);

	ret = libertas_hard_start_xmit(skb, dev);
	lbs_deb_leave_args(LBS_DEB_NET, "ret %d", ret);
	return ret;
}

static void libertas_tx_timeout(struct net_device *dev)
{
	wlan_private *priv = (wlan_private *) dev->priv;

	lbs_deb_enter(LBS_DEB_TX);

	lbs_pr_err("tx watch dog timeout\n");

	priv->dnld_sent = DNLD_RES_RECEIVED;
	dev->trans_start = jiffies;

	if (priv->adapter->currenttxskb) {
		if (priv->adapter->radiomode == WLAN_RADIOMODE_RADIOTAP) {
			/* If we are here, we have not received feedback from
			   the previous packet.  Assume TX_FAIL and move on. */
			priv->adapter->eventcause = 0x01000000;
			libertas_send_tx_feedback(priv);
		} else
			wake_up_interruptible(&priv->waitq);
	} else if (priv->adapter->connect_status == LIBERTAS_CONNECTED) {
		netif_wake_queue(priv->dev);
		netif_wake_queue(priv->mesh_dev);
	}

	lbs_deb_leave(LBS_DEB_TX);
}

/**
 *  @brief This function returns the network statistics
 *
 *  @param dev     A pointer to wlan_private structure
 *  @return 	   A pointer to net_device_stats structure
 */
static struct net_device_stats *libertas_get_stats(struct net_device *dev)
{
	wlan_private *priv = (wlan_private *) dev->priv;

	return &priv->stats;
}

static int libertas_set_mac_address(struct net_device *dev, void *addr)
{
	int ret = 0;
	wlan_private *priv = (wlan_private *) dev->priv;
	wlan_adapter *adapter = priv->adapter;
	struct sockaddr *phwaddr = addr;

	lbs_deb_enter(LBS_DEB_NET);

	/* In case it was called from the mesh device */
	dev = priv->dev ;

	memset(adapter->current_addr, 0, ETH_ALEN);

	/* dev->dev_addr is 8 bytes */
	lbs_dbg_hex("dev->dev_addr:", dev->dev_addr, ETH_ALEN);

	lbs_dbg_hex("addr:", phwaddr->sa_data, ETH_ALEN);
	memcpy(adapter->current_addr, phwaddr->sa_data, ETH_ALEN);

	ret = libertas_prepare_and_send_command(priv, CMD_802_11_MAC_ADDRESS,
				    CMD_ACT_SET,
				    CMD_OPTION_WAITFORRSP, 0, NULL);

	if (ret) {
		lbs_deb_net("set MAC address failed\n");
		ret = -1;
		goto done;
	}

	lbs_dbg_hex("adapter->macaddr:", adapter->current_addr, ETH_ALEN);
	memcpy(dev->dev_addr, adapter->current_addr, ETH_ALEN);
	if (priv->mesh_dev)
		memcpy(priv->mesh_dev->dev_addr, adapter->current_addr, ETH_ALEN);

done:
	lbs_deb_leave_args(LBS_DEB_NET, "ret %d", ret);
	return ret;
}

static int libertas_copy_multicast_address(wlan_adapter * adapter,
				     struct net_device *dev)
{
	int i = 0;
	struct dev_mc_list *mcptr = dev->mc_list;

	for (i = 0; i < dev->mc_count; i++) {
		memcpy(&adapter->multicastlist[i], mcptr->dmi_addr, ETH_ALEN);
		mcptr = mcptr->next;
	}

	return i;

}

static void libertas_set_multicast_list(struct net_device *dev)
{
	wlan_private *priv = dev->priv;
	wlan_adapter *adapter = priv->adapter;
	int oldpacketfilter;

	lbs_deb_enter(LBS_DEB_NET);

	oldpacketfilter = adapter->currentpacketfilter;

	if (dev->flags & IFF_PROMISC) {
		lbs_deb_net("enable promiscuous mode\n");
		adapter->currentpacketfilter |=
		    CMD_ACT_MAC_PROMISCUOUS_ENABLE;
		adapter->currentpacketfilter &=
		    ~(CMD_ACT_MAC_ALL_MULTICAST_ENABLE |
		      CMD_ACT_MAC_MULTICAST_ENABLE);
	} else {
		/* Multicast */
		adapter->currentpacketfilter &=
		    ~CMD_ACT_MAC_PROMISCUOUS_ENABLE;

		if (dev->flags & IFF_ALLMULTI || dev->mc_count >
		    MRVDRV_MAX_MULTICAST_LIST_SIZE) {
			lbs_deb_net( "enabling all multicast\n");
			adapter->currentpacketfilter |=
			    CMD_ACT_MAC_ALL_MULTICAST_ENABLE;
			adapter->currentpacketfilter &=
			    ~CMD_ACT_MAC_MULTICAST_ENABLE;
		} else {
			adapter->currentpacketfilter &=
			    ~CMD_ACT_MAC_ALL_MULTICAST_ENABLE;

			if (!dev->mc_count) {
				lbs_deb_net("no multicast addresses, "
				       "disabling multicast\n");
				adapter->currentpacketfilter &=
				    ~CMD_ACT_MAC_MULTICAST_ENABLE;
			} else {
				int i;

				adapter->currentpacketfilter |=
				    CMD_ACT_MAC_MULTICAST_ENABLE;

				adapter->nr_of_multicastmacaddr =
				    libertas_copy_multicast_address(adapter, dev);

				lbs_deb_net("multicast addresses: %d\n",
				       dev->mc_count);

				for (i = 0; i < dev->mc_count; i++) {
					lbs_deb_net("Multicast address %d:"
					       MAC_FMT "\n", i,
					       adapter->multicastlist[i][0],
					       adapter->multicastlist[i][1],
					       adapter->multicastlist[i][2],
					       adapter->multicastlist[i][3],
					       adapter->multicastlist[i][4],
					       adapter->multicastlist[i][5]);
				}
				/* send multicast addresses to firmware */
				libertas_prepare_and_send_command(priv,
						      CMD_MAC_MULTICAST_ADR,
						      CMD_ACT_SET, 0, 0,
						      NULL);
			}
		}
	}

	if (adapter->currentpacketfilter != oldpacketfilter) {
		libertas_set_mac_packet_filter(priv);
	}

	lbs_deb_leave(LBS_DEB_NET);
}

/**
 *  @brief This function handles the major jobs in the WLAN driver.
 *  It handles all events generated by firmware, RX data received
 *  from firmware and TX data sent from kernel.
 *
 *  @param data    A pointer to wlan_thread structure
 *  @return 	   0
 */
static int libertas_thread(void *data)
{
	struct net_device *dev = data;
	wlan_private *priv = dev->priv;
	wlan_adapter *adapter = priv->adapter;
	wait_queue_t wait;
	u8 ireg = 0;

	lbs_deb_enter(LBS_DEB_THREAD);

	init_waitqueue_entry(&wait, current);

	for (;;) {
		lbs_deb_thread( "main-thread 111: intcounter=%d "
		       "currenttxskb=%p dnld_sent=%d\n",
		       adapter->intcounter,
		       adapter->currenttxskb, priv->dnld_sent);

		add_wait_queue(&priv->waitq, &wait);
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irq(&adapter->driver_lock);
		if ((adapter->psstate == PS_STATE_SLEEP) ||
		    (!adapter->intcounter
		     && (priv->dnld_sent || adapter->cur_cmd ||
			 list_empty(&adapter->cmdpendingq)))) {
			lbs_deb_thread(
			       "main-thread sleeping... Conn=%d IntC=%d PS_mode=%d PS_State=%d\n",
			       adapter->connect_status, adapter->intcounter,
			       adapter->psmode, adapter->psstate);
			spin_unlock_irq(&adapter->driver_lock);
			schedule();
		} else
			spin_unlock_irq(&adapter->driver_lock);


		lbs_deb_thread(
		       "main-thread 222 (waking up): intcounter=%d currenttxskb=%p "
		       "dnld_sent=%d\n", adapter->intcounter,
		       adapter->currenttxskb, priv->dnld_sent);

		set_current_state(TASK_RUNNING);
		remove_wait_queue(&priv->waitq, &wait);
		try_to_freeze();

		lbs_deb_thread("main-thread 333: intcounter=%d currenttxskb=%p "
		       "dnld_sent=%d\n",
		       adapter->intcounter,
		       adapter->currenttxskb, priv->dnld_sent);

		if (kthread_should_stop()
		    || adapter->surpriseremoved) {
			lbs_deb_thread(
			       "main-thread: break from main thread: surpriseremoved=0x%x\n",
			       adapter->surpriseremoved);
			break;
		}


		spin_lock_irq(&adapter->driver_lock);
		if (adapter->intcounter) {
			u8 int_status;
			adapter->intcounter = 0;
			int_status = priv->hw_get_int_status(priv, &ireg);

			if (int_status) {
				lbs_deb_thread(
				       "main-thread: reading HOST_INT_STATUS_REG failed\n");
				spin_unlock_irq(&adapter->driver_lock);
				continue;
			}
			adapter->hisregcpy |= ireg;
		}

		lbs_deb_thread("main-thread 444: intcounter=%d currenttxskb=%p "
		       "dnld_sent=%d\n",
		       adapter->intcounter,
		       adapter->currenttxskb, priv->dnld_sent);

		/* command response? */
		if (adapter->hisregcpy & his_cmdupldrdy) {
			lbs_deb_thread("main-thread: cmd response ready\n");

			adapter->hisregcpy &= ~his_cmdupldrdy;
			spin_unlock_irq(&adapter->driver_lock);
			libertas_process_rx_command(priv);
			spin_lock_irq(&adapter->driver_lock);
		}

		/* Any Card Event */
		if (adapter->hisregcpy & his_cardevent) {
			lbs_deb_thread("main-thread: Card Event Activity\n");

			adapter->hisregcpy &= ~his_cardevent;

			if (priv->hw_read_event_cause(priv)) {
				lbs_pr_alert(
				       "main-thread: hw_read_event_cause failed\n");
				spin_unlock_irq(&adapter->driver_lock);
				continue;
			}
			spin_unlock_irq(&adapter->driver_lock);
			libertas_process_event(priv);
		} else
			spin_unlock_irq(&adapter->driver_lock);

		/* Check if we need to confirm Sleep Request received previously */
		if (adapter->psstate == PS_STATE_PRE_SLEEP) {
			if (!priv->dnld_sent && !adapter->cur_cmd) {
				if (adapter->connect_status ==
				    LIBERTAS_CONNECTED) {
					lbs_deb_thread(
					       "main_thread: PRE_SLEEP--intcounter=%d currenttxskb=%p "
					       "dnld_sent=%d cur_cmd=%p, confirm now\n",
					       adapter->intcounter,
					       adapter->currenttxskb,
					       priv->dnld_sent,
					       adapter->cur_cmd);

					libertas_ps_confirm_sleep(priv,
						       (u16) adapter->psmode);
				} else {
					/* workaround for firmware sending
					 * deauth/linkloss event immediately
					 * after sleep request, remove this
					 * after firmware fixes it
					 */
					adapter->psstate = PS_STATE_AWAKE;
					lbs_pr_alert(
					       "main-thread: ignore PS_SleepConfirm in non-connected state\n");
				}
			}
		}

		/* The PS state is changed during processing of Sleep Request
		 * event above
		 */
		if ((priv->adapter->psstate == PS_STATE_SLEEP) ||
		    (priv->adapter->psstate == PS_STATE_PRE_SLEEP))
			continue;

		/* Execute the next command */
		if (!priv->dnld_sent && !priv->adapter->cur_cmd)
			libertas_execute_next_command(priv);

		/* Wake-up command waiters which can't sleep in
		 * libertas_prepare_and_send_command
		 */
		if (!adapter->nr_cmd_pending)
			wake_up_all(&adapter->cmd_pending);

		libertas_tx_runqueue(priv);
	}

	del_timer(&adapter->command_timer);
	adapter->nr_cmd_pending = 0;
	wake_up_all(&adapter->cmd_pending);

	lbs_deb_leave(LBS_DEB_THREAD);
	return 0;
}

/**
 *  @brief This function downloads firmware image, gets
 *  HW spec from firmware and set basic parameters to
 *  firmware.
 *
 *  @param priv    A pointer to wlan_private structure
 *  @return 	   0 or -1
 */
static int wlan_setup_station_hw(wlan_private * priv)
{
	int ret = -1;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_FW);

	ret = priv->hw_prog_firmware(priv);

	if (ret) {
		lbs_deb_fw("bootloader in invalid state\n");
		ret = -1;
		goto done;
	}

	/*
	 * Read MAC address from HW
	 */
	memset(adapter->current_addr, 0xff, ETH_ALEN);

	ret = libertas_prepare_and_send_command(priv, CMD_GET_HW_SPEC,
				    0, CMD_OPTION_WAITFORRSP, 0, NULL);

	if (ret) {
		ret = -1;
		goto done;
	}

	libertas_set_mac_packet_filter(priv);

	/* Get the supported Data rates */
	ret = libertas_prepare_and_send_command(priv, CMD_802_11_DATA_RATE,
				    CMD_ACT_GET_TX_RATE,
				    CMD_OPTION_WAITFORRSP, 0, NULL);

	if (ret) {
		ret = -1;
		goto done;
	}

	ret = 0;
done:
	lbs_deb_leave_args(LBS_DEB_FW, "ret %d", ret);
	return ret;
}

static void command_timer_fn(unsigned long data);

/**
 *  This function handles the timeout of command sending.
 *  It will re-send the same command again.
 */
static void command_timer_fn(unsigned long data)
{
	wlan_private *priv = (wlan_private *)data;
	wlan_adapter *adapter = priv->adapter;
	struct cmd_ctrl_node *ptempnode;
	struct cmd_ds_command *cmd;
	unsigned long flags;

	ptempnode = adapter->cur_cmd;
	if (ptempnode == NULL) {
		lbs_deb_fw("ptempnode empty\n");
		return;
	}

	cmd = (struct cmd_ds_command *)ptempnode->bufvirtualaddr;
	if (!cmd) {
		lbs_deb_fw("cmd is NULL\n");
		return;
	}

	lbs_deb_fw("command_timer_fn fired, cmd %x\n", cmd->command);

	if (!adapter->fw_ready)
		return;

	spin_lock_irqsave(&adapter->driver_lock, flags);
	adapter->cur_cmd = NULL;
	spin_unlock_irqrestore(&adapter->driver_lock, flags);

	lbs_deb_fw("re-sending same command because of timeout\n");
	libertas_queue_cmd(adapter, ptempnode, 0);

	wake_up_interruptible(&priv->waitq);

	return;
}

static void libertas_free_adapter(wlan_private * priv)
{
	wlan_adapter *adapter = priv->adapter;

	if (!adapter) {
		lbs_deb_fw("why double free adapter?\n");
		return;
	}

	lbs_deb_fw("free command buffer\n");
	libertas_free_cmd_buffer(priv);

	lbs_deb_fw("free command_timer\n");
	del_timer(&adapter->command_timer);

	lbs_deb_fw("free scan results table\n");
	kfree(adapter->networks);
	adapter->networks = NULL;

	/* Free the adapter object itself */
	lbs_deb_fw("free adapter\n");
	kfree(adapter);
	priv->adapter = NULL;
}

static int wlan_allocate_adapter(wlan_private * priv)
{
	size_t bufsize;
	wlan_adapter *adapter = priv->adapter;

	/* Allocate buffer to store the BSSID list */
	bufsize = MAX_NETWORK_COUNT * sizeof(struct bss_descriptor);
	adapter->networks = kzalloc(bufsize, GFP_KERNEL);
	if (!adapter->networks) {
		lbs_pr_err("Out of memory allocating beacons\n");
		libertas_free_adapter(priv);
		return -ENOMEM;
	}

	/* Allocate the command buffers */
	libertas_allocate_cmd_buffer(priv);

	memset(&adapter->libertas_ps_confirm_sleep, 0, sizeof(struct PS_CMD_ConfirmSleep));
	adapter->libertas_ps_confirm_sleep.seqnum = cpu_to_le16(++adapter->seqnum);
	adapter->libertas_ps_confirm_sleep.command =
	    cpu_to_le16(CMD_802_11_PS_MODE);
	adapter->libertas_ps_confirm_sleep.size =
	    cpu_to_le16(sizeof(struct PS_CMD_ConfirmSleep));
	adapter->libertas_ps_confirm_sleep.result = 0;
	adapter->libertas_ps_confirm_sleep.action =
	    cpu_to_le16(CMD_SUBCMD_SLEEP_CONFIRMED);

	return 0;
}

static void wlan_init_adapter(wlan_private * priv)
{
	wlan_adapter *adapter = priv->adapter;
	int i;

	adapter->scanprobes = 0;

	adapter->bcn_avg_factor = DEFAULT_BCN_AVG_FACTOR;
	adapter->data_avg_factor = DEFAULT_DATA_AVG_FACTOR;

	/* ATIM params */
	adapter->atimwindow = 0;

	adapter->connect_status = LIBERTAS_DISCONNECTED;
	memset(adapter->current_addr, 0xff, ETH_ALEN);

	/* scan type */
	adapter->scantype = CMD_SCAN_TYPE_ACTIVE;

	/* scan mode */
	adapter->scanmode = CMD_BSS_TYPE_ANY;

	/* 802.11 specific */
	adapter->secinfo.wep_enabled = 0;
	for (i = 0; i < sizeof(adapter->wep_keys) / sizeof(adapter->wep_keys[0]);
	     i++)
		memset(&adapter->wep_keys[i], 0, sizeof(struct enc_key));
	adapter->wep_tx_keyidx = 0;
	adapter->secinfo.auth_mode = IW_AUTH_ALG_OPEN_SYSTEM;
	adapter->mode = IW_MODE_INFRA;

	adapter->pending_assoc_req = NULL;
	adapter->in_progress_assoc_req = NULL;

	/* Initialize scan result lists */
	INIT_LIST_HEAD(&adapter->network_free_list);
	INIT_LIST_HEAD(&adapter->network_list);
	for (i = 0; i < MAX_NETWORK_COUNT; i++) {
		list_add_tail(&adapter->networks[i].list,
			      &adapter->network_free_list);
	}

	mutex_init(&adapter->lock);

	adapter->prescan = 1;

	memset(&adapter->curbssparams, 0, sizeof(adapter->curbssparams));
	adapter->curbssparams.channel = DEFAULT_AD_HOC_CHANNEL;

	/* PnP and power profile */
	adapter->surpriseremoved = 0;

	adapter->currentpacketfilter =
	    CMD_ACT_MAC_RX_ON | CMD_ACT_MAC_TX_ON;

	adapter->radioon = RADIO_ON;
	adapter->txantenna = RF_ANTENNA_2;
	adapter->rxantenna = RF_ANTENNA_AUTO;

	adapter->auto_rate = 1;
	adapter->cur_rate = 0;
	adapter->adhoc_grate_enabled = 0;

	adapter->beaconperiod = MRVDRV_BEACON_INTERVAL;

	// set default capabilities
	adapter->capability = WLAN_CAPABILITY_SHORT_PREAMBLE;

	adapter->psmode = WLAN802_11POWERMODECAM;
	adapter->multipledtim = MRVDRV_DEFAULT_MULTIPLE_DTIM;

	adapter->listeninterval = MRVDRV_DEFAULT_LISTEN_INTERVAL;

	adapter->psstate = PS_STATE_FULL_POWER;
	adapter->needtowakeup = 0;
	adapter->locallisteninterval = 0;	/* default value in firmware will be used */

	adapter->intcounter = 0;

	adapter->currenttxskb = NULL;
	adapter->pkttxctrl = 0;

	memset(&adapter->tx_queue_ps, 0, NR_TX_QUEUE*sizeof(struct sk_buff*));
	adapter->tx_queue_idx = 0;
	spin_lock_init(&adapter->txqueue_lock);

	return;
}

static int libertas_init_fw(wlan_private * priv)
{
	int ret = -1;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_FW);

	/* Allocate adapter structure */
	if ((ret = wlan_allocate_adapter(priv)) != 0)
		goto done;

	/* init adapter structure */
	wlan_init_adapter(priv);

	/* init timer etc. */
	setup_timer(&adapter->command_timer, command_timer_fn,
			(unsigned long)priv);

	/* download fimrware etc. */
	if ((ret = wlan_setup_station_hw(priv)) != 0) {
		del_timer_sync(&adapter->command_timer);
		goto done;
	}

	/* init 802.11d */
	libertas_init_11d(priv);

	ret = 0;
done:
	lbs_deb_leave_args(LBS_DEB_FW, "ret %d", ret);
	return ret;
}

/**
 * @brief This function adds the card. it will probe the
 * card, allocate the wlan_priv and initialize the device.
 *
 *  @param card    A pointer to card
 *  @return 	   A pointer to wlan_private structure
 */
wlan_private *libertas_add_card(void *card, struct device *dmdev)
{
	struct net_device *dev = NULL;
	wlan_private *priv = NULL;

	lbs_deb_enter(LBS_DEB_NET);

	/* Allocate an Ethernet device and register it */
	if (!(dev = alloc_etherdev(sizeof(wlan_private)))) {
		lbs_pr_err("init ethX device failed\n");
		return NULL;
	}
	priv = dev->priv;

	/* allocate buffer for wlan_adapter */
	if (!(priv->adapter = kzalloc(sizeof(wlan_adapter), GFP_KERNEL))) {
		lbs_pr_err("allocate buffer for wlan_adapter failed\n");
		goto err_kzalloc;
	}

	priv->dev = dev;
	priv->card = card;
	priv->mesh_open = 0;
	priv->infra_open = 0;

	SET_MODULE_OWNER(dev);

	/* Setup the OS Interface to our functions */
	dev->open = libertas_open;
	dev->hard_start_xmit = libertas_pre_start_xmit;
	dev->stop = libertas_close;
	dev->set_mac_address = libertas_set_mac_address;
	dev->tx_timeout = libertas_tx_timeout;
	dev->get_stats = libertas_get_stats;
	dev->watchdog_timeo = 5 * HZ;
	dev->ethtool_ops = &libertas_ethtool_ops;
#ifdef	WIRELESS_EXT
	dev->wireless_handlers = (struct iw_handler_def *)&libertas_handler_def;
#endif
#define NETIF_F_DYNALLOC 16
	dev->features |= NETIF_F_DYNALLOC;
	dev->flags |= IFF_BROADCAST | IFF_MULTICAST;
	dev->set_multicast_list = libertas_set_multicast_list;

	SET_NETDEV_DEV(dev, dmdev);

	INIT_LIST_HEAD(&priv->adapter->cmdfreeq);
	INIT_LIST_HEAD(&priv->adapter->cmdpendingq);

	spin_lock_init(&priv->adapter->driver_lock);
	init_waitqueue_head(&priv->adapter->cmd_pending);
	priv->adapter->nr_cmd_pending = 0;
	goto done;

err_kzalloc:
	free_netdev(dev);
	priv = NULL;
done:
	lbs_deb_leave_args(LBS_DEB_NET, "priv %p", priv);
	return priv;
}
EXPORT_SYMBOL_GPL(libertas_add_card);

int libertas_activate_card(wlan_private *priv)
{
	struct net_device *dev = priv->dev;
	int ret = -1;

	lbs_deb_enter(LBS_DEB_MAIN);

	lbs_deb_thread("Starting main thread...\n");
	init_waitqueue_head(&priv->waitq);
	priv->main_thread = kthread_run(libertas_thread, dev, "libertas_main");
	if (IS_ERR(priv->main_thread)) {
		lbs_deb_thread("Error creating main thread.\n");
		goto done;
	}

	priv->assoc_thread =
		create_singlethread_workqueue("libertas_assoc");
	INIT_DELAYED_WORK(&priv->assoc_work, libertas_association_worker);
	INIT_WORK(&priv->sync_channel, libertas_sync_channel);

	/*
	 * Register the device. Fillup the private data structure with
	 * relevant information from the card and request for the required
	 * IRQ.
	 */
	if (priv->hw_register_dev(priv) < 0) {
		lbs_pr_err("failed to register WLAN device\n");
		goto err_registerdev;
	}

	/* init FW and HW */
	if (libertas_init_fw(priv)) {
		lbs_pr_err("firmware init failed\n");
		goto err_registerdev;
	}

	if (register_netdev(dev)) {
		lbs_pr_err("cannot register ethX device\n");
		goto err_init_fw;
	}

	lbs_pr_info("%s: Marvell WLAN 802.11 adapter\n", dev->name);

	libertas_debugfs_init_one(priv, dev);

	ret = 0;
	goto done;

err_init_fw:
	priv->hw_unregister_dev(priv);
err_registerdev:
	destroy_workqueue(priv->assoc_thread);
	/* Stop the thread servicing the interrupts */
	wake_up_interruptible(&priv->waitq);
	kthread_stop(priv->main_thread);
done:
	lbs_deb_leave_args(LBS_DEB_NET, "ret %d", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(libertas_activate_card);


/**
 * @brief This function adds mshX interface
 *
 *  @param priv    A pointer to the wlan_private structure
 *  @return 	   0 if successful, -X otherwise
 */
int libertas_add_mesh(wlan_private *priv, struct device *dev)
{
	struct net_device *mesh_dev = NULL;
	int ret = 0;

	lbs_deb_enter(LBS_DEB_MESH);

	/* Allocate a virtual mesh device */
	if (!(mesh_dev = alloc_netdev(0, "msh%d", ether_setup))) {
		lbs_deb_mesh("init mshX device failed\n");
		ret = -ENOMEM;
		goto done;
	}
	mesh_dev->priv = priv;
	priv->mesh_dev = mesh_dev;

	SET_MODULE_OWNER(mesh_dev);

	mesh_dev->open = libertas_mesh_open;
	mesh_dev->hard_start_xmit = libertas_mesh_pre_start_xmit;
	mesh_dev->stop = libertas_mesh_close;
	mesh_dev->get_stats = libertas_get_stats;
	mesh_dev->set_mac_address = libertas_set_mac_address;
	mesh_dev->ethtool_ops = &libertas_ethtool_ops;
	memcpy(mesh_dev->dev_addr, priv->dev->dev_addr,
			sizeof(priv->dev->dev_addr));

	SET_NETDEV_DEV(priv->mesh_dev, dev);

#ifdef	WIRELESS_EXT
	mesh_dev->wireless_handlers = (struct iw_handler_def *)&mesh_handler_def;
#endif
#define NETIF_F_DYNALLOC 16

	/* Register virtual mesh interface */
	ret = register_netdev(mesh_dev);
	if (ret) {
		lbs_pr_err("cannot register mshX virtual interface\n");
		goto err_free;
	}

	ret = device_create_file(&(mesh_dev->dev), &dev_attr_anycast_mask);
	if (ret)
		goto err_unregister;

	/* Everything successful */
	ret = 0;
	goto done;


err_unregister:
	unregister_netdev(mesh_dev);

err_free:
	free_netdev(mesh_dev);

done:
	lbs_deb_leave_args(LBS_DEB_MESH, "ret %d", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(libertas_add_mesh);

static void wake_pending_cmdnodes(wlan_private *priv)
{
	struct cmd_ctrl_node *cmdnode;
	unsigned long flags;

	lbs_deb_enter(LBS_DEB_CMD);

	spin_lock_irqsave(&priv->adapter->driver_lock, flags);
	list_for_each_entry(cmdnode, &priv->adapter->cmdpendingq, list) {
		cmdnode->cmdwaitqwoken = 1;
		wake_up_interruptible(&cmdnode->cmdwait_q);
	}
	spin_unlock_irqrestore(&priv->adapter->driver_lock, flags);
}


int libertas_remove_card(wlan_private *priv)
{
	wlan_adapter *adapter;
	struct net_device *dev;
	union iwreq_data wrqu;

	lbs_deb_enter(LBS_DEB_NET);

	if (!priv)
		goto out;

	adapter = priv->adapter;

	if (!adapter)
		goto out;

	dev = priv->dev;

	netif_stop_queue(priv->dev);
	netif_carrier_off(priv->dev);

	wake_pending_cmdnodes(priv);

	unregister_netdev(dev);

	cancel_delayed_work(&priv->assoc_work);
	destroy_workqueue(priv->assoc_thread);

	if (adapter->psmode == WLAN802_11POWERMODEMAX_PSP) {
		adapter->psmode = WLAN802_11POWERMODECAM;
		libertas_ps_wakeup(priv, CMD_OPTION_WAITFORRSP);
	}

	memset(wrqu.ap_addr.sa_data, 0xaa, ETH_ALEN);
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	wireless_send_event(priv->dev, SIOCGIWAP, &wrqu, NULL);

	adapter->surpriseremoved = 1;

	/* Stop the thread servicing the interrupts */
	kthread_stop(priv->main_thread);

	libertas_debugfs_remove_one(priv);

	lbs_deb_net("free adapter\n");
	libertas_free_adapter(priv);

	lbs_deb_net("unregister finish\n");

	priv->dev = NULL;
	free_netdev(dev);

out:
	lbs_deb_leave(LBS_DEB_NET);
	return 0;
}
EXPORT_SYMBOL_GPL(libertas_remove_card);


void libertas_remove_mesh(wlan_private *priv)
{
	struct net_device *mesh_dev;

	lbs_deb_enter(LBS_DEB_NET);

	if (!priv)
		goto out;

	mesh_dev = priv->mesh_dev;

	netif_stop_queue(mesh_dev);
	netif_carrier_off(priv->mesh_dev);

	device_remove_file(&(mesh_dev->dev), &dev_attr_anycast_mask);
	unregister_netdev(mesh_dev);

	priv->mesh_dev = NULL ;
	free_netdev(mesh_dev);

out:
	lbs_deb_leave(LBS_DEB_NET);
}
EXPORT_SYMBOL_GPL(libertas_remove_mesh);

/**
 *  @brief This function finds the CFP in
 *  region_cfp_table based on region and band parameter.
 *
 *  @param region  The region code
 *  @param band	   The band
 *  @param cfp_no  A pointer to CFP number
 *  @return 	   A pointer to CFP
 */
struct chan_freq_power *libertas_get_region_cfp_table(u8 region, u8 band, int *cfp_no)
{
	int i, end;

	lbs_deb_enter(LBS_DEB_MAIN);

	end = sizeof(region_cfp_table)/sizeof(struct region_cfp_table);

	for (i = 0; i < end ; i++) {
		lbs_deb_main("region_cfp_table[i].region=%d\n",
			region_cfp_table[i].region);
		if (region_cfp_table[i].region == region) {
			*cfp_no = region_cfp_table[i].cfp_no_BG;
			lbs_deb_leave(LBS_DEB_MAIN);
			return region_cfp_table[i].cfp_BG;
		}
	}

	lbs_deb_leave_args(LBS_DEB_MAIN, "ret NULL");
	return NULL;
}

int libertas_set_regiontable(wlan_private * priv, u8 region, u8 band)
{
	wlan_adapter *adapter = priv->adapter;
	int ret = 0;
	int i = 0;

	struct chan_freq_power *cfp;
	int cfp_no;

	lbs_deb_enter(LBS_DEB_MAIN);

	memset(adapter->region_channel, 0, sizeof(adapter->region_channel));

	{
		cfp = libertas_get_region_cfp_table(region, band, &cfp_no);
		if (cfp != NULL) {
			adapter->region_channel[i].nrcfp = cfp_no;
			adapter->region_channel[i].CFP = cfp;
		} else {
			lbs_deb_main("wrong region code %#x in band B/G\n",
			       region);
			ret = -1;
			goto out;
		}
		adapter->region_channel[i].valid = 1;
		adapter->region_channel[i].region = region;
		adapter->region_channel[i].band = band;
		i++;
	}
out:
	lbs_deb_leave_args(LBS_DEB_MAIN, "ret %d", ret);
	return ret;
}

/**
 *  @brief This function handles the interrupt. it will change PS
 *  state if applicable. it will wake up main_thread to handle
 *  the interrupt event as well.
 *
 *  @param dev     A pointer to net_device structure
 *  @return 	   n/a
 */
void libertas_interrupt(struct net_device *dev)
{
	wlan_private *priv = dev->priv;

	lbs_deb_enter(LBS_DEB_THREAD);

	lbs_deb_thread("libertas_interrupt: intcounter=%d\n",
	       priv->adapter->intcounter);

	priv->adapter->intcounter++;

	if (priv->adapter->psstate == PS_STATE_SLEEP) {
		priv->adapter->psstate = PS_STATE_AWAKE;
		netif_wake_queue(dev);
		netif_wake_queue(priv->mesh_dev);
	}

	wake_up_interruptible(&priv->waitq);

	lbs_deb_leave(LBS_DEB_THREAD);
}
EXPORT_SYMBOL_GPL(libertas_interrupt);

int libertas_reset_device(wlan_private *priv)
{
	int ret;

	lbs_deb_enter(LBS_DEB_MAIN);
	ret = libertas_prepare_and_send_command(priv, CMD_802_11_RESET,
				    CMD_ACT_HALT, 0, 0, NULL);
	msleep_interruptible(10);

	lbs_deb_leave_args(LBS_DEB_MAIN, "ret %d", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(libertas_reset_device);

static int libertas_init_module(void)
{
	lbs_deb_enter(LBS_DEB_MAIN);
	libertas_debugfs_init();
	lbs_deb_leave(LBS_DEB_MAIN);
	return 0;
}

static void libertas_exit_module(void)
{
	lbs_deb_enter(LBS_DEB_MAIN);

	libertas_debugfs_remove();

	lbs_deb_leave(LBS_DEB_MAIN);
}

module_init(libertas_init_module);
module_exit(libertas_exit_module);

MODULE_DESCRIPTION("Libertas WLAN Driver Library");
MODULE_AUTHOR("Marvell International Ltd.");
MODULE_LICENSE("GPL");
