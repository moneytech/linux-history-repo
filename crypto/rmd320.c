/*
 * Cryptographic API.
 *
 * RIPEMD-320 - RACE Integrity Primitives Evaluation Message Digest.
 *
 * Based on the reference implementation by Antoon Bosselaers, ESAT-COSIC
 *
 * Copyright (c) 2008 Adrian-Ken Rueegsegger <rueegsegger (at) swiss-it.ch>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <linux/types.h>
#include <asm/byteorder.h>

#include "ripemd.h"

struct rmd320_ctx {
	u64 byte_count;
	u32 state[10];
	u32 buffer[16];
};

#define K1  RMD_K1
#define K2  RMD_K2
#define K3  RMD_K3
#define K4  RMD_K4
#define K5  RMD_K5
#define KK1 RMD_K6
#define KK2 RMD_K7
#define KK3 RMD_K8
#define KK4 RMD_K9
#define KK5 RMD_K1

#define F1(x, y, z) (x ^ y ^ z)		/* XOR */
#define F2(x, y, z) (z ^ (x & (y ^ z)))	/* x ? y : z */
#define F3(x, y, z) ((x | ~y) ^ z)
#define F4(x, y, z) (y ^ (z & (x ^ y)))	/* z ? x : y */
#define F5(x, y, z) (x ^ (y | ~z))

#define ROUND(a, b, c, d, e, f, k, x, s)  { \
	(a) += f((b), (c), (d)) + le32_to_cpu(x) + (k); \
	(a) = rol32((a), (s)) + (e); \
	(c) = rol32((c), 10); \
}

static void rmd320_transform(u32 *state, u32 const *in)
{
	u32 aa, bb, cc, dd, ee, aaa, bbb, ccc, ddd, eee, tmp;

	/* Initialize left lane */
	aa = state[0];
	bb = state[1];
	cc = state[2];
	dd = state[3];
	ee = state[4];

	/* Initialize right lane */
	aaa = state[5];
	bbb = state[6];
	ccc = state[7];
	ddd = state[8];
	eee = state[9];

	/* round 1: left lane */
	ROUND(aa, bb, cc, dd, ee, F1, K1, in[0],  11);
	ROUND(ee, aa, bb, cc, dd, F1, K1, in[1],  14);
	ROUND(dd, ee, aa, bb, cc, F1, K1, in[2],  15);
	ROUND(cc, dd, ee, aa, bb, F1, K1, in[3],  12);
	ROUND(bb, cc, dd, ee, aa, F1, K1, in[4],   5);
	ROUND(aa, bb, cc, dd, ee, F1, K1, in[5],   8);
	ROUND(ee, aa, bb, cc, dd, F1, K1, in[6],   7);
	ROUND(dd, ee, aa, bb, cc, F1, K1, in[7],   9);
	ROUND(cc, dd, ee, aa, bb, F1, K1, in[8],  11);
	ROUND(bb, cc, dd, ee, aa, F1, K1, in[9],  13);
	ROUND(aa, bb, cc, dd, ee, F1, K1, in[10], 14);
	ROUND(ee, aa, bb, cc, dd, F1, K1, in[11], 15);
	ROUND(dd, ee, aa, bb, cc, F1, K1, in[12],  6);
	ROUND(cc, dd, ee, aa, bb, F1, K1, in[13],  7);
	ROUND(bb, cc, dd, ee, aa, F1, K1, in[14],  9);
	ROUND(aa, bb, cc, dd, ee, F1, K1, in[15],  8);

	/* round 1: right lane */
	ROUND(aaa, bbb, ccc, ddd, eee, F5, KK1, in[5],   8);
	ROUND(eee, aaa, bbb, ccc, ddd, F5, KK1, in[14],  9);
	ROUND(ddd, eee, aaa, bbb, ccc, F5, KK1, in[7],   9);
	ROUND(ccc, ddd, eee, aaa, bbb, F5, KK1, in[0],  11);
	ROUND(bbb, ccc, ddd, eee, aaa, F5, KK1, in[9],  13);
	ROUND(aaa, bbb, ccc, ddd, eee, F5, KK1, in[2],  15);
	ROUND(eee, aaa, bbb, ccc, ddd, F5, KK1, in[11], 15);
	ROUND(ddd, eee, aaa, bbb, ccc, F5, KK1, in[4],   5);
	ROUND(ccc, ddd, eee, aaa, bbb, F5, KK1, in[13],  7);
	ROUND(bbb, ccc, ddd, eee, aaa, F5, KK1, in[6],   7);
	ROUND(aaa, bbb, ccc, ddd, eee, F5, KK1, in[15],  8);
	ROUND(eee, aaa, bbb, ccc, ddd, F5, KK1, in[8],  11);
	ROUND(ddd, eee, aaa, bbb, ccc, F5, KK1, in[1],  14);
	ROUND(ccc, ddd, eee, aaa, bbb, F5, KK1, in[10], 14);
	ROUND(bbb, ccc, ddd, eee, aaa, F5, KK1, in[3],  12);
	ROUND(aaa, bbb, ccc, ddd, eee, F5, KK1, in[12],  6);

	/* Swap contents of "a" registers */
	tmp = aa; aa = aaa; aaa = tmp;

	/* round 2: left lane" */
	ROUND(ee, aa, bb, cc, dd, F2, K2, in[7],   7);
	ROUND(dd, ee, aa, bb, cc, F2, K2, in[4],   6);
	ROUND(cc, dd, ee, aa, bb, F2, K2, in[13],  8);
	ROUND(bb, cc, dd, ee, aa, F2, K2, in[1],  13);
	ROUND(aa, bb, cc, dd, ee, F2, K2, in[10], 11);
	ROUND(ee, aa, bb, cc, dd, F2, K2, in[6],   9);
	ROUND(dd, ee, aa, bb, cc, F2, K2, in[15],  7);
	ROUND(cc, dd, ee, aa, bb, F2, K2, in[3],  15);
	ROUND(bb, cc, dd, ee, aa, F2, K2, in[12],  7);
	ROUND(aa, bb, cc, dd, ee, F2, K2, in[0],  12);
	ROUND(ee, aa, bb, cc, dd, F2, K2, in[9],  15);
	ROUND(dd, ee, aa, bb, cc, F2, K2, in[5],   9);
	ROUND(cc, dd, ee, aa, bb, F2, K2, in[2],  11);
	ROUND(bb, cc, dd, ee, aa, F2, K2, in[14],  7);
	ROUND(aa, bb, cc, dd, ee, F2, K2, in[11], 13);
	ROUND(ee, aa, bb, cc, dd, F2, K2, in[8],  12);

	/* round 2: right lane */
	ROUND(eee, aaa, bbb, ccc, ddd, F4, KK2, in[6],   9);
	ROUND(ddd, eee, aaa, bbb, ccc, F4, KK2, in[11], 13);
	ROUND(ccc, ddd, eee, aaa, bbb, F4, KK2, in[3],  15);
	ROUND(bbb, ccc, ddd, eee, aaa, F4, KK2, in[7],   7);
	ROUND(aaa, bbb, ccc, ddd, eee, F4, KK2, in[0],  12);
	ROUND(eee, aaa, bbb, ccc, ddd, F4, KK2, in[13],  8);
	ROUND(ddd, eee, aaa, bbb, ccc, F4, KK2, in[5],   9);
	ROUND(ccc, ddd, eee, aaa, bbb, F4, KK2, in[10], 11);
	ROUND(bbb, ccc, ddd, eee, aaa, F4, KK2, in[14],  7);
	ROUND(aaa, bbb, ccc, ddd, eee, F4, KK2, in[15],  7);
	ROUND(eee, aaa, bbb, ccc, ddd, F4, KK2, in[8],  12);
	ROUND(ddd, eee, aaa, bbb, ccc, F4, KK2, in[12],  7);
	ROUND(ccc, ddd, eee, aaa, bbb, F4, KK2, in[4],   6);
	ROUND(bbb, ccc, ddd, eee, aaa, F4, KK2, in[9],  15);
	ROUND(aaa, bbb, ccc, ddd, eee, F4, KK2, in[1],  13);
	ROUND(eee, aaa, bbb, ccc, ddd, F4, KK2, in[2],  11);

	/* Swap contents of "b" registers */
	tmp = bb; bb = bbb; bbb = tmp;

	/* round 3: left lane" */
	ROUND(dd, ee, aa, bb, cc, F3, K3, in[3],  11);
	ROUND(cc, dd, ee, aa, bb, F3, K3, in[10], 13);
	ROUND(bb, cc, dd, ee, aa, F3, K3, in[14],  6);
	ROUND(aa, bb, cc, dd, ee, F3, K3, in[4],   7);
	ROUND(ee, aa, bb, cc, dd, F3, K3, in[9],  14);
	ROUND(dd, ee, aa, bb, cc, F3, K3, in[15],  9);
	ROUND(cc, dd, ee, aa, bb, F3, K3, in[8],  13);
	ROUND(bb, cc, dd, ee, aa, F3, K3, in[1],  15);
	ROUND(aa, bb, cc, dd, ee, F3, K3, in[2],  14);
	ROUND(ee, aa, bb, cc, dd, F3, K3, in[7],   8);
	ROUND(dd, ee, aa, bb, cc, F3, K3, in[0],  13);
	ROUND(cc, dd, ee, aa, bb, F3, K3, in[6],   6);
	ROUND(bb, cc, dd, ee, aa, F3, K3, in[13],  5);
	ROUND(aa, bb, cc, dd, ee, F3, K3, in[11], 12);
	ROUND(ee, aa, bb, cc, dd, F3, K3, in[5],   7);
	ROUND(dd, ee, aa, bb, cc, F3, K3, in[12],  5);

	/* round 3: right lane */
	ROUND(ddd, eee, aaa, bbb, ccc, F3, KK3, in[15],  9);
	ROUND(ccc, ddd, eee, aaa, bbb, F3, KK3, in[5],   7);
	ROUND(bbb, ccc, ddd, eee, aaa, F3, KK3, in[1],  15);
	ROUND(aaa, bbb, ccc, ddd, eee, F3, KK3, in[3],  11);
	ROUND(eee, aaa, bbb, ccc, ddd, F3, KK3, in[7],   8);
	ROUND(ddd, eee, aaa, bbb, ccc, F3, KK3, in[14],  6);
	ROUND(ccc, ddd, eee, aaa, bbb, F3, KK3, in[6],   6);
	ROUND(bbb, ccc, ddd, eee, aaa, F3, KK3, in[9],  14);
	ROUND(aaa, bbb, ccc, ddd, eee, F3, KK3, in[11], 12);
	ROUND(eee, aaa, bbb, ccc, ddd, F3, KK3, in[8],  13);
	ROUND(ddd, eee, aaa, bbb, ccc, F3, KK3, in[12],  5);
	ROUND(ccc, ddd, eee, aaa, bbb, F3, KK3, in[2],  14);
	ROUND(bbb, ccc, ddd, eee, aaa, F3, KK3, in[10], 13);
	ROUND(aaa, bbb, ccc, ddd, eee, F3, KK3, in[0],  13);
	ROUND(eee, aaa, bbb, ccc, ddd, F3, KK3, in[4],   7);
	ROUND(ddd, eee, aaa, bbb, ccc, F3, KK3, in[13],  5);

	/* Swap contents of "c" registers */
	tmp = cc; cc = ccc; ccc = tmp;

	/* round 4: left lane" */
	ROUND(cc, dd, ee, aa, bb, F4, K4, in[1],  11);
	ROUND(bb, cc, dd, ee, aa, F4, K4, in[9],  12);
	ROUND(aa, bb, cc, dd, ee, F4, K4, in[11], 14);
	ROUND(ee, aa, bb, cc, dd, F4, K4, in[10], 15);
	ROUND(dd, ee, aa, bb, cc, F4, K4, in[0],  14);
	ROUND(cc, dd, ee, aa, bb, F4, K4, in[8],  15);
	ROUND(bb, cc, dd, ee, aa, F4, K4, in[12],  9);
	ROUND(aa, bb, cc, dd, ee, F4, K4, in[4],   8);
	ROUND(ee, aa, bb, cc, dd, F4, K4, in[13],  9);
	ROUND(dd, ee, aa, bb, cc, F4, K4, in[3],  14);
	ROUND(cc, dd, ee, aa, bb, F4, K4, in[7],   5);
	ROUND(bb, cc, dd, ee, aa, F4, K4, in[15],  6);
	ROUND(aa, bb, cc, dd, ee, F4, K4, in[14],  8);
	ROUND(ee, aa, bb, cc, dd, F4, K4, in[5],   6);
	ROUND(dd, ee, aa, bb, cc, F4, K4, in[6],   5);
	ROUND(cc, dd, ee, aa, bb, F4, K4, in[2],  12);

	/* round 4: right lane */
	ROUND(ccc, ddd, eee, aaa, bbb, F2, KK4, in[8],  15);
	ROUND(bbb, ccc, ddd, eee, aaa, F2, KK4, in[6],   5);
	ROUND(aaa, bbb, ccc, ddd, eee, F2, KK4, in[4],   8);
	ROUND(eee, aaa, bbb, ccc, ddd, F2, KK4, in[1],  11);
	ROUND(ddd, eee, aaa, bbb, ccc, F2, KK4, in[3],  14);
	ROUND(ccc, ddd, eee, aaa, bbb, F2, KK4, in[11], 14);
	ROUND(bbb, ccc, ddd, eee, aaa, F2, KK4, in[15],  6);
	ROUND(aaa, bbb, ccc, ddd, eee, F2, KK4, in[0],  14);
	ROUND(eee, aaa, bbb, ccc, ddd, F2, KK4, in[5],   6);
	ROUND(ddd, eee, aaa, bbb, ccc, F2, KK4, in[12],  9);
	ROUND(ccc, ddd, eee, aaa, bbb, F2, KK4, in[2],  12);
	ROUND(bbb, ccc, ddd, eee, aaa, F2, KK4, in[13],  9);
	ROUND(aaa, bbb, ccc, ddd, eee, F2, KK4, in[9],  12);
	ROUND(eee, aaa, bbb, ccc, ddd, F2, KK4, in[7],   5);
	ROUND(ddd, eee, aaa, bbb, ccc, F2, KK4, in[10], 15);
	ROUND(ccc, ddd, eee, aaa, bbb, F2, KK4, in[14],  8);

	/* Swap contents of "d" registers */
	tmp = dd; dd = ddd; ddd = tmp;

	/* round 5: left lane" */
	ROUND(bb, cc, dd, ee, aa, F5, K5, in[4],   9);
	ROUND(aa, bb, cc, dd, ee, F5, K5, in[0],  15);
	ROUND(ee, aa, bb, cc, dd, F5, K5, in[5],   5);
	ROUND(dd, ee, aa, bb, cc, F5, K5, in[9],  11);
	ROUND(cc, dd, ee, aa, bb, F5, K5, in[7],   6);
	ROUND(bb, cc, dd, ee, aa, F5, K5, in[12],  8);
	ROUND(aa, bb, cc, dd, ee, F5, K5, in[2],  13);
	ROUND(ee, aa, bb, cc, dd, F5, K5, in[10], 12);
	ROUND(dd, ee, aa, bb, cc, F5, K5, in[14],  5);
	ROUND(cc, dd, ee, aa, bb, F5, K5, in[1],  12);
	ROUND(bb, cc, dd, ee, aa, F5, K5, in[3],  13);
	ROUND(aa, bb, cc, dd, ee, F5, K5, in[8],  14);
	ROUND(ee, aa, bb, cc, dd, F5, K5, in[11], 11);
	ROUND(dd, ee, aa, bb, cc, F5, K5, in[6],   8);
	ROUND(cc, dd, ee, aa, bb, F5, K5, in[15],  5);
	ROUND(bb, cc, dd, ee, aa, F5, K5, in[13],  6);

	/* round 5: right lane */
	ROUND(bbb, ccc, ddd, eee, aaa, F1, KK5, in[12],  8);
	ROUND(aaa, bbb, ccc, ddd, eee, F1, KK5, in[15],  5);
	ROUND(eee, aaa, bbb, ccc, ddd, F1, KK5, in[10], 12);
	ROUND(ddd, eee, aaa, bbb, ccc, F1, KK5, in[4],   9);
	ROUND(ccc, ddd, eee, aaa, bbb, F1, KK5, in[1],  12);
	ROUND(bbb, ccc, ddd, eee, aaa, F1, KK5, in[5],   5);
	ROUND(aaa, bbb, ccc, ddd, eee, F1, KK5, in[8],  14);
	ROUND(eee, aaa, bbb, ccc, ddd, F1, KK5, in[7],   6);
	ROUND(ddd, eee, aaa, bbb, ccc, F1, KK5, in[6],   8);
	ROUND(ccc, ddd, eee, aaa, bbb, F1, KK5, in[2],  13);
	ROUND(bbb, ccc, ddd, eee, aaa, F1, KK5, in[13],  6);
	ROUND(aaa, bbb, ccc, ddd, eee, F1, KK5, in[14],  5);
	ROUND(eee, aaa, bbb, ccc, ddd, F1, KK5, in[0],  15);
	ROUND(ddd, eee, aaa, bbb, ccc, F1, KK5, in[3],  13);
	ROUND(ccc, ddd, eee, aaa, bbb, F1, KK5, in[9],  11);
	ROUND(bbb, ccc, ddd, eee, aaa, F1, KK5, in[11], 11);

	/* Swap contents of "e" registers */
	tmp = ee; ee = eee; eee = tmp;

	/* combine results */
	state[0] += aa;
	state[1] += bb;
	state[2] += cc;
	state[3] += dd;
	state[4] += ee;
	state[5] += aaa;
	state[6] += bbb;
	state[7] += ccc;
	state[8] += ddd;
	state[9] += eee;

	return;
}

static void rmd320_init(struct crypto_tfm *tfm)
{
	struct rmd320_ctx *rctx = crypto_tfm_ctx(tfm);

	rctx->byte_count = 0;

	rctx->state[0] = RMD_H0;
	rctx->state[1] = RMD_H1;
	rctx->state[2] = RMD_H2;
	rctx->state[3] = RMD_H3;
	rctx->state[4] = RMD_H4;
	rctx->state[5] = RMD_H5;
	rctx->state[6] = RMD_H6;
	rctx->state[7] = RMD_H7;
	rctx->state[8] = RMD_H8;
	rctx->state[9] = RMD_H9;

	memset(rctx->buffer, 0, sizeof(rctx->buffer));
}

static void rmd320_update(struct crypto_tfm *tfm, const u8 *data,
			  unsigned int len)
{
	struct rmd320_ctx *rctx = crypto_tfm_ctx(tfm);
	const u32 avail = sizeof(rctx->buffer) - (rctx->byte_count & 0x3f);

	rctx->byte_count += len;

	/* Enough space in buffer? If so copy and we're done */
	if (avail > len) {
		memcpy((char *)rctx->buffer + (sizeof(rctx->buffer) - avail),
		       data, len);
		return;
	}

	memcpy((char *)rctx->buffer + (sizeof(rctx->buffer) - avail),
	       data, avail);

	rmd320_transform(rctx->state, rctx->buffer);
	data += avail;
	len -= avail;

	while (len >= sizeof(rctx->buffer)) {
		memcpy(rctx->buffer, data, sizeof(rctx->buffer));
		rmd320_transform(rctx->state, rctx->buffer);
		data += sizeof(rctx->buffer);
		len -= sizeof(rctx->buffer);
	}

	memcpy(rctx->buffer, data, len);
}

/* Add padding and return the message digest. */
static void rmd320_final(struct crypto_tfm *tfm, u8 *out)
{
	struct rmd320_ctx *rctx = crypto_tfm_ctx(tfm);
	u32 i, index, padlen;
	u64 bits;
	u32 *dst = (u32 *)out;
	static const u8 padding[64] = { 0x80, };

	bits = cpu_to_le64(rctx->byte_count << 3);

	/* Pad out to 56 mod 64 */
	index = rctx->byte_count & 0x3f;
	padlen = (index < 56) ? (56 - index) : ((64+56) - index);
	rmd320_update(tfm, padding, padlen);

	/* Append length */
	rmd320_update(tfm, (const u8 *)&bits, sizeof(bits));

	/* Store state in digest */
	for (i = 0; i < 10; i++)
		dst[i] = cpu_to_le32(rctx->state[i]);

	/* Wipe context */
	memset(rctx, 0, sizeof(*rctx));
}

static struct crypto_alg alg = {
	.cra_name	 =	"rmd320",
	.cra_driver_name =	"rmd320",
	.cra_flags	 =	CRYPTO_ALG_TYPE_DIGEST,
	.cra_blocksize	 =	RMD320_BLOCK_SIZE,
	.cra_ctxsize	 =	sizeof(struct rmd320_ctx),
	.cra_module	 =	THIS_MODULE,
	.cra_list	 =	LIST_HEAD_INIT(alg.cra_list),
	.cra_u		 =	{ .digest = {
	.dia_digestsize	 =	RMD320_DIGEST_SIZE,
	.dia_init	 =	rmd320_init,
	.dia_update	 =	rmd320_update,
	.dia_final	 =	rmd320_final } }
};

static int __init rmd320_mod_init(void)
{
	return crypto_register_alg(&alg);
}

static void __exit rmd320_mod_fini(void)
{
	crypto_unregister_alg(&alg);
}

module_init(rmd320_mod_init);
module_exit(rmd320_mod_fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RIPEMD-320 Message Digest");

MODULE_ALIAS("rmd320");
