/*
 * ssl_utils.c: Utility functions with ssl
 *
 * Copyright (C) 2022-2023 IOPSYS Software Solutions AB. All rights reserved.
 *
 * See LICENSE file for license related information.
 */

#ifdef LMBEDTLS
#include <mbedtls/md.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#endif
#ifdef LOPENSSL
#include <openssl/hmac.h>
#include <openssl/rand.h>
#endif

#ifdef LWOLFSSL
#include <wolfssl/options.h>
#include <wolfssl/openssl/hmac.h>
#include <wolfssl/openssl/rand.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "ssl_utils.h"
#include "common.h"
#include "log.h"

static int rand_bytes(unsigned char *output, size_t len)
{
#ifdef LMBEDTLS
	mbedtls_entropy_context ec = {0};
	mbedtls_ctr_drbg_context cd_ctx = {0};
	int res = 1;

	union {
		uint64_t seed;
		uint8_t buffer[8];
	} rand_buffer;

	FILE *urand = fopen("/dev/urandom", "r");
	if (urand) {
		size_t bytes = fread(&rand_buffer.seed, 1, sizeof(rand_buffer.seed), urand);
		fclose(urand);
		if (bytes < sizeof(rand_buffer.seed)) {
			CWMP_LOG(INFO, "Failed to seed random [%d::%d]", sizeof(rand_buffer.seed), bytes);
		}
	} else {
		rand_buffer.seed = (uint64_t)clock();
	}

	mbedtls_entropy_init(&ec);
	mbedtls_ctr_drbg_init(&cd_ctx);

	if (mbedtls_ctr_drbg_seed(&cd_ctx, mbedtls_entropy_func, &ec, (const unsigned char *)rand_buffer.buffer, 8) != 0) {
		CWMP_LOG(ERROR, "Failed to initialize random generator");
		res = -1;
		goto end;
	}

	if (mbedtls_ctr_drbg_random(&cd_ctx, output, len) != 0) {
		CWMP_LOG(ERROR, "Failed to generate random bytes");
		res = -1;
	}

end:
	mbedtls_ctr_drbg_free(&cd_ctx);
	mbedtls_entropy_free(&ec);
	return res;
#else
	return RAND_bytes(output, len);
#endif
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
	int result_len = 20;
	struct config *conf;
	conf = &(cwmp_main->conf);

#ifdef LMBEDTLS
	unsigned char result[MBEDTLS_MD_MAX_SIZE] = {0};
	const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);

	mbedtls_md_hmac(md_info, (unsigned char *)conf->acs_passwd, CWMP_STRLEN(conf->acs_passwd), (unsigned char *)msg_out, CWMP_STRLEN(msg_out), result);
#else
	unsigned char result[EVP_MAX_MD_SIZE] = {0};

	HMAC(EVP_sha1(), conf->acs_passwd, CWMP_STRLEN(conf->acs_passwd), (unsigned char *)msg_out, CWMP_STRLEN(msg_out), result, NULL);
#endif

	for (int i = 0; i < result_len; i++) {
		if (len - CWMP_STRLEN(signature) < 3) // each time 2 hex chars + '\0' at end so needed space is 3 bytes
			break;

		snprintf(&(signature[i * 2]), 3, "%02X", result[i]);
	}
}


void calulate_md5_hash(struct list_head *buff_list, uint8_t *output, size_t outlen)
{
	unsigned int bytes = 0;

#ifdef LMBEDTLS
	mbedtls_md_context_t enpctx;
	mbedtls_md_context_t *mdctx = &enpctx;
	const mbedtls_md_info_t *md;
	unsigned char md_value[MBEDTLS_MD_MAX_SIZE];
#else
	EVP_MD_CTX *mdctx;
	const EVP_MD *md;
	unsigned char md_value[EVP_MAX_MD_SIZE];
#endif

	if (!buff_list || !output)
		return;

#ifndef LMBEDTLS
	// makes all algorithms available to the EVP* routines
	OpenSSL_add_all_algorithms();
#endif

#ifdef LMBEDTLS
	md = mbedtls_md_info_from_string("MD5");
	mbedtls_md_init(mdctx);
	mbedtls_md_init_ctx(mdctx, md);
#else
	md = EVP_get_digestbyname("MD5");
	mdctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(mdctx, md, NULL);
#endif

	if (md == NULL)
		goto end;

	bin_list_t *iter;
	list_for_each_entry(iter, buff_list, list) {
#ifdef LMBEDTLS
		mbedtls_md_update(mdctx, iter->bin, iter->len);
#else
		EVP_DigestUpdate(mdctx, iter->bin, iter->len);
#endif
	}

#ifdef LMBEDTLS
	mbedtls_md_finish(mdctx, md_value);
	bytes = mbedtls_md_get_size(md);
#else
	bytes = 0;
	EVP_DigestFinal_ex(mdctx, md_value, &bytes);
#endif

	CWMP_MEMCPY(output, &md_value, ((bytes<outlen)?bytes:outlen));

end:
#ifdef LMBEDTLS
	mbedtls_md_free(mdctx);
#else
	EVP_MD_CTX_destroy(mdctx);
	EVP_cleanup();
#endif
}

