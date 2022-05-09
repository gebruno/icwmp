#ifndef DIGAUTH_H_
#define DIGAUTH_H_

#include <stdio.h>

extern char *nonce_key;

int get_nonce_key(void);
int validate_http_digest_auth(const char *http_meth, const char *uri, const char *hdr,
			      const char *rlm, const char *usr, const char *psw,
			      unsigned int timeout);
int http_authentication_failure_resp(FILE *fp, const char *http_meth, const char *uri,
				     const char *rlm, const char *opq);

#endif /* DIGAUTH_H_ */
