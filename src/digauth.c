/*
 * digauth.c - HTTP digest authentication utility
 *
 * Copyright (C) 2022, IOPSYS Software Solutions AB.
 *
 * Author: suvendhu.hansa@iopsys.eu
 *
 * See LICENSE file for license related information.
 *
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

#include "log.h"
#include "digauth.h"
#include "ssl_utils.h"
#include "common.h"

#ifdef LMBEDTLS
#include <mbedtls/md5.h>
#define MD5_CTX mbedtls_md5_context
#define MD5_INIT(X) { mbedtls_md5_init(X); mbedtls_md5_starts_ret(X); }
#define MD5_UPDATE(X, Y, Z) mbedtls_md5_update_ret(X, (unsigned char *)Y, Z)
#define MD5_FINAL(X, Y) mbedtls_md5_finish_ret(Y, X)
#elif LOPENSSL
#include <openssl/md5.h>
#define MD5_CTX MD5_CTX
#define MD5_INIT MD5_Init
#define MD5_UPDATE MD5_Update
#define MD5_FINAL MD5_Final
#else
#include <wolfssl/options.h>
#include <wolfssl/openssl/md5.h>
#define MD5_CTX MD5_CTX
#define MD5_INIT MD5_Init
#define MD5_UPDATE MD5_Update
#define MD5_FINAL MD5_Final
#endif

#ifndef MD5_DIGEST_SIZE
#define MD5_DIGEST_SIZE 16
#endif

#define MD5_HASH_HEX_LEN (2 * MD5_DIGEST_SIZE)

char *nonce_key = NULL;

struct parameters {
    char *key;
    char value[2049];
};

enum param_index {
	E_USERNAME,
	E_REALM,
	E_NONCE,
	E_URI,
	E_QOP,
	E_NC,
	E_CNONCE,
	E_RESPONSE,
	__E_MAX
};

struct parameters param[__E_MAX] = {
    { "username", {'\0'} },
    { "realm", {'\0'} },
    { "nonce", {'\0'} },
    { "uri", {'\0'} },
    { "qop", {'\0'} },
    { "nc", {'\0'} },
    { "cnonce", {'\0'} },
    { "response", {'\0'} }
};

static void clear_param_values(void)
{
	unsigned int i;

	for (i = 0; i < (sizeof(param)/sizeof(param[0])); i++) {
		memset(param[i].value, 0, sizeof(param[i].value));
	}
}

static int get_param_index(char *key)
{
	unsigned int i;

	for (i = 0; i < (sizeof(param)/sizeof(param[0])); i++) {
		if (strncmp(key, param[i].key, strlen(param[i].key)) == 0)
			return i;
	}

	return -1;
}

static void strip_lead_trail_char(char *str, char ch)
{
	/* First remove leading strip-char */
	const char* first_valid = str;

	while(*first_valid != '\0' && *first_valid == ch) {
		++first_valid;
	}

	size_t len = strlen(first_valid) + 1;

	memmove(str, first_valid, len);

	/* Now remove trailing strip-char */
	char* end_str = str + strlen(str) - 1;

	while(str < end_str  && *end_str == ch) {
		*end_str = '\0';
		--end_str ;
	}
}

static void get_hexstring(unsigned const char *hash, int len, char *hexstr, int buflen)
{
	int i;

	if (hash == NULL || hexstr == NULL)
		return;

	if (buflen <= len * 2)
		return;

	memset(hexstr, 0, buflen);

	for (i = 0; i < len; ++i) {
		unsigned int j;
		j = (hash[i] >> 4) & 0x0f;
		hexstr[i * 2] = j <= 9 ? (j + '0') : (j + 'a' - 10);
		j = hash[i] & 0x0f;
		hexstr[i * 2 + 1] = j <= 9 ? (j + '0') : (j + 'a' - 10);
	}
	hexstr[len * 2] = '\0';
}

static void get_value_from_header(const char *data)
{
	if (CWMP_STRLEN(data) == 0)
		return;

	int header_len = CWMP_STRLEN(data) + 1;
	char header[header_len];
	memset(header, 0, header_len);
	strncpy(header, data, header_len);

	clear_param_values();

	char *start = strtok(header, ",");
	while (start) {
		char *eq = strchr(start, '=');
		if (eq == NULL)
			return;

		int len = eq - start + 1;
		char key[len];
		snprintf(key, len, "%s", start);
		strip_lead_trail_char(key, ' ');
		strip_lead_trail_char(key, '\"');

		eq = eq + 1;
		char *end = eq + CWMP_STRLEN(eq) - 1;
		len = end - eq + 2;
		char val[len];
		snprintf(val, len, "%s", eq);
		strip_lead_trail_char(val, ' ');
		strip_lead_trail_char(val, '\"');

		int ind = get_param_index(key);
		if (ind >= 0) {
			snprintf(param[ind].value, sizeof(param[ind].value), "%s", val);
		}

		start = strtok(NULL, ",");
	}
}

static void get_digest_ha1(const char *algo, const char *uname, const char *rlm,
			   const char *psw, const char *nonce, const char *cnonce,
			   char *skey, int skey_len)
{
	unsigned char digest[MD5_DIGEST_SIZE];
	MD5_CTX context;

	if (algo == NULL || uname == NULL || rlm == NULL ||
	    psw == NULL || nonce == NULL || cnonce == NULL || skey == NULL)
		return;

	int len = strlen(uname) + strlen(rlm) + strlen(psw) + 3;
	char *a = (char *)calloc(sizeof(char), len);
	if (a == NULL)
		return;

	snprintf(a, len, "%s:%s:%s", uname, rlm, psw);

	MD5_INIT(&context);
	MD5_UPDATE(&context, (unsigned char *)a, strlen(a));
	MD5_FINAL(digest, &context);

	free(a);
	a = NULL;

	if (0 == strcasecmp(algo, "md5-sess")) {
		len = strlen(nonce) + strlen(cnonce) + 3;
		a = (char *)calloc(sizeof(char), len);
		if (a == NULL)
			return;

		snprintf(a, len, ":%s:%s", nonce, cnonce);

		MD5_INIT(&context);
		MD5_UPDATE(&context, (unsigned char *)digest, sizeof(digest));
		MD5_UPDATE(&context, (unsigned char *)a, strlen(a));
		MD5_FINAL(digest, &context);

		free(a);
	}

	get_hexstring(digest, sizeof(digest), skey, skey_len);
}

static void get_digest_ha2(const char *method, const char *uri, char *ha2, int ha2_len)
{
	unsigned char digest[MD5_DIGEST_SIZE];
	MD5_CTX context;

	if (method == NULL || uri == NULL || ha2 == NULL)
		return;

	int len = strlen(method) + strlen(uri) + 2;
	char *a = (char *)calloc(sizeof(char), len);
	if (a == NULL)
		return;

	snprintf(a, len, "%s:%s", method, uri);

	MD5_INIT(&context);
	MD5_UPDATE(&context, (unsigned char *)a, strlen(a));
	MD5_FINAL(digest, &context);

	free(a);

	get_hexstring(digest, sizeof(digest), ha2, ha2_len);
}

static void get_digest_response(const char *ha1, const char *nonce, const char *nonce_cnt,
				const char *cnonce, const char *qop, const char *ha2,
				char *resp, int resp_len)
{
	MD5_CTX context;
	unsigned char digest[MD5_DIGEST_SIZE];

	if (ha1 == NULL || nonce == NULL || nonce_cnt == NULL || cnonce == NULL ||
	    qop == NULL || ha2 == NULL || resp == NULL)
		return;

	int len = strlen(nonce) + 3;
	char *a = (char *)calloc(sizeof(char), len);
	if (a == NULL)
		return;

	snprintf(a, len, ":%s:", nonce);

	if (qop[0] != '\0') {
		len = len + strlen(nonce_cnt) + strlen(cnonce) + strlen(qop) + 3;
		char *b = (char *)calloc(sizeof(char), len);
		if (b == NULL) {
			free(a);
			return;
		}

		snprintf(b, len, "%s%s:%s:%s:", a, nonce_cnt, cnonce, qop);

		free(a);
		a = b;
	}

	MD5_INIT(&context);
	MD5_UPDATE(&context, (unsigned char *)ha1, MD5_HASH_HEX_LEN);
	MD5_UPDATE(&context, (unsigned char *)a, strlen(a));
	MD5_UPDATE(&context, (unsigned char *)ha2, MD5_HASH_HEX_LEN);
	MD5_FINAL(digest, &context);

	free(a);
	get_hexstring(digest, sizeof(digest), resp, resp_len);
}

static void get_nonce(uint32_t time, const char* method, const char *rand,
		      unsigned int rand_size, const char *uri, const char *rlm,
		      char *nonce, unsigned int nonce_size)
{
	unsigned char ts[4];

	if (method == NULL || uri == NULL || rlm == NULL || nonce == NULL)
		return;

	int i;
	for (i = 3; i >= 0; i--) {
		ts[i] = (time >> (8 * (3 - i))) & 0xff;
	}

	char tshex[sizeof(ts) * 2 + 1];
	get_hexstring(ts, sizeof(ts), tshex, sizeof(tshex));

	unsigned int len = strlen(method) + 3;
	char *meth = (char *)calloc(sizeof(char), len);
	if (meth == NULL)
		return;

	snprintf(meth, len, ":%s:", method);

	len = strlen(uri) + strlen(rlm) + 3;
	char *uri_realm = (char *)calloc(sizeof(char), len);
	if (uri_realm == NULL) {
		free(meth);
		return;
	}

	snprintf(uri_realm, len, ":%s:%s", uri, rlm);

	MD5_CTX context;
	unsigned char digest[MD5_DIGEST_SIZE];

	MD5_INIT(&context);
	MD5_UPDATE(&context, (unsigned char *)ts, 4);
	MD5_UPDATE(&context, (unsigned char *)meth, strlen(meth));
	if (rand != NULL && rand_size > 0)
		MD5_UPDATE(&context, (unsigned char *)rand, rand_size);
	MD5_UPDATE(&context, (unsigned char *)uri_realm, strlen(uri_realm));
	MD5_FINAL(digest, &context);

	free(meth);
	free(uri_realm);
	memset(nonce, 0, nonce_size);
	get_hexstring(digest, sizeof(digest), nonce, nonce_size);
	len = nonce_size - strlen(nonce) - 1;
	strncat(nonce, tshex, len);
}

int http_authentication_failure_resp(FILE *fp, const char *http_meth, const char *uri,
				     const char *rlm, const char *opq)
{
	if (fp == NULL || http_meth == NULL || uri == NULL || rlm == NULL || opq == NULL)
		return 0;

	int len;
	char nonce[MD5_HASH_HEX_LEN + 9];
	uint32_t tm;

	tm = (uint32_t)time(NULL);

	len = nonce_key ? strlen(nonce_key) : 0;
	get_nonce(tm, http_meth, nonce_key, len, uri, rlm, nonce, sizeof(nonce));

	if (fprintf(fp, "WWW-Authenticate: Digest realm=\"%s\",qop=\"auth\",nonce=\"%s\",opaque=\"%s\"", rlm, nonce, opq) < 0)
		return 0;

	return 1;
}

int validate_http_digest_auth(const char *http_meth, const char *uri, const char *hdr,
			      const char *rlm, const char *usr, const char *psw,
			      unsigned int timeout)
{
	get_value_from_header(hdr);

	if (strcmp(param[E_USERNAME].value, usr) != 0)
		return 0;

	if (strlen(param[E_REALM].value) == 0)
		return 0;

	if (strcmp(param[E_REALM].value, rlm) != 0)
		return 0;

	if (strlen(param[E_CNONCE].value) == 0)
		return 0;

	if (strlen(param[E_QOP].value) == 0)
		return 0;

	if (strlen(param[E_NC].value) == 0)
		return 0;

	if (strlen(param[E_RESPONSE].value) == 0)
		return 0;

	int len = strlen(param[E_NONCE].value);
	if (len == 0)
		return 0;

	char *tms = param[E_NONCE].value + len - 8;
	uint32_t tm = strtoul(tms, NULL, 16);
	uint32_t cur_tm = (uint32_t)time(NULL);

	if (cur_tm > tm + timeout) {
		CWMP_LOG(ERROR, "Time exceeded the timeout");
		return 0;
	}

	if (nonce_key ==NULL) {
		if (get_nonce_key() != CWMP_OK)
			return -1;
	}

	char nonce[MD5_HASH_HEX_LEN + 9];
	get_nonce(tm, http_meth, nonce_key, strlen(nonce_key), uri, rlm, nonce, sizeof(nonce));

	if (strcmp(param[E_NONCE].value, nonce) != 0) {
		CWMP_LOG(ERROR, "Nonce value is probably fabricated");
		return 0;
	}

	if (strlen(param[E_URI].value) == 0)
		return 0;

	if (strncmp(param[E_URI].value, uri, strlen(uri)) != 0) {
		CWMP_LOG(ERROR, "Authentication failed, URI is not matched");
		return 0;
	}

	if ((strcmp(param[E_QOP].value, "auth") != 0) && (strcmp(param[E_QOP].value, "") != 0))
		return 0;

	char *tmp;
	unsigned long int nc_int = strtoul(param[E_NC].value, &tmp, 16);
	if ((*tmp != '\0') || (nc_int == LONG_MAX && errno == ERANGE)) {
		CWMP_LOG(ERROR, "Authentication failed due to invalid format");
		return 0;
	}

	char ha1[MD5_HASH_HEX_LEN + 1];
	char ha2[MD5_HASH_HEX_LEN + 1];
	char resp[MD5_HASH_HEX_LEN + 1];

	get_digest_ha1("md5", usr, rlm, psw, param[E_NONCE].value, param[E_CNONCE].value, ha1, sizeof(ha1));
	get_digest_ha2(http_meth, uri, ha2, sizeof(ha2));
	get_digest_response(ha1, param[E_NONCE].value, param[E_NC].value, param[E_CNONCE].value,
			    param[E_QOP].value, ha2, resp, sizeof(resp));

	if (strcmp(resp, param[E_RESPONSE].value) != 0)
		return 0;

	return 1;
}

int get_nonce_key(void)
{
	nonce_key = generate_random_string(28);
	if (nonce_key == NULL)
		return CWMP_GEN_ERR;

	return CWMP_OK;
}
