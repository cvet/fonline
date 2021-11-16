/*	$OpenBSD: ssl_ciphers.c,v 1.11 2021/03/11 17:14:46 jsing Exp $ */
/*
 * Copyright (c) 2015-2017 Doug Hogan <doug@openbsd.org>
 * Copyright (c) 2015-2018, 2020 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2019 Theo Buehler <tb@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <openssl/safestack.h>

#include "bytestring.h"
#include "ssl_locl.h"

int
ssl_cipher_in_list(STACK_OF(SSL_CIPHER) *ciphers, const SSL_CIPHER *cipher)
{
	int i;

	for (i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
		if (sk_SSL_CIPHER_value(ciphers, i)->id == cipher->id)
			return 1;
	}

	return 0;
}

int
ssl_cipher_allowed_in_tls_version_range(const SSL_CIPHER *cipher, uint16_t min_ver,
    uint16_t max_ver)
{
	switch(cipher->algorithm_ssl) {
	case SSL_SSLV3:
		return (min_ver <= TLS1_2_VERSION);
	case SSL_TLSV1_2:
		return (min_ver <= TLS1_2_VERSION && TLS1_2_VERSION <= max_ver);
	case SSL_TLSV1_3:
		return (min_ver <= TLS1_3_VERSION && TLS1_3_VERSION <= max_ver);
	}
	return 0;
}

int
ssl_cipher_list_to_bytes(SSL *s, STACK_OF(SSL_CIPHER) *ciphers, CBB *cbb)
{
	SSL_CIPHER *cipher;
	int num_ciphers = 0;
	uint16_t min_vers, max_vers;
	int i;

	if (ciphers == NULL)
		return 0;

	if (!ssl_supported_tls_version_range(s, &min_vers, &max_vers))
		return 0;

	for (i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
		if ((cipher = sk_SSL_CIPHER_value(ciphers, i)) == NULL)
			return 0;
		if (!ssl_cipher_allowed_in_tls_version_range(cipher, min_vers,
		    max_vers))
			continue;
		if (!CBB_add_u16(cbb, ssl3_cipher_get_value(cipher)))
			return 0;

		num_ciphers++;
	}

	/* Add SCSV if there are other ciphers and we're not renegotiating. */
	if (num_ciphers > 0 && !s->internal->renegotiate) {
		if (!CBB_add_u16(cbb, SSL3_CK_SCSV & SSL3_CK_VALUE_MASK))
			return 0;
	}

	if (!CBB_flush(cbb))
		return 0;

	return 1;
}

STACK_OF(SSL_CIPHER) *
ssl_bytes_to_cipher_list(SSL *s, CBS *cbs)
{
	STACK_OF(SSL_CIPHER) *ciphers = NULL;
	const SSL_CIPHER *cipher;
	uint16_t cipher_value;
	unsigned long cipher_id;

	S3I(s)->send_connection_binding = 0;

	if ((ciphers = sk_SSL_CIPHER_new_null()) == NULL) {
		SSLerror(s, ERR_R_MALLOC_FAILURE);
		goto err;
	}

	while (CBS_len(cbs) > 0) {
		if (!CBS_get_u16(cbs, &cipher_value)) {
			SSLerror(s, SSL_R_ERROR_IN_RECEIVED_CIPHER_LIST);
			goto err;
		}

		cipher_id = SSL3_CK_ID | cipher_value;

		if (cipher_id == SSL3_CK_SCSV) {
			/*
			 * TLS_EMPTY_RENEGOTIATION_INFO_SCSV is fatal if
			 * renegotiating.
			 */
			if (s->internal->renegotiate) {
				SSLerror(s, SSL_R_SCSV_RECEIVED_WHEN_RENEGOTIATING);
				ssl3_send_alert(s, SSL3_AL_FATAL,
				    SSL_AD_HANDSHAKE_FAILURE);

				goto err;
			}
			S3I(s)->send_connection_binding = 1;
			continue;
		}

		if (cipher_id == SSL3_CK_FALLBACK_SCSV) {
			/*
			 * TLS_FALLBACK_SCSV indicates that the client
			 * previously tried a higher protocol version.
			 * Fail if the current version is an unexpected
			 * downgrade.
			 */
			if (S3I(s)->hs.negotiated_tls_version <
			    S3I(s)->hs.our_max_tls_version) {
				SSLerror(s, SSL_R_INAPPROPRIATE_FALLBACK);
				ssl3_send_alert(s, SSL3_AL_FATAL,
					SSL_AD_INAPPROPRIATE_FALLBACK);
				goto err;
			}
			continue;
		}

		if ((cipher = ssl3_get_cipher_by_value(cipher_value)) != NULL) {
			if (!sk_SSL_CIPHER_push(ciphers, cipher)) {
				SSLerror(s, ERR_R_MALLOC_FAILURE);
				goto err;
			}
		}
	}

	return (ciphers);

 err:
	sk_SSL_CIPHER_free(ciphers);

	return (NULL);
}

struct ssl_tls13_ciphersuite {
	const char *name;
	const char *alias;
	unsigned long cid;
};

static const struct ssl_tls13_ciphersuite ssl_tls13_ciphersuites[] = {
	{
		.name = TLS1_3_TXT_AES_128_GCM_SHA256,
		.alias = "TLS_AES_128_GCM_SHA256",
		.cid = TLS1_3_CK_AES_128_GCM_SHA256,
	},
	{
		.name = TLS1_3_TXT_AES_256_GCM_SHA384,
		.alias = "TLS_AES_256_GCM_SHA384",
		.cid = TLS1_3_CK_AES_256_GCM_SHA384,
	},
	{
		.name = TLS1_3_TXT_CHACHA20_POLY1305_SHA256,
		.alias = "TLS_CHACHA20_POLY1305_SHA256",
		.cid = TLS1_3_CK_CHACHA20_POLY1305_SHA256,
	},
	{
		.name = TLS1_3_TXT_AES_128_CCM_SHA256,
		.alias = "TLS_AES_128_CCM_SHA256",
		.cid = TLS1_3_CK_AES_128_CCM_SHA256,
	},
	{
		.name = TLS1_3_TXT_AES_128_CCM_8_SHA256,
		.alias = "TLS_AES_128_CCM_8_SHA256",
		.cid = TLS1_3_CK_AES_128_CCM_8_SHA256,
	},
	{
		.name = NULL,
	},
};

int
ssl_parse_ciphersuites(STACK_OF(SSL_CIPHER) **out_ciphers, const char *str)
{
	const struct ssl_tls13_ciphersuite *ciphersuite;
	STACK_OF(SSL_CIPHER) *ciphers;
	const SSL_CIPHER *cipher;
	char *s = NULL;
	char *p, *q;
	int i;
	int ret = 0;

	if ((ciphers = sk_SSL_CIPHER_new_null()) == NULL)
		goto err;

	/* An empty string is valid and means no ciphers. */
	if (strcmp(str, "") == 0)
		goto done;

	if ((s = strdup(str)) == NULL)
		goto err;

	q = s;
	while ((p = strsep(&q, ":")) != NULL) {
		ciphersuite = &ssl_tls13_ciphersuites[0];
		for (i = 0; ciphersuite->name != NULL; i++) {
			if (strcmp(p, ciphersuite->name) == 0)
				break;
			if (strcmp(p, ciphersuite->alias) == 0)
				break;
			ciphersuite = &ssl_tls13_ciphersuites[i];
		}
		if (ciphersuite->name == NULL)
			goto err;

		/* We know about the cipher suite, but it is not supported. */
		if ((cipher = ssl3_get_cipher_by_id(ciphersuite->cid)) == NULL)
			continue;

		if (!sk_SSL_CIPHER_push(ciphers, cipher))
			goto err;
	}

 done:
	sk_SSL_CIPHER_free(*out_ciphers);
	*out_ciphers = ciphers;
	ciphers = NULL;
	ret = 1;

 err:
	sk_SSL_CIPHER_free(ciphers);
	free(s);

	return ret;
}

int
ssl_merge_cipherlists(STACK_OF(SSL_CIPHER) *cipherlist,
    STACK_OF(SSL_CIPHER) *cipherlist_tls13,
    STACK_OF(SSL_CIPHER) **out_cipherlist)
{
	STACK_OF(SSL_CIPHER) *ciphers = NULL;
	const SSL_CIPHER *cipher;
	int i, ret = 0;

	if ((ciphers = sk_SSL_CIPHER_dup(cipherlist_tls13)) == NULL)
		goto err;
	for (i = 0; i < sk_SSL_CIPHER_num(cipherlist); i++) {
		cipher = sk_SSL_CIPHER_value(cipherlist, i);
		if (cipher->algorithm_ssl == SSL_TLSV1_3)
			continue;
		if (!sk_SSL_CIPHER_push(ciphers, cipher))
			goto err;
	}

	sk_SSL_CIPHER_free(*out_cipherlist);
	*out_cipherlist = ciphers;
	ciphers = NULL;

	ret = 1;

 err:
	sk_SSL_CIPHER_free(ciphers);

	return ret;
}
