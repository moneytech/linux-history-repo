/**
  * This file contains the handling of command
  * responses as well as events generated by firmware.
  */
#include <linux/delay.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>

#include <net/iw_handler.h>

#include "host.h"
#include "decl.h"
#include "defs.h"
#include "dev.h"
#include "join.h"
#include "wext.h"

/**
 *  @brief This function handles disconnect event. it
 *  reports disconnect to upper layer, clean tx/rx packets,
 *  reset link state etc.
 *
 *  @param priv    A pointer to wlan_private structure
 *  @return 	   n/a
 */
void libertas_mac_event_disconnected(wlan_private * priv)
{
	wlan_adapter *adapter = priv->adapter;
	union iwreq_data wrqu;

	if (adapter->connect_status != libertas_connected)
		return;

	lbs_deb_cmd("Handles disconnect event.\n");

	memset(wrqu.ap_addr.sa_data, 0x00, ETH_ALEN);
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;

	/*
	 * Cisco AP sends EAP failure and de-auth in less than 0.5 ms.
	 * It causes problem in the Supplicant
	 */

	msleep_interruptible(1000);
	wireless_send_event(priv->dev, SIOCGIWAP, &wrqu, NULL);

	/* Free Tx and Rx packets */
	kfree_skb(priv->adapter->currenttxskb);
	priv->adapter->currenttxskb = NULL;

	/* report disconnect to upper layer */
	netif_stop_queue(priv->dev);
	netif_carrier_off(priv->dev);

	/* reset SNR/NF/RSSI values */
	memset(adapter->SNR, 0x00, sizeof(adapter->SNR));
	memset(adapter->NF, 0x00, sizeof(adapter->NF));
	memset(adapter->RSSI, 0x00, sizeof(adapter->RSSI));
	memset(adapter->rawSNR, 0x00, sizeof(adapter->rawSNR));
	memset(adapter->rawNF, 0x00, sizeof(adapter->rawNF));
	adapter->nextSNRNF = 0;
	adapter->numSNRNF = 0;
	adapter->rxpd_rate = 0;
	lbs_deb_cmd("Current SSID=%s, ssid length=%u\n",
	       adapter->curbssparams.ssid.ssid,
	       adapter->curbssparams.ssid.ssidlength);
	lbs_deb_cmd("Previous SSID=%s, ssid length=%u\n",
	       adapter->previousssid.ssid, adapter->previousssid.ssidlength);

	adapter->connect_status = libertas_disconnected;

	/* Save previous SSID and BSSID for possible reassociation */
	memcpy(&adapter->previousssid,
	       &adapter->curbssparams.ssid, sizeof(struct WLAN_802_11_SSID));
	memcpy(adapter->previousbssid,
	       adapter->curbssparams.bssid, ETH_ALEN);

	/* Clear out associated SSID and BSSID since connection is
	 * no longer valid.
	 */
	memset(&adapter->curbssparams.bssid, 0, ETH_ALEN);
	memset(&adapter->curbssparams.ssid, 0, sizeof(struct WLAN_802_11_SSID));

	if (adapter->psstate != PS_STATE_FULL_POWER) {
		/* make firmware to exit PS mode */
		lbs_deb_cmd("Disconnected, so exit PS mode.\n");
		libertas_ps_wakeup(priv, 0);
	}
}

/**
 *  @brief This function handles MIC failure event.
 *
 *  @param priv    A pointer to wlan_private structure
 *  @para  event   the event id
 *  @return 	   n/a
 */
static void handle_mic_failureevent(wlan_private * priv, u32 event)
{
	char buf[50];

	memset(buf, 0, sizeof(buf));

	sprintf(buf, "%s", "MLME-MICHAELMICFAILURE.indication ");

	if (event == MACREG_INT_CODE_MIC_ERR_UNICAST) {
		strcat(buf, "unicast ");
	} else {
		strcat(buf, "multicast ");
	}

	libertas_send_iwevcustom_event(priv, buf);
}

static int wlan_ret_reg_access(wlan_private * priv,
			       u16 type, struct cmd_ds_command *resp)
{
	int ret = 0;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_CMD);

	switch (type) {
	case cmd_ret_mac_reg_access:
		{
			struct cmd_ds_mac_reg_access *reg;

			reg =
			    (struct cmd_ds_mac_reg_access *)&resp->params.
			    macreg;

			adapter->offsetvalue.offset = reg->offset;
			adapter->offsetvalue.value = reg->value;
			break;
		}

	case cmd_ret_bbp_reg_access:
		{
			struct cmd_ds_bbp_reg_access *reg;
			reg =
			    (struct cmd_ds_bbp_reg_access *)&resp->params.
			    bbpreg;

			adapter->offsetvalue.offset = reg->offset;
			adapter->offsetvalue.value = reg->value;
			break;
		}

	case cmd_ret_rf_reg_access:
		{
			struct cmd_ds_rf_reg_access *reg;
			reg =
			    (struct cmd_ds_rf_reg_access *)&resp->params.
			    rfreg;

			adapter->offsetvalue.offset = reg->offset;
			adapter->offsetvalue.value = reg->value;
			break;
		}

	default:
		ret = -1;
	}

	lbs_deb_enter_args(LBS_DEB_CMD, "ret %d", ret);
	return ret;
}

static int wlan_ret_get_hw_spec(wlan_private * priv,
				struct cmd_ds_command *resp)
{
	u32 i;
	struct cmd_ds_get_hw_spec *hwspec = &resp->params.hwspec;
	wlan_adapter *adapter = priv->adapter;
	int ret = 0;

	lbs_deb_enter(LBS_DEB_CMD);

	adapter->fwcapinfo = le32_to_cpu(hwspec->fwcapinfo);

	adapter->fwreleasenumber = hwspec->fwreleasenumber;

	lbs_deb_cmd("GET_HW_SPEC: FWReleaseVersion- 0x%X\n",
	       adapter->fwreleasenumber);
	lbs_deb_cmd("GET_HW_SPEC: Permanent addr- %2x:%2x:%2x:%2x:%2x:%2x\n",
	       hwspec->permanentaddr[0], hwspec->permanentaddr[1],
	       hwspec->permanentaddr[2], hwspec->permanentaddr[3],
	       hwspec->permanentaddr[4], hwspec->permanentaddr[5]);
	lbs_deb_cmd("GET_HW_SPEC: hwifversion=0x%X  version=0x%X\n",
	       hwspec->hwifversion, hwspec->version);

	adapter->regioncode = le16_to_cpu(hwspec->regioncode);

	for (i = 0; i < MRVDRV_MAX_REGION_CODE; i++) {
		/* use the region code to search for the index */
		if (adapter->regioncode == libertas_region_code_to_index[i]) {
			adapter->regiontableindex = (u16) i;
			break;
		}
	}

	/* if it's unidentified region code, use the default (USA) */
	if (i >= MRVDRV_MAX_REGION_CODE) {
		adapter->regioncode = 0x10;
		adapter->regiontableindex = 0;
		lbs_pr_info(
		       "unidentified region code, use the default (USA)\n");
	}

	if (adapter->current_addr[0] == 0xff) {
		memmove(adapter->current_addr, hwspec->permanentaddr,
			ETH_ALEN);
	}

	memcpy(priv->dev->dev_addr, adapter->current_addr, ETH_ALEN);
	if (priv->mesh_dev)
		memcpy(priv->mesh_dev->dev_addr, adapter->current_addr,
		       ETH_ALEN);

	if (libertas_set_regiontable(priv, adapter->regioncode, 0)) {
		ret = -1;
		goto done;
	}

	if (libertas_set_universaltable(priv, 0)) {
		ret = -1;
		goto done;
	}

done:
	lbs_deb_enter_args(LBS_DEB_CMD, "ret %d", ret);
	return ret;
}

static int wlan_ret_802_11_sleep_params(wlan_private * priv,
					struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_sleep_params *sp = &resp->params.sleep_params;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_CMD);

	lbs_deb_cmd("error=%x offset=%x stabletime=%x calcontrol=%x\n"
	       " extsleepclk=%x\n", sp->error, sp->offset,
	       sp->stabletime, sp->calcontrol, sp->externalsleepclk);
	adapter->sp.sp_error = le16_to_cpu(sp->error);
	adapter->sp.sp_offset = le16_to_cpu(sp->offset);
	adapter->sp.sp_stabletime = le16_to_cpu(sp->stabletime);
	adapter->sp.sp_calcontrol = le16_to_cpu(sp->calcontrol);
	adapter->sp.sp_extsleepclk = le16_to_cpu(sp->externalsleepclk);
	adapter->sp.sp_reserved = le16_to_cpu(sp->reserved);

	lbs_deb_enter(LBS_DEB_CMD);
	return 0;
}

static int wlan_ret_802_11_stat(wlan_private * priv,
				struct cmd_ds_command *resp)
{
/*	currently adapter->wlan802_11Stat is unused

	struct cmd_ds_802_11_get_stat *p11Stat = &resp->params.gstat;
	wlan_adapter *adapter = priv->adapter;

	// TODO Convert it to Big endian befor copy
	memcpy(&adapter->wlan802_11Stat,
	       p11Stat, sizeof(struct cmd_ds_802_11_get_stat));
*/
	return 0;
}

static int wlan_ret_802_11_snmp_mib(wlan_private * priv,
				    struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_snmp_mib *smib = &resp->params.smib;
	u16 oid = le16_to_cpu(smib->oid);
	u16 querytype = le16_to_cpu(smib->querytype);

	lbs_deb_enter(LBS_DEB_CMD);

	lbs_deb_cmd("SNMP_RESP: value of the oid = %x, querytype=%x\n", oid,
	       querytype);
	lbs_deb_cmd("SNMP_RESP: Buf size  = %x\n",
	       le16_to_cpu(smib->bufsize));

	if (querytype == cmd_act_get) {
		switch (oid) {
		case fragthresh_i:
			priv->adapter->fragthsd =
			    le16_to_cpu(*
					     ((unsigned short *)(smib->value)));
			lbs_deb_cmd("SNMP_RESP: fragthsd =%u\n",
			       priv->adapter->fragthsd);
			break;
		case rtsthresh_i:
			priv->adapter->rtsthsd =
			    le16_to_cpu(*
					     ((unsigned short *)(smib->value)));
			lbs_deb_cmd("SNMP_RESP: rtsthsd =%u\n",
			       priv->adapter->rtsthsd);
			break;
		case short_retrylim_i:
			priv->adapter->txretrycount =
			    le16_to_cpu(*
					     ((unsigned short *)(smib->value)));
			lbs_deb_cmd("SNMP_RESP: txretrycount =%u\n",
			       priv->adapter->rtsthsd);
			break;
		default:
			break;
		}
	}

	lbs_deb_enter(LBS_DEB_CMD);
	return 0;
}

static int wlan_ret_802_11_key_material(wlan_private * priv,
					struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_key_material *pkeymaterial =
	    &resp->params.keymaterial;
	wlan_adapter *adapter = priv->adapter;
	u16 action = le16_to_cpu(pkeymaterial->action);

	lbs_deb_enter(LBS_DEB_CMD);

	/* Copy the returned key to driver private data */
	if (action == cmd_act_get) {
		u8 * buf_ptr = (u8 *) &pkeymaterial->keyParamSet;
		u8 * resp_end = (u8 *) (resp + le16_to_cpu(resp->size));

		while (buf_ptr < resp_end) {
			struct MrvlIEtype_keyParamSet * pkeyparamset =
			    (struct MrvlIEtype_keyParamSet *) buf_ptr;
			struct WLAN_802_11_KEY * pkey;
			u16 key_info = le16_to_cpu(pkeyparamset->keyinfo);
			u16 param_set_len = le16_to_cpu(pkeyparamset->length);
			u8 * end;
			u16 key_len = le16_to_cpu(pkeyparamset->keylen);

			end = (u8 *) pkeyparamset + sizeof (pkeyparamset->type)
			                          + sizeof (pkeyparamset->length)
			                          + param_set_len;
			/* Make sure we don't access past the end of the IEs */
			if (end > resp_end)
				break;

			if (key_info & KEY_INFO_WPA_UNICAST)
				pkey = &adapter->wpa_unicast_key;
			else if (key_info & KEY_INFO_WPA_MCAST)
				pkey = &adapter->wpa_mcast_key;
			else
				break;

			/* Copy returned key into driver */
			memset(pkey, 0, sizeof(struct WLAN_802_11_KEY));
			if (key_len > sizeof(pkey->key))
				break;
			pkey->type = le16_to_cpu(pkeyparamset->keytypeid);
			pkey->flags = le16_to_cpu(pkeyparamset->keyinfo);
			pkey->len = le16_to_cpu(pkeyparamset->keylen);
			memcpy(pkey->key, pkeyparamset->key, pkey->len);

			buf_ptr = end + 1;
		}
	}

	lbs_deb_enter(LBS_DEB_CMD);
	return 0;
}

static int wlan_ret_802_11_mac_address(wlan_private * priv,
				       struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_mac_address *macadd = &resp->params.macadd;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_CMD);

	memcpy(adapter->current_addr, macadd->macadd, ETH_ALEN);

	lbs_deb_enter(LBS_DEB_CMD);
	return 0;
}

static int wlan_ret_802_11_rf_tx_power(wlan_private * priv,
				       struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_rf_tx_power *rtp = &resp->params.txp;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_CMD);

	adapter->txpowerlevel = le16_to_cpu(rtp->currentlevel);

	lbs_deb_cmd("Current TxPower Level = %d\n", adapter->txpowerlevel);

	lbs_deb_enter(LBS_DEB_CMD);
	return 0;
}

static int wlan_ret_802_11_rf_antenna(wlan_private * priv,
				      struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_rf_antenna *pAntenna = &resp->params.rant;
	wlan_adapter *adapter = priv->adapter;
	u16 action = le16_to_cpu(pAntenna->action);

	if (action == cmd_act_get_rx)
		adapter->rxantennamode =
		    le16_to_cpu(pAntenna->antennamode);

	if (action == cmd_act_get_tx)
		adapter->txantennamode =
		    le16_to_cpu(pAntenna->antennamode);

	lbs_deb_cmd("RF_ANT_RESP: action = 0x%x, mode = 0x%04x\n",
	       action, le16_to_cpu(pAntenna->antennamode));

	return 0;
}

static int wlan_ret_802_11_rate_adapt_rateset(wlan_private * priv,
					      struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_rate_adapt_rateset *rates =
	    &resp->params.rateset;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_CMD);

	if (rates->action == cmd_act_get) {
		adapter->enablehwauto = rates->enablehwauto;
		adapter->ratebitmap = rates->bitmap;
	}

	lbs_deb_enter(LBS_DEB_CMD);
	return 0;
}

static int wlan_ret_802_11_data_rate(wlan_private * priv,
				     struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_data_rate *pdatarate = &resp->params.drate;
	wlan_adapter *adapter = priv->adapter;
	u8 dot11datarate;

	lbs_deb_enter(LBS_DEB_CMD);

	lbs_dbg_hex("DATA_RATE_RESP: data_rate- ",
		(u8 *) pdatarate, sizeof(struct cmd_ds_802_11_data_rate));

	dot11datarate = pdatarate->datarate[0];
	if (pdatarate->action == cmd_act_get_tx_rate) {
		memcpy(adapter->libertas_supported_rates, pdatarate->datarate,
		       sizeof(adapter->libertas_supported_rates));
	}
	adapter->datarate = libertas_index_to_data_rate(dot11datarate);

	lbs_deb_enter(LBS_DEB_CMD);
	return 0;
}

static int wlan_ret_802_11_rf_channel(wlan_private * priv,
				      struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_rf_channel *rfchannel =
	    &resp->params.rfchannel;
	wlan_adapter *adapter = priv->adapter;
	u16 action = le16_to_cpu(rfchannel->action);
	u16 newchannel = le16_to_cpu(rfchannel->currentchannel);

	lbs_deb_enter(LBS_DEB_CMD);

	if (action == cmd_opt_802_11_rf_channel_get
	    && adapter->curbssparams.channel != newchannel) {
		lbs_deb_cmd("channel Switch: %d to %d\n",
		       adapter->curbssparams.channel, newchannel);

		/* Update the channel again */
		adapter->curbssparams.channel = newchannel;
	}

	lbs_deb_enter(LBS_DEB_CMD);
	return 0;
}

static int wlan_ret_802_11_rssi(wlan_private * priv,
				struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_rssi_rsp *rssirsp = &resp->params.rssirsp;
	wlan_adapter *adapter = priv->adapter;

	/* store the non average value */
	adapter->SNR[TYPE_BEACON][TYPE_NOAVG] = le16_to_cpu(rssirsp->SNR);
	adapter->NF[TYPE_BEACON][TYPE_NOAVG] =
	    le16_to_cpu(rssirsp->noisefloor);

	adapter->SNR[TYPE_BEACON][TYPE_AVG] = le16_to_cpu(rssirsp->avgSNR);
	adapter->NF[TYPE_BEACON][TYPE_AVG] =
	    le16_to_cpu(rssirsp->avgnoisefloor);

	adapter->RSSI[TYPE_BEACON][TYPE_NOAVG] =
	    CAL_RSSI(adapter->SNR[TYPE_BEACON][TYPE_NOAVG],
		     adapter->NF[TYPE_BEACON][TYPE_NOAVG]);

	adapter->RSSI[TYPE_BEACON][TYPE_AVG] =
	    CAL_RSSI(adapter->SNR[TYPE_BEACON][TYPE_AVG] / AVG_SCALE,
		     adapter->NF[TYPE_BEACON][TYPE_AVG] / AVG_SCALE);

	lbs_deb_cmd("Beacon RSSI value = 0x%x\n",
	       adapter->RSSI[TYPE_BEACON][TYPE_AVG]);

	return 0;
}

static int wlan_ret_802_11_eeprom_access(wlan_private * priv,
				  struct cmd_ds_command *resp)
{
	wlan_adapter *adapter = priv->adapter;
	struct wlan_ioctl_regrdwr *pbuf;
	pbuf = (struct wlan_ioctl_regrdwr *) adapter->prdeeprom;

	lbs_deb_cmd("eeprom read len=%x\n",
	       le16_to_cpu(resp->params.rdeeprom.bytecount));
	if (pbuf->NOB < le16_to_cpu(resp->params.rdeeprom.bytecount)) {
		pbuf->NOB = 0;
		lbs_deb_cmd("eeprom read return length is too big\n");
		return -1;
	}
	pbuf->NOB = le16_to_cpu(resp->params.rdeeprom.bytecount);
	if (pbuf->NOB > 0) {

		memcpy(&pbuf->value, (u8 *) & resp->params.rdeeprom.value,
		       le16_to_cpu(resp->params.rdeeprom.bytecount));
		lbs_dbg_hex("adapter", (char *)&pbuf->value,
			le16_to_cpu(resp->params.rdeeprom.bytecount));
	}
	return 0;
}

static int wlan_ret_get_log(wlan_private * priv,
			    struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_get_log *logmessage =
	    (struct cmd_ds_802_11_get_log *)&resp->params.glog;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_CMD);

	/* TODO Convert it to Big Endian before copy */
	memcpy(&adapter->logmsg, logmessage,
	       sizeof(struct cmd_ds_802_11_get_log));

	lbs_deb_enter(LBS_DEB_CMD);
	return 0;
}

static inline int handle_cmd_response(u16 respcmd,
				      struct cmd_ds_command *resp,
				      wlan_private *priv)
{
	int ret = 0;
	unsigned long flags;
	wlan_adapter *adapter = priv->adapter;

	switch (respcmd) {
	case cmd_ret_mac_reg_access:
	case cmd_ret_bbp_reg_access:
	case cmd_ret_rf_reg_access:
		ret = wlan_ret_reg_access(priv, respcmd, resp);
		break;

	case cmd_ret_hw_spec_info:
		ret = wlan_ret_get_hw_spec(priv, resp);
		break;

	case cmd_ret_802_11_scan:
		ret = libertas_ret_80211_scan(priv, resp);
		break;

	case cmd_ret_802_11_get_log:
		ret = wlan_ret_get_log(priv, resp);
		break;

	case cmd_ret_802_11_associate:
	case cmd_ret_802_11_reassociate:
		ret = libertas_ret_80211_associate(priv, resp);
		break;

	case cmd_ret_802_11_disassociate:
	case cmd_ret_802_11_deauthenticate:
		ret = libertas_ret_80211_disassociate(priv, resp);
		break;

	case cmd_ret_802_11_ad_hoc_start:
	case cmd_ret_802_11_ad_hoc_join:
		ret = libertas_ret_80211_ad_hoc_start(priv, resp);
		break;

	case cmd_ret_802_11_stat:
		ret = wlan_ret_802_11_stat(priv, resp);
		break;

	case cmd_ret_802_11_snmp_mib:
		ret = wlan_ret_802_11_snmp_mib(priv, resp);
		break;

	case cmd_ret_802_11_rf_tx_power:
		ret = wlan_ret_802_11_rf_tx_power(priv, resp);
		break;

	case cmd_ret_802_11_set_afc:
	case cmd_ret_802_11_get_afc:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		memmove(adapter->cur_cmd->pdata_buf,
			&resp->params.afc,
			sizeof(struct cmd_ds_802_11_afc));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);

		break;
	case cmd_ret_802_11_rf_antenna:
		ret = wlan_ret_802_11_rf_antenna(priv, resp);
		break;

	case cmd_ret_mac_multicast_adr:
	case cmd_ret_mac_control:
	case cmd_ret_802_11_set_wep:
	case cmd_ret_802_11_reset:
	case cmd_ret_802_11_authenticate:
	case cmd_ret_802_11_radio_control:
	case cmd_ret_802_11_beacon_stop:
	case cmd_ret_802_11_enable_rsn:
		break;

	case cmd_ret_802_11_data_rate:
		ret = wlan_ret_802_11_data_rate(priv, resp);
		break;
	case cmd_ret_802_11_rate_adapt_rateset:
		ret = wlan_ret_802_11_rate_adapt_rateset(priv, resp);
		break;
	case cmd_ret_802_11_rf_channel:
		ret = wlan_ret_802_11_rf_channel(priv, resp);
		break;

	case cmd_ret_802_11_rssi:
		ret = wlan_ret_802_11_rssi(priv, resp);
		break;

	case cmd_ret_802_11_mac_address:
		ret = wlan_ret_802_11_mac_address(priv, resp);
		break;

	case cmd_ret_802_11_ad_hoc_stop:
		ret = libertas_ret_80211_ad_hoc_stop(priv, resp);
		break;

	case cmd_ret_802_11_key_material:
		lbs_deb_cmd("CMD_RESP: KEY_MATERIAL command response\n");
		ret = wlan_ret_802_11_key_material(priv, resp);
		break;

	case cmd_ret_802_11_eeprom_access:
		ret = wlan_ret_802_11_eeprom_access(priv, resp);
		break;

	case cmd_ret_802_11d_domain_info:
		ret = libertas_ret_802_11d_domain_info(priv, resp);
		break;

	case cmd_ret_802_11_sleep_params:
		ret = wlan_ret_802_11_sleep_params(priv, resp);
		break;
	case cmd_ret_802_11_inactivity_timeout:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		*((u16 *) adapter->cur_cmd->pdata_buf) =
		    le16_to_cpu(resp->params.inactivity_timeout.timeout);
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;

	case cmd_ret_802_11_tpc_cfg:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		memmove(adapter->cur_cmd->pdata_buf,
			&resp->params.tpccfg,
			sizeof(struct cmd_ds_802_11_tpc_cfg));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;
	case cmd_ret_802_11_led_gpio_ctrl:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		memmove(adapter->cur_cmd->pdata_buf,
			&resp->params.ledgpio,
			sizeof(struct cmd_ds_802_11_led_ctrl));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;
	case cmd_ret_802_11_pwr_cfg:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		memmove(adapter->cur_cmd->pdata_buf,
			&resp->params.pwrcfg,
			sizeof(struct cmd_ds_802_11_pwr_cfg));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);

		break;

	case cmd_ret_get_tsf:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		memcpy(priv->adapter->cur_cmd->pdata_buf,
		       &resp->params.gettsf.tsfvalue, sizeof(u64));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;
	case cmd_ret_bt_access:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		if (adapter->cur_cmd->pdata_buf)
			memcpy(adapter->cur_cmd->pdata_buf,
			       &resp->params.bt.addr1, 2 * ETH_ALEN);
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;
	case cmd_ret_fwt_access:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		if (adapter->cur_cmd->pdata_buf)
			memcpy(adapter->cur_cmd->pdata_buf,
			       &resp->params.fwt,
				sizeof(resp->params.fwt));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;
	case cmd_ret_mesh_access:
		if (adapter->cur_cmd->pdata_buf)
			memcpy(adapter->cur_cmd->pdata_buf,
			       &resp->params.mesh,
			       sizeof(resp->params.mesh));
		break;
	case cmd_rte_802_11_tx_rate_query:
		priv->adapter->txrate = resp->params.txrate.txrate;
		break;
	default:
		lbs_deb_cmd("CMD_RESP: Unknown command response %#x\n",
		       resp->command);
		break;
	}
	return ret;
}

int libertas_process_rx_command(wlan_private * priv)
{
	u16 respcmd;
	struct cmd_ds_command *resp;
	wlan_adapter *adapter = priv->adapter;
	int ret = 0;
	ulong flags;
	u16 result;

	lbs_deb_enter(LBS_DEB_CMD);

	lbs_deb_cmd("CMD_RESP: @ %lu\n", jiffies);

	/* Now we got response from FW, cancel the command timer */
	del_timer(&adapter->command_timer);

	mutex_lock(&adapter->lock);
	spin_lock_irqsave(&adapter->driver_lock, flags);

	if (!adapter->cur_cmd) {
		lbs_deb_cmd("CMD_RESP: NULL cur_cmd=%p\n", adapter->cur_cmd);
		ret = -1;
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		goto done;
	}
	resp = (struct cmd_ds_command *)(adapter->cur_cmd->bufvirtualaddr);

	lbs_dbg_hex("CMD_RESP:", adapter->cur_cmd->bufvirtualaddr,
		priv->upld_len);

	respcmd = le16_to_cpu(resp->command);

	result = le16_to_cpu(resp->result);

	lbs_deb_cmd("CMD_RESP: %x result: %d length: %d\n", respcmd,
	       result, priv->upld_len);

	if (!(respcmd & 0x8000)) {
		lbs_deb_cmd("Invalid response to command!");
		adapter->cur_cmd_retcode = -1;
		__libertas_cleanup_and_insert_cmd(priv, adapter->cur_cmd);
		adapter->nr_cmd_pending--;
		adapter->cur_cmd = NULL;
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		ret = -1;
		goto done;
	}

	/* Store the response code to cur_cmd_retcode. */
	adapter->cur_cmd_retcode = le16_to_cpu(resp->result);

	if (respcmd == cmd_ret_802_11_ps_mode) {
		struct cmd_ds_802_11_ps_mode *psmode;

		psmode = &resp->params.psmode;
		lbs_deb_cmd(
		       "CMD_RESP: PS_MODE cmd reply result=%#x action=0x%X\n",
		       resp->result, psmode->action);
		psmode->action = cpu_to_le16(psmode->action);

		if (result) {
			lbs_deb_cmd("CMD_RESP: PS command failed- %#x \n",
			       resp->result);
			if (adapter->mode == IW_MODE_ADHOC) {
				/*
				 * We should not re-try enter-ps command in
				 * ad-hoc mode. It takes place in
				 * libertas_execute_next_command().
				 */
				if (psmode->action == cmd_subcmd_enter_ps)
					adapter->psmode =
					    wlan802_11powermodecam;
			}
		} else if (psmode->action == cmd_subcmd_enter_ps) {
			adapter->needtowakeup = 0;
			adapter->psstate = PS_STATE_AWAKE;

			lbs_deb_cmd("CMD_RESP: Enter_PS command response\n");
			if (adapter->connect_status != libertas_connected) {
				/*
				 * When Deauth Event received before Enter_PS command
				 * response, We need to wake up the firmware.
				 */
				lbs_deb_cmd(
				       "Disconnected, Going to invoke libertas_ps_wakeup\n");

				spin_unlock_irqrestore(&adapter->driver_lock, flags);
				mutex_unlock(&adapter->lock);
				libertas_ps_wakeup(priv, 0);
				mutex_lock(&adapter->lock);
				spin_lock_irqsave(&adapter->driver_lock, flags);
			}
		} else if (psmode->action == cmd_subcmd_exit_ps) {
			adapter->needtowakeup = 0;
			adapter->psstate = PS_STATE_FULL_POWER;
			lbs_deb_cmd("CMD_RESP: Exit_PS command response\n");
		} else {
			lbs_deb_cmd("CMD_RESP: PS- action=0x%X\n",
			       psmode->action);
		}

		__libertas_cleanup_and_insert_cmd(priv, adapter->cur_cmd);
		adapter->nr_cmd_pending--;
		adapter->cur_cmd = NULL;
		spin_unlock_irqrestore(&adapter->driver_lock, flags);

		ret = 0;
		goto done;
	}

	if (adapter->cur_cmd->cmdflags & CMD_F_HOSTCMD) {
		/* Copy the response back to response buffer */
		memcpy(adapter->cur_cmd->pdata_buf, resp, resp->size);

		adapter->cur_cmd->cmdflags &= ~CMD_F_HOSTCMD;
	}

	/* If the command is not successful, cleanup and return failure */
	if ((result != 0 || !(respcmd & 0x8000))) {
		lbs_deb_cmd("CMD_RESP: command reply %#x result=%#x\n",
		       resp->command, resp->result);
		/*
		 * Handling errors here
		 */
		switch (respcmd) {
		case cmd_ret_hw_spec_info:
		case cmd_ret_802_11_reset:
			lbs_deb_cmd("CMD_RESP: Reset command failed\n");
			break;

		}

		__libertas_cleanup_and_insert_cmd(priv, adapter->cur_cmd);
		adapter->nr_cmd_pending--;
		adapter->cur_cmd = NULL;
		spin_unlock_irqrestore(&adapter->driver_lock, flags);

		ret = -1;
		goto done;
	}

	spin_unlock_irqrestore(&adapter->driver_lock, flags);

	ret = handle_cmd_response(respcmd, resp, priv);

	spin_lock_irqsave(&adapter->driver_lock, flags);
	if (adapter->cur_cmd) {
		/* Clean up and Put current command back to cmdfreeq */
		__libertas_cleanup_and_insert_cmd(priv, adapter->cur_cmd);
		adapter->nr_cmd_pending--;
		WARN_ON(adapter->nr_cmd_pending > 128);
		adapter->cur_cmd = NULL;
	}
	spin_unlock_irqrestore(&adapter->driver_lock, flags);

done:
	mutex_unlock(&adapter->lock);
	lbs_deb_enter_args(LBS_DEB_CMD, "ret %d", ret);
	return ret;
}

int libertas_process_event(wlan_private * priv)
{
	int ret = 0;
	wlan_adapter *adapter = priv->adapter;
	u32 eventcause;

	spin_lock_irq(&adapter->driver_lock);
	eventcause = adapter->eventcause;
	spin_unlock_irq(&adapter->driver_lock);

	lbs_deb_enter(LBS_DEB_CMD);

	lbs_deb_cmd("EVENT Cause %x\n", eventcause);

	switch (eventcause >> SBI_EVENT_CAUSE_SHIFT) {
	case MACREG_INT_CODE_LINK_SENSED:
		lbs_deb_cmd("EVENT: MACREG_INT_CODE_LINK_SENSED\n");
		break;

	case MACREG_INT_CODE_DEAUTHENTICATED:
		lbs_deb_cmd("EVENT: Deauthenticated\n");
		libertas_mac_event_disconnected(priv);
		break;

	case MACREG_INT_CODE_DISASSOCIATED:
		lbs_deb_cmd("EVENT: Disassociated\n");
		libertas_mac_event_disconnected(priv);
		break;

	case MACREG_INT_CODE_LINK_LOSE_NO_SCAN:
		lbs_deb_cmd("EVENT: Link lost\n");
		libertas_mac_event_disconnected(priv);
		break;

	case MACREG_INT_CODE_PS_SLEEP:
		lbs_deb_cmd("EVENT: SLEEP\n");
		lbs_deb_cmd("_");

		/* handle unexpected PS SLEEP event */
		if (adapter->psstate == PS_STATE_FULL_POWER) {
			lbs_deb_cmd(
			       "EVENT: In FULL POWER mode - ignore PS SLEEP\n");
			break;
		}
		adapter->psstate = PS_STATE_PRE_SLEEP;

		libertas_ps_confirm_sleep(priv, (u16) adapter->psmode);

		break;

	case MACREG_INT_CODE_PS_AWAKE:
		lbs_deb_cmd("EVENT: AWAKE \n");
		lbs_deb_cmd("|");

		/* handle unexpected PS AWAKE event */
		if (adapter->psstate == PS_STATE_FULL_POWER) {
			lbs_deb_cmd(
			       "EVENT: In FULL POWER mode - ignore PS AWAKE\n");
			break;
		}

		adapter->psstate = PS_STATE_AWAKE;

		if (adapter->needtowakeup) {
			/*
			 * wait for the command processing to finish
			 * before resuming sending
			 * adapter->needtowakeup will be set to FALSE
			 * in libertas_ps_wakeup()
			 */
			lbs_deb_cmd("Waking up...\n");
			libertas_ps_wakeup(priv, 0);
		}
		break;

	case MACREG_INT_CODE_MIC_ERR_UNICAST:
		lbs_deb_cmd("EVENT: UNICAST MIC ERROR\n");
		handle_mic_failureevent(priv, MACREG_INT_CODE_MIC_ERR_UNICAST);
		break;

	case MACREG_INT_CODE_MIC_ERR_MULTICAST:
		lbs_deb_cmd("EVENT: MULTICAST MIC ERROR\n");
		handle_mic_failureevent(priv, MACREG_INT_CODE_MIC_ERR_MULTICAST);
		break;
	case MACREG_INT_CODE_MIB_CHANGED:
	case MACREG_INT_CODE_INIT_DONE:
		break;

	case MACREG_INT_CODE_ADHOC_BCN_LOST:
		lbs_deb_cmd("EVENT: HWAC - ADHOC BCN LOST\n");
		break;

	case MACREG_INT_CODE_RSSI_LOW:
		lbs_pr_alert( "EVENT: RSSI_LOW\n");
		break;
	case MACREG_INT_CODE_SNR_LOW:
		lbs_pr_alert( "EVENT: SNR_LOW\n");
		break;
	case MACREG_INT_CODE_MAX_FAIL:
		lbs_pr_alert( "EVENT: MAX_FAIL\n");
		break;
	case MACREG_INT_CODE_RSSI_HIGH:
		lbs_pr_alert( "EVENT: RSSI_HIGH\n");
		break;
	case MACREG_INT_CODE_SNR_HIGH:
		lbs_pr_alert( "EVENT: SNR_HIGH\n");
		break;

	case MACREG_INT_CODE_MESH_AUTO_STARTED:
		lbs_pr_alert( "EVENT: MESH_AUTO_STARTED\n");
		adapter->connect_status = libertas_connected ;
		if (priv->mesh_open == 1) {
			netif_wake_queue(priv->mesh_dev) ;
			netif_carrier_on(priv->mesh_dev) ;
		}
		adapter->mode = IW_MODE_ADHOC ;
		break;

	default:
		lbs_pr_alert( "EVENT: unknown event id: %#x\n",
		       eventcause >> SBI_EVENT_CAUSE_SHIFT);
		break;
	}

	spin_lock_irq(&adapter->driver_lock);
	adapter->eventcause = 0;
	spin_unlock_irq(&adapter->driver_lock);

	lbs_deb_enter_args(LBS_DEB_CMD, "ret %d", ret);
	return ret;
}
