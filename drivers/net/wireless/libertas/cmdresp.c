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

	if (adapter->connect_status != LIBERTAS_CONNECTED)
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
	lbs_deb_cmd("Current SSID='%s', ssid length=%u\n",
	            escape_essid(adapter->curbssparams.ssid,
	                         adapter->curbssparams.ssid_len),
	            adapter->curbssparams.ssid_len);
	lbs_deb_cmd("Previous SSID='%s', ssid length=%u\n",
	            escape_essid(adapter->prev_ssid, adapter->prev_ssid_len),
	            adapter->prev_ssid_len);

	adapter->connect_status = LIBERTAS_DISCONNECTED;

	/* Save previous SSID and BSSID for possible reassociation */
	memcpy(&adapter->prev_ssid, &adapter->curbssparams.ssid,
	       IW_ESSID_MAX_SIZE);
	adapter->prev_ssid_len = adapter->curbssparams.ssid_len;
	memcpy(adapter->prev_bssid, adapter->curbssparams.bssid, ETH_ALEN);

	/* Clear out associated SSID and BSSID since connection is
	 * no longer valid.
	 */
	memset(&adapter->curbssparams.bssid, 0, ETH_ALEN);
	memset(&adapter->curbssparams.ssid, 0, IW_ESSID_MAX_SIZE);
	adapter->curbssparams.ssid_len = 0;

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
	case CMD_RET_MAC_REG_ACCESS:
		{
			struct cmd_ds_mac_reg_access *reg = &resp->params.macreg;

			adapter->offsetvalue.offset = (u32)le16_to_cpu(reg->offset);
			adapter->offsetvalue.value = le32_to_cpu(reg->value);
			break;
		}

	case CMD_RET_BBP_REG_ACCESS:
		{
			struct cmd_ds_bbp_reg_access *reg = &resp->params.bbpreg;

			adapter->offsetvalue.offset = (u32)le16_to_cpu(reg->offset);
			adapter->offsetvalue.value = reg->value;
			break;
		}

	case CMD_RET_RF_REG_ACCESS:
		{
			struct cmd_ds_rf_reg_access *reg = &resp->params.rfreg;

			adapter->offsetvalue.offset = (u32)le16_to_cpu(reg->offset);
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

	memcpy(adapter->fwreleasenumber, hwspec->fwreleasenumber, 4);

	lbs_deb_cmd("GET_HW_SPEC: FWReleaseVersion: %u.%u.%u.p%u\n",
		    adapter->fwreleasenumber[2], adapter->fwreleasenumber[1],
		    adapter->fwreleasenumber[0], adapter->fwreleasenumber[3]);
	lbs_deb_cmd("GET_HW_SPEC: Permanent addr: " MAC_FMT "\n",
	       MAC_ARG(hwspec->permanentaddr));
	lbs_deb_cmd("GET_HW_SPEC: hwifversion: 0x%x version:0x%x\n",
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
		lbs_pr_info("unidentified region code; using the default (USA)\n");
	}

	if (adapter->current_addr[0] == 0xff)
		memmove(adapter->current_addr, hwspec->permanentaddr, ETH_ALEN);

	memcpy(priv->dev->dev_addr, adapter->current_addr, ETH_ALEN);
	if (priv->mesh_dev)
		memcpy(priv->mesh_dev->dev_addr, adapter->current_addr, ETH_ALEN);

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
		    " extsleepclk=%x\n", le16_to_cpu(sp->error),
		    le16_to_cpu(sp->offset), le16_to_cpu(sp->stabletime),
		    sp->calcontrol, sp->externalsleepclk);

	adapter->sp.sp_error = le16_to_cpu(sp->error);
	adapter->sp.sp_offset = le16_to_cpu(sp->offset);
	adapter->sp.sp_stabletime = le16_to_cpu(sp->stabletime);
	adapter->sp.sp_calcontrol = sp->calcontrol;
	adapter->sp.sp_extsleepclk = sp->externalsleepclk;
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
	lbs_deb_cmd("SNMP_RESP: Buf size  = %x\n", le16_to_cpu(smib->bufsize));

	if (querytype == CMD_ACT_GET) {
		switch (oid) {
		case FRAGTHRESH_I:
			priv->adapter->fragthsd =
				le16_to_cpu(*((__le16 *)(smib->value)));
			lbs_deb_cmd("SNMP_RESP: fragthsd =%u\n",
				    priv->adapter->fragthsd);
			break;
		case RTSTHRESH_I:
			priv->adapter->rtsthsd =
				le16_to_cpu(*((__le16 *)(smib->value)));
			lbs_deb_cmd("SNMP_RESP: rtsthsd =%u\n",
				    priv->adapter->rtsthsd);
			break;
		case SHORT_RETRYLIM_I:
			priv->adapter->txretrycount =
				le16_to_cpu(*((__le16 *)(smib->value)));
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
	if (action == CMD_ACT_GET) {
		u8 * buf_ptr = (u8 *) &pkeymaterial->keyParamSet;
		u8 * resp_end = (u8 *) (resp + le16_to_cpu(resp->size));

		while (buf_ptr < resp_end) {
			struct MrvlIEtype_keyParamSet * pkeyparamset =
			    (struct MrvlIEtype_keyParamSet *) buf_ptr;
			struct enc_key * pkey;
			u16 param_set_len = le16_to_cpu(pkeyparamset->length);
			u16 key_len = le16_to_cpu(pkeyparamset->keylen);
			u16 key_flags = le16_to_cpu(pkeyparamset->keyinfo);
			u16 key_type = le16_to_cpu(pkeyparamset->keytypeid);
			u8 * end;

			end = (u8 *) pkeyparamset + sizeof (pkeyparamset->type)
			                          + sizeof (pkeyparamset->length)
			                          + param_set_len;
			/* Make sure we don't access past the end of the IEs */
			if (end > resp_end)
				break;

			if (key_flags & KEY_INFO_WPA_UNICAST)
				pkey = &adapter->wpa_unicast_key;
			else if (key_flags & KEY_INFO_WPA_MCAST)
				pkey = &adapter->wpa_mcast_key;
			else
				break;

			/* Copy returned key into driver */
			memset(pkey, 0, sizeof(struct enc_key));
			if (key_len > sizeof(pkey->key))
				break;
			pkey->type = key_type;
			pkey->flags = key_flags;
			pkey->len = key_len;
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

	if (action == CMD_ACT_GET_RX)
		adapter->rxantennamode = le16_to_cpu(pAntenna->antennamode);

	if (action == CMD_ACT_GET_TX)
		adapter->txantennamode = le16_to_cpu(pAntenna->antennamode);

	lbs_deb_cmd("RF_ANT_RESP: action = 0x%x, mode = 0x%04x\n",
	       action, le16_to_cpu(pAntenna->antennamode));

	return 0;
}

static int wlan_ret_802_11_rate_adapt_rateset(wlan_private * priv,
					      struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_rate_adapt_rateset *rates = &resp->params.rateset;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_CMD);

	if (rates->action == CMD_ACT_GET) {
		adapter->enablehwauto = le16_to_cpu(rates->enablehwauto);
		adapter->ratebitmap = le16_to_cpu(rates->bitmap);
	}

	lbs_deb_enter(LBS_DEB_CMD);
	return 0;
}

static int wlan_ret_802_11_data_rate(wlan_private * priv,
				     struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_data_rate *pdatarate = &resp->params.drate;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_CMD);

	lbs_dbg_hex("DATA_RATE_RESP: data_rate- ", (u8 *) pdatarate,
		sizeof(struct cmd_ds_802_11_data_rate));

	/* FIXME: get actual rates FW can do if this command actually returns
	 * all data rates supported.
	 */
	adapter->cur_rate = libertas_fw_index_to_data_rate(pdatarate->rates[0]);

	lbs_deb_leave(LBS_DEB_CMD);
	return 0;
}

static int wlan_ret_802_11_rf_channel(wlan_private * priv,
				      struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_rf_channel *rfchannel = &resp->params.rfchannel;
	wlan_adapter *adapter = priv->adapter;
	u16 action = le16_to_cpu(rfchannel->action);
	u16 newchannel = le16_to_cpu(rfchannel->currentchannel);

	lbs_deb_enter(LBS_DEB_CMD);

	if (action == CMD_OPT_802_11_RF_CHANNEL_GET
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
	adapter->NF[TYPE_BEACON][TYPE_NOAVG] = le16_to_cpu(rssirsp->noisefloor);

	adapter->SNR[TYPE_BEACON][TYPE_AVG] = le16_to_cpu(rssirsp->avgSNR);
	adapter->NF[TYPE_BEACON][TYPE_AVG] = le16_to_cpu(rssirsp->avgnoisefloor);

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
	struct cmd_ds_802_11_get_log *logmessage = &resp->params.glog;
	wlan_adapter *adapter = priv->adapter;

	lbs_deb_enter(LBS_DEB_CMD);

	/* Stored little-endian */
	memcpy(&adapter->logmsg, logmessage, sizeof(struct cmd_ds_802_11_get_log));

	lbs_deb_enter(LBS_DEB_CMD);
	return 0;
}

static int libertas_ret_802_11_enable_rsn(wlan_private * priv,
                                          struct cmd_ds_command *resp)
{
	struct cmd_ds_802_11_enable_rsn *enable_rsn = &resp->params.enbrsn;
	wlan_adapter *adapter = priv->adapter;
	u32 * pdata_buf = adapter->cur_cmd->pdata_buf;

	lbs_deb_enter(LBS_DEB_CMD);

	if (enable_rsn->action == cpu_to_le16(CMD_ACT_GET)) {
		if (pdata_buf)
			*pdata_buf = (u32) le16_to_cpu(enable_rsn->enable);
	}

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
	case CMD_RET_MAC_REG_ACCESS:
	case CMD_RET_BBP_REG_ACCESS:
	case CMD_RET_RF_REG_ACCESS:
		ret = wlan_ret_reg_access(priv, respcmd, resp);
		break;

	case CMD_RET_HW_SPEC_INFO:
		ret = wlan_ret_get_hw_spec(priv, resp);
		break;

	case CMD_RET_802_11_SCAN:
		ret = libertas_ret_80211_scan(priv, resp);
		break;

	case CMD_RET_802_11_GET_LOG:
		ret = wlan_ret_get_log(priv, resp);
		break;

	case CMD_RET_802_11_ASSOCIATE:
	case CMD_RET_802_11_REASSOCIATE:
		ret = libertas_ret_80211_associate(priv, resp);
		break;

	case CMD_RET_802_11_DISASSOCIATE:
	case CMD_RET_802_11_DEAUTHENTICATE:
		ret = libertas_ret_80211_disassociate(priv, resp);
		break;

	case CMD_RET_802_11_AD_HOC_START:
	case CMD_RET_802_11_AD_HOC_JOIN:
		ret = libertas_ret_80211_ad_hoc_start(priv, resp);
		break;

	case CMD_RET_802_11_STAT:
		ret = wlan_ret_802_11_stat(priv, resp);
		break;

	case CMD_RET_802_11_SNMP_MIB:
		ret = wlan_ret_802_11_snmp_mib(priv, resp);
		break;

	case CMD_RET_802_11_RF_TX_POWER:
		ret = wlan_ret_802_11_rf_tx_power(priv, resp);
		break;

	case CMD_RET_802_11_SET_AFC:
	case CMD_RET_802_11_GET_AFC:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		memmove(adapter->cur_cmd->pdata_buf, &resp->params.afc,
			sizeof(struct cmd_ds_802_11_afc));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);

		break;
	case CMD_RET_802_11_RF_ANTENNA:
		ret = wlan_ret_802_11_rf_antenna(priv, resp);
		break;

	case CMD_RET_MAC_MULTICAST_ADR:
	case CMD_RET_MAC_CONTROL:
	case CMD_RET_802_11_SET_WEP:
	case CMD_RET_802_11_RESET:
	case CMD_RET_802_11_AUTHENTICATE:
	case CMD_RET_802_11_RADIO_CONTROL:
	case CMD_RET_802_11_BEACON_STOP:
		break;

	case CMD_RET_802_11_ENABLE_RSN:
		ret = libertas_ret_802_11_enable_rsn(priv, resp);
		break;

	case CMD_RET_802_11_DATA_RATE:
		ret = wlan_ret_802_11_data_rate(priv, resp);
		break;
	case CMD_RET_802_11_RATE_ADAPT_RATESET:
		ret = wlan_ret_802_11_rate_adapt_rateset(priv, resp);
		break;
	case CMD_RET_802_11_RF_CHANNEL:
		ret = wlan_ret_802_11_rf_channel(priv, resp);
		break;

	case CMD_RET_802_11_RSSI:
		ret = wlan_ret_802_11_rssi(priv, resp);
		break;

	case CMD_RET_802_11_MAC_ADDRESS:
		ret = wlan_ret_802_11_mac_address(priv, resp);
		break;

	case CMD_RET_802_11_AD_HOC_STOP:
		ret = libertas_ret_80211_ad_hoc_stop(priv, resp);
		break;

	case CMD_RET_802_11_KEY_MATERIAL:
		lbs_deb_cmd("CMD_RESP: KEY_MATERIAL command response\n");
		ret = wlan_ret_802_11_key_material(priv, resp);
		break;

	case CMD_RET_802_11_EEPROM_ACCESS:
		ret = wlan_ret_802_11_eeprom_access(priv, resp);
		break;

	case CMD_RET_802_11D_DOMAIN_INFO:
		ret = libertas_ret_802_11d_domain_info(priv, resp);
		break;

	case CMD_RET_802_11_SLEEP_PARAMS:
		ret = wlan_ret_802_11_sleep_params(priv, resp);
		break;
	case CMD_RET_802_11_INACTIVITY_TIMEOUT:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		*((u16 *) adapter->cur_cmd->pdata_buf) =
		    le16_to_cpu(resp->params.inactivity_timeout.timeout);
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;

	case CMD_RET_802_11_TPC_CFG:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		memmove(adapter->cur_cmd->pdata_buf, &resp->params.tpccfg,
			sizeof(struct cmd_ds_802_11_tpc_cfg));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;
	case CMD_RET_802_11_LED_GPIO_CTRL:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		memmove(adapter->cur_cmd->pdata_buf, &resp->params.ledgpio,
			sizeof(struct cmd_ds_802_11_led_ctrl));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;
	case CMD_RET_802_11_PWR_CFG:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		memmove(adapter->cur_cmd->pdata_buf, &resp->params.pwrcfg,
			sizeof(struct cmd_ds_802_11_pwr_cfg));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);

		break;

	case CMD_RET_GET_TSF:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		memcpy(priv->adapter->cur_cmd->pdata_buf,
		       &resp->params.gettsf.tsfvalue, sizeof(u64));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;
	case CMD_RET_BT_ACCESS:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		if (adapter->cur_cmd->pdata_buf)
			memcpy(adapter->cur_cmd->pdata_buf,
			       &resp->params.bt.addr1, 2 * ETH_ALEN);
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;
	case CMD_RET_FWT_ACCESS:
		spin_lock_irqsave(&adapter->driver_lock, flags);
		if (adapter->cur_cmd->pdata_buf)
			memcpy(adapter->cur_cmd->pdata_buf, &resp->params.fwt,
			       sizeof(resp->params.fwt));
		spin_unlock_irqrestore(&adapter->driver_lock, flags);
		break;
	case CMD_RET_MESH_ACCESS:
		if (adapter->cur_cmd->pdata_buf)
			memcpy(adapter->cur_cmd->pdata_buf, &resp->params.mesh,
			       sizeof(resp->params.mesh));
		break;
	case CMD_RTE_802_11_TX_RATE_QUERY:
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
	adapter->cur_cmd_retcode = result;;

	if (respcmd == CMD_RET_802_11_PS_MODE) {
		struct cmd_ds_802_11_ps_mode *psmode = &resp->params.psmode;
		u16 action = le16_to_cpu(psmode->action);

		lbs_deb_cmd(
		       "CMD_RESP: PS_MODE cmd reply result=%#x action=0x%X\n",
		       result, action);

		if (result) {
			lbs_deb_cmd("CMD_RESP: PS command failed- %#x \n",
				    result);
			/*
			 * We should not re-try enter-ps command in
			 * ad-hoc mode. It takes place in
			 * libertas_execute_next_command().
			 */
			if (adapter->mode == IW_MODE_ADHOC &&
			    action == CMD_SUBCMD_ENTER_PS)
				adapter->psmode = WLAN802_11POWERMODECAM;
		} else if (action == CMD_SUBCMD_ENTER_PS) {
			adapter->needtowakeup = 0;
			adapter->psstate = PS_STATE_AWAKE;

			lbs_deb_cmd("CMD_RESP: Enter_PS command response\n");
			if (adapter->connect_status != LIBERTAS_CONNECTED) {
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
		} else if (action == CMD_SUBCMD_EXIT_PS) {
			adapter->needtowakeup = 0;
			adapter->psstate = PS_STATE_FULL_POWER;
			lbs_deb_cmd("CMD_RESP: Exit_PS command response\n");
		} else {
			lbs_deb_cmd("CMD_RESP: PS- action=0x%X\n", action);
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
		       respcmd, result);
		/*
		 * Handling errors here
		 */
		switch (respcmd) {
		case CMD_RET_HW_SPEC_INFO:
		case CMD_RET_802_11_RESET:
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
		adapter->connect_status = LIBERTAS_CONNECTED ;
		if (priv->mesh_open == 1) {
			netif_wake_queue(priv->mesh_dev) ;
			netif_carrier_on(priv->mesh_dev) ;
		}
		adapter->mode = IW_MODE_ADHOC ;
		schedule_work(&priv->sync_channel);
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
