/*
 * Atheros AR9170 driver
 *
 * Hardware-specific definitions
 *
 * Copyright 2008, Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, see
 * http://www.gnu.org/licenses/.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *    Copyright (c) 2007-2008 Atheros Communications, Inc.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __AR9170_HW_H
#define __AR9170_HW_H

#define AR9170_MAX_CMD_LEN	64

enum ar9170_cmd {
	AR9170_CMD_RREG		= 0x00,
	AR9170_CMD_WREG		= 0x01,
	AR9170_CMD_RMEM		= 0x02,
	AR9170_CMD_WMEM		= 0x03,
	AR9170_CMD_BITAND	= 0x04,
	AR9170_CMD_BITOR	= 0x05,
	AR9170_CMD_EKEY		= 0x28,
	AR9170_CMD_DKEY		= 0x29,
	AR9170_CMD_FREQUENCY	= 0x30,
	AR9170_CMD_RF_INIT	= 0x31,
	AR9170_CMD_SYNTH	= 0x32,
	AR9170_CMD_FREQ_START	= 0x33,
	AR9170_CMD_ECHO		= 0x80,
	AR9170_CMD_TALLY	= 0x81,
	AR9170_CMD_TALLY_APD	= 0x82,
	AR9170_CMD_CONFIG	= 0x83,
	AR9170_CMD_RESET	= 0x90,
	AR9170_CMD_DKRESET	= 0x91,
	AR9170_CMD_DKTX_STATUS	= 0x92,
	AR9170_CMD_FDC		= 0xA0,
	AR9170_CMD_WREEPROM	= 0xB0,
	AR9170_CMD_WFLASH	= 0xB0,
	AR9170_CMD_FLASH_ERASE	= 0xB1,
	AR9170_CMD_FLASH_PROG	= 0xB2,
	AR9170_CMD_FLASH_CHKSUM	= 0xB3,
	AR9170_CMD_FLASH_READ	= 0xB4,
	AR9170_CMD_FW_DL_INIT	= 0xB5,
	AR9170_CMD_MEM_WREEPROM	= 0xBB,
};

/* endpoints */
#define AR9170_EP_TX				1
#define AR9170_EP_RX				2
#define AR9170_EP_IRQ				3
#define AR9170_EP_CMD				4

#define AR9170_EEPROM_START			0x1600

#define AR9170_GPIO_REG_BASE			0x1d0100
#define AR9170_GPIO_REG_PORT_TYPE		AR9170_GPIO_REG_BASE
#define AR9170_GPIO_REG_DATA			(AR9170_GPIO_REG_BASE + 4)
#define AR9170_NUM_LEDS				2


#define AR9170_USB_REG_BASE			0x1e1000
#define AR9170_USB_REG_DMA_CTL			(AR9170_USB_REG_BASE + 0x108)
#define		AR9170_DMA_CTL_ENABLE_TO_DEVICE		0x1
#define		AR9170_DMA_CTL_ENABLE_FROM_DEVICE	0x2
#define		AR9170_DMA_CTL_HIGH_SPEED		0x4
#define		AR9170_DMA_CTL_PACKET_MODE		0x8

#define AR9170_USB_REG_MAX_AGG_UPLOAD		(AR9170_USB_REG_BASE + 0x110)
#define AR9170_USB_REG_UPLOAD_TIME_CTL		(AR9170_USB_REG_BASE + 0x114)



#define AR9170_MAC_REG_BASE			0x1c3000

#define AR9170_MAC_REG_TSF_L			(AR9170_MAC_REG_BASE + 0x514)
#define AR9170_MAC_REG_TSF_H			(AR9170_MAC_REG_BASE + 0x518)

#define AR9170_MAC_REG_ATIM_WINDOW		(AR9170_MAC_REG_BASE + 0x51C)
#define AR9170_MAC_REG_BCN_PERIOD		(AR9170_MAC_REG_BASE + 0x520)
#define AR9170_MAC_REG_PRETBTT			(AR9170_MAC_REG_BASE + 0x524)

#define AR9170_MAC_REG_MAC_ADDR_L		(AR9170_MAC_REG_BASE + 0x610)
#define AR9170_MAC_REG_MAC_ADDR_H		(AR9170_MAC_REG_BASE + 0x614)
#define AR9170_MAC_REG_BSSID_L			(AR9170_MAC_REG_BASE + 0x618)
#define AR9170_MAC_REG_BSSID_H			(AR9170_MAC_REG_BASE + 0x61c)

#define AR9170_MAC_REG_GROUP_HASH_TBL_L		(AR9170_MAC_REG_BASE + 0x624)
#define AR9170_MAC_REG_GROUP_HASH_TBL_H		(AR9170_MAC_REG_BASE + 0x628)

#define AR9170_MAC_REG_RX_TIMEOUT		(AR9170_MAC_REG_BASE + 0x62C)

#define AR9170_MAC_REG_BASIC_RATE		(AR9170_MAC_REG_BASE + 0x630)
#define AR9170_MAC_REG_MANDATORY_RATE		(AR9170_MAC_REG_BASE + 0x634)
#define AR9170_MAC_REG_RTS_CTS_RATE		(AR9170_MAC_REG_BASE + 0x638)
#define AR9170_MAC_REG_BACKOFF_PROTECT		(AR9170_MAC_REG_BASE + 0x63c)
#define AR9170_MAC_REG_RX_THRESHOLD		(AR9170_MAC_REG_BASE + 0x640)
#define AR9170_MAC_REG_RX_PE_DELAY		(AR9170_MAC_REG_BASE + 0x64C)

#define AR9170_MAC_REG_DYNAMIC_SIFS_ACK		(AR9170_MAC_REG_BASE + 0x658)
#define AR9170_MAC_REG_SNIFFER			(AR9170_MAC_REG_BASE + 0x674)
#define		AR9170_MAC_REG_SNIFFER_ENABLE_PROMISC	BIT(0)
#define		AR9170_MAC_REG_SNIFFER_DEFAULTS		0x02000000
#define AR9170_MAC_REG_ENCRYPTION		(AR9170_MAC_REG_BASE + 0x678)
#define		AR9170_MAC_REG_ENCRYPTION_RX_SOFTWARE	BIT(3)
#define		AR9170_MAC_REG_ENCRYPTION_DEFAULTS	0x70

#define AR9170_MAC_REG_MISC_680			(AR9170_MAC_REG_BASE + 0x680)
#define AR9170_MAC_REG_TX_UNDERRUN		(AR9170_MAC_REG_BASE + 0x688)

#define AR9170_MAC_REG_FRAMETYPE_FILTER		(AR9170_MAC_REG_BASE + 0x68c)
#define		AR9170_MAC_REG_FTF_ASSOC_REQ		BIT(0)
#define		AR9170_MAC_REG_FTF_ASSOC_RESP		BIT(1)
#define		AR9170_MAC_REG_FTF_REASSOC_REQ		BIT(2)
#define		AR9170_MAC_REG_FTF_REASSOC_RESP		BIT(3)
#define		AR9170_MAC_REG_FTF_PRB_REQ		BIT(4)
#define		AR9170_MAC_REG_FTF_PRB_RESP		BIT(5)
#define		AR9170_MAC_REG_FTF_BIT6			BIT(6)
#define		AR9170_MAC_REG_FTF_BIT7			BIT(7)
#define		AR9170_MAC_REG_FTF_BEACON		BIT(8)
#define		AR9170_MAC_REG_FTF_ATIM			BIT(9)
#define		AR9170_MAC_REG_FTF_DEASSOC		BIT(10)
#define		AR9170_MAC_REG_FTF_AUTH			BIT(11)
#define		AR9170_MAC_REG_FTF_DEAUTH		BIT(12)
#define		AR9170_MAC_REG_FTF_BIT13		BIT(13)
#define		AR9170_MAC_REG_FTF_BIT14		BIT(14)
#define		AR9170_MAC_REG_FTF_BIT15		BIT(15)
#define		AR9170_MAC_REG_FTF_BAR			BIT(24)
#define		AR9170_MAC_REG_FTF_BIT25		BIT(25)
#define		AR9170_MAC_REG_FTF_PSPOLL		BIT(26)
#define		AR9170_MAC_REG_FTF_RTS			BIT(27)
#define		AR9170_MAC_REG_FTF_CTS			BIT(28)
#define		AR9170_MAC_REG_FTF_ACK			BIT(29)
#define		AR9170_MAC_REG_FTF_CFE			BIT(30)
#define		AR9170_MAC_REG_FTF_CFE_ACK		BIT(31)
#define		AR9170_MAC_REG_FTF_DEFAULTS		0x0500ffff
#define		AR9170_MAC_REG_FTF_MONITOR		0xfd00ffff

#define AR9170_MAC_REG_RX_TOTAL			(AR9170_MAC_REG_BASE + 0x6A0)
#define AR9170_MAC_REG_RX_CRC32			(AR9170_MAC_REG_BASE + 0x6A4)
#define AR9170_MAC_REG_RX_CRC16			(AR9170_MAC_REG_BASE + 0x6A8)
#define AR9170_MAC_REG_RX_ERR_DECRYPTION_UNI	(AR9170_MAC_REG_BASE + 0x6AC)
#define AR9170_MAC_REG_RX_OVERRUN		(AR9170_MAC_REG_BASE + 0x6B0)
#define AR9170_MAC_REG_RX_ERR_DECRYPTION_MUL	(AR9170_MAC_REG_BASE + 0x6BC)
#define AR9170_MAC_REG_TX_RETRY			(AR9170_MAC_REG_BASE + 0x6CC)
#define AR9170_MAC_REG_TX_TOTAL			(AR9170_MAC_REG_BASE + 0x6F4)


#define AR9170_MAC_REG_ACK_EXTENSION		(AR9170_MAC_REG_BASE + 0x690)
#define AR9170_MAC_REG_EIFS_AND_SIFS		(AR9170_MAC_REG_BASE + 0x698)

#define AR9170_MAC_REG_SLOT_TIME		(AR9170_MAC_REG_BASE + 0x6F0)

#define AR9170_MAC_REG_POWERMANAGEMENT		(AR9170_MAC_REG_BASE + 0x700)
#define		AR9170_MAC_REG_POWERMGT_IBSS		0xe0
#define		AR9170_MAC_REG_POWERMGT_AP		0xa1
#define		AR9170_MAC_REG_POWERMGT_STA		0x2
#define		AR9170_MAC_REG_POWERMGT_AP_WDS		0x3
#define		AR9170_MAC_REG_POWERMGT_DEFAULTS	(0xf << 24)

#define AR9170_MAC_REG_ROLL_CALL_TBL_L		(AR9170_MAC_REG_BASE + 0x704)
#define AR9170_MAC_REG_ROLL_CALL_TBL_H		(AR9170_MAC_REG_BASE + 0x708)

#define AR9170_MAC_REG_AC0_CW			(AR9170_MAC_REG_BASE + 0xB00)
#define AR9170_MAC_REG_AC1_CW			(AR9170_MAC_REG_BASE + 0xB04)
#define AR9170_MAC_REG_AC2_CW			(AR9170_MAC_REG_BASE + 0xB08)
#define AR9170_MAC_REG_AC3_CW			(AR9170_MAC_REG_BASE + 0xB0C)
#define AR9170_MAC_REG_AC4_CW			(AR9170_MAC_REG_BASE + 0xB10)
#define AR9170_MAC_REG_AC1_AC0_AIFS		(AR9170_MAC_REG_BASE + 0xB14)
#define AR9170_MAC_REG_AC3_AC2_AIFS		(AR9170_MAC_REG_BASE + 0xB18)

#define AR9170_MAC_REG_RETRY_MAX		(AR9170_MAC_REG_BASE + 0xB28)

#define AR9170_MAC_REG_FCS_SELECT		(AR9170_MAC_REG_BASE + 0xBB0)
#define		AR9170_MAC_FCS_SWFCS		0x1
#define		AR9170_MAC_FCS_FIFO_PROT	0x4


#define AR9170_MAC_REG_TXOP_NOT_ENOUGH_IND	(AR9170_MAC_REG_BASE + 0xB30)

#define AR9170_MAC_REG_AC1_AC0_TXOP		(AR9170_MAC_REG_BASE + 0xB44)
#define AR9170_MAC_REG_AC3_AC2_TXOP		(AR9170_MAC_REG_BASE + 0xB48)

#define AR9170_MAC_REG_ACK_TABLE		(AR9170_MAC_REG_BASE + 0xC00)
#define AR9170_MAC_REG_AMPDU_RX_THRESH		(AR9170_MAC_REG_BASE + 0xC50)

#define AR9170_MAC_REG_TXRX_MPI			(AR9170_MAC_REG_BASE + 0xD7C)
#define		AR9170_MAC_TXRX_MPI_TX_MPI_MASK	0x0000000f
#define		AR9170_MAC_TXRX_MPI_TX_TO_MASK	0x0000fff0
#define		AR9170_MAC_TXRX_MPI_RX_MPI_MASK	0x000f0000
#define		AR9170_MAC_TXRX_MPI_RX_TO_MASK	0xfff00000

#define AR9170_MAC_REG_BCN_ADDR			(AR9170_MAC_REG_BASE + 0xD84)
#define AR9170_MAC_REG_BCN_LENGTH		(AR9170_MAC_REG_BASE + 0xD88)
#define AR9170_MAC_REG_BCN_PLCP			(AR9170_MAC_REG_BASE + 0xD90)
#define AR9170_MAC_REG_BCN_CTRL			(AR9170_MAC_REG_BASE + 0xD94)
#define AR9170_MAC_REG_BCN_HT1			(AR9170_MAC_REG_BASE + 0xDA0)
#define AR9170_MAC_REG_BCN_HT2			(AR9170_MAC_REG_BASE + 0xDA4)


#define AR9170_PWR_REG_BASE			0x1D4000

#define AR9170_PWR_REG_CLOCK_SEL		(AR9170_PWR_REG_BASE + 0x008)
#define		AR9170_PWR_CLK_AHB_40MHZ	0
#define		AR9170_PWR_CLK_AHB_20_22MHZ	1
#define		AR9170_PWR_CLK_AHB_40_44MHZ	2
#define		AR9170_PWR_CLK_AHB_80_88MHZ	3
#define		AR9170_PWR_CLK_DAC_160_INV_DLY	0x70


/* put beacon here in memory */
#define AR9170_BEACON_BUFFER_ADDRESS		0x117900


struct ar9170_tx_control {
	__le16 length;
	__le16 mac_control;
	__le32 phy_control;
	u8 frame_data[0];
} __packed;

/* these are either-or */
#define AR9170_TX_MAC_PROT_RTS			0x0001
#define AR9170_TX_MAC_PROT_CTS			0x0002

#define AR9170_TX_MAC_NO_ACK			0x0004
/* if unset, MAC will only do SIFS space before frame */
#define AR9170_TX_MAC_BACKOFF			0x0008
#define AR9170_TX_MAC_BURST			0x0010
#define AR9170_TX_MAC_AGGR			0x0020

/* encryption is a two-bit field */
#define AR9170_TX_MAC_ENCR_NONE			0x0000
#define AR9170_TX_MAC_ENCR_RC4			0x0040
#define AR9170_TX_MAC_ENCR_CENC			0x0080
#define AR9170_TX_MAC_ENCR_AES			0x00c0

#define AR9170_TX_MAC_MMIC			0x0100
#define AR9170_TX_MAC_HW_DURATION		0x0200
#define AR9170_TX_MAC_QOS_SHIFT			10
#define AR9170_TX_MAC_QOS_MASK			(3 << AR9170_TX_MAC_QOS_SHIFT)
#define AR9170_TX_MAC_AGGR_QOS_BIT1		0x0400
#define AR9170_TX_MAC_AGGR_QOS_BIT2		0x0800
#define AR9170_TX_MAC_DISABLE_TXOP		0x1000
#define AR9170_TX_MAC_TXOP_RIFS			0x2000
#define AR9170_TX_MAC_IMM_AMPDU			0x4000
#define AR9170_TX_MAC_RATE_PROBE		0x8000

/* either-or */
#define AR9170_TX_PHY_MOD_CCK			0x00000000
#define AR9170_TX_PHY_MOD_OFDM			0x00000001
#define AR9170_TX_PHY_MOD_HT			0x00000002

/* depends on modulation */
#define AR9170_TX_PHY_SHORT_PREAMBLE		0x00000004
#define AR9170_TX_PHY_GREENFIELD		0x00000004

#define AR9170_TX_PHY_BW_SHIFT			3
#define AR9170_TX_PHY_BW_MASK			(3 << AR9170_TX_PHY_BW_SHIFT)
#define AR9170_TX_PHY_BW_20MHZ			0
#define AR9170_TX_PHY_BW_40MHZ			2
#define AR9170_TX_PHY_BW_40MHZ_DUP		3

#define AR9170_TX_PHY_TX_HEAVY_CLIP_SHIFT	6
#define AR9170_TX_PHY_TX_HEAVY_CLIP_MASK	(7 << AR9170_TX_PHY_TX_HEAVY_CLIP_SHIFT)

#define AR9170_TX_PHY_TX_PWR_SHIFT		9
#define AR9170_TX_PHY_TX_PWR_MASK		(0x3f << AR9170_TX_PHY_TX_PWR_SHIFT)

/* not part of the hw-spec */
#define AR9170_TX_PHY_QOS_SHIFT			25
#define AR9170_TX_PHY_QOS_MASK			(3 << AR9170_TX_PHY_QOS_SHIFT)

#define AR9170_TX_PHY_TXCHAIN_SHIFT		15
#define AR9170_TX_PHY_TXCHAIN_MASK		(7 << AR9170_TX_PHY_TXCHAIN_SHIFT)
#define AR9170_TX_PHY_TXCHAIN_1			1
/* use for cck, ofdm 6/9/12/18/24 and HT if capable */
#define AR9170_TX_PHY_TXCHAIN_2			5

#define AR9170_TX_PHY_MCS_SHIFT			18
#define AR9170_TX_PHY_MCS_MASK			(0x7f << AR9170_TX_PHY_MCS_SHIFT)

#define AR9170_TX_PHY_SHORT_GI			0x80000000

struct ar9170_rx_head {
	u8 plcp[12];
} __packed;

struct ar9170_rx_phystatus {
	union {
		struct {
			u8 rssi_ant0, rssi_ant1, rssi_ant2,
			   rssi_ant0x, rssi_ant1x, rssi_ant2x,
			   rssi_combined;
		} __packed;
		u8 rssi[7];
	} __packed;

	u8 evm_stream0[6], evm_stream1[6];
	u8 phy_err;
} __packed;

struct ar9170_rx_macstatus {
	u8 SAidx, DAidx;
	u8 error;
	u8 status;
} __packed;

#define AR9170_ENC_ALG_NONE			0x0
#define AR9170_ENC_ALG_WEP64			0x1
#define AR9170_ENC_ALG_TKIP			0x2
#define AR9170_ENC_ALG_AESCCMP			0x4
#define AR9170_ENC_ALG_WEP128			0x5
#define AR9170_ENC_ALG_WEP256			0x6
#define AR9170_ENC_ALG_CENC			0x7

#define AR9170_RX_ENC_SOFTWARE			0x8

static inline u8 ar9170_get_decrypt_type(struct ar9170_rx_macstatus *t)
{
	return (t->SAidx & 0xc0) >> 4 |
	       (t->DAidx & 0xc0) >> 6;
}

#define AR9170_RX_STATUS_MODULATION_MASK	0x03
#define AR9170_RX_STATUS_MODULATION_CCK		0x00
#define AR9170_RX_STATUS_MODULATION_OFDM	0x01
#define AR9170_RX_STATUS_MODULATION_HT		0x02
#define AR9170_RX_STATUS_MODULATION_DUPOFDM	0x03

/* depends on modulation */
#define AR9170_RX_STATUS_SHORT_PREAMBLE		0x08
#define AR9170_RX_STATUS_GREENFIELD		0x08

#define AR9170_RX_STATUS_MPDU_MASK		0x30
#define AR9170_RX_STATUS_MPDU_SINGLE		0x00
#define AR9170_RX_STATUS_MPDU_FIRST		0x20
#define AR9170_RX_STATUS_MPDU_MIDDLE		0x30
#define AR9170_RX_STATUS_MPDU_LAST		0x10

#define AR9170_RX_ERROR_RXTO			0x01
#define AR9170_RX_ERROR_OVERRUN			0x02
#define AR9170_RX_ERROR_DECRYPT			0x04
#define AR9170_RX_ERROR_FCS			0x08
#define AR9170_RX_ERROR_WRONG_RA		0x10
#define AR9170_RX_ERROR_PLCP			0x20
#define AR9170_RX_ERROR_MMIC			0x40
#define AR9170_RX_ERROR_FATAL			0x80

struct ar9170_cmd_tx_status {
	__le16 unkn;
	u8 dst[ETH_ALEN];
	__le32 rate;
	__le16 status;
} __packed;

#define AR9170_TX_STATUS_COMPLETE		0x00
#define AR9170_TX_STATUS_RETRY			0x01
#define AR9170_TX_STATUS_FAILED			0x02

struct ar9170_cmd_ba_failed_count {
	__le16 failed;
	__le16 rate;
} __packed;

struct ar9170_cmd_response {
	u8 flag;
	u8 type;

	union {
		struct ar9170_cmd_tx_status		tx_status;
		struct ar9170_cmd_ba_failed_count	ba_fail_cnt;
		u8 data[0];
	};
} __packed;

/* QoS */

/* mac80211 queue to HW/FW map */
static const u8 ar9170_qos_hwmap[4] = { 3, 2, 0, 1 };

/* HW/FW queue to mac80211 map */
static const u8 ar9170_qos_mac80211map[4] = { 2, 3, 1, 0 };

enum ar9170_txq {
	AR9170_TXQ_BE,
	AR9170_TXQ_BK,
	AR9170_TXQ_VI,
	AR9170_TXQ_VO,

	__AR9170_NUM_TXQ,
};

#endif /* __AR9170_HW_H */
