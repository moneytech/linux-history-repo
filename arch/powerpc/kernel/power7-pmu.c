/*
 * Performance counter support for POWER7 processors.
 *
 * Copyright 2009 Paul Mackerras, IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/perf_counter.h>
#include <asm/reg.h>

/*
 * Bits in event code for POWER7
 */
#define PM_PMC_SH	16	/* PMC number (1-based) for direct events */
#define PM_PMC_MSK	0xf
#define PM_PMC_MSKS	(PM_PMC_MSK << PM_PMC_SH)
#define PM_UNIT_SH	12	/* TTMMUX number and setting - unit select */
#define PM_UNIT_MSK	0xf
#define PM_COMBINE_SH	11	/* Combined event bit */
#define PM_COMBINE_MSK	1
#define PM_COMBINE_MSKS	0x800
#define PM_L2SEL_SH	8	/* L2 event select */
#define PM_L2SEL_MSK	7
#define PM_PMCSEL_MSK	0xff

/*
 * Bits in MMCR1 for POWER7
 */
#define MMCR1_TTM0SEL_SH	60
#define MMCR1_TTM1SEL_SH	56
#define MMCR1_TTM2SEL_SH	52
#define MMCR1_TTM3SEL_SH	48
#define MMCR1_TTMSEL_MSK	0xf
#define MMCR1_L2SEL_SH		45
#define MMCR1_L2SEL_MSK		7
#define MMCR1_PMC1_COMBINE_SH	35
#define MMCR1_PMC2_COMBINE_SH	34
#define MMCR1_PMC3_COMBINE_SH	33
#define MMCR1_PMC4_COMBINE_SH	32
#define MMCR1_PMC1SEL_SH	24
#define MMCR1_PMC2SEL_SH	16
#define MMCR1_PMC3SEL_SH	8
#define MMCR1_PMC4SEL_SH	0
#define MMCR1_PMCSEL_SH(n)	(MMCR1_PMC1SEL_SH - (n) * 8)
#define MMCR1_PMCSEL_MSK	0xff

/*
 * Bits in MMCRA
 */

/*
 * Layout of constraint bits:
 * 6666555555555544444444443333333333222222222211111111110000000000
 * 3210987654321098765432109876543210987654321098765432109876543210
 *                                                 [  ><><><><><><>
 *                                                  NC P6P5P4P3P2P1
 *
 * NC - number of counters
 *     15: NC error 0x8000
 *     12-14: number of events needing PMC1-4 0x7000
 *
 * P6
 *     11: P6 error 0x800
 *     10-11: Count of events needing PMC6
 *
 * P1..P5
 *     0-9: Count of events needing PMC1..PMC5
 */

static int power7_get_constraint(u64 event, u64 *maskp, u64 *valp)
{
	int pmc, sh;
	u64 mask = 0, value = 0;

	pmc = (event >> PM_PMC_SH) & PM_PMC_MSK;
	if (pmc) {
		if (pmc > 6)
			return -1;
		sh = (pmc - 1) * 2;
		mask |= 2 << sh;
		value |= 1 << sh;
		if (pmc >= 5 && !(event == 0x500fa || event == 0x600f4))
			return -1;
	}
	if (pmc < 5) {
		/* need a counter from PMC1-4 set */
		mask  |= 0x8000;
		value |= 0x1000;
	}
	*maskp = mask;
	*valp = value;
	return 0;
}

#define MAX_ALT	2	/* at most 2 alternatives for any event */

static const unsigned int event_alternatives[][MAX_ALT] = {
	{ 0x200f2, 0x300f2 },		/* PM_INST_DISP */
	{ 0x200f4, 0x600f4 },		/* PM_RUN_CYC */
	{ 0x400fa, 0x500fa },		/* PM_RUN_INST_CMPL */
};

/*
 * Scan the alternatives table for a match and return the
 * index into the alternatives table if found, else -1.
 */
static int find_alternative(u64 event)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(event_alternatives); ++i) {
		if (event < event_alternatives[i][0])
			break;
		for (j = 0; j < MAX_ALT && event_alternatives[i][j]; ++j)
			if (event == event_alternatives[i][j])
				return i;
	}
	return -1;
}

static s64 find_alternative_decode(u64 event)
{
	int pmc, psel;

	/* this only handles the 4x decode events */
	pmc = (event >> PM_PMC_SH) & PM_PMC_MSK;
	psel = event & PM_PMCSEL_MSK;
	if ((pmc == 2 || pmc == 4) && (psel & ~7) == 0x40)
		return event - (1 << PM_PMC_SH) + 8;
	if ((pmc == 1 || pmc == 3) && (psel & ~7) == 0x48)
		return event + (1 << PM_PMC_SH) - 8;
	return -1;
}

static int power7_get_alternatives(u64 event, unsigned int flags, u64 alt[])
{
	int i, j, nalt = 1;
	s64 ae;

	alt[0] = event;
	nalt = 1;
	i = find_alternative(event);
	if (i >= 0) {
		for (j = 0; j < MAX_ALT; ++j) {
			ae = event_alternatives[i][j];
			if (ae && ae != event)
				alt[nalt++] = ae;
		}
	} else {
		ae = find_alternative_decode(event);
		if (ae > 0)
			alt[nalt++] = ae;
	}

	if (flags & PPMU_ONLY_COUNT_RUN) {
		/*
		 * We're only counting in RUN state,
		 * so PM_CYC is equivalent to PM_RUN_CYC
		 * and PM_INST_CMPL === PM_RUN_INST_CMPL.
		 * This doesn't include alternatives that don't provide
		 * any extra flexibility in assigning PMCs.
		 */
		j = nalt;
		for (i = 0; i < nalt; ++i) {
			switch (alt[i]) {
			case 0x1e:	/* PM_CYC */
				alt[j++] = 0x600f4;	/* PM_RUN_CYC */
				break;
			case 0x600f4:	/* PM_RUN_CYC */
				alt[j++] = 0x1e;
				break;
			case 0x2:	/* PM_PPC_CMPL */
				alt[j++] = 0x500fa;	/* PM_RUN_INST_CMPL */
				break;
			case 0x500fa:	/* PM_RUN_INST_CMPL */
				alt[j++] = 0x2;	/* PM_PPC_CMPL */
				break;
			}
		}
		nalt = j;
	}

	return nalt;
}

/*
 * Returns 1 if event counts things relating to marked instructions
 * and thus needs the MMCRA_SAMPLE_ENABLE bit set, or 0 if not.
 */
static int power7_marked_instr_event(u64 event)
{
	int pmc, psel;
	int unit;

	pmc = (event >> PM_PMC_SH) & PM_PMC_MSK;
	unit = (event >> PM_UNIT_SH) & PM_UNIT_MSK;
	psel = event & PM_PMCSEL_MSK & ~1;	/* trim off edge/level bit */
	if (pmc >= 5)
		return 0;

	switch (psel >> 4) {
	case 2:
		return pmc == 2 || pmc == 4;
	case 3:
		if (psel == 0x3c)
			return pmc == 1;
		if (psel == 0x3e)
			return pmc != 2;
		return 1;
	case 4:
	case 5:
		return unit == 0xd;
	case 6:
		if (psel == 0x64)
			return pmc >= 3;
	case 8:
		return unit == 0xd;
	}
	return 0;
}

static int power7_compute_mmcr(u64 event[], int n_ev,
			       unsigned int hwc[], u64 mmcr[])
{
	u64 mmcr1 = 0;
	u64 mmcra = 0;
	unsigned int pmc, unit, combine, l2sel, psel;
	unsigned int pmc_inuse = 0;
	int i;

	/* First pass to count resource use */
	for (i = 0; i < n_ev; ++i) {
		pmc = (event[i] >> PM_PMC_SH) & PM_PMC_MSK;
		if (pmc) {
			if (pmc > 6)
				return -1;
			if (pmc_inuse & (1 << (pmc - 1)))
				return -1;
			pmc_inuse |= 1 << (pmc - 1);
		}
	}

	/* Second pass: assign PMCs, set all MMCR1 fields */
	for (i = 0; i < n_ev; ++i) {
		pmc = (event[i] >> PM_PMC_SH) & PM_PMC_MSK;
		unit = (event[i] >> PM_UNIT_SH) & PM_UNIT_MSK;
		combine = (event[i] >> PM_COMBINE_SH) & PM_COMBINE_MSK;
		l2sel = (event[i] >> PM_L2SEL_SH) & PM_L2SEL_MSK;
		psel = event[i] & PM_PMCSEL_MSK;
		if (!pmc) {
			/* Bus event or any-PMC direct event */
			for (pmc = 0; pmc < 4; ++pmc) {
				if (!(pmc_inuse & (1 << pmc)))
					break;
			}
			if (pmc >= 4)
				return -1;
			pmc_inuse |= 1 << pmc;
		} else {
			/* Direct or decoded event */
			--pmc;
		}
		if (pmc <= 3) {
			mmcr1 |= (u64) unit << (MMCR1_TTM0SEL_SH - 4 * pmc);
			mmcr1 |= (u64) combine << (MMCR1_PMC1_COMBINE_SH - pmc);
			mmcr1 |= psel << MMCR1_PMCSEL_SH(pmc);
			if (unit == 6)	/* L2 events */
				mmcr1 |= (u64) l2sel << MMCR1_L2SEL_SH;
		}
		if (power7_marked_instr_event(event[i]))
			mmcra |= MMCRA_SAMPLE_ENABLE;
		hwc[i] = pmc;
	}

	/* Return MMCRx values */
	mmcr[0] = 0;
	if (pmc_inuse & 1)
		mmcr[0] = MMCR0_PMC1CE;
	if (pmc_inuse & 0x3e)
		mmcr[0] |= MMCR0_PMCjCE;
	mmcr[1] = mmcr1;
	mmcr[2] = mmcra;
	return 0;
}

static void power7_disable_pmc(unsigned int pmc, u64 mmcr[])
{
	if (pmc <= 3)
		mmcr[1] &= ~(0xffULL << MMCR1_PMCSEL_SH(pmc));
}

static int power7_generic_events[] = {
	[PERF_COUNT_CPU_CYCLES] = 0x1e,
	[PERF_COUNT_INSTRUCTIONS] = 2,
	[PERF_COUNT_CACHE_REFERENCES] = 0xc880,		/* LD_REF_L1_LSU */
	[PERF_COUNT_CACHE_MISSES] = 0x400f0,		/* LD_MISS_L1 */
	[PERF_COUNT_BRANCH_INSTRUCTIONS] = 0x10068,	/* BRU_FIN */
	[PERF_COUNT_BRANCH_MISSES] = 0x400f6,		/* BR_MPRED */
};

#define C(x)	PERF_COUNT_HW_CACHE_##x

/*
 * Table of generalized cache-related events.
 * 0 means not supported, -1 means nonsensical, other values
 * are event codes.
 */
static int power7_cache_events[C(MAX)][C(OP_MAX)][C(RESULT_MAX)] = {
	[C(L1D)] = {		/* 	RESULT_ACCESS	RESULT_MISS */
		[C(OP_READ)] = {	0x400f0,	0xc880	},
		[C(OP_WRITE)] = {	0,		0x300f0	},
		[C(OP_PREFETCH)] = {	0xd8b8,		0	},
	},
	[C(L1I)] = {		/* 	RESULT_ACCESS	RESULT_MISS */
		[C(OP_READ)] = {	0,		0x200fc	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	0x408a,		0	},
	},
	[C(L2)] = {		/* 	RESULT_ACCESS	RESULT_MISS */
		[C(OP_READ)] = {	0x6080,		0x6084	},
		[C(OP_WRITE)] = {	0x6082,		0x6086	},
		[C(OP_PREFETCH)] = {	0,		0	},
	},
	[C(DTLB)] = {		/* 	RESULT_ACCESS	RESULT_MISS */
		[C(OP_READ)] = {	0,		0x300fc	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
	[C(ITLB)] = {		/* 	RESULT_ACCESS	RESULT_MISS */
		[C(OP_READ)] = {	0,		0x400fc	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
	[C(BPU)] = {		/* 	RESULT_ACCESS	RESULT_MISS */
		[C(OP_READ)] = {	0x10068,	0x400f6	},
		[C(OP_WRITE)] = {	-1,		-1	},
		[C(OP_PREFETCH)] = {	-1,		-1	},
	},
};

struct power_pmu power7_pmu = {
	.n_counter = 6,
	.max_alternatives = MAX_ALT + 1,
	.add_fields = 0x1555ull,
	.test_adder = 0x3000ull,
	.compute_mmcr = power7_compute_mmcr,
	.get_constraint = power7_get_constraint,
	.get_alternatives = power7_get_alternatives,
	.disable_pmc = power7_disable_pmc,
	.n_generic = ARRAY_SIZE(power7_generic_events),
	.generic_events = power7_generic_events,
	.cache_events = &power7_cache_events,
};
