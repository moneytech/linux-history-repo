/*
 *  longhaul.h
 *  (C) 2003 Dave Jones.
 *
 *  Licensed under the terms of the GNU GPL License version 2.
 *
 *  VIA-specific information
 */

union msr_bcr2 {
	struct {
		unsigned Reseved:19,	// 18:0
		ESOFTBF:1,		// 19
		Reserved2:3,		// 22:20
		CLOCKMUL:4,		// 26:23
		Reserved3:5;		// 31:27
	} bits;
	unsigned long val;
};

union msr_longhaul {
	struct {
		unsigned RevisionID:4,	// 3:0
		RevisionKey:4,		// 7:4
		EnableSoftBusRatio:1,	// 8
		EnableSoftVID:1,	// 9
		EnableSoftBSEL:1,	// 10
		Reserved:3,		// 11:13
		SoftBusRatio4:1,	// 14
		VRMRev:1,		// 15
		SoftBusRatio:4,		// 19:16
		SoftVID:5,		// 24:20
		Reserved2:3,		// 27:25
		SoftBSEL:2,		// 29:28
		Reserved3:2,		// 31:30
		MaxMHzBR:4,		// 35:32
		MaximumVID:5,		// 40:36
		MaxMHzFSB:2,		// 42:41
		MaxMHzBR4:1,		// 43
		Reserved4:4,		// 47:44
		MinMHzBR:4,		// 51:48
		MinimumVID:5,		// 56:52
		MinMHzFSB:2,		// 58:57
		MinMHzBR4:1,		// 59
		Reserved5:4;		// 63:60
	} bits;
	unsigned long long val;
};

/*
 * Clock ratio tables. Div/Mod by 10 to get ratio.
 * The eblcr ones specify the ratio read from the CPU.
 * The clock_ratio ones specify what to write to the CPU.
 */

/*
 * VIA C3 Samuel 1  & Samuel 2 (stepping 0)
 */
static int __initdata samuel1_clock_ratio[16] = {
	-1, /* 0000 -> RESERVED */
	30, /* 0001 ->  3.0x */
	40, /* 0010 ->  4.0x */
	-1, /* 0011 -> RESERVED */
	-1, /* 0100 -> RESERVED */
	35, /* 0101 ->  3.5x */
	45, /* 0110 ->  4.5x */
	55, /* 0111 ->  5.5x */
	60, /* 1000 ->  6.0x */
	70, /* 1001 ->  7.0x */
	80, /* 1010 ->  8.0x */
	50, /* 1011 ->  5.0x */
	65, /* 1100 ->  6.5x */
	75, /* 1101 ->  7.5x */
	-1, /* 1110 -> RESERVED */
	-1, /* 1111 -> RESERVED */
};

static int __initdata samuel1_eblcr[16] = {
	50, /* 0000 -> RESERVED */
	30, /* 0001 ->  3.0x */
	40, /* 0010 ->  4.0x */
	-1, /* 0011 -> RESERVED */
	55, /* 0100 ->  5.5x */
	35, /* 0101 ->  3.5x */
	45, /* 0110 ->  4.5x */
	-1, /* 0111 -> RESERVED */
	-1, /* 1000 -> RESERVED */
	70, /* 1001 ->  7.0x */
	80, /* 1010 ->  8.0x */
	60, /* 1011 ->  6.0x */
	-1, /* 1100 -> RESERVED */
	75, /* 1101 ->  7.5x */
	-1, /* 1110 -> RESERVED */
	65, /* 1111 ->  6.5x */
};

/*
 * VIA C3 Samuel2 Stepping 1->15
 */
static int __initdata samuel2_eblcr[16] = {
	50,  /* 0000 ->  5.0x */
	30,  /* 0001 ->  3.0x */
	40,  /* 0010 ->  4.0x */
	100, /* 0011 -> 10.0x */
	55,  /* 0100 ->  5.5x */
	35,  /* 0101 ->  3.5x */
	45,  /* 0110 ->  4.5x */
	110, /* 0111 -> 11.0x */
	90,  /* 1000 ->  9.0x */
	70,  /* 1001 ->  7.0x */
	80,  /* 1010 ->  8.0x */
	60,  /* 1011 ->  6.0x */
	120, /* 1100 -> 12.0x */
	75,  /* 1101 ->  7.5x */
	130, /* 1110 -> 13.0x */
	65,  /* 1111 ->  6.5x */
};

/*
 * VIA C3 Ezra
 */
static int __initdata ezra_clock_ratio[16] = {
	100, /* 0000 -> 10.0x */
	30,  /* 0001 ->  3.0x */
	40,  /* 0010 ->  4.0x */
	90,  /* 0011 ->  9.0x */
	95,  /* 0100 ->  9.5x */
	35,  /* 0101 ->  3.5x */
	45,  /* 0110 ->  4.5x */
	55,  /* 0111 ->  5.5x */
	60,  /* 1000 ->  6.0x */
	70,  /* 1001 ->  7.0x */
	80,  /* 1010 ->  8.0x */
	50,  /* 1011 ->  5.0x */
	65,  /* 1100 ->  6.5x */
	75,  /* 1101 ->  7.5x */
	85,  /* 1110 ->  8.5x */
	120, /* 1111 -> 12.0x */
};

static int __initdata ezra_eblcr[16] = {
	50,  /* 0000 ->  5.0x */
	30,  /* 0001 ->  3.0x */
	40,  /* 0010 ->  4.0x */
	100, /* 0011 -> 10.0x */
	55,  /* 0100 ->  5.5x */
	35,  /* 0101 ->  3.5x */
	45,  /* 0110 ->  4.5x */
	95,  /* 0111 ->  9.5x */
	90,  /* 1000 ->  9.0x */
	70,  /* 1001 ->  7.0x */
	80,  /* 1010 ->  8.0x */
	60,  /* 1011 ->  6.0x */
	120, /* 1100 -> 12.0x */
	75,  /* 1101 ->  7.5x */
	85,  /* 1110 ->  8.5x */
	65,  /* 1111 ->  6.5x */
};

/*
 * VIA C3 (Ezra-T) [C5M].
 */
static int __initdata ezrat_clock_ratio[32] = {
	100, /* 0000 -> 10.0x */
	30,  /* 0001 ->  3.0x */
	40,  /* 0010 ->  4.0x */
	90,  /* 0011 ->  9.0x */
	95,  /* 0100 ->  9.5x */
	35,  /* 0101 ->  3.5x */
	45,  /* 0110 ->  4.5x */
	55,  /* 0111 ->  5.5x */
	60,  /* 1000 ->  6.0x */
	70,  /* 1001 ->  7.0x */
	80,  /* 1010 ->  8.0x */
	50,  /* 1011 ->  5.0x */
	65,  /* 1100 ->  6.5x */
	75,  /* 1101 ->  7.5x */
	85,  /* 1110 ->  8.5x */
	120, /* 1111 ->  12.0x */

	-1,  /* 0000 -> RESERVED (10.0x) */
	110, /* 0001 -> 11.0x */
	120, /* 0010 -> 12.0x */
	-1,  /* 0011 -> RESERVED (9.0x)*/
	105, /* 0100 -> 10.5x */
	115, /* 0101 -> 11.5x */
	125, /* 0110 -> 12.5x */
	135, /* 0111 -> 13.5x */
	140, /* 1000 -> 14.0x */
	150, /* 1001 -> 15.0x */
	160, /* 1010 -> 16.0x */
	130, /* 1011 -> 13.0x */
	145, /* 1100 -> 14.5x */
	155, /* 1101 -> 15.5x */
	-1,  /* 1110 -> RESERVED (13.0x) */
	-1,  /* 1111 -> RESERVED (12.0x) */
};

static int __initdata ezrat_eblcr[32] = {
	50,  /* 0000 ->  5.0x */
	30,  /* 0001 ->  3.0x */
	40,  /* 0010 ->  4.0x */
	100, /* 0011 -> 10.0x */
	55,  /* 0100 ->  5.5x */
	35,  /* 0101 ->  3.5x */
	45,  /* 0110 ->  4.5x */
	95,  /* 0111 ->  9.5x */
	90,  /* 1000 ->  9.0x */
	70,  /* 1001 ->  7.0x */
	80,  /* 1010 ->  8.0x */
	60,  /* 1011 ->  6.0x */
	120, /* 1100 -> 12.0x */
	75,  /* 1101 ->  7.5x */
	85,  /* 1110 ->  8.5x */
	65,  /* 1111 ->  6.5x */

	-1,  /* 0000 -> RESERVED (9.0x) */
	110, /* 0001 -> 11.0x */
	120, /* 0010 -> 12.0x */
	-1,  /* 0011 -> RESERVED (10.0x)*/
	135, /* 0100 -> 13.5x */
	115, /* 0101 -> 11.5x */
	125, /* 0110 -> 12.5x */
	105, /* 0111 -> 10.5x */
	130, /* 1000 -> 13.0x */
	150, /* 1001 -> 15.0x */
	160, /* 1010 -> 16.0x */
	140, /* 1011 -> 14.0x */
	-1,  /* 1100 -> RESERVED (12.0x) */
	155, /* 1101 -> 15.5x */
	-1,  /* 1110 -> RESERVED (13.0x) */
	145, /* 1111 -> 14.5x */
};

/*
 * VIA C3 Nehemiah */

static int __initdata  nehemiah_clock_ratio[32] = {
	100, /* 0000 -> 10.0x */
	160, /* 0001 -> 16.0x */
	40,  /* 0010 ->  4.0x */
	90,  /* 0011 ->  9.0x */
	95,  /* 0100 ->  9.5x */
	-1,  /* 0101 ->  RESERVED */
	45,  /* 0110 ->  4.5x */
	55,  /* 0111 ->  5.5x */
	60,  /* 1000 ->  6.0x */
	70,  /* 1001 ->  7.0x */
	80,  /* 1010 ->  8.0x */
	50,  /* 1011 ->  5.0x */
	65,  /* 1100 ->  6.5x */
	75,  /* 1101 ->  7.5x */
	85,  /* 1110 ->  8.5x */
	120, /* 1111 -> 12.0x */
	100, /* 0000 -> 10.0x */
	110, /* 0001 -> 11.0x */
	120, /* 0010 -> 12.0x */
	90,  /* 0011 ->  9.0x */
	105, /* 0100 -> 10.5x */
	115, /* 0101 -> 11.5x */
	125, /* 0110 -> 12.5x */
	135, /* 0111 -> 13.5x */
	140, /* 1000 -> 14.0x */
	150, /* 1001 -> 15.0x */
	160, /* 1010 -> 16.0x */
	130, /* 1011 -> 13.0x */
	145, /* 1100 -> 14.5x */
	155, /* 1101 -> 15.5x */
	-1,  /* 1110 -> RESERVED (13.0x) */
	120, /* 1111 -> 12.0x */
};

static int __initdata nehemiah_eblcr[32] = {
	50,  /* 0000 ->  5.0x */
	160, /* 0001 -> 16.0x */
	40,  /* 0010 ->  4.0x */
	100, /* 0011 -> 10.0x */
	55,  /* 0100 ->  5.5x */
	-1,  /* 0101 ->  RESERVED */
	45,  /* 0110 ->  4.5x */
	95,  /* 0111 ->  9.5x */
	90,  /* 1000 ->  9.0x */
	70,  /* 1001 ->  7.0x */
	80,  /* 1010 ->  8.0x */
	60,  /* 1011 ->  6.0x */
	120, /* 1100 -> 12.0x */
	75,  /* 1101 ->  7.5x */
	85,  /* 1110 ->  8.5x */
	65,  /* 1111 ->  6.5x */
	90,  /* 0000 ->  9.0x */
	110, /* 0001 -> 11.0x */
	120, /* 0010 -> 12.0x */
	100, /* 0011 -> 10.0x */
	135, /* 0100 -> 13.5x */
	115, /* 0101 -> 11.5x */
	125, /* 0110 -> 12.5x */
	105, /* 0111 -> 10.5x */
	130, /* 1000 -> 13.0x */
	150, /* 1001 -> 15.0x */
	160, /* 1010 -> 16.0x */
	140, /* 1011 -> 14.0x */
	120, /* 1100 -> 12.0x */
	155, /* 1101 -> 15.5x */
	-1,  /* 1110 -> RESERVED (13.0x) */
	145 /* 1111 -> 14.5x */
};

/*
 * Voltage scales. Div/Mod by 1000 to get actual voltage.
 * Which scale to use depends on the VRM type in use.
 */

struct mV_pos {
	unsigned short mV;
	unsigned short pos;
};

static struct mV_pos __initdata vrm85_mV[32] = {
	{1250, 8},	{1200, 6},	{1150, 4},	{1100, 2},
	{1050, 0},	{1800, 30},	{1750, 28},	{1700, 26},
	{1650, 24},	{1600, 22},	{1550, 20},	{1500, 18},
	{1450, 16},	{1400, 14},	{1350, 12},	{1300, 10},
	{1275, 9},	{1225, 7},	{1175, 5},	{1125, 3},
	{1075, 1},	{1825, 31},	{1775, 29},	{1725, 27},
	{1675, 25},	{1625, 23},	{1575, 21},	{1525, 19},
	{1475, 17},	{1425, 15},	{1375, 13},	{1325, 11}
};

static unsigned char __initdata mV_vrm85[32] = {
	0x04,	0x14,	0x03,	0x13,	0x02,	0x12,	0x01,	0x11,
	0x00,	0x10,	0x0f,	0x1f,	0x0e,	0x1e,	0x0d,	0x1d,
	0x0c,	0x1c,	0x0b,	0x1b,	0x0a,	0x1a,	0x09,	0x19,
	0x08,	0x18,	0x07,	0x17,	0x06,	0x16,	0x05,	0x15
};

static struct mV_pos __initdata mobilevrm_mV[32] = {
	{1750, 31},	{1700, 30},	{1650, 29},	{1600, 28},
	{1550, 27},	{1500, 26},	{1450, 25},	{1400, 24},
	{1350, 23},	{1300, 22},	{1250, 21},	{1200, 20},
	{1150, 19},	{1100, 18},	{1050, 17},	{1000, 16},
	{975, 15},	{950, 14},	{925, 13},	{900, 12},
	{875, 11},	{850, 10},	{825, 9},	{800, 8},
	{775, 7},	{750, 6},	{725, 5},	{700, 4},
	{675, 3},	{650, 2},	{625, 1},	{600, 0}
};

static unsigned char __initdata mV_mobilevrm[32] = {
	0x1f,	0x1e,	0x1d,	0x1c,	0x1b,	0x1a,	0x19,	0x18,
	0x17,	0x16,	0x15,	0x14,	0x13,	0x12,	0x11,	0x10,
	0x0f,	0x0e,	0x0d,	0x0c,	0x0b,	0x0a,	0x09,	0x08,
	0x07,	0x06,	0x05,	0x04,	0x03,	0x02,	0x01,	0x00
};

