#ifndef _UDA134X_CODEC_H
#define _UDA134X_CODEC_H

#define UDA134X_L3ADDR	5
#define UDA134X_DATA0_ADDR	((UDA134X_L3ADDR << 2) | 0)
#define UDA134X_DATA1_ADDR	((UDA134X_L3ADDR << 2) | 1)
#define UDA134X_STATUS_ADDR	((UDA134X_L3ADDR << 2) | 2)

#define UDA134X_EXTADDR_PREFIX	0xC0
#define UDA134X_EXTDATA_PREFIX	0xE0

/* UDA134X registers */
#define UDA134X_EA000	0
#define UDA134X_EA001	1
#define UDA134X_EA010	2
#define UDA134X_EA011	3
#define UDA134X_EA100	4
#define UDA134X_EA101	5
#define UDA134X_EA110	6
#define UDA134X_EA111	7
#define UDA134X_STATUS0 8
#define UDA134X_STATUS1 9
#define UDA134X_DATA000 10
#define UDA134X_DATA001 11
#define UDA134X_DATA010 12
#define UDA134X_DATA1	13

#define UDA134X_REGS_NUM 14

#define STATUS0_DAIFMT_MASK (~(7<<1))
#define STATUS0_SYSCLK_MASK (~(3<<4))

extern struct snd_soc_dai uda134x_dai;
extern struct snd_soc_codec_device soc_codec_dev_uda134x;

#endif
