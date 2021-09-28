/*
 * Copyright (c) 2019 Semidrive Semiconductor.
 * All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <crypto/internal/hash.h>
#include <linux/dma-direction.h>

#include "sdrv-hash.h"
#include "sx_hash.h"
#include "cryptlib.h"

/* crypto hw padding constant for first operation */
#define SHA_PADDING     64
#define SHA_PADDING_MASK    (SHA_PADDING - 1)

static LIST_HEAD(ahash_algs);

static const u32 std_iv_sha1[SHA256_DIGEST_SIZE / sizeof(u32)] = {
	SHA1_H0, SHA1_H1, SHA1_H2, SHA1_H3, SHA1_H4, 0, 0, 0
};

static const u32 std_iv_sha224[SHA256_DIGEST_SIZE / sizeof(u32)] = {
	SHA224_H0, SHA224_H1, SHA224_H2, SHA224_H3,
	SHA224_H4, SHA224_H5, SHA224_H6, SHA224_H7
};

static const u32 std_iv_sha256[SHA256_DIGEST_SIZE / sizeof(u32)] = {
	SHA256_H0, SHA256_H1, SHA256_H2, SHA256_H3,
	SHA256_H4, SHA256_H5, SHA256_H6, SHA256_H7
};

static const u64 std_iv_sha384[SHA512_DIGEST_SIZE / sizeof(u64)] = {
	SHA384_H0, SHA384_H1, SHA384_H2, SHA384_H3,
	SHA384_H4, SHA384_H5, SHA384_H6, SHA384_H7
};

static const u64 std_iv_sha512[SHA512_DIGEST_SIZE / sizeof(u64)] = {
	SHA512_H0, SHA512_H1, SHA512_H2, SHA512_H3,
	SHA512_H4, SHA512_H5, SHA512_H6, SHA512_H7
};

static int semidrive_ahash_async_req_handle(
	struct crypto_async_request *async_req)
{
	struct ahash_request *req = ahash_request_cast(async_req);
	struct semidrive_sha_reqctx *rctx = ahash_request_ctx(req);
	struct semidrive_sha_ctx *ctx = crypto_tfm_ctx(async_req->tfm);
	struct semidrive_ce_alg_template *tmpl = to_ahash_tmpl(async_req->tfm);
	struct crypto_ahash *ahash = crypto_ahash_reqtfm(req);
	unsigned int digestsize = crypto_ahash_digestsize(ahash);
	struct crypto_dev *sdce = tmpl->sdce;
	unsigned long flags = rctx->flags;
	int ret, i;
	addr_t phy_addr = 0;
	u32 phy_offset = 0;
	u32 cpy_offset = 0;
	void *input_buff = NULL;
	bool first_part = false;
	hash_alg_t hash_type;


	if (IS_SHA_HMAC(flags)) {
		rctx->authkey = ctx->authkey;
		rctx->authklen = 128;//SD_SHA_HMAC_KEY_SIZE;
	}
	else if (IS_CMAC(flags)) {
		rctx->authkey = ctx->authkey;
		rctx->authklen = 128;//AES_KEYSIZE_128;
	}

	rctx->src_nents = sg_nents_for_len(req->src, req->nbytes);

	if (rctx->src_nents < 0) {
		dev_err(sdce->dev, "Invalid numbers of src SG.\n");
		return rctx->src_nents;
	}

	//maybe this exchange can be removed
	switch (rctx->flags) {
		/*case SD_HASH_MD5:
		    hash_type = ALG_MD5;
		    break;
		*/
		case SD_HASH_SHA1:
			hash_type = ALG_SHA1;
			break;

		case SD_HASH_SHA224:
			hash_type = ALG_SHA224;
			break;

		case SD_HASH_SHA256:
			hash_type = ALG_SHA256;
			break;

		case SD_HASH_SHA384:
			hash_type = ALG_SHA384;
			break;

		case SD_HASH_SHA512:
			hash_type = ALG_SHA512;
			break;

		case SD_HASH_SM3:
			hash_type = ALG_SM3;
			break;

		default:
			hash_type = ALG_SHA256;
	}

	//not align part of last update or none
	block_t rctx_tmpbuf = BLOCK_T_CONV(rctx->tmpbuf, rctx->buflen, EXT_MEM);
	block_t rctx_digest = BLOCK_T_CONV((u8 *)rctx->digest, digestsize, EXT_MEM);

	if (rctx->last_blk) {
		if (rctx->nbytes_orig == 0) {
			ret = hash_finish_blk(sdce, hash_type, false, &rctx_tmpbuf, &rctx_digest, rctx->count);
		}
		else {
			input_buff = kzalloc(
							 rctx->nbytes_orig + CACHE_LINE + rctx->buflen,
							 GFP_KERNEL);

			if (!input_buff) {
				pr_err("alloc fail line : %d\n", __LINE__);
				return -ENOMEM;
			}

			phy_addr = _paddr(input_buff);

			if (phy_addr & (CACHE_LINE - 1)) {
				phy_offset = CACHE_LINE
							 - (phy_addr & (CACHE_LINE - 1));
			}

			for (i = 0; i < rctx->src_nents; i++) {
				memcpy(input_buff + phy_offset + cpy_offset,
					   sg_virt(&rctx->src_orig[i]),
					   (rctx->src_orig[i]).length);
				cpy_offset += (rctx->src_orig[i]).length;
			}

			if (rctx->buflen) {
				memcpy(input_buff + phy_offset + cpy_offset,
					   rctx->tmpbuf, rctx->buflen);
			}

			block_t block_tmp1 = BLOCK_T_CONV((u8 *)(input_buff + phy_offset), rctx->nbytes_orig + rctx->buflen, EXT_MEM);
			block_t block_tmp2 = BLOCK_T_CONV(rctx->digest, digestsize, EXT_MEM);
			ret = hash_finish_blk(sdce, hash_type, false, &block_tmp1, &block_tmp2, rctx->count);
		}

		memcpy(req->result, rctx->digest, digestsize);

		sdce->async_req_done(tmpl->sdce, ret);
	}
	else {
		input_buff = kzalloc(
						 rctx->nbytes_orig + CACHE_LINE, GFP_KERNEL);

		if (!input_buff) {
			pr_err("alloc fail, line : %d", __LINE__);
			return -ENOMEM;
		}

		phy_addr = _paddr(input_buff);

		if (phy_addr & (CACHE_LINE - 1)) {
			phy_offset = CACHE_LINE - (phy_addr & (CACHE_LINE - 1));
		}

		if (rctx->first_blk) {
			first_part = true;
			rctx->first_blk = false;
		}

		for (i = 0; i < rctx->src_nents; i++) {
			memcpy(input_buff + phy_offset + cpy_offset,
				   sg_virt(&rctx->src_orig[i]),
				   (rctx->src_orig[i]).length);
			cpy_offset += (rctx->src_orig[i]).length;
		}

		block_t block_tmp3 = BLOCK_T_CONV((u8 *)(input_buff + phy_offset), rctx->nbytes_orig - rctx->buflen, EXT_MEM);
		ret = hash_update_blk(sdce, hash_type, first_part, &block_tmp3);

		sdce->async_req_done(tmpl->sdce, ret);
	}

	if (input_buff) {
		kfree(input_buff);
	}

	return ret;
}

static int semidrive_ahash_init(struct ahash_request *req)
{
	struct semidrive_sha_reqctx *rctx = ahash_request_ctx(req);
	struct semidrive_ce_alg_template *tmpl = to_ahash_tmpl(req->base.tfm);

	memset(rctx, 0, sizeof(*rctx));
	rctx->first_blk = true;
	rctx->last_blk = false;
	rctx->flags = tmpl->alg_flags;

	return 0;
}

static int semidrive_ahash_export(struct ahash_request *req, void *out)
{
	struct crypto_ahash *ahash = crypto_ahash_reqtfm(req);
	struct semidrive_sha_reqctx *rctx = ahash_request_ctx(req);
	unsigned long flags = rctx->flags;
	unsigned int digestsize = crypto_ahash_digestsize(ahash);
	unsigned int blocksize =
		crypto_tfm_alg_blocksize(crypto_ahash_tfm(ahash));

	if (IS_SHA1(flags) || IS_SHA1_HMAC(flags)) {
		struct sha1_state *out_state = out;

		out_state->count = rctx->count;
		semidrive_cpu_to_be32p_array((__be32 *)out_state->state,
									 rctx->digest, digestsize);
		memcpy(out_state->buffer, rctx->buf, blocksize);
	}
	else if (IS_SHA256(flags) || IS_SHA256_HMAC(flags) \
			 || IS_SHA224(flags) || IS_SHA224_HMAC(flags)) {
		struct sha256_state *out_state = out;

		out_state->count = rctx->count;
		semidrive_cpu_to_be32p_array((__be32 *)out_state->state,
									 rctx->digest, digestsize);
		memcpy(out_state->buf, rctx->buf, blocksize);
	}
	else if (IS_SHA512(flags) || IS_SHA512_HMAC(flags)
			 || IS_SHA384(flags) || IS_SHA384_HMAC(flags)) {
		struct sha512_state *out_state = out;

		//out_state->count = rctx->count;
		semidrive_cpu_to_be32p_array((__be32 *)out_state->state,
									 rctx->digest, digestsize);
		memcpy(out_state->buf, rctx->buf, blocksize);
	}
	else {
		return -EINVAL;
	}

	return 0;
}

static int semidrive_import_common_sha256(struct ahash_request *req,
		u64 in_count, const u32 *state, const u8 *buffer, bool hmac)
{
	struct crypto_ahash *ahash = crypto_ahash_reqtfm(req);
	struct semidrive_sha_reqctx *rctx = ahash_request_ctx(req);
	unsigned int digestsize = crypto_ahash_digestsize(ahash);
	unsigned int blocksize;
	u64 count = in_count;

	blocksize = crypto_tfm_alg_blocksize(crypto_ahash_tfm(ahash));
	rctx->count = in_count;
	memcpy(rctx->buf, buffer, blocksize);

	if (in_count <= blocksize) {
		rctx->first_blk = 1;
	}
	else {
		rctx->first_blk = 0;

		if (hmac) {
			count += SHA_PADDING;
		}
	}

	rctx->byte_count[0] = (__force __be32)(count & ~SHA_PADDING_MASK);
	rctx->byte_count[1] = (__force __be32)(count >> 32);
	semidrive_cpu_to_be32p_array((__be32 *)rctx->digest, (const u8 *)state,
								 digestsize);
	rctx->buflen = (unsigned int)(in_count & (blocksize - 1));

	return 0;
}

/*need more debug*/
static int semidrive_import_common_sha512(struct ahash_request *req,
		u64 *in_count, const u64 *state, const u8 *buffer, bool hmac)
{
	struct crypto_ahash *ahash = crypto_ahash_reqtfm(req);
	struct semidrive_sha_reqctx *rctx = ahash_request_ctx(req);
	unsigned int digestsize = crypto_ahash_digestsize(ahash);
	unsigned int blocksize;
	u64 count = in_count[0];

	blocksize = crypto_tfm_alg_blocksize(crypto_ahash_tfm(ahash));
	rctx->count = count;
	memcpy(rctx->buf, buffer, blocksize);

	if (count <= blocksize) {
		rctx->first_blk = 1;
	}
	else {
		rctx->first_blk = 0;

		if (hmac) {
			count += SHA_PADDING;
		}
	}

	rctx->byte_count[0] = (__force __be32)(count & ~SHA_PADDING_MASK);
	rctx->byte_count[1] = (__force __be32)(count >> 32);
	semidrive_cpu_to_be32p_array((__be32 *)rctx->digest, (const u8 *)state,
								 digestsize);
	rctx->buflen = (unsigned int)(in_count[0] & (blocksize - 1));

	return 0;
}

static int semidrive_ahash_import(struct ahash_request *req, const void *in)
{
	struct semidrive_sha_reqctx *rctx = ahash_request_ctx(req);
	unsigned long flags = rctx->flags;
	bool hmac = IS_SHA_HMAC(flags);
	int ret = -EINVAL;

	if (IS_SHA1(flags) || IS_SHA1_HMAC(flags)) {
		const struct sha1_state *state = in;

		ret = semidrive_import_common_sha256(req, state->count,
											 state->state, state->buffer, hmac);
	}
	else if (IS_SHA256(flags) || IS_SHA256_HMAC(flags)
			 || IS_SHA224(flags) || IS_SHA224_HMAC(flags)) {
		const struct sha256_state *state = in;

		ret = semidrive_import_common_sha256(req, state->count,
											 state->state, state->buf, hmac);
	}
	else if (IS_SHA512(flags) || IS_SHA512_HMAC(flags)
			 || IS_SHA384(flags) || IS_SHA384_HMAC(flags)) {
		const struct sha512_state *state = in;

		ret = semidrive_import_common_sha512(req, state->count,
											 state->state, state->buf, hmac);
	}

	return ret;
}

static int semidrive_ahash_update(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct semidrive_sha_reqctx *rctx = ahash_request_ctx(req);
	struct semidrive_ce_alg_template *tmpl = to_ahash_tmpl(req->base.tfm);
	struct crypto_dev *sdce = tmpl->sdce;
	struct scatterlist *sg_last, *sg;
	unsigned int total, len;
	unsigned int hash_later;
	unsigned int nbytes;
	unsigned int blocksize;

	blocksize = crypto_tfm_alg_blocksize(crypto_ahash_tfm(tfm));
	rctx->count += req->nbytes;

	/* check for buffer from previous updates and append it */
	total = req->nbytes + rctx->buflen;

	if (total <= blocksize) {
		scatterwalk_map_and_copy(rctx->buf + rctx->buflen, req->src,
								 0, req->nbytes, 0);
		rctx->buflen += req->nbytes;
		return 0;
	}

	/* save the original req structure fields */
	rctx->src_orig = req->src;
	rctx->nbytes_orig = req->nbytes;

	/*
	 * if we have data from previous update copy them on buffer. The old
	 * data will be combined with current request bytes.
	 */
	if (rctx->buflen) {
		memcpy(rctx->tmpbuf, rctx->buf, rctx->buflen);
	}

	/* calculate how many bytes will be hashed later */
	hash_later = total % blocksize;

	if (hash_later) {
		unsigned int src_offset = req->nbytes - hash_later;
		scatterwalk_map_and_copy(rctx->buf, req->src, src_offset,
								 hash_later, 0);
	}

	/* here nbytes is multiple of blocksize */
	nbytes = total - hash_later;

	len = rctx->buflen;
	sg = sg_last = req->src;

	while (len < nbytes && sg) {
		if (len + sg_dma_len(sg) > nbytes) {
			break;
		}

		len += sg_dma_len(sg);
		sg_last = sg;
		sg = sg_next(sg);
	}

	if (!sg_last) {
		return -EINVAL;
	}

	sg_mark_end(sg_last);

	if (rctx->buflen) {
		sg_init_table(rctx->sg, 2);
		sg_set_buf(rctx->sg, rctx->tmpbuf, rctx->buflen);
		sg_chain(rctx->sg, 2, req->src);
		req->src = rctx->sg;
	}

	req->nbytes = nbytes;
	rctx->buflen = hash_later;

	return sdce->async_req_enqueue(tmpl->sdce, &req->base);
}

static int semidrive_ahash_final(struct ahash_request *req)
{
	struct semidrive_sha_reqctx *rctx = ahash_request_ctx(req);
	struct semidrive_ce_alg_template *tmpl = to_ahash_tmpl(req->base.tfm);
	struct crypto_dev *sdce = tmpl->sdce;

	rctx->last_blk = true;

	rctx->src_orig = req->src;
	rctx->nbytes_orig = req->nbytes;

	memcpy(rctx->tmpbuf, rctx->buf, rctx->buflen);
	sg_init_one(rctx->sg, rctx->tmpbuf, rctx->buflen);

	req->src = rctx->sg;
	req->nbytes = rctx->buflen;

	return sdce->async_req_enqueue(tmpl->sdce, &req->base);
}

static int semidrive_ahash_digest(struct ahash_request *req)
{
	struct semidrive_sha_reqctx *rctx = ahash_request_ctx(req);
	struct semidrive_ce_alg_template *tmpl = to_ahash_tmpl(req->base.tfm);
	struct crypto_dev *sdce = tmpl->sdce;
	int ret;

	ret = semidrive_ahash_init(req);

	if (ret) {
		return ret;
	}

	rctx->src_orig = req->src;
	rctx->nbytes_orig = req->nbytes;
	rctx->first_blk = true;
	rctx->last_blk = true;

	return sdce->async_req_enqueue(tmpl->sdce, &req->base);
}

struct semidrive_ahash_result {
	struct completion completion;
	int error;
};

static void semidrive_digest_complete(
	struct crypto_async_request *req, int error)
{
	struct semidrive_ahash_result *result = req->data;

	if (error == -EINPROGRESS) {
		return;
	}

	result->error = error;
	complete(&result->completion);
}

static int semidrive_ahash_hmac_setkey(struct crypto_ahash *tfm,
									   const u8 *key, unsigned int keylen)
{
	return 0;

	/*
	unsigned int digestsize = crypto_ahash_digestsize(tfm);
	struct semidrive_sha_ctx *ctx = crypto_tfm_ctx(&tfm->base);
	struct semidrive_ahash_result result;
	struct ahash_request *req;
	struct scatterlist sg;
	unsigned int blocksize;
	struct crypto_ahash *ahash_tfm;
	u8 *buf;
	int ret;
	const char *alg_name;

	blocksize = crypto_tfm_alg_blocksize(crypto_ahash_tfm(tfm));
	memset(ctx->authkey, 0, sizeof(ctx->authkey));

	if (keylen <= blocksize) {
	    memcpy(ctx->authkey, key, keylen);
	    return 0;
	}

	if (digestsize == SHA1_DIGEST_SIZE)
	    alg_name = "sha1-semidriver";
	else if (digestsize == SHA256_DIGEST_SIZE)
	    alg_name = "sha256-semidrive";
	else
	    return -EINVAL;

	ahash_tfm = crypto_alloc_ahash(alg_name, CRYPTO_ALG_TYPE_AHASH,
	                   CRYPTO_ALG_TYPE_AHASH_MASK);
	if (IS_ERR(ahash_tfm))
	    return PTR_ERR(ahash_tfm);

	req = ahash_request_alloc(ahash_tfm, GFP_KERNEL);
	if (!req) {
	    ret = -ENOMEM;
	    goto err_free_ahash;
	}

	init_completion(&result.completion);
	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
	               semidrive_digest_complete, &result);
	crypto_ahash_clear_flags(ahash_tfm, ~0);

	buf = kzalloc(keylen + sd_MAX_ALIGN_SIZE, GFP_KERNEL);
	if (!buf) {
	    ret = -ENOMEM;
	    goto err_free_req;
	}

	memcpy(buf, key, keylen);
	sg_init_one(&sg, buf, keylen);
	ahash_request_set_crypt(req, &sg, ctx->authkey, keylen);

	ret = crypto_ahash_digest(req);
	if (ret == -EINPROGRESS || ret == -EBUSY) {
	    ret = wait_for_completion_interruptible(&result.completion);
	    if (!ret)
	        ret = result.error;
	}

	if (ret)
	    crypto_ahash_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);

	kfree(buf);
	err_free_req:
	ahash_request_free(req);
	err_free_ahash:
	crypto_free_ahash(ahash_tfm);
	return ret;
	*/
}

static int semidrive_ahash_cra_init(struct crypto_tfm *tfm)
{
	struct crypto_ahash *ahash = __crypto_ahash_cast(tfm);
	struct semidrive_sha_ctx *ctx = crypto_tfm_ctx(tfm);

	crypto_ahash_set_reqsize(ahash, sizeof(struct semidrive_sha_reqctx));
	memset(ctx, 0, sizeof(*ctx));

	return 0;
}

struct semidrive_ahash_def {
	unsigned long flags;
	const char *name;
	const char *drv_name;
	unsigned int digestsize;
	unsigned int blocksize;
	unsigned int statesize;
	//const u32 *std_iv;
};

static const struct semidrive_ahash_def ahash_def[] = {
	/*{
	    .flags      = SD_HASH_MD5,
	    .name       = "md5",
	    .drv_name   = "md5-sdce",
	    .digestsize = MD5_DIGEST_SIZE,
	    .blocksize  = MD5_BLOCK_SIZE,
	    .statesize  = sizeof(struct sha1_state),
	    .std_iv     = std_iv_sha1,
	},
	*/
	{
		.flags      = SD_HASH_SHA1,
		.name       = "sha1",
		.drv_name   = "sha1-sdce",
		.digestsize = SHA1_DIGEST_SIZE,
		.blocksize  = SHA1_BLOCK_SIZE,
		.statesize  = sizeof(struct sha1_state),
		//.std_iv       = std_iv_sha1,
	},
	{
		.flags      = SD_HASH_SHA224,
		.name       = "sha224",
		.drv_name   = "sha224-sdce",
		.digestsize = SHA224_DIGEST_SIZE,
		.blocksize  = SHA224_BLOCK_SIZE,
		.statesize  = sizeof(struct sha256_state),
		//.std_iv       = std_iv_sha224,
	},
	{
		.flags      = SD_HASH_SHA256,
		.name       = "sha256",
		.drv_name   = "sha256-sdce",
		.digestsize = SHA256_DIGEST_SIZE,
		.blocksize  = SHA256_BLOCK_SIZE,
		.statesize  = sizeof(struct sha256_state),
		//.std_iv       = std_iv_sha256,
	},
	{
		.flags      = SD_HASH_SHA384,
		.name       = "sha384",
		.drv_name   = "sha384-sdce",
		.digestsize = SHA384_DIGEST_SIZE,
		.blocksize  = SHA384_BLOCK_SIZE,
		.statesize  = sizeof(struct sha512_state),
		//.std_iv       = std_iv_sha384,
	},
	{
		.flags      = SD_HASH_SHA512,
		.name       = "sha512",
		.drv_name   = "sha512-sdce",
		.digestsize = SHA512_DIGEST_SIZE,
		.blocksize  = SHA512_BLOCK_SIZE,
		.statesize  = sizeof(struct sha512_state),
		//.std_iv       = std_iv_sha512,
	},
	{
		.flags      = SD_HASH_SM3,
		.name       = "sm3",
		.drv_name   = "sm3-sdce",
		.digestsize = SHA256_DIGEST_SIZE,
		.blocksize  = SHA256_BLOCK_SIZE,
		.statesize  = sizeof(struct sha256_state),
		//.std_iv       = std_iv_sha256,
	},
	{
		.flags      = SD_HASH_SHA1_HMAC,
		.name       = "hmac(sha1)",
		.drv_name   = "hmac-sha1-sdce",
		.digestsize = SHA1_DIGEST_SIZE,
		.blocksize  = SHA1_BLOCK_SIZE,
		.statesize  = sizeof(struct sha1_state),
		//.std_iv       = std_iv_sha1,
	},
	{
		.flags      = SD_HASH_SHA224_HMAC,
		.name       = "hmac(sha224)",
		.drv_name   = "hmac-sha224-sdce",
		.digestsize = SHA224_DIGEST_SIZE,
		.blocksize  = SHA224_BLOCK_SIZE,
		.statesize  = sizeof(struct sha256_state),
		//.std_iv       = std_iv_sha224,
	},
	{
		.flags      = SD_HASH_SHA256_HMAC,
		.name       = "hmac(sha256)",
		.drv_name   = "hmac-sha256-sdce",
		.digestsize = SHA256_DIGEST_SIZE,
		.blocksize  = SHA256_BLOCK_SIZE,
		.statesize  = sizeof(struct sha256_state),
		//.std_iv       = std_iv_sha256,
	},
	{
		.flags      = SD_HASH_SHA384_HMAC,
		.name       = "hmac(sha384)",
		.drv_name   = "hmac-sha384-sdce",
		.digestsize = SHA384_DIGEST_SIZE,
		.blocksize  = SHA384_BLOCK_SIZE,
		.statesize  = sizeof(struct sha512_state),
		//.std_iv       = std_iv_sha384,
	},
	{
		.flags      = SD_HASH_SHA512_HMAC,
		.name       = "hmac(sha512)",
		.drv_name   = "hmac-sha512-sdce",
		.digestsize = SHA512_DIGEST_SIZE,
		.blocksize  = SHA512_BLOCK_SIZE,
		.statesize  = sizeof(struct sha512_state),
		//.std_iv       = std_iv_sha512,
	},
};

static int semidrive_ahash_register_one(const struct semidrive_ahash_def *def,
										struct crypto_dev *sdce)
{
	struct semidrive_ce_alg_template *tmpl;
	struct ahash_alg *alg;
	struct crypto_alg *base;
	int ret;

	tmpl = kzalloc(sizeof(*tmpl), GFP_KERNEL);

	if (!tmpl) {
		return -ENOMEM;
	}

	//tmpl->std_iv = def->std_iv;

	alg = &tmpl->alg.ahash;
	alg->init = semidrive_ahash_init;
	alg->update = semidrive_ahash_update;
	alg->final = semidrive_ahash_final;
	alg->digest = semidrive_ahash_digest;
	alg->export = semidrive_ahash_export;
	alg->import = semidrive_ahash_import;

	if (IS_SHA_HMAC(def->flags)) {
		alg->setkey = semidrive_ahash_hmac_setkey;
	}

	alg->halg.digestsize = def->digestsize;
	alg->halg.statesize = def->statesize;

	base = &alg->halg.base;
	base->cra_blocksize = def->blocksize;
	base->cra_priority = 300;
	base->cra_flags = CRYPTO_ALG_ASYNC;
	base->cra_ctxsize = sizeof(struct semidrive_sha_ctx);
	base->cra_alignmask = 0;
	base->cra_module = THIS_MODULE;
	base->cra_init = semidrive_ahash_cra_init;
	INIT_LIST_HEAD(&base->cra_list);

	snprintf(base->cra_name, CRYPTO_MAX_ALG_NAME, "%s", def->name);
	snprintf(base->cra_driver_name, CRYPTO_MAX_ALG_NAME, "%s",
			 def->drv_name);

	INIT_LIST_HEAD(&tmpl->entry);
	tmpl->crypto_alg_type = CRYPTO_ALG_TYPE_AHASH;
	tmpl->alg_flags = def->flags;
	tmpl->sdce = sdce;

	ret = crypto_register_ahash(alg);

	if (ret) {
		kfree(tmpl);
		dev_err(sdce->dev, "%s registration failed\n", base->cra_name);
		return ret;
	}

	list_add_tail(&tmpl->entry, &ahash_algs);
	pr_info("%s is registered\n", base->cra_name);
	return 0;
}

static void semidrive_ahash_unregister(struct crypto_dev *qce)
{
	struct semidrive_ce_alg_template *tmpl, *n;

	list_for_each_entry_safe(tmpl, n, &ahash_algs, entry) {
		crypto_unregister_ahash(&tmpl->alg.ahash);
		list_del(&tmpl->entry);
		kfree(tmpl);
	}
}

static int semidrive_ahash_register(struct crypto_dev *sdce)
{
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(ahash_def); i++) {
		ret = semidrive_ahash_register_one(&ahash_def[i], sdce);

		if (ret) {
			goto err;
		}
	}

	return 0;
err:
	semidrive_ahash_unregister(sdce);
	return ret;
}

const struct semidrive_alg_ops ahash_ops = {
	.type = CRYPTO_ALG_TYPE_AHASH,
	.register_algs = semidrive_ahash_register,
	.unregister_algs = semidrive_ahash_unregister,
	.async_req_handle = semidrive_ahash_async_req_handle,
};

