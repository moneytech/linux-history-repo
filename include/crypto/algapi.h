/*
 * Cryptographic API for algorithms (i.e., low-level API).
 *
 * Copyright (c) 2006 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 */
#ifndef _CRYPTO_ALGAPI_H
#define _CRYPTO_ALGAPI_H

#include <linux/crypto.h>
#include <linux/list.h>
#include <linux/kernel.h>

struct module;
struct rtattr;
struct seq_file;

struct crypto_type {
	unsigned int (*ctxsize)(struct crypto_alg *alg, u32 type, u32 mask);
	int (*init)(struct crypto_tfm *tfm, u32 type, u32 mask);
	void (*exit)(struct crypto_tfm *tfm);
	void (*show)(struct seq_file *m, struct crypto_alg *alg);
};

struct crypto_instance {
	struct crypto_alg alg;

	struct crypto_template *tmpl;
	struct hlist_node list;

	void *__ctx[] CRYPTO_MINALIGN_ATTR;
};

struct crypto_template {
	struct list_head list;
	struct hlist_head instances;
	struct module *module;

	struct crypto_instance *(*alloc)(struct rtattr **tb);
	void (*free)(struct crypto_instance *inst);

	char name[CRYPTO_MAX_ALG_NAME];
};

struct crypto_spawn {
	struct list_head list;
	struct crypto_alg *alg;
	struct crypto_instance *inst;
};

struct crypto_queue {
	struct list_head list;
	struct list_head *backlog;

	unsigned int qlen;
	unsigned int max_qlen;
};

struct scatter_walk {
	struct scatterlist *sg;
	unsigned int offset;
};

struct blkcipher_walk {
	union {
		struct {
			struct page *page;
			unsigned long offset;
		} phys;

		struct {
			u8 *page;
			u8 *addr;
		} virt;
	} src, dst;

	struct scatter_walk in;
	unsigned int nbytes;

	struct scatter_walk out;
	unsigned int total;

	void *page;
	u8 *buffer;
	u8 *iv;

	int flags;
};

extern const struct crypto_type crypto_ablkcipher_type;
extern const struct crypto_type crypto_blkcipher_type;
extern const struct crypto_type crypto_hash_type;

void crypto_mod_put(struct crypto_alg *alg);

int crypto_register_template(struct crypto_template *tmpl);
void crypto_unregister_template(struct crypto_template *tmpl);
struct crypto_template *crypto_lookup_template(const char *name);

int crypto_init_spawn(struct crypto_spawn *spawn, struct crypto_alg *alg,
		      struct crypto_instance *inst);
void crypto_drop_spawn(struct crypto_spawn *spawn);
struct crypto_tfm *crypto_spawn_tfm(struct crypto_spawn *spawn, u32 type,
				    u32 mask);

struct crypto_attr_type *crypto_get_attr_type(struct rtattr **tb);
int crypto_check_attr_type(struct rtattr **tb, u32 type);
struct crypto_alg *crypto_get_attr_alg(struct rtattr **tb, u32 type, u32 mask);
struct crypto_instance *crypto_alloc_instance(const char *name,
					      struct crypto_alg *alg);

void crypto_init_queue(struct crypto_queue *queue, unsigned int max_qlen);
int crypto_enqueue_request(struct crypto_queue *queue,
			   struct crypto_async_request *request);
struct crypto_async_request *crypto_dequeue_request(struct crypto_queue *queue);
int crypto_tfm_in_queue(struct crypto_queue *queue, struct crypto_tfm *tfm);

int blkcipher_walk_done(struct blkcipher_desc *desc,
			struct blkcipher_walk *walk, int err);
int blkcipher_walk_virt(struct blkcipher_desc *desc,
			struct blkcipher_walk *walk);
int blkcipher_walk_phys(struct blkcipher_desc *desc,
			struct blkcipher_walk *walk);

static inline void *crypto_tfm_ctx_aligned(struct crypto_tfm *tfm)
{
	unsigned long addr = (unsigned long)crypto_tfm_ctx(tfm);
	unsigned long align = crypto_tfm_alg_alignmask(tfm);

	if (align <= crypto_tfm_ctx_alignment())
		align = 1;
	return (void *)ALIGN(addr, align);
}

static inline void *crypto_instance_ctx(struct crypto_instance *inst)
{
	return inst->__ctx;
}

static inline struct ablkcipher_alg *crypto_ablkcipher_alg(
	struct crypto_ablkcipher *tfm)
{
	return &crypto_ablkcipher_tfm(tfm)->__crt_alg->cra_ablkcipher;
}

static inline void *crypto_ablkcipher_ctx(struct crypto_ablkcipher *tfm)
{
	return crypto_tfm_ctx(&tfm->base);
}

static inline void *crypto_blkcipher_ctx(struct crypto_blkcipher *tfm)
{
	return crypto_tfm_ctx(&tfm->base);
}

static inline void *crypto_blkcipher_ctx_aligned(struct crypto_blkcipher *tfm)
{
	return crypto_tfm_ctx_aligned(&tfm->base);
}

static inline struct crypto_cipher *crypto_spawn_cipher(
	struct crypto_spawn *spawn)
{
	u32 type = CRYPTO_ALG_TYPE_CIPHER;
	u32 mask = CRYPTO_ALG_TYPE_MASK;

	return __crypto_cipher_cast(crypto_spawn_tfm(spawn, type, mask));
}

static inline struct cipher_alg *crypto_cipher_alg(struct crypto_cipher *tfm)
{
	return &crypto_cipher_tfm(tfm)->__crt_alg->cra_cipher;
}

static inline struct crypto_hash *crypto_spawn_hash(struct crypto_spawn *spawn)
{
	u32 type = CRYPTO_ALG_TYPE_HASH;
	u32 mask = CRYPTO_ALG_TYPE_HASH_MASK;

	return __crypto_hash_cast(crypto_spawn_tfm(spawn, type, mask));
}

static inline void *crypto_hash_ctx_aligned(struct crypto_hash *tfm)
{
	return crypto_tfm_ctx_aligned(&tfm->base);
}

static inline void blkcipher_walk_init(struct blkcipher_walk *walk,
				       struct scatterlist *dst,
				       struct scatterlist *src,
				       unsigned int nbytes)
{
	walk->in.sg = src;
	walk->out.sg = dst;
	walk->total = nbytes;
}

static inline struct crypto_async_request *crypto_get_backlog(
	struct crypto_queue *queue)
{
	return queue->backlog == &queue->list ? NULL :
	       container_of(queue->backlog, struct crypto_async_request, list);
}

static inline int ablkcipher_enqueue_request(struct ablkcipher_alg *alg,
					     struct ablkcipher_request *request)
{
	return crypto_enqueue_request(alg->queue, &request->base);
}

static inline struct ablkcipher_request *ablkcipher_dequeue_request(
	struct ablkcipher_alg *alg)
{
	return ablkcipher_request_cast(crypto_dequeue_request(alg->queue));
}

static inline void *ablkcipher_request_ctx(struct ablkcipher_request *req)
{
	return req->__ctx;
}

static inline int ablkcipher_tfm_in_queue(struct crypto_ablkcipher *tfm)
{
	return crypto_tfm_in_queue(crypto_ablkcipher_alg(tfm)->queue,
				   crypto_ablkcipher_tfm(tfm));
}

#endif	/* _CRYPTO_ALGAPI_H */

