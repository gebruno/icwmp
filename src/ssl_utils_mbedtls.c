/*
 * ssl_utils.c: Utility functions with ssl
 *
 * Copyright (C) 2022-2023 IOPSYS Software Solutions AB. All rights reserved.
 *
 * See LICENSE file for license related information.
 */

#include <mbedtls/md.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

#include <string.h>
#include <stdlib.h>

#include "ssl_utils.h"
#include "common.h"
#include "log.h"

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;

static int rand_bytes(unsigned char *output, size_t len)
{
    static int initialized = 0;
    
    if (!initialized) {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        
        if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0) != 0) {
            return -1;
        }
        initialized = 1;
    }
    
    return mbedtls_ctr_drbg_random(&ctr_drbg, output, len) == 0 ? 1 : 0;
}

char *generate_random_string(size_t size)
{
	unsigned char *buf = NULL;
	char *hex = NULL;

	buf = (unsigned char *)calloc(size + 1, sizeof(unsigned char));
	if (buf == NULL) {
		CWMP_LOG(ERROR, "Unable to allocate memory for buf string");
		goto end;
	}

	int written = rand_bytes(buf, size);
	if (written != 1) {
		CWMP_LOG(ERROR,"Failed to get random bytes");
		goto end;
	}

	hex = string_to_hex(buf, size);
	if (hex == NULL)
		goto end;

	hex[size] = '\0';

end:
	FREE(buf);
	return hex;
}

void message_compute_signature(char *msg_out, char *signature, size_t len)
{
    unsigned char result[MBEDTLS_MD_MAX_SIZE];
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *md_info;
    
    mbedtls_md_init(&ctx);
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    
    mbedtls_md_setup(&ctx, md_info, 1); // 1 for HMAC
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *)cwmp_ctx.conf.acs_passwd, 
                          CWMP_STRLEN(cwmp_ctx.conf.acs_passwd));
    mbedtls_md_hmac_update(&ctx, (const unsigned char *)msg_out, CWMP_STRLEN(msg_out));
    mbedtls_md_hmac_finish(&ctx, result);
    
    for (int i = 0; i < 20; i++) {
        if (len - CWMP_STRLEN(signature) < 3)
            break;
        snprintf(&(signature[i * 2]), 3, "%02X", result[i]);
    }
    
    mbedtls_md_free(&ctx);
}


void calulate_md5_hash(struct list_head *buff_list, uint8_t *output, size_t outlen)
{
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *md_info;
    unsigned char md_value[MBEDTLS_MD_MAX_SIZE];
    unsigned int bytes;
    
    if (!buff_list || !output)
        return;
    
    mbedtls_md_init(&ctx);
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    
    if (mbedtls_md_setup(&ctx, md_info, 0) != 0) // 0 for no HMAC
        goto end;
    
    mbedtls_md_starts(&ctx);
    
    bin_list_t *iter;
    list_for_each_entry(iter, buff_list, list) {
        mbedtls_md_update(&ctx, iter->bin, iter->len);
    }
    
    mbedtls_md_finish(&ctx, md_value);
    bytes = mbedtls_md_get_size(md_info);
    
    CWMP_MEMCPY(output, &md_value, ((bytes<outlen)?bytes:outlen));
    
end:
    mbedtls_md_free(&ctx);
}

