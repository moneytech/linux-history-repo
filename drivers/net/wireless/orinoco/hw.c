/* Encapsulate basic setting changes and retrieval on Hermes hardware
 *
 * See copyright notice in main.c
 */
#include <linux/kernel.h>
#include <linux/if_arp.h>
#include <linux/ieee80211.h>
#include <linux/wireless.h>

#include "hermes.h"
#include "hermes_rid.h"
#include "orinoco.h"

#include "hw.h"

#define SYMBOL_MAX_VER_LEN	(14)

/* Symbol firmware has a bug allocating buffers larger than this */
#define TX_NICBUF_SIZE_BUG	1585

/********************************************************************/
/* Data tables                                                      */
/********************************************************************/

/* This tables gives the actual meanings of the bitrate IDs returned
 * by the firmware. */
static const struct {
	int bitrate; /* in 100s of kilobits */
	int automatic;
	u16 agere_txratectrl;
	u16 intersil_txratectrl;
} bitrate_table[] = {
	{110, 1,  3, 15}, /* Entry 0 is the default */
	{10,  0,  1,  1},
	{10,  1,  1,  1},
	{20,  0,  2,  2},
	{20,  1,  6,  3},
	{55,  0,  4,  4},
	{55,  1,  7,  7},
	{110, 0,  5,  8},
};
#define BITRATE_TABLE_SIZE ARRAY_SIZE(bitrate_table)

/* Firmware version encoding */
struct comp_id {
	u16 id, variant, major, minor;
} __attribute__ ((packed));

static inline fwtype_t determine_firmware_type(struct comp_id *nic_id)
{
	if (nic_id->id < 0x8000)
		return FIRMWARE_TYPE_AGERE;
	else if (nic_id->id == 0x8000 && nic_id->major == 0)
		return FIRMWARE_TYPE_SYMBOL;
	else
		return FIRMWARE_TYPE_INTERSIL;
}

/* Set priv->firmware type, determine firmware properties */
int determine_fw_capabilities(struct orinoco_private *priv)
{
	struct net_device *dev = priv->ndev;
	hermes_t *hw = &priv->hw;
	int err;
	struct comp_id nic_id, sta_id;
	unsigned int firmver;
	char tmp[SYMBOL_MAX_VER_LEN+1] __attribute__((aligned(2)));

	/* Get the hardware version */
	err = HERMES_READ_RECORD(hw, USER_BAP, HERMES_RID_NICID, &nic_id);
	if (err) {
		printk(KERN_ERR "%s: Cannot read hardware identity: error %d\n",
		       dev->name, err);
		return err;
	}

	le16_to_cpus(&nic_id.id);
	le16_to_cpus(&nic_id.variant);
	le16_to_cpus(&nic_id.major);
	le16_to_cpus(&nic_id.minor);
	printk(KERN_DEBUG "%s: Hardware identity %04x:%04x:%04x:%04x\n",
	       dev->name, nic_id.id, nic_id.variant,
	       nic_id.major, nic_id.minor);

	priv->firmware_type = determine_firmware_type(&nic_id);

	/* Get the firmware version */
	err = HERMES_READ_RECORD(hw, USER_BAP, HERMES_RID_STAID, &sta_id);
	if (err) {
		printk(KERN_ERR "%s: Cannot read station identity: error %d\n",
		       dev->name, err);
		return err;
	}

	le16_to_cpus(&sta_id.id);
	le16_to_cpus(&sta_id.variant);
	le16_to_cpus(&sta_id.major);
	le16_to_cpus(&sta_id.minor);
	printk(KERN_DEBUG "%s: Station identity  %04x:%04x:%04x:%04x\n",
	       dev->name, sta_id.id, sta_id.variant,
	       sta_id.major, sta_id.minor);

	switch (sta_id.id) {
	case 0x15:
		printk(KERN_ERR "%s: Primary firmware is active\n",
		       dev->name);
		return -ENODEV;
	case 0x14b:
		printk(KERN_ERR "%s: Tertiary firmware is active\n",
		       dev->name);
		return -ENODEV;
	case 0x1f:	/* Intersil, Agere, Symbol Spectrum24 */
	case 0x21:	/* Symbol Spectrum24 Trilogy */
		break;
	default:
		printk(KERN_NOTICE "%s: Unknown station ID, please report\n",
		       dev->name);
		break;
	}

	/* Default capabilities */
	priv->has_sensitivity = 1;
	priv->has_mwo = 0;
	priv->has_preamble = 0;
	priv->has_port3 = 1;
	priv->has_ibss = 1;
	priv->has_wep = 0;
	priv->has_big_wep = 0;
	priv->has_alt_txcntl = 0;
	priv->has_ext_scan = 0;
	priv->has_wpa = 0;
	priv->do_fw_download = 0;

	/* Determine capabilities from the firmware version */
	switch (priv->firmware_type) {
	case FIRMWARE_TYPE_AGERE:
		/* Lucent Wavelan IEEE, Lucent Orinoco, Cabletron RoamAbout,
		   ELSA, Melco, HP, IBM, Dell 1150, Compaq 110/210 */
		snprintf(priv->fw_name, sizeof(priv->fw_name) - 1,
			 "Lucent/Agere %d.%02d", sta_id.major, sta_id.minor);

		firmver = ((unsigned long)sta_id.major << 16) | sta_id.minor;

		priv->has_ibss = (firmver >= 0x60006);
		priv->has_wep = (firmver >= 0x40020);
		priv->has_big_wep = 1; /* FIXME: this is wrong - how do we tell
					  Gold cards from the others? */
		priv->has_mwo = (firmver >= 0x60000);
		priv->has_pm = (firmver >= 0x40020); /* Don't work in 7.52 ? */
		priv->ibss_port = 1;
		priv->has_hostscan = (firmver >= 0x8000a);
		priv->do_fw_download = 1;
		priv->broken_monitor = (firmver >= 0x80000);
		priv->has_alt_txcntl = (firmver >= 0x90000); /* All 9.x ? */
		priv->has_ext_scan = (firmver >= 0x90000); /* All 9.x ? */
		priv->has_wpa = (firmver >= 0x9002a);
		/* Tested with Agere firmware :
		 *	1.16 ; 4.08 ; 4.52 ; 6.04 ; 6.16 ; 7.28 => Jean II
		 * Tested CableTron firmware : 4.32 => Anton */
		break;
	case FIRMWARE_TYPE_SYMBOL:
		/* Symbol , 3Com AirConnect, Intel, Ericsson WLAN */
		/* Intel MAC : 00:02:B3:* */
		/* 3Com MAC : 00:50:DA:* */
		memset(tmp, 0, sizeof(tmp));
		/* Get the Symbol firmware version */
		err = hermes_read_ltv(hw, USER_BAP,
				      HERMES_RID_SECONDARYVERSION_SYMBOL,
				      SYMBOL_MAX_VER_LEN, NULL, &tmp);
		if (err) {
			printk(KERN_WARNING
			       "%s: Error %d reading Symbol firmware info. "
			       "Wildly guessing capabilities...\n",
			       dev->name, err);
			firmver = 0;
			tmp[0] = '\0';
		} else {
			/* The firmware revision is a string, the format is
			 * something like : "V2.20-01".
			 * Quick and dirty parsing... - Jean II
			 */
			firmver = ((tmp[1] - '0') << 16)
				| ((tmp[3] - '0') << 12)
				| ((tmp[4] - '0') << 8)
				| ((tmp[6] - '0') << 4)
				| (tmp[7] - '0');

			tmp[SYMBOL_MAX_VER_LEN] = '\0';
		}

		snprintf(priv->fw_name, sizeof(priv->fw_name) - 1,
			 "Symbol %s", tmp);

		priv->has_ibss = (firmver >= 0x20000);
		priv->has_wep = (firmver >= 0x15012);
		priv->has_big_wep = (firmver >= 0x20000);
		priv->has_pm = (firmver >= 0x20000 && firmver < 0x22000) ||
			       (firmver >= 0x29000 && firmver < 0x30000) ||
			       firmver >= 0x31000;
		priv->has_preamble = (firmver >= 0x20000);
		priv->ibss_port = 4;

		/* Symbol firmware is found on various cards, but
		 * there has been no attempt to check firmware
		 * download on non-spectrum_cs based cards.
		 *
		 * Given that the Agere firmware download works
		 * differently, we should avoid doing a firmware
		 * download with the Symbol algorithm on non-spectrum
		 * cards.
		 *
		 * For now we can identify a spectrum_cs based card
		 * because it has a firmware reset function.
		 */
		priv->do_fw_download = (priv->stop_fw != NULL);

		priv->broken_disableport = (firmver == 0x25013) ||
				(firmver >= 0x30000 && firmver <= 0x31000);
		priv->has_hostscan = (firmver >= 0x31001) ||
				     (firmver >= 0x29057 && firmver < 0x30000);
		/* Tested with Intel firmware : 0x20015 => Jean II */
		/* Tested with 3Com firmware : 0x15012 & 0x22001 => Jean II */
		break;
	case FIRMWARE_TYPE_INTERSIL:
		/* D-Link, Linksys, Adtron, ZoomAir, and many others...
		 * Samsung, Compaq 100/200 and Proxim are slightly
		 * different and less well tested */
		/* D-Link MAC : 00:40:05:* */
		/* Addtron MAC : 00:90:D1:* */
		snprintf(priv->fw_name, sizeof(priv->fw_name) - 1,
			 "Intersil %d.%d.%d", sta_id.major, sta_id.minor,
			 sta_id.variant);

		firmver = ((unsigned long)sta_id.major << 16) |
			((unsigned long)sta_id.minor << 8) | sta_id.variant;

		priv->has_ibss = (firmver >= 0x000700); /* FIXME */
		priv->has_big_wep = priv->has_wep = (firmver >= 0x000800);
		priv->has_pm = (firmver >= 0x000700);
		priv->has_hostscan = (firmver >= 0x010301);

		if (firmver >= 0x000800)
			priv->ibss_port = 0;
		else {
			printk(KERN_NOTICE "%s: Intersil firmware earlier "
			       "than v0.8.x - several features not supported\n",
			       dev->name);
			priv->ibss_port = 1;
		}
		break;
	}
	printk(KERN_DEBUG "%s: Firmware determined as %s\n", dev->name,
	       priv->fw_name);

	return 0;
}

/* Read settings from EEPROM into our private structure.
 * MAC address gets dropped into callers buffer */
int orinoco_hw_read_card_settings(struct orinoco_private *priv, u8 *dev_addr)
{
	struct net_device *dev = priv->ndev;
	struct hermes_idstring nickbuf;
	hermes_t *hw = &priv->hw;
	int len;
	int err;
	u16 reclen;

	/* Get the MAC address */
	err = hermes_read_ltv(hw, USER_BAP, HERMES_RID_CNFOWNMACADDR,
			      ETH_ALEN, NULL, dev_addr);
	if (err) {
		printk(KERN_WARNING "%s: failed to read MAC address!\n",
		       dev->name);
		goto out;
	}

	printk(KERN_DEBUG "%s: MAC address %pM\n",
	       dev->name, dev_addr);

	/* Get the station name */
	err = hermes_read_ltv(hw, USER_BAP, HERMES_RID_CNFOWNNAME,
			      sizeof(nickbuf), &reclen, &nickbuf);
	if (err) {
		printk(KERN_ERR "%s: failed to read station name\n",
		       dev->name);
		goto out;
	}
	if (nickbuf.len)
		len = min(IW_ESSID_MAX_SIZE, (int)le16_to_cpu(nickbuf.len));
	else
		len = min(IW_ESSID_MAX_SIZE, 2 * reclen);
	memcpy(priv->nick, &nickbuf.val, len);
	priv->nick[len] = '\0';

	printk(KERN_DEBUG "%s: Station name \"%s\"\n", dev->name, priv->nick);

	/* Get allowed channels */
	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CHANNELLIST,
				  &priv->channel_mask);
	if (err) {
		printk(KERN_ERR "%s: failed to read channel list!\n",
		       dev->name);
		goto out;
	}

	/* Get initial AP density */
	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CNFSYSTEMSCALE,
				  &priv->ap_density);
	if (err || priv->ap_density < 1 || priv->ap_density > 3)
		priv->has_sensitivity = 0;

	/* Get initial RTS threshold */
	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CNFRTSTHRESHOLD,
				  &priv->rts_thresh);
	if (err) {
		printk(KERN_ERR "%s: failed to read RTS threshold!\n",
		       dev->name);
		goto out;
	}

	/* Get initial fragmentation settings */
	if (priv->has_mwo)
		err = hermes_read_wordrec(hw, USER_BAP,
					  HERMES_RID_CNFMWOROBUST_AGERE,
					  &priv->mwo_robust);
	else
		err = hermes_read_wordrec(hw, USER_BAP,
					  HERMES_RID_CNFFRAGMENTATIONTHRESHOLD,
					  &priv->frag_thresh);
	if (err) {
		printk(KERN_ERR "%s: failed to read fragmentation settings!\n",
		       dev->name);
		goto out;
	}

	/* Power management setup */
	if (priv->has_pm) {
		priv->pm_on = 0;
		priv->pm_mcast = 1;
		err = hermes_read_wordrec(hw, USER_BAP,
					  HERMES_RID_CNFMAXSLEEPDURATION,
					  &priv->pm_period);
		if (err) {
			printk(KERN_ERR "%s: failed to read power management "
			       "period!\n", dev->name);
			goto out;
		}
		err = hermes_read_wordrec(hw, USER_BAP,
					  HERMES_RID_CNFPMHOLDOVERDURATION,
					  &priv->pm_timeout);
		if (err) {
			printk(KERN_ERR "%s: failed to read power management "
			       "timeout!\n", dev->name);
			goto out;
		}
	}

	/* Preamble setup */
	if (priv->has_preamble) {
		err = hermes_read_wordrec(hw, USER_BAP,
					  HERMES_RID_CNFPREAMBLE_SYMBOL,
					  &priv->preamble);
	}

out:
	return err;
}

int orinoco_hw_allocate_fid(struct orinoco_private *priv)
{
	struct net_device *dev = priv->ndev;
	struct hermes *hw = &priv->hw;
	int err;

	err = hermes_allocate(hw, priv->nicbuf_size, &priv->txfid);
	if (err == -EIO && priv->nicbuf_size > TX_NICBUF_SIZE_BUG) {
		/* Try workaround for old Symbol firmware bug */
		priv->nicbuf_size = TX_NICBUF_SIZE_BUG;
		err = hermes_allocate(hw, priv->nicbuf_size, &priv->txfid);

		printk(KERN_WARNING "%s: firmware ALLOC bug detected "
		       "(old Symbol firmware?). Work around %s\n",
		       dev->name, err ? "failed!" : "ok.");
	}

	return err;
}

int orinoco_get_bitratemode(int bitrate, int automatic)
{
	int ratemode = -1;
	int i;

	if ((bitrate != 10) && (bitrate != 20) &&
	    (bitrate != 55) && (bitrate != 110))
		return ratemode;

	for (i = 0; i < BITRATE_TABLE_SIZE; i++) {
		if ((bitrate_table[i].bitrate == bitrate) &&
		    (bitrate_table[i].automatic == automatic)) {
			ratemode = i;
			break;
		}
	}
	return ratemode;
}

void orinoco_get_ratemode_cfg(int ratemode, int *bitrate, int *automatic)
{
	BUG_ON((ratemode < 0) || (ratemode >= BITRATE_TABLE_SIZE));

	*bitrate = bitrate_table[ratemode].bitrate * 100000;
	*automatic = bitrate_table[ratemode].automatic;
}

/* Get tsc from the firmware */
int orinoco_hw_get_tkip_iv(struct orinoco_private *priv, int key, u8 *tsc)
{
	hermes_t *hw = &priv->hw;
	int err = 0;
	u8 tsc_arr[4][IW_ENCODE_SEQ_MAX_SIZE];

	if ((key < 0) || (key > 4))
		return -EINVAL;

	err = hermes_read_ltv(hw, USER_BAP, HERMES_RID_CURRENT_TKIP_IV,
			      sizeof(tsc_arr), NULL, &tsc_arr);
	if (!err)
		memcpy(tsc, &tsc_arr[key][0], sizeof(tsc_arr[0]));

	return err;
}

int __orinoco_hw_set_bitrate(struct orinoco_private *priv)
{
	hermes_t *hw = &priv->hw;
	int ratemode = priv->bitratemode;
	int err = 0;

	if (ratemode >= BITRATE_TABLE_SIZE) {
		printk(KERN_ERR "%s: BUG: Invalid bitrate mode %d\n",
		       priv->ndev->name, ratemode);
		return -EINVAL;
	}

	switch (priv->firmware_type) {
	case FIRMWARE_TYPE_AGERE:
		err = hermes_write_wordrec(hw, USER_BAP,
				HERMES_RID_CNFTXRATECONTROL,
				bitrate_table[ratemode].agere_txratectrl);
		break;
	case FIRMWARE_TYPE_INTERSIL:
	case FIRMWARE_TYPE_SYMBOL:
		err = hermes_write_wordrec(hw, USER_BAP,
				HERMES_RID_CNFTXRATECONTROL,
				bitrate_table[ratemode].intersil_txratectrl);
		break;
	default:
		BUG();
	}

	return err;
}

int orinoco_hw_get_act_bitrate(struct orinoco_private *priv, int *bitrate)
{
	hermes_t *hw = &priv->hw;
	int i;
	int err = 0;
	u16 val;

	err = hermes_read_wordrec(hw, USER_BAP,
				  HERMES_RID_CURRENTTXRATE, &val);
	if (err)
		return err;

	switch (priv->firmware_type) {
	case FIRMWARE_TYPE_AGERE: /* Lucent style rate */
		/* Note : in Lucent firmware, the return value of
		 * HERMES_RID_CURRENTTXRATE is the bitrate in Mb/s,
		 * and therefore is totally different from the
		 * encoding of HERMES_RID_CNFTXRATECONTROL.
		 * Don't forget that 6Mb/s is really 5.5Mb/s */
		if (val == 6)
			*bitrate = 5500000;
		else
			*bitrate = val * 1000000;
		break;
	case FIRMWARE_TYPE_INTERSIL: /* Intersil style rate */
	case FIRMWARE_TYPE_SYMBOL: /* Symbol style rate */
		for (i = 0; i < BITRATE_TABLE_SIZE; i++)
			if (bitrate_table[i].intersil_txratectrl == val)
				break;

		if (i >= BITRATE_TABLE_SIZE)
			printk(KERN_INFO "%s: Unable to determine current bitrate (0x%04hx)\n",
			       priv->ndev->name, val);

		*bitrate = bitrate_table[i].bitrate * 100000;
		break;
	default:
		BUG();
	}

	return err;
}

/* Set fixed AP address */
int __orinoco_hw_set_wap(struct orinoco_private *priv)
{
	int roaming_flag;
	int err = 0;
	hermes_t *hw = &priv->hw;

	switch (priv->firmware_type) {
	case FIRMWARE_TYPE_AGERE:
		/* not supported */
		break;
	case FIRMWARE_TYPE_INTERSIL:
		if (priv->bssid_fixed)
			roaming_flag = 2;
		else
			roaming_flag = 1;

		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFROAMINGMODE,
					   roaming_flag);
		break;
	case FIRMWARE_TYPE_SYMBOL:
		err = HERMES_WRITE_RECORD(hw, USER_BAP,
					  HERMES_RID_CNFMANDATORYBSSID_SYMBOL,
					  &priv->desired_bssid);
		break;
	}
	return err;
}

/* Change the WEP keys and/or the current keys.  Can be called
 * either from __orinoco_hw_setup_enc() or directly from
 * orinoco_ioctl_setiwencode().  In the later case the association
 * with the AP is not broken (if the firmware can handle it),
 * which is needed for 802.1x implementations. */
int __orinoco_hw_setup_wepkeys(struct orinoco_private *priv)
{
	hermes_t *hw = &priv->hw;
	int err = 0;

	switch (priv->firmware_type) {
	case FIRMWARE_TYPE_AGERE:
		err = HERMES_WRITE_RECORD(hw, USER_BAP,
					  HERMES_RID_CNFWEPKEYS_AGERE,
					  &priv->keys);
		if (err)
			return err;
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFTXKEY_AGERE,
					   priv->tx_key);
		if (err)
			return err;
		break;
	case FIRMWARE_TYPE_INTERSIL:
	case FIRMWARE_TYPE_SYMBOL:
		{
			int keylen;
			int i;

			/* Force uniform key length to work around
			 * firmware bugs */
			keylen = le16_to_cpu(priv->keys[priv->tx_key].len);

			if (keylen > LARGE_KEY_SIZE) {
				printk(KERN_ERR "%s: BUG: Key %d has oversize length %d.\n",
				       priv->ndev->name, priv->tx_key, keylen);
				return -E2BIG;
			}

			/* Write all 4 keys */
			for (i = 0; i < ORINOCO_MAX_KEYS; i++) {
				err = hermes_write_ltv(hw, USER_BAP,
						HERMES_RID_CNFDEFAULTKEY0 + i,
						HERMES_BYTES_TO_RECLEN(keylen),
						priv->keys[i].data);
				if (err)
					return err;
			}

			/* Write the index of the key used in transmission */
			err = hermes_write_wordrec(hw, USER_BAP,
						HERMES_RID_CNFWEPDEFAULTKEYID,
						priv->tx_key);
			if (err)
				return err;
		}
		break;
	}

	return 0;
}

int __orinoco_hw_setup_enc(struct orinoco_private *priv)
{
	hermes_t *hw = &priv->hw;
	int err = 0;
	int master_wep_flag;
	int auth_flag;
	int enc_flag;

	/* Setup WEP keys for WEP and WPA */
	if (priv->encode_alg)
		__orinoco_hw_setup_wepkeys(priv);

	if (priv->wep_restrict)
		auth_flag = HERMES_AUTH_SHARED_KEY;
	else
		auth_flag = HERMES_AUTH_OPEN;

	if (priv->wpa_enabled)
		enc_flag = 2;
	else if (priv->encode_alg == IW_ENCODE_ALG_WEP)
		enc_flag = 1;
	else
		enc_flag = 0;

	switch (priv->firmware_type) {
	case FIRMWARE_TYPE_AGERE: /* Agere style WEP */
		if (priv->encode_alg == IW_ENCODE_ALG_WEP) {
			/* Enable the shared-key authentication. */
			err = hermes_write_wordrec(hw, USER_BAP,
					HERMES_RID_CNFAUTHENTICATION_AGERE,
					auth_flag);
		}
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFWEPENABLED_AGERE,
					   enc_flag);
		if (err)
			return err;

		if (priv->has_wpa) {
			/* Set WPA key management */
			err = hermes_write_wordrec(hw, USER_BAP,
				  HERMES_RID_CNFSETWPAAUTHMGMTSUITE_AGERE,
				  priv->key_mgmt);
			if (err)
				return err;
		}

		break;

	case FIRMWARE_TYPE_INTERSIL: /* Intersil style WEP */
	case FIRMWARE_TYPE_SYMBOL: /* Symbol style WEP */
		if (priv->encode_alg == IW_ENCODE_ALG_WEP) {
			if (priv->wep_restrict ||
			    (priv->firmware_type == FIRMWARE_TYPE_SYMBOL))
				master_wep_flag = HERMES_WEP_PRIVACY_INVOKED |
						  HERMES_WEP_EXCL_UNENCRYPTED;
			else
				master_wep_flag = HERMES_WEP_PRIVACY_INVOKED;

			err = hermes_write_wordrec(hw, USER_BAP,
						   HERMES_RID_CNFAUTHENTICATION,
						   auth_flag);
			if (err)
				return err;
		} else
			master_wep_flag = 0;

		if (priv->iw_mode == IW_MODE_MONITOR)
			master_wep_flag |= HERMES_WEP_HOST_DECRYPT;

		/* Master WEP setting : on/off */
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFWEPFLAGS_INTERSIL,
					   master_wep_flag);
		if (err)
			return err;

		break;
	}

	return 0;
}

/* key must be 32 bytes, including the tx and rx MIC keys.
 * rsc must be 8 bytes
 * tsc must be 8 bytes or NULL
 */
int __orinoco_hw_set_tkip_key(hermes_t *hw, int key_idx, int set_tx,
			      u8 *key, u8 *rsc, u8 *tsc)
{
	struct {
		__le16 idx;
		u8 rsc[IW_ENCODE_SEQ_MAX_SIZE];
		u8 key[TKIP_KEYLEN];
		u8 tx_mic[MIC_KEYLEN];
		u8 rx_mic[MIC_KEYLEN];
		u8 tsc[IW_ENCODE_SEQ_MAX_SIZE];
	} __attribute__ ((packed)) buf;
	int ret;
	int err;
	int k;
	u16 xmitting;

	key_idx &= 0x3;

	if (set_tx)
		key_idx |= 0x8000;

	buf.idx = cpu_to_le16(key_idx);
	memcpy(buf.key, key,
	       sizeof(buf.key) + sizeof(buf.tx_mic) + sizeof(buf.rx_mic));

	if (rsc == NULL)
		memset(buf.rsc, 0, sizeof(buf.rsc));
	else
		memcpy(buf.rsc, rsc, sizeof(buf.rsc));

	if (tsc == NULL) {
		memset(buf.tsc, 0, sizeof(buf.tsc));
		buf.tsc[4] = 0x10;
	} else {
		memcpy(buf.tsc, tsc, sizeof(buf.tsc));
	}

	/* Wait upto 100ms for tx queue to empty */
	for (k = 100; k > 0; k--) {
		udelay(1000);
		ret = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_TXQUEUEEMPTY,
					  &xmitting);
		if (ret || !xmitting)
			break;
	}

	if (k == 0)
		ret = -ETIMEDOUT;

	err = HERMES_WRITE_RECORD(hw, USER_BAP,
				  HERMES_RID_CNFADDDEFAULTTKIPKEY_AGERE,
				  &buf);

	return ret ? ret : err;
}

int orinoco_clear_tkip_key(struct orinoco_private *priv, int key_idx)
{
	hermes_t *hw = &priv->hw;
	int err;

	memset(&priv->tkip_key[key_idx], 0, sizeof(priv->tkip_key[key_idx]));
	err = hermes_write_wordrec(hw, USER_BAP,
				   HERMES_RID_CNFREMDEFAULTTKIPKEY_AGERE,
				   key_idx);
	if (err)
		printk(KERN_WARNING "%s: Error %d clearing TKIP key %d\n",
		       priv->ndev->name, err, key_idx);
	return err;
}

int __orinoco_hw_set_multicast_list(struct orinoco_private *priv,
				    struct dev_addr_list *mc_list,
				    int mc_count, int promisc)
{
	hermes_t *hw = &priv->hw;
	int err = 0;

	if (promisc != priv->promiscuous) {
		err = hermes_write_wordrec(hw, USER_BAP,
					   HERMES_RID_CNFPROMISCUOUSMODE,
					   promisc);
		if (err) {
			printk(KERN_ERR "%s: Error %d setting PROMISCUOUSMODE to 1.\n",
			       priv->ndev->name, err);
		} else
			priv->promiscuous = promisc;
	}

	/* If we're not in promiscuous mode, then we need to set the
	 * group address if either we want to multicast, or if we were
	 * multicasting and want to stop */
	if (!promisc && (mc_count || priv->mc_count)) {
		struct dev_mc_list *p = mc_list;
		struct hermes_multicast mclist;
		int i;

		for (i = 0; i < mc_count; i++) {
			/* paranoia: is list shorter than mc_count? */
			BUG_ON(!p);
			/* paranoia: bad address size in list? */
			BUG_ON(p->dmi_addrlen != ETH_ALEN);

			memcpy(mclist.addr[i], p->dmi_addr, ETH_ALEN);
			p = p->next;
		}

		if (p)
			printk(KERN_WARNING "%s: Multicast list is "
			       "longer than mc_count\n", priv->ndev->name);

		err = hermes_write_ltv(hw, USER_BAP,
				   HERMES_RID_CNFGROUPADDRESSES,
				   HERMES_BYTES_TO_RECLEN(mc_count * ETH_ALEN),
				   &mclist);
		if (err)
			printk(KERN_ERR "%s: Error %d setting multicast list.\n",
			       priv->ndev->name, err);
		else
			priv->mc_count = mc_count;
	}
	return err;
}

/* Return : < 0 -> error code ; >= 0 -> length */
int orinoco_hw_get_essid(struct orinoco_private *priv, int *active,
			 char buf[IW_ESSID_MAX_SIZE+1])
{
	hermes_t *hw = &priv->hw;
	int err = 0;
	struct hermes_idstring essidbuf;
	char *p = (char *)(&essidbuf.val);
	int len;
	unsigned long flags;

	if (orinoco_lock(priv, &flags) != 0)
		return -EBUSY;

	if (strlen(priv->desired_essid) > 0) {
		/* We read the desired SSID from the hardware rather
		   than from priv->desired_essid, just in case the
		   firmware is allowed to change it on us. I'm not
		   sure about this */
		/* My guess is that the OWNSSID should always be whatever
		 * we set to the card, whereas CURRENT_SSID is the one that
		 * may change... - Jean II */
		u16 rid;

		*active = 1;

		rid = (priv->port_type == 3) ? HERMES_RID_CNFOWNSSID :
			HERMES_RID_CNFDESIREDSSID;

		err = hermes_read_ltv(hw, USER_BAP, rid, sizeof(essidbuf),
				      NULL, &essidbuf);
		if (err)
			goto fail_unlock;
	} else {
		*active = 0;

		err = hermes_read_ltv(hw, USER_BAP, HERMES_RID_CURRENTSSID,
				      sizeof(essidbuf), NULL, &essidbuf);
		if (err)
			goto fail_unlock;
	}

	len = le16_to_cpu(essidbuf.len);
	BUG_ON(len > IW_ESSID_MAX_SIZE);

	memset(buf, 0, IW_ESSID_MAX_SIZE);
	memcpy(buf, p, len);
	err = len;

 fail_unlock:
	orinoco_unlock(priv, &flags);

	return err;
}

int orinoco_hw_get_freq(struct orinoco_private *priv)
{
	hermes_t *hw = &priv->hw;
	int err = 0;
	u16 channel;
	int freq = 0;
	unsigned long flags;

	if (orinoco_lock(priv, &flags) != 0)
		return -EBUSY;

	err = hermes_read_wordrec(hw, USER_BAP, HERMES_RID_CURRENTCHANNEL,
				  &channel);
	if (err)
		goto out;

	/* Intersil firmware 1.3.5 returns 0 when the interface is down */
	if (channel == 0) {
		err = -EBUSY;
		goto out;
	}

	if ((channel < 1) || (channel > NUM_CHANNELS)) {
		printk(KERN_WARNING "%s: Channel out of range (%d)!\n",
		       priv->ndev->name, channel);
		err = -EBUSY;
		goto out;

	}
	freq = ieee80211_dsss_chan_to_freq(channel);

 out:
	orinoco_unlock(priv, &flags);

	if (err > 0)
		err = -EBUSY;
	return err ? err : freq;
}

int orinoco_hw_get_bitratelist(struct orinoco_private *priv,
			       int *numrates, s32 *rates, int max)
{
	hermes_t *hw = &priv->hw;
	struct hermes_idstring list;
	unsigned char *p = (unsigned char *)&list.val;
	int err = 0;
	int num;
	int i;
	unsigned long flags;

	if (orinoco_lock(priv, &flags) != 0)
		return -EBUSY;

	err = hermes_read_ltv(hw, USER_BAP, HERMES_RID_SUPPORTEDDATARATES,
			      sizeof(list), NULL, &list);
	orinoco_unlock(priv, &flags);

	if (err)
		return err;

	num = le16_to_cpu(list.len);
	*numrates = num;
	num = min(num, max);

	for (i = 0; i < num; i++)
		rates[i] = (p[i] & 0x7f) * 500000; /* convert to bps */

	return 0;
}
