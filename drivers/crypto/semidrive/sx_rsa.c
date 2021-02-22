/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#include <string.h>

#include "sx_rsa.h"
#include "sx_pke_conf.h"
#include "sx_hash.h"
#include "sx_errors.h"
#include "sx_rsa_pad.h"
#include "sx_dma.h"

#define LOCAL_TRACE 0 //close local trace 1->0

/* internal memory map:
 * location 0x0: Modulus:n, input
 * location 0x4: Ciphered text: C, output
 * location 0x5: Plain text: M, input
 * location 0x8: Public key: e, input
 */
uint32_t rsa_encrypt_blk(uint32_t vce_id,
                         rsa_pad_types_t padding_type,
                         block_t message,
                         block_t n,
                         block_t public_expo,
                         block_t result,
                         hash_alg_t hashType)
{
    uint8_t * msg_padding;
    struct mem_node * mem_n;
    uint32_t status = CRYPTOLIB_SUCCESS;
    uint32_t size = n.len;

    //Checks that the algoritm is valid for signature
    if (padding_type != ESEC_RSA_PADDING_NONE && padding_type != ESEC_RSA_PADDING_OAEP &&
            padding_type != ESEC_RSA_PADDING_EME_PKCS) {
        return CRYPTOLIB_INVALID_PARAM;
    }

    // Set command to enable byte-swap
    pke_set_command(vce_id, BA414EP_OPTYPE_RSA_ENC, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);

#if AUTO_OUTPUT_BY_CE
    pke_set_dst_param(vce_id, size, 0x1 << BA414EP_MEMLOC_4, (addr_t)(result.addr), result.addr_type);
#endif

    // Location 0 -> Modulus N
    status = mem2CryptoRAM_rev(vce_id, n, size, BA414EP_MEMLOC_0, true);

    if (status) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)n.addr);
        return status;
    }

    mem_n = ce_malloc(RSA_MAX_SIZE);

    if(mem_n != NULL){
        msg_padding = mem_n->ptr;
    }else{
        return CRYPTOLIB_PK_N_NOTVALID;
    }

    if (padding_type == ESEC_RSA_PADDING_OAEP) {
        status = rsa_pad_eme_oaep_encode(vce_id, n.len, ALG_SHA256, msg_padding, message, message.len);

        if (status) {
            ce_free(mem_n);
            return status;
        }

        status = mem2CryptoRAM_rev(vce_id, BLOCK_T_CONV(msg_padding, n.len, 0), size, BA414EP_MEMLOC_5, true);

        if (status) {
            LTRACEF("src addr: 0x%lx\n", (addr_t)msg_padding);
            ce_free(mem_n);
            return status;
        }
    }
    else if (padding_type == ESEC_RSA_PADDING_EME_PKCS) {
        status = rsa_pad_eme_pkcs_encode(vce_id, n.len, msg_padding, message, message.len);

        if (status) {
            ce_free(mem_n);
            return status;
        }

        status = mem2CryptoRAM_rev(vce_id, BLOCK_T_CONV(msg_padding, n.len, 0), size, BA414EP_MEMLOC_5, true);

        if (status) {
            LTRACEF("src addr: 0x%lx\n", (addr_t)msg_padding);
            ce_free(mem_n);
            return status;
        }
    }
    else { //No padding
        status = mem2CryptoRAM_rev(vce_id, message, size, BA414EP_MEMLOC_5, true);

        if (status) {
            ce_free(mem_n);
            return status;
            LTRACEF("src addr: 0x%lx\n", (addr_t)message.addr);
        }
    }

    // Location 8 -> Public exponent
    status = mem2CryptoRAM_rev(vce_id, public_expo, size, BA414EP_MEMLOC_8, true);

    if (status) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)public_expo.addr);
        ce_free(mem_n);
        return status;
    }

    /* RSA encryption */
    status = pke_start_wait_status(vce_id);

    if (status) {
        ce_free(mem_n);
        return status;
    }

#if AUTO_OUTPUT_BY_CE
#else
    // Location 4 -> Cipher C optional: pke_set_dst_param, copy result by manual mode
    CryptoRAM2mem(vce_id, result, n.len, BA414EP_MEMLOC_4, true);
#endif
    ce_free(mem_n);
    return status;
}

#define BLOCK_S_CONST_ADDR             0x10000000
/* internal memory map:
 * location 0x0: Modulus:n, input
 * location 0x4: Ciphered text: C, input
 * location 0x5: Plain text: M, output
 * location 0x6: Private key: d, input
 *
 * internal memory map for crt decryption:
 * location 0x2: p, input
 * location 0x3: q: d, input
 * location 0x4: Cipher text: C, input
 * location 0x5: Plain text: M, output
 * location 0xA: dP =d mod(p-1), input
 * location 0xB: dQ = d mod(q-1), input
 * location 0xC: qInv = 1/q mod p, input
 */
uint32_t rsa_decrypt_blk(uint32_t vce_id,
                         rsa_pad_types_t padding_type,
                         block_t cipher,
                         block_t n,
                         block_t private_key,
                         block_t result,
                         uint32_t crt,
                         uint32_t* msg_len,
                         hash_alg_t hashType)
{
    uint32_t error = 0;
    uint32_t size = n.len;

#if PK_CM_ENABLED

    if (PK_CM_RANDPROJ_MOD) {
        if (crt) {
            size += 2 * PK_CM_RAND_SIZE;
        }
        else {
            size += PK_CM_RAND_SIZE;
        }
    }

#endif

    //Checks that the algoritm is valid for signature
    if (padding_type != ESEC_RSA_PADDING_NONE && padding_type != ESEC_RSA_PADDING_OAEP &&
            padding_type != ESEC_RSA_PADDING_EME_PKCS) {
        return CRYPTOLIB_INVALID_PARAM;
    }

    if (!crt) {
        // Set command to enable byte-swap
        pke_set_command(vce_id, BA414EP_OPTYPE_RSA_DEC, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);

        // Location 0 -> Modulus N
        error = mem2CryptoRAM_rev(vce_id, n, size, BA414EP_MEMLOC_0, true);

        if (error) {
            LTRACEF("src addr: 0x%lx\n", (addr_t)n.addr);
            return error;
        }
    }
    else {
        // Set command to enable byte-swap
        pke_set_command(vce_id, BA414EP_OPTYPE_RSA_CRT_DEC, size, BA414EP_BIGEND, BA414EP_SELCUR_NO_ACCELERATOR);
    }

#if AUTO_OUTPUT_BY_CE
    pke_set_dst_param(vce_id, size, 0x1 << BA414EP_MEMLOC_5, (addr_t)(result.addr), result.addr_type);
#endif
    // Location 4 -> Cipher
    error = mem2CryptoRAM_rev(vce_id, cipher, size, BA414EP_MEMLOC_4, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)cipher.addr);
        return error;
    }

    if (crt) {
        uint32_t crtLen = n.len / 2;
        private_key.len = crtLen;
        error = mem2CryptoRAM_rev(vce_id, private_key, size, BA414EP_MEMLOC_2, true);

        if (error) {
            LTRACEF("src addr: 0x%lx\n", (addr_t)private_key.addr);
            return error;
        }

        if (!(private_key.addr_type & BLOCK_S_CONST_ADDR)) {
            private_key.addr += crtLen;
        }

        error = mem2CryptoRAM_rev(vce_id, private_key, size, BA414EP_MEMLOC_3, true);

        if (error) {
            LTRACEF("src addr: 0x%lx\n", (addr_t)private_key.addr);
            return error;
        }

        if (!(private_key.addr_type & BLOCK_S_CONST_ADDR)) {
            private_key.addr += crtLen;
        }

        error = mem2CryptoRAM_rev(vce_id, private_key, size, BA414EP_MEMLOC_10, true);

        if (error) {
            LTRACEF("src addr: 0x%lx\n", (addr_t)private_key.addr);
            return error;
        }

        if (!(private_key.addr_type & BLOCK_S_CONST_ADDR)) {
            private_key.addr += crtLen;
        }

        error = mem2CryptoRAM_rev(vce_id, private_key, size, BA414EP_MEMLOC_11, true);

        if (error) {
            LTRACEF("src addr: 0x%lx\n", (addr_t)private_key.addr);
            return error;
        }

        if (!(private_key.addr_type & BLOCK_S_CONST_ADDR)) {
            private_key.addr += crtLen;
        }

        error = mem2CryptoRAM_rev(vce_id, private_key, size, BA414EP_MEMLOC_12, true);

        if (error) {
            LTRACEF("src addr: 0x%lx\n", (addr_t)private_key.addr);
            return error;
        }
    }
    else {
        private_key.len = n.len;

        // Location 6 -> Private key
        mem2CryptoRAM_rev(vce_id, private_key, size, BA414EP_MEMLOC_6, true);
    }

    error |= pke_start_wait_status(vce_id);

    if (error) {
        return error;
    }

#if AUTO_OUTPUT_BY_CE
#else
    // Fetch result optional: pke_set_dst_param, fetch result by manual mode
    CryptoRAM2mem(vce_id, result, result.len, BA414EP_MEMLOC_5, true);
#endif

    // Location 5 -> M
    if (padding_type != ESEC_RSA_PADDING_NONE) {
        uint8_t * msg_padding;
        struct mem_node * mem_n;
        uint8_t* addr;

        mem_n = ce_malloc(RSA_MAX_SIZE + 4);

        if(mem_n != NULL){
            msg_padding = mem_n->ptr;
        }else{
            return CRYPTOLIB_PK_N_NOTVALID;
        }

        memcpy(msg_padding, result.addr, *msg_len);

        if (padding_type == ESEC_RSA_PADDING_OAEP) {
            error |= rsa_pad_eme_oaep_decode(vce_id, n.len, hashType, msg_padding, &addr, (size_t*)msg_len);
        }
        else if (padding_type == ESEC_RSA_PADDING_EME_PKCS) {
            error |= rsa_pad_eme_pkcs_decode(n.len, msg_padding, &addr, (size_t*)msg_len);
        }

        memcpy(result.addr, addr, *msg_len);

        ce_free(mem_n);
    }
    else {
        *msg_len = n.len;
    }

    return error;
}

//TODO If possible, try to refactor this function and decrypt for CRT
/* internal memory map for decryption:
 * location 0x0: Modulus:n, input
 * location 0x6: Private key: d, input
 * location 0xB: Signature: s, output
 * location 0x8: Hash of message: h, input
 *
 */
uint32_t rsa_signature_generation_blk(uint32_t vce_id,
                                      hash_alg_t sha_type,
                                      rsa_pad_types_t padding_type,
                                      block_t message,
                                      block_t result,
                                      block_t n,
                                      block_t private_key,
                                      uint32_t salt_length)
{
    uint32_t error = 0;
    uint32_t size = n.len;
    uint8_t * hash;
    struct mem_node * mem_n;
    uint8_t * msg_padding;
    struct mem_node * mem_n_1;
    block_t hash_block;

    //Checks that the algoritm is valid for signature
    if (padding_type != ESEC_RSA_PADDING_NONE && padding_type != ESEC_RSA_PADDING_EMSA_PKCS &&
            padding_type != ESEC_RSA_PADDING_PSS) {
        return CRYPTOLIB_INVALID_PARAM;
    }

    mem_n = ce_malloc(MAX_DIGESTSIZE);

    if(mem_n != NULL){
        hash = mem_n->ptr;
    }else{
        return CRYPTOLIB_PK_N_NOTVALID;
    }

    /*Calculate hash of the message*/
    error |= hash_blk(sha_type, vce_id, BLOCK_T_CONV(NULL, 0, 0), message, BLOCK_T_CONV(hash, MAX_DIGESTSIZE, EXT_MEM));

    if (error) {
        ce_free(mem_n);
        return error;
    }

    int padError = 0;

    mem_n_1 = ce_malloc(RSA_MAX_SIZE);

    if(mem_n_1 != NULL){
        msg_padding = mem_n_1->ptr;
    }else{
        ce_free(mem_n);
        return CRYPTOLIB_PK_N_NOTVALID;
    }

    if (padding_type == ESEC_RSA_PADDING_EMSA_PKCS) {
        padError = rsa_pad_emsa_pkcs_encode(vce_id, n.len, sha_type, msg_padding, hash);
        hash_block.addr   = msg_padding;
        hash_block.len    = n.len;
        hash_block.addr_type  = EXT_MEM;
    }
    else if (padding_type == ESEC_RSA_PADDING_PSS) {
        padError = rsa_pad_emsa_pss_encode(vce_id, n.len, sha_type, msg_padding, hash, n.addr[0],
                                           salt_length);
        hash_block.addr   = msg_padding;
        hash_block.len    = n.len;
        hash_block.addr_type  = EXT_MEM;
    }
    else { //No padding
        rsa_pad_zeros(msg_padding, n.len, hash, hash_get_digest_size(sha_type));
        hash_block.addr   = msg_padding;
        hash_block.len    = n.len;
        hash_block.addr_type  = EXT_MEM;
    }

    if (padError) {
        ce_free(mem_n);
        ce_free(mem_n_1);
        return padError;
    }

    // Set command to enable byte-swap
    pke_set_command(vce_id, BA414EP_OPTYPE_RSA_SIGN_GEN, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);

#if AUTO_OUTPUT_BY_CE
    pke_set_dst_param(vce_id, size, 0x1 << BA414EP_MEMLOC_11, (addr_t)(result.addr), result.addr_type);
#endif
    // Location 0 -> N
    error = mem2CryptoRAM_rev(vce_id, n, size, BA414EP_MEMLOC_0, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)n.addr);
        ce_free(mem_n);
        ce_free(mem_n_1);
        return error;
    }

    private_key.len = n.len;

    // Location 6 -> Private key
    error = mem2CryptoRAM_rev(vce_id, private_key, size, BA414EP_MEMLOC_6, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)private_key.addr);
        ce_free(mem_n);
        ce_free(mem_n_1);
        return error;
    }

    // Location 12 -> hash
    error = mem2CryptoRAM_rev(vce_id, hash_block, size, BA414EP_MEMLOC_12, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)hash_block.addr);
        ce_free(mem_n);
        ce_free(mem_n_1);
        return error;
    }

    // Start BA414EP
    error |= pke_start_wait_status(vce_id);

    if (error) {
        LTRACEF("sig_gen pke error: 0x%x\n", error);
        ce_free(mem_n);
        ce_free(mem_n_1);
        return error;
    }

    // Fetch result optional: pke_set_dst_param, fetch result by manual mode
#if AUTO_OUTPUT_BY_CE
#else
    CryptoRAM2mem(vce_id, result, result.len, BA414EP_MEMLOC_11, true);
#endif
    ce_free(mem_n);
    ce_free(mem_n_1);
    return CRYPTOLIB_SUCCESS;
}

/* internal memory map:
 * location 0x0: Modulus:n, input
 * location 0x8: Public key: e, input
 * location 0xB: Signature: s, input
 * location 0x8: Hash of message: h, input
 */
uint32_t rsa_signature_verification_blk(uint32_t vce_id,
                                        hash_alg_t sha_type,
                                        rsa_pad_types_t padding_type,
                                        block_t message,
                                        block_t n,
                                        block_t public_expo,
                                        block_t signature,
                                        uint32_t salt_length)
{
    uint32_t error;
    uint32_t size = n.len;
    uint8_t * hash;
    struct mem_node * mem_n;
    uint8_t * msg_padding;
    struct mem_node * mem_n_1;
    block_t hash_block;

    //Checks that the algoritm is valid for signature
    if (padding_type != ESEC_RSA_PADDING_NONE && padding_type != ESEC_RSA_PADDING_EMSA_PKCS &&
            padding_type != ESEC_RSA_PADDING_PSS) {
        return CRYPTOLIB_INVALID_PARAM;
    }

    mem_n = ce_malloc(MAX_DIGESTSIZE);

    if(mem_n != NULL){
        hash = mem_n->ptr;
    }else{
        return CRYPTOLIB_PK_N_NOTVALID;
    }

    hash_block = BLOCK_T_CONV(hash, hash_get_digest_size(sha_type), EXT_MEM);

    error = hash_blk(sha_type, vce_id, BLOCK_T_CONV(NULL, 0, 0), message, hash_block);

    if (error) {
        ce_free(mem_n);
        return error;
    }

    int padError = 0;

    mem_n_1 = ce_malloc(RSA_MAX_SIZE);

    if(mem_n_1 != NULL){
        msg_padding = mem_n_1->ptr;
    }else{
        ce_free(mem_n);
        return CRYPTOLIB_PK_N_NOTVALID;
    }

    if (padding_type == ESEC_RSA_PADDING_EMSA_PKCS) {
        padError = rsa_pad_emsa_pkcs_encode(vce_id, n.len, sha_type, msg_padding, hash_block.addr);
        hash_block = BLOCK_T_CONV(msg_padding, n.len, 0);
    }
    else if (padding_type == ESEC_RSA_PADDING_NONE) { //No padding
        rsa_pad_zeros(msg_padding, n.len, hash_block.addr, hash_get_digest_size(sha_type));
        hash_block = BLOCK_T_CONV(msg_padding, n.len, 0);
    }

    if (padError) {
        ce_free(mem_n);
        ce_free(mem_n_1);
        return padError;
    }

    // Set command to enable byte-swap
    if (padding_type != ESEC_RSA_PADDING_PSS) {
        pke_set_command(vce_id, BA414EP_OPTYPE_RSA_SIGN_VERIF, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
    }
    else {
        pke_set_command(vce_id, BA414EP_OPTYPE_RSA_ENC, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
    }

    // Location 0 -> Modulus N
    error = mem2CryptoRAM_rev(vce_id, n, size, BA414EP_MEMLOC_0, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)n.addr);
        ce_free(mem_n);
        ce_free(mem_n_1);
        return error;
    }

    // Location 8 -> Public exponent
    error = mem2CryptoRAM_rev(vce_id, public_expo, size, BA414EP_MEMLOC_8, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)public_expo.addr);
        ce_free(mem_n);
        ce_free(mem_n_1);
        return error;
    }

    if (padding_type != ESEC_RSA_PADDING_PSS) {
        // Location 11 -> Signature
        error = mem2CryptoRAM_rev(vce_id, signature, signature.len, BA414EP_MEMLOC_11, true);

        if (error) {
            LTRACEF("src addr: 0x%lx\n", (addr_t)signature.addr);
            ce_free(mem_n);
            ce_free(mem_n_1);
            return error;
        }

        // Location 12 -> Hash
        error = mem2CryptoRAM_rev(vce_id, hash_block, hash_block.len, BA414EP_MEMLOC_12, true);

        if (error) {
            LTRACEF("src addr: 0x%lx\n", (addr_t)hash_block.addr);
            ce_free(mem_n);
            ce_free(mem_n_1);
            return error;
        }
    }
    else { //PSS
        // Location 5 -> Signature
        error = mem2CryptoRAM_rev(vce_id, signature, signature.len, BA414EP_MEMLOC_5, true);

        if (error) {
            LTRACEF("src addr: 0x%lx\n", (addr_t)signature.addr);
            ce_free(mem_n);
            ce_free(mem_n_1);
            return error;
        }
    }

    // Start BA414EP
    error = pke_start_wait_status(vce_id);

    if (error) {
        ce_free(mem_n);
        ce_free(mem_n_1);
        return error;
    }

    if (padding_type == ESEC_RSA_PADDING_PSS) {
        // Fetch result by manual mode
        CryptoRAM2mem_rev(vce_id, BLOCK_T_CONV(msg_padding, n.len, 0), n.len, BA414EP_MEMLOC_4, true);
        error = rsa_pad_emsa_pss_decode(vce_id, n.len, sha_type, msg_padding, hash_block.addr,
                                        salt_length, n.addr[0]);
    }
    ce_free(mem_n);
    ce_free(mem_n_1);
    return error;
}

/* internal memory map:
 * location 0x0: Modulus:n, output
 * location 0x1: lcm(p-1, q-1), output
 * location 0x2: p, input
 * location 0x3: q, input
 * location 0x6: Private key: d, output
 * location 0x8: Public key: e, input
 */
uint32_t rsa_private_key_generation_blk(uint32_t vce_id,
                                        block_t p,
                                        block_t q,
                                        block_t public_expo,
                                        block_t n,
                                        block_t private_key,
                                        uint32_t size,
                                        uint32_t lambda)
{
    uint32_t error;

    // Set command to enable byte-swap
    pke_set_command(vce_id, BA414EP_OPTYPE_RSA_PK_GEN, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
    //ce will write to this addr auto, do not use this .
#if AUTO_OUTPUT_BY_CE
    pke_set_dst_param(vce_id, size, 0x1 << BA414EP_MEMLOC_6, (addr_t)(private_key.addr), private_key.addr_type);
#endif
    // Location 2 -> P
    error = mem2CryptoRAM_rev(vce_id, p, size, BA414EP_MEMLOC_2, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)p.addr);
        return error;
    }

    // Location 3 -> Q
    error = mem2CryptoRAM_rev(vce_id, q, size, BA414EP_MEMLOC_3, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)q.addr);
        return error;
    }

    // Location 8 -> Public Expo
    error = mem2CryptoRAM_rev(vce_id, public_expo, size, BA414EP_MEMLOC_8, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)public_expo.addr);
        return error;
    }

    invalidate_cache_block(&n, vce_id);
    invalidate_cache_block(&private_key, vce_id);

    // Start BA414EP
    error = pke_start_wait_status(vce_id);

    if (error) {
        return error;
    }

    // Result (Lambda - location 1)
    if (lambda) {
        CryptoRAM2mem(vce_id, private_key, size, BA414EP_MEMLOC_1, true);

        if (!(private_key.addr_type & BLOCK_S_CONST_ADDR)) {
            private_key.addr += size;
        }
    }

    // Result (N - location 0) TODO: why not false & flush
    CryptoRAM2mem(vce_id, n, size, BA414EP_MEMLOC_0, false);
    //flush_cache((addr_t)n.addr, size);

#if AUTO_OUTPUT_BY_CE
#else
    //Result (Private key - location 6), fetch result by manual mode
    CryptoRAM2mem(vce_id, private_key, size, BA414EP_MEMLOC_6, false);
    //flush_cache((addr_t)private_key.addr, size);
#endif
    return CRYPTOLIB_SUCCESS;
}

/* internal memory map:
 * location 0x2: p, input
 * location 0x3: q, input
 * location 0x6: Private key: d, input
 * location 0xA: dP =d mod(p-1), output
 * location 0xB: dQ = d mod(q-1), output
 * location 0xC: qInv = 1/q mod p, output
 */
uint32_t rsa_crt_key_generation_blk(uint32_t vce_id,
                                    block_t p,
                                    block_t q,
                                    block_t d,
                                    block_t dp,
                                    block_t dq,
                                    block_t inv,
                                    uint32_t size)
{
    uint32_t error;

    // Set command to enable byte-swap
    pke_set_command(vce_id, BA414EP_OPTYPE_RSA_CRT_GEN, size, BA414EP_LITTLEEND, BA414EP_SELCUR_NO_ACCELERATOR);
#if AUTO_OUTPUT_BY_CE
    //? location 10 is dp
    pke_set_dst_param(vce_id, size, 0x1 << BA414EP_MEMLOC_10, (addr_t)(p.addr), p.addr_type);
#endif
    // Location 2 -> P
    error = mem2CryptoRAM_rev(vce_id, p, size, BA414EP_MEMLOC_2, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)p.addr);
        return error;
    }

    // Location 3 -> Q
    error = mem2CryptoRAM_rev(vce_id, q, size, BA414EP_MEMLOC_3, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)q.addr);
        return error;
    }

    // Location 6 -> D
    error = mem2CryptoRAM_rev(vce_id, d, size, BA414EP_MEMLOC_6, true);

    if (error) {
        LTRACEF("src addr: 0x%lx\n", (addr_t)d.addr);
        return error;
    }

    // Start BA414EP
    error = pke_start_wait_status(vce_id);

    if (error) {
        return error;
    }

#if AUTO_OUTPUT_BY_CE
#else
    // Result (DP - location 10)
    CryptoRAM2mem_rev(vce_id, dp, size, BA414EP_MEMLOC_10, true);
#endif
    // Result (DQ - location 11)
    CryptoRAM2mem(vce_id, dq, size, BA414EP_MEMLOC_11, true);

    // Result (INV - location 12)
    CryptoRAM2mem(vce_id, inv, size, BA414EP_MEMLOC_12, true);

    return CRYPTOLIB_SUCCESS;
}
