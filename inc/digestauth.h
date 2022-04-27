/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *  HTTP digest auth functions: originally imported from libmicrohttpd
 *
 *	Copyright (C) 2013 Oussama Ghorbel <oussama.ghorbel@pivasoftware.com>
 *
 */

#ifndef DIGESTAUTH_H_
#define DIGESTAUTH_H_

/**
 * MHD-internal return code for "YES".
 */
#define MHD_YES 1

/**
 * MHD-internal return code for "NO".
 */
#define MHD_NO 0

/**
 * MHD digest auth internal code for an invalid nonce.
 */
#define MHD_INVALID_NONCE -1

extern char *nonce_privacy_key;

/**
 * make response to request authentication from the client
 *
 * @param fp
 * @param http_method
 * @param url
 * @param realm the realm presented to the client
 * @param opaque string to user for opaque value
 * @return 'MHD_YES' on success, otherwise 'MHD_NO'
 */
int http_digest_auth_fail_response(FILE *fp, const char *http_method, const char *url, const char *realm, const char *opaque);

/**
 * Authenticates the authorization header sent by the client
 *
 * @param http_method
 * @param url
 * @param header: pointer to the position just after the string "Authorization: Digest "
 * @param realm The realm presented to the client
 * @param username The username needs to be authenticated
 * @param password The password used in the authentication
 * @param nonce_timeout The amount of time for a nonce to be invalid in seconds
 * @return 'MHD_YES' if authenticated, 'MHD_NO' if not, 'MHD_INVALID_NONCE' if nonce is invalid
 */
int http_digest_auth_check(const char *http_method, const char *url, const char *header, const char *realm, const char *username, const char *password, unsigned int nonce_timeout);

int generate_nonce_priv_key(void);

#endif /* DIGESTAUTH_H_ */
