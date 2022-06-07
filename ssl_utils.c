/*
 * ssl_utils.c: Utility functions with ssl
 *
 * Copyright (C) 2022 iopsys Software Solutions AB. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifdef LMBEDTLS
#include <mbedtls/md.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#endif
#ifdef LOPENSSL
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#endif

#ifdef LWOLFSSL
#include <wolfssl/options.h>
#include <wolfssl/openssl/ssl.h>
#endif

#include <string.h>
#include <stdlib.h>

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
	struct cwmp *cwmp = &cwmp_main;
	struct config *conf;
	conf = &(cwmp->conf);

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
