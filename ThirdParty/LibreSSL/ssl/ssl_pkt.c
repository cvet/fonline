/* $OpenBSD: ssl_pkt.c,v 1.50 2021/08/30 19:25:43 jsing Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <errno.h>
#include <stdio.h>

#include <openssl/buffer.h>
#include <openssl/evp.h>

#include "bytestring.h"
#include "dtls_locl.h"
#include "ssl_locl.h"

static int do_ssl3_write(SSL *s, int type, const unsigned char *buf,
    unsigned int len);
static int ssl3_get_record(SSL *s);

/*
 * Force a WANT_READ return for certain error conditions where
 * we don't want to spin internally.
 */
static void
ssl_force_want_read(SSL *s)
{
	BIO * bio;

	bio = SSL_get_rbio(s);
	BIO_clear_retry_flags(bio);
	BIO_set_retry_read(bio);
	s->internal->rwstate = SSL_READING;
}

/*
 * If extend == 0, obtain new n-byte packet; if extend == 1, increase
 * packet by another n bytes.
 * The packet will be in the sub-array of S3I(s)->rbuf.buf specified
 * by s->internal->packet and s->internal->packet_length.
 * (If s->internal->read_ahead is set, 'max' bytes may be stored in rbuf
 * [plus s->internal->packet_length bytes if extend == 1].)
 */
static int
ssl3_read_n(SSL *s, int n, int max, int extend)
{
	SSL3_BUFFER_INTERNAL *rb = &(S3I(s)->rbuf);
	int i, len, left;
	size_t align;
	unsigned char *pkt;

	if (n <= 0)
		return n;

	if (rb->buf == NULL)
		if (!ssl3_setup_read_buffer(s))
			return -1;

	left = rb->left;
	align = (size_t)rb->buf + SSL3_RT_HEADER_LENGTH;
	align = (-align) & (SSL3_ALIGN_PAYLOAD - 1);

	if (!extend) {
		/* start with empty packet ... */
		if (left == 0)
			rb->offset = align;
		else if (align != 0 && left >= SSL3_RT_HEADER_LENGTH) {
			/* check if next packet length is large
			 * enough to justify payload alignment... */
			pkt = rb->buf + rb->offset;
			if (pkt[0] == SSL3_RT_APPLICATION_DATA &&
			    (pkt[3]<<8|pkt[4]) >= 128) {
				/* Note that even if packet is corrupted
				 * and its length field is insane, we can
				 * only be led to wrong decision about
				 * whether memmove will occur or not.
				 * Header values has no effect on memmove
				 * arguments and therefore no buffer
				 * overrun can be triggered. */
				memmove(rb->buf + align, pkt, left);
				rb->offset = align;
			}
		}
		s->internal->packet = rb->buf + rb->offset;
		s->internal->packet_length = 0;
		/* ... now we can act as if 'extend' was set */
	}

	/* For DTLS/UDP reads should not span multiple packets
	 * because the read operation returns the whole packet
	 * at once (as long as it fits into the buffer). */
	if (SSL_is_dtls(s)) {
		if (left > 0 && n > left)
			n = left;
	}

	/* if there is enough in the buffer from a previous read, take some */
	if (left >= n) {
		s->internal->packet_length += n;
		rb->left = left - n;
		rb->offset += n;
		return (n);
	}

	/* else we need to read more data */

	len = s->internal->packet_length;
	pkt = rb->buf + align;
	/* Move any available bytes to front of buffer:
	 * 'len' bytes already pointed to by 'packet',
	 * 'left' extra ones at the end */
	if (s->internal->packet != pkt)  {
		/* len > 0 */
		memmove(pkt, s->internal->packet, len + left);
		s->internal->packet = pkt;
		rb->offset = len + align;
	}

	if (n > (int)(rb->len - rb->offset)) {
		/* does not happen */
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		return -1;
	}

	if (s->internal->read_ahead || SSL_is_dtls(s)) {
		if (max < n)
			max = n;
		if (max > (int)(rb->len - rb->offset))
			max = rb->len - rb->offset;
	} else {
		/* ignore max parameter */
		max = n;
	}

	while (left < n) {
		/* Now we have len+left bytes at the front of S3I(s)->rbuf.buf
		 * and need to read in more until we have len+n (up to
		 * len+max if possible) */

		errno = 0;
		if (s->rbio != NULL) {
			s->internal->rwstate = SSL_READING;
			i = BIO_read(s->rbio, pkt + len + left, max - left);
		} else {
			SSLerror(s, SSL_R_READ_BIO_NOT_SET);
			i = -1;
		}

		if (i <= 0) {
			rb->left = left;
			if (s->internal->mode & SSL_MODE_RELEASE_BUFFERS &&
			    !SSL_is_dtls(s)) {
				if (len + left == 0)
					ssl3_release_read_buffer(s);
			}
			return (i);
		}
		left += i;

		/*
		 * reads should *never* span multiple packets for DTLS because
		 * the underlying transport protocol is message oriented as
		 * opposed to byte oriented as in the TLS case.
		 */
		if (SSL_is_dtls(s)) {
			if (n > left)
				n = left; /* makes the while condition false */
		}
	}

	/* done reading, now the book-keeping */
	rb->offset += n;
	rb->left = left - n;
	s->internal->packet_length += n;
	s->internal->rwstate = SSL_NOTHING;

	return (n);
}

int
ssl3_packet_read(SSL *s, int plen)
{
	int n;

	n = ssl3_read_n(s, plen, S3I(s)->rbuf.len, 0);
	if (n <= 0)
		return n;
	if (s->internal->packet_length < plen)
		return s->internal->packet_length;

	return plen;
}

int
ssl3_packet_extend(SSL *s, int plen)
{
	int rlen, n;

	if (s->internal->packet_length >= plen)
		return plen;
	rlen = plen - s->internal->packet_length;

	n = ssl3_read_n(s, rlen, rlen, 1);
	if (n <= 0)
		return n;
	if (s->internal->packet_length < plen)
		return s->internal->packet_length;

	return plen;
}

/* Call this to get a new input record.
 * It will return <= 0 if more data is needed, normally due to an error
 * or non-blocking IO.
 * When it finishes, one packet has been decoded and can be found in
 * ssl->s3->internal->rrec.type    - is the type of record
 * ssl->s3->internal->rrec.data, 	 - data
 * ssl->s3->internal->rrec.length, - number of bytes
 */
/* used only by ssl3_read_bytes */
static int
ssl3_get_record(SSL *s)
{
	SSL3_BUFFER_INTERNAL *rb = &(S3I(s)->rbuf);
	SSL3_RECORD_INTERNAL *rr = &(S3I(s)->rrec);
	uint8_t alert_desc;
	uint8_t *out;
	size_t out_len;
	int al, n;
	int ret = -1;

 again:
	/* check if we have the header */
	if ((s->internal->rstate != SSL_ST_READ_BODY) ||
	    (s->internal->packet_length < SSL3_RT_HEADER_LENGTH)) {
		CBS header;
		uint16_t len, ssl_version;
		uint8_t type;

		n = ssl3_packet_read(s, SSL3_RT_HEADER_LENGTH);
		if (n <= 0)
			return (n);

		s->internal->mac_packet = 1;
		s->internal->rstate = SSL_ST_READ_BODY;

		if (s->server && s->internal->first_packet) {
			if ((ret = ssl_server_legacy_first_packet(s)) != 1)
				return (ret);
			ret = -1;
		}

		CBS_init(&header, s->internal->packet, SSL3_RT_HEADER_LENGTH);

		/* Pull apart the header into the SSL3_RECORD_INTERNAL */
		if (!CBS_get_u8(&header, &type) ||
		    !CBS_get_u16(&header, &ssl_version) ||
		    !CBS_get_u16(&header, &len)) {
			SSLerror(s, SSL_R_BAD_PACKET_LENGTH);
			goto err;
		}

		rr->type = type;
		rr->length = len;

		/* Lets check version */
		if (!s->internal->first_packet && ssl_version != s->version) {
			if ((s->version & 0xFF00) == (ssl_version & 0xFF00) &&
			    !tls12_record_layer_write_protected(s->internal->rl)) {
				/* Send back error using their minor version number :-) */
				s->version = ssl_version;
			}
			SSLerror(s, SSL_R_WRONG_VERSION_NUMBER);
			al = SSL_AD_PROTOCOL_VERSION;
			goto fatal_err;
		}

		if ((ssl_version >> 8) != SSL3_VERSION_MAJOR) {
			SSLerror(s, SSL_R_WRONG_VERSION_NUMBER);
			goto err;
		}

		if (rr->length > rb->len - SSL3_RT_HEADER_LENGTH) {
			al = SSL_AD_RECORD_OVERFLOW;
			SSLerror(s, SSL_R_PACKET_LENGTH_TOO_LONG);
			goto fatal_err;
		}
	}

	n = ssl3_packet_extend(s, SSL3_RT_HEADER_LENGTH + rr->length);
	if (n <= 0)
		return (n);
	if (n != SSL3_RT_HEADER_LENGTH + rr->length)
		return (n);

	s->internal->rstate = SSL_ST_READ_HEADER; /* set state for later operations */

	/*
	 * A full record has now been read from the wire, which now needs
	 * to be processed.
	 */
	tls12_record_layer_set_version(s->internal->rl, s->version);

	if (!tls12_record_layer_open_record(s->internal->rl, s->internal->packet,
	    s->internal->packet_length, &out, &out_len)) {
		tls12_record_layer_alert(s->internal->rl, &alert_desc);

		if (alert_desc == 0)
			goto err;

		if (alert_desc == SSL_AD_RECORD_OVERFLOW)
			SSLerror(s, SSL_R_ENCRYPTED_LENGTH_TOO_LONG);
		else if (alert_desc == SSL_AD_BAD_RECORD_MAC)
			SSLerror(s, SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC);

		al = alert_desc;
		goto fatal_err;
	}

	rr->data = out;
	rr->length = out_len;
	rr->off = 0;

	/* we have pulled in a full packet so zero things */
	s->internal->packet_length = 0;

	if (rr->length == 0) {
		/*
		 * Zero-length fragments are only permitted for application
		 * data, as per RFC 5246 section 6.2.1.
		 */
		if (rr->type != SSL3_RT_APPLICATION_DATA) {
			SSLerror(s, SSL_R_BAD_LENGTH);
			al = SSL_AD_UNEXPECTED_MESSAGE;
			goto fatal_err;
		}

		/*
		 * CBC countermeasures for known IV weaknesses can legitimately
		 * insert a single empty record, so we allow ourselves to read
		 * once past a single empty record without forcing want_read.
		 */
		if (s->internal->empty_record_count++ > SSL_MAX_EMPTY_RECORDS) {
			SSLerror(s, SSL_R_PEER_BEHAVING_BADLY);
			return -1;
		}
		if (s->internal->empty_record_count > 1) {
			ssl_force_want_read(s);
			return -1;
		}
		goto again;
	}

	s->internal->empty_record_count = 0;

	return (1);

 fatal_err:
	ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
	return (ret);
}

/* Call this to write data in records of type 'type'
 * It will return <= 0 if not all data has been sent or non-blocking IO.
 */
int
ssl3_write_bytes(SSL *s, int type, const void *buf_, int len)
{
	const unsigned char *buf = buf_;
	unsigned int tot, n, nw;
	int i;

	if (len < 0) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		return -1;
	}

	s->internal->rwstate = SSL_NOTHING;
	tot = S3I(s)->wnum;
	S3I(s)->wnum = 0;

	if (SSL_in_init(s) && !s->internal->in_handshake) {
		i = s->internal->handshake_func(s);
		if (i < 0)
			return (i);
		if (i == 0) {
			SSLerror(s, SSL_R_SSL_HANDSHAKE_FAILURE);
			return -1;
		}
	}

	if (len < tot)
		len = tot;
	n = (len - tot);
	for (;;) {
		if (n > s->max_send_fragment)
			nw = s->max_send_fragment;
		else
			nw = n;

		i = do_ssl3_write(s, type, &(buf[tot]), nw);
		if (i <= 0) {
			S3I(s)->wnum = tot;
			return i;
		}

		if ((i == (int)n) || (type == SSL3_RT_APPLICATION_DATA &&
		    (s->internal->mode & SSL_MODE_ENABLE_PARTIAL_WRITE))) {
			/*
			 * Next chunk of data should get another prepended
			 * empty fragment in ciphersuites with known-IV
			 * weakness.
			 */
			S3I(s)->empty_fragment_done = 0;

			return tot + i;
		}

		n -= i;
		tot += i;
	}
}

static int
do_ssl3_write(SSL *s, int type, const unsigned char *buf, unsigned int len)
{
	SSL3_BUFFER_INTERNAL *wb = &(S3I(s)->wbuf);
	SSL_SESSION *sess = s->session;
	int need_empty_fragment = 0;
	size_t align, out_len;
	uint16_t version;
	CBB cbb;
	int ret;

	memset(&cbb, 0, sizeof(cbb));

	if (wb->buf == NULL)
		if (!ssl3_setup_write_buffer(s))
			return -1;

	/*
	 * First check if there is a SSL3_BUFFER_INTERNAL still being written
	 * out.  This will happen with non blocking IO.
	 */
	if (wb->left != 0)
		return (ssl3_write_pending(s, type, buf, len));

	/* If we have an alert to send, let's send it. */
	if (S3I(s)->alert_dispatch) {
		if ((ret = ssl3_dispatch_alert(s)) <= 0)
			return (ret);
		/* If it went, fall through and send more stuff. */

		/* We may have released our buffer, if so get it again. */
		if (wb->buf == NULL)
			if (!ssl3_setup_write_buffer(s))
				return -1;
	}

	if (len == 0)
		return 0;

	/*
	 * Some servers hang if initial client hello is larger than 256
	 * bytes and record version number > TLS 1.0.
	 */
	version = s->version;
	if (S3I(s)->hs.state == SSL3_ST_CW_CLNT_HELLO_B &&
	    !s->internal->renegotiate &&
	    S3I(s)->hs.our_max_tls_version > TLS1_VERSION)
		version = TLS1_VERSION;

	/*
	 * Countermeasure against known-IV weakness in CBC ciphersuites
	 * (see http://www.openssl.org/~bodo/tls-cbc.txt). Note that this
	 * is unnecessary for AEAD.
	 */
	if (sess != NULL && tls12_record_layer_write_protected(s->internal->rl)) {
		if (S3I(s)->need_empty_fragments &&
		    !S3I(s)->empty_fragment_done &&
		    type == SSL3_RT_APPLICATION_DATA)
			need_empty_fragment = 1;
	}

	/*
	 * An extra fragment would be a couple of cipher blocks, which would
	 * be a multiple of SSL3_ALIGN_PAYLOAD, so if we want to align the real
	 * payload, then we can just simply pretend we have two headers.
	 */
	align = (size_t)wb->buf + SSL3_RT_HEADER_LENGTH;
	if (need_empty_fragment)
		align += SSL3_RT_HEADER_LENGTH;
	align = (-align) & (SSL3_ALIGN_PAYLOAD - 1);
	wb->offset = align;

	if (!CBB_init_fixed(&cbb, wb->buf + align, wb->len - align))
		goto err;

	tls12_record_layer_set_version(s->internal->rl, version);

	if (need_empty_fragment) {
		if (!tls12_record_layer_seal_record(s->internal->rl, type,
		    buf, 0, &cbb))
			goto err;
		S3I(s)->empty_fragment_done = 1;
	}

	if (!tls12_record_layer_seal_record(s->internal->rl, type, buf, len, &cbb))
		goto err;

	if (!CBB_finish(&cbb, NULL, &out_len))
		goto err;

	wb->left = out_len;

	/*
	 * Memorize arguments so that ssl3_write_pending can detect
	 * bad write retries later.
	 */
	S3I(s)->wpend_tot = len;
	S3I(s)->wpend_buf = buf;
	S3I(s)->wpend_type = type;
	S3I(s)->wpend_ret = len;

	/* We now just need to write the buffer. */
	return ssl3_write_pending(s, type, buf, len);

 err:
	CBB_cleanup(&cbb);

	return -1;
}

/* if S3I(s)->wbuf.left != 0, we need to call this */
int
ssl3_write_pending(SSL *s, int type, const unsigned char *buf, unsigned int len)
{
	int i;
	SSL3_BUFFER_INTERNAL *wb = &(S3I(s)->wbuf);

	/* XXXX */
	if ((S3I(s)->wpend_tot > (int)len) || ((S3I(s)->wpend_buf != buf) &&
	    !(s->internal->mode & SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER)) ||
	    (S3I(s)->wpend_type != type)) {
		SSLerror(s, SSL_R_BAD_WRITE_RETRY);
		return (-1);
	}

	for (;;) {
		errno = 0;
		if (s->wbio != NULL) {
			s->internal->rwstate = SSL_WRITING;
			i = BIO_write(s->wbio, (char *)&(wb->buf[wb->offset]),
			    (unsigned int)wb->left);
		} else {
			SSLerror(s, SSL_R_BIO_NOT_SET);
			i = -1;
		}
		if (i == wb->left) {
			wb->left = 0;
			wb->offset += i;
			if (s->internal->mode & SSL_MODE_RELEASE_BUFFERS &&
			    !SSL_is_dtls(s))
				ssl3_release_write_buffer(s);
			s->internal->rwstate = SSL_NOTHING;
			return (S3I(s)->wpend_ret);
		} else if (i <= 0) {
			/*
			 * For DTLS, just drop it. That's kind of the
			 * whole point in using a datagram service.
			 */
			if (SSL_is_dtls(s))
				wb->left = 0;
			return (i);
		}
		wb->offset += i;
		wb->left -= i;
	}
}

/* Return up to 'len' payload bytes received in 'type' records.
 * 'type' is one of the following:
 *
 *   -  SSL3_RT_HANDSHAKE (when ssl3_get_message calls us)
 *   -  SSL3_RT_APPLICATION_DATA (when ssl3_read calls us)
 *   -  0 (during a shutdown, no data has to be returned)
 *
 * If we don't have stored data to work from, read a SSL/TLS record first
 * (possibly multiple records if we still don't have anything to return).
 *
 * This function must handle any surprises the peer may have for us, such as
 * Alert records (e.g. close_notify), ChangeCipherSpec records (not really
 * a surprise, but handled as if it were), or renegotiation requests.
 * Also if record payloads contain fragments too small to process, we store
 * them until there is enough for the respective protocol (the record protocol
 * may use arbitrary fragmentation and even interleaving):
 *     Change cipher spec protocol
 *             just 1 byte needed, no need for keeping anything stored
 *     Alert protocol
 *             2 bytes needed (AlertLevel, AlertDescription)
 *     Handshake protocol
 *             4 bytes needed (HandshakeType, uint24 length) -- we just have
 *             to detect unexpected Client Hello and Hello Request messages
 *             here, anything else is handled by higher layers
 *     Application data protocol
 *             none of our business
 */
int
ssl3_read_bytes(SSL *s, int type, unsigned char *buf, int len, int peek)
{
	int al, i, ret, rrcount = 0;
	unsigned int n;
	SSL3_RECORD_INTERNAL *rr;

	if (S3I(s)->rbuf.buf == NULL) /* Not initialized yet */
		if (!ssl3_setup_read_buffer(s))
			return (-1);

	if (len < 0) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		return -1;
	}

	if ((type && type != SSL3_RT_APPLICATION_DATA &&
	    type != SSL3_RT_HANDSHAKE) ||
	    (peek && (type != SSL3_RT_APPLICATION_DATA))) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		return -1;
	}

	if ((type == SSL3_RT_HANDSHAKE) &&
	    (S3I(s)->handshake_fragment_len > 0)) {
		/* (partially) satisfy request from storage */
		unsigned char *src = S3I(s)->handshake_fragment;
		unsigned char *dst = buf;
		unsigned int k;

		/* peek == 0 */
		n = 0;
		while ((len > 0) && (S3I(s)->handshake_fragment_len > 0)) {
			*dst++ = *src++;
			len--;
			S3I(s)->handshake_fragment_len--;
			n++;
		}
		/* move any remaining fragment bytes: */
		for (k = 0; k < S3I(s)->handshake_fragment_len; k++)
			S3I(s)->handshake_fragment[k] = *src++;
		return n;
	}

	/*
	 * Now S3I(s)->handshake_fragment_len == 0 if
	 * type == SSL3_RT_HANDSHAKE.
	 */
	if (!s->internal->in_handshake && SSL_in_init(s)) {
		/* type == SSL3_RT_APPLICATION_DATA */
		i = s->internal->handshake_func(s);
		if (i < 0)
			return (i);
		if (i == 0) {
			SSLerror(s, SSL_R_SSL_HANDSHAKE_FAILURE);
			return (-1);
		}
	}

 start:
	/*
	 * Do not process more than three consecutive records, otherwise the
	 * peer can cause us to loop indefinitely. Instead, return with an
	 * SSL_ERROR_WANT_READ so the caller can choose when to handle further
	 * processing. In the future, the total number of non-handshake and
	 * non-application data records per connection should probably also be
	 * limited...
	 */
	if (rrcount++ >= 3) {
		ssl_force_want_read(s);
		return -1;
	}

	s->internal->rwstate = SSL_NOTHING;

	/*
	 * S3I(s)->rrec.type	    - is the type of record
	 * S3I(s)->rrec.data,    - data
	 * S3I(s)->rrec.off,     - offset into 'data' for next read
	 * S3I(s)->rrec.length,  - number of bytes.
	 */
	rr = &(S3I(s)->rrec);

	/* get new packet if necessary */
	if ((rr->length == 0) || (s->internal->rstate == SSL_ST_READ_BODY)) {
		ret = ssl3_get_record(s);
		if (ret <= 0)
			return (ret);
	}

	/* we now have a packet which can be read and processed */

	if (S3I(s)->change_cipher_spec /* set when we receive ChangeCipherSpec,
	                               * reset by ssl3_get_finished */
	    && (rr->type != SSL3_RT_HANDSHAKE)) {
		al = SSL_AD_UNEXPECTED_MESSAGE;
		SSLerror(s, SSL_R_DATA_BETWEEN_CCS_AND_FINISHED);
		goto fatal_err;
	}

	/* If the other end has shut down, throw anything we read away
	 * (even in 'peek' mode) */
	if (s->internal->shutdown & SSL_RECEIVED_SHUTDOWN) {
		rr->length = 0;
		s->internal->rwstate = SSL_NOTHING;
		return (0);
	}


	/* SSL3_RT_APPLICATION_DATA or SSL3_RT_HANDSHAKE */
	if (type == rr->type) {
		/* make sure that we are not getting application data when we
		 * are doing a handshake for the first time */
		if (SSL_in_init(s) && type == SSL3_RT_APPLICATION_DATA &&
		    !tls12_record_layer_read_protected(s->internal->rl)) {
			al = SSL_AD_UNEXPECTED_MESSAGE;
			SSLerror(s, SSL_R_APP_DATA_IN_HANDSHAKE);
			goto fatal_err;
		}

		if (len <= 0)
			return (len);

		if ((unsigned int)len > rr->length)
			n = rr->length;
		else
			n = (unsigned int)len;

		memcpy(buf, &(rr->data[rr->off]), n);
		if (!peek) {
			memset(&(rr->data[rr->off]), 0, n);
			rr->length -= n;
			rr->off += n;
			if (rr->length == 0) {
				s->internal->rstate = SSL_ST_READ_HEADER;
				rr->off = 0;
				if (s->internal->mode & SSL_MODE_RELEASE_BUFFERS &&
				    S3I(s)->rbuf.left == 0)
					ssl3_release_read_buffer(s);
			}
		}
		return (n);
	}


	/* If we get here, then type != rr->type; if we have a handshake
	 * message, then it was unexpected (Hello Request or Client Hello). */

	{
		/*
		 * In case of record types for which we have 'fragment'
		 * storage, * fill that so that we can process the data
		 * at a fixed place.
		 */
		unsigned int dest_maxlen = 0;
		unsigned char *dest = NULL;
		unsigned int *dest_len = NULL;

		if (rr->type == SSL3_RT_HANDSHAKE) {
			dest_maxlen = sizeof S3I(s)->handshake_fragment;
			dest = S3I(s)->handshake_fragment;
			dest_len = &S3I(s)->handshake_fragment_len;
		} else if (rr->type == SSL3_RT_ALERT) {
			dest_maxlen = sizeof S3I(s)->alert_fragment;
			dest = S3I(s)->alert_fragment;
			dest_len = &S3I(s)->alert_fragment_len;
		}
		if (dest_maxlen > 0) {
			/* available space in 'dest' */
			n = dest_maxlen - *dest_len;
			if (rr->length < n)
				n = rr->length; /* available bytes */

			/* now move 'n' bytes: */
			while (n-- > 0) {
				dest[(*dest_len)++] = rr->data[rr->off++];
				rr->length--;
			}

			if (*dest_len < dest_maxlen)
				goto start; /* fragment was too small */
		}
	}

	/* S3I(s)->handshake_fragment_len == 4  iff  rr->type == SSL3_RT_HANDSHAKE;
	 * S3I(s)->alert_fragment_len == 2      iff  rr->type == SSL3_RT_ALERT.
	 * (Possibly rr is 'empty' now, i.e. rr->length may be 0.) */

	/* If we are a client, check for an incoming 'Hello Request': */
	if ((!s->server) && (S3I(s)->handshake_fragment_len >= 4) &&
	    (S3I(s)->handshake_fragment[0] == SSL3_MT_HELLO_REQUEST) &&
	    (s->session != NULL) && (s->session->cipher != NULL)) {
		S3I(s)->handshake_fragment_len = 0;

		if ((S3I(s)->handshake_fragment[1] != 0) ||
		    (S3I(s)->handshake_fragment[2] != 0) ||
		    (S3I(s)->handshake_fragment[3] != 0)) {
			al = SSL_AD_DECODE_ERROR;
			SSLerror(s, SSL_R_BAD_HELLO_REQUEST);
			goto fatal_err;
		}

		ssl_msg_callback(s, 0, SSL3_RT_HANDSHAKE,
		    S3I(s)->handshake_fragment, 4);

		if (SSL_is_init_finished(s) &&
		    !(s->s3->flags & SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS) &&
		    !S3I(s)->renegotiate) {
			ssl3_renegotiate(s);
			if (ssl3_renegotiate_check(s)) {
				i = s->internal->handshake_func(s);
				if (i < 0)
					return (i);
				if (i == 0) {
					SSLerror(s, SSL_R_SSL_HANDSHAKE_FAILURE);
					return (-1);
				}

				if (!(s->internal->mode & SSL_MODE_AUTO_RETRY)) {
					if (S3I(s)->rbuf.left == 0) {
						/* no read-ahead left? */
			/* In the case where we try to read application data,
			 * but we trigger an SSL handshake, we return -1 with
			 * the retry option set.  Otherwise renegotiation may
			 * cause nasty problems in the blocking world */
						ssl_force_want_read(s);
						return (-1);
					}
				}
			}
		}
		/* we either finished a handshake or ignored the request,
		 * now try again to obtain the (application) data we were asked for */
		goto start;
	}
	/* Disallow client initiated renegotiation if configured. */
	if (s->server && SSL_is_init_finished(s) &&
	    S3I(s)->handshake_fragment_len >= 4 &&
	    S3I(s)->handshake_fragment[0] == SSL3_MT_CLIENT_HELLO &&
	    (s->internal->options & SSL_OP_NO_CLIENT_RENEGOTIATION)) {
		al = SSL_AD_NO_RENEGOTIATION;
		goto fatal_err;
	}
	/* If we are a server and get a client hello when renegotiation isn't
	 * allowed send back a no renegotiation alert and carry on.
	 * WARNING: experimental code, needs reviewing (steve)
	 */
	if (s->server &&
	    SSL_is_init_finished(s) &&
	    !S3I(s)->send_connection_binding &&
	    (S3I(s)->handshake_fragment_len >= 4) &&
	    (S3I(s)->handshake_fragment[0] == SSL3_MT_CLIENT_HELLO) &&
	    (s->session != NULL) && (s->session->cipher != NULL)) {
		/*S3I(s)->handshake_fragment_len = 0;*/
		rr->length = 0;
		ssl3_send_alert(s, SSL3_AL_WARNING, SSL_AD_NO_RENEGOTIATION);
		goto start;
	}
	if (S3I(s)->alert_fragment_len >= 2) {
		int alert_level = S3I(s)->alert_fragment[0];
		int alert_descr = S3I(s)->alert_fragment[1];

		S3I(s)->alert_fragment_len = 0;

		ssl_msg_callback(s, 0, SSL3_RT_ALERT,
		    S3I(s)->alert_fragment, 2);

		ssl_info_callback(s, SSL_CB_READ_ALERT,
		    (alert_level << 8) | alert_descr);

		if (alert_level == SSL3_AL_WARNING) {
			S3I(s)->warn_alert = alert_descr;
			if (alert_descr == SSL_AD_CLOSE_NOTIFY) {
				s->internal->shutdown |= SSL_RECEIVED_SHUTDOWN;
				return (0);
			}
			/* This is a warning but we receive it if we requested
			 * renegotiation and the peer denied it. Terminate with
			 * a fatal alert because if application tried to
			 * renegotiatie it presumably had a good reason and
			 * expects it to succeed.
			 *
			 * In future we might have a renegotiation where we
			 * don't care if the peer refused it where we carry on.
			 */
			else if (alert_descr == SSL_AD_NO_RENEGOTIATION) {
				al = SSL_AD_HANDSHAKE_FAILURE;
				SSLerror(s, SSL_R_NO_RENEGOTIATION);
				goto fatal_err;
			}
		} else if (alert_level == SSL3_AL_FATAL) {
			s->internal->rwstate = SSL_NOTHING;
			S3I(s)->fatal_alert = alert_descr;
			SSLerror(s, SSL_AD_REASON_OFFSET + alert_descr);
			ERR_asprintf_error_data("SSL alert number %d",
			    alert_descr);
			s->internal->shutdown |= SSL_RECEIVED_SHUTDOWN;
			SSL_CTX_remove_session(s->ctx, s->session);
			return (0);
		} else {
			al = SSL_AD_ILLEGAL_PARAMETER;
			SSLerror(s, SSL_R_UNKNOWN_ALERT_TYPE);
			goto fatal_err;
		}

		goto start;
	}

	if (s->internal->shutdown & SSL_SENT_SHUTDOWN) {
		/* but we have not received a shutdown */
		s->internal->rwstate = SSL_NOTHING;
		rr->length = 0;
		return (0);
	}

	if (rr->type == SSL3_RT_CHANGE_CIPHER_SPEC) {
		/* 'Change Cipher Spec' is just a single byte, so we know
		 * exactly what the record payload has to look like */
		if ((rr->length != 1) || (rr->off != 0) ||
			(rr->data[0] != SSL3_MT_CCS)) {
			al = SSL_AD_ILLEGAL_PARAMETER;
			SSLerror(s, SSL_R_BAD_CHANGE_CIPHER_SPEC);
			goto fatal_err;
		}

		/* Check we have a cipher to change to */
		if (S3I(s)->hs.cipher == NULL) {
			al = SSL_AD_UNEXPECTED_MESSAGE;
			SSLerror(s, SSL_R_CCS_RECEIVED_EARLY);
			goto fatal_err;
		}

		/* Check that we should be receiving a Change Cipher Spec. */
		if (!(s->s3->flags & SSL3_FLAGS_CCS_OK)) {
			al = SSL_AD_UNEXPECTED_MESSAGE;
			SSLerror(s, SSL_R_CCS_RECEIVED_EARLY);
			goto fatal_err;
		}
		s->s3->flags &= ~SSL3_FLAGS_CCS_OK;

		rr->length = 0;

		ssl_msg_callback(s, 0, SSL3_RT_CHANGE_CIPHER_SPEC, rr->data, 1);

		S3I(s)->change_cipher_spec = 1;
		if (!ssl3_do_change_cipher_spec(s))
			goto err;
		else
			goto start;
	}

	/* Unexpected handshake message (Client Hello, or protocol violation) */
	if ((S3I(s)->handshake_fragment_len >= 4) && !s->internal->in_handshake) {
		if (((S3I(s)->hs.state&SSL_ST_MASK) == SSL_ST_OK) &&
		    !(s->s3->flags & SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS)) {
			S3I(s)->hs.state = s->server ? SSL_ST_ACCEPT : SSL_ST_CONNECT;
			s->internal->renegotiate = 1;
			s->internal->new_session = 1;
		}
		i = s->internal->handshake_func(s);
		if (i < 0)
			return (i);
		if (i == 0) {
			SSLerror(s, SSL_R_SSL_HANDSHAKE_FAILURE);
			return (-1);
		}

		if (!(s->internal->mode & SSL_MODE_AUTO_RETRY)) {
			if (S3I(s)->rbuf.left == 0) { /* no read-ahead left? */
				/* In the case where we try to read application data,
				 * but we trigger an SSL handshake, we return -1 with
				 * the retry option set.  Otherwise renegotiation may
				 * cause nasty problems in the blocking world */
				ssl_force_want_read(s);
				return (-1);
			}
		}
		goto start;
	}

	switch (rr->type) {
	default:
		/*
		 * TLS up to v1.1 just ignores unknown message types:
		 * TLS v1.2 give an unexpected message alert.
		 */
		if (s->version >= TLS1_VERSION &&
		    s->version <= TLS1_1_VERSION) {
			rr->length = 0;
			goto start;
		}
		al = SSL_AD_UNEXPECTED_MESSAGE;
		SSLerror(s, SSL_R_UNEXPECTED_RECORD);
		goto fatal_err;
	case SSL3_RT_CHANGE_CIPHER_SPEC:
	case SSL3_RT_ALERT:
	case SSL3_RT_HANDSHAKE:
		/* we already handled all of these, with the possible exception
		 * of SSL3_RT_HANDSHAKE when s->internal->in_handshake is set, but that
		 * should not happen when type != rr->type */
		al = SSL_AD_UNEXPECTED_MESSAGE;
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		goto fatal_err;
	case SSL3_RT_APPLICATION_DATA:
		/* At this point, we were expecting handshake data,
		 * but have application data.  If the library was
		 * running inside ssl3_read() (i.e. in_read_app_data
		 * is set) and it makes sense to read application data
		 * at this point (session renegotiation not yet started),
		 * we will indulge it.
		 */
		if (S3I(s)->in_read_app_data &&
		    (S3I(s)->total_renegotiations != 0) &&
		    (((S3I(s)->hs.state & SSL_ST_CONNECT) &&
		    (S3I(s)->hs.state >= SSL3_ST_CW_CLNT_HELLO_A) &&
		    (S3I(s)->hs.state <= SSL3_ST_CR_SRVR_HELLO_A)) ||
		    ((S3I(s)->hs.state & SSL_ST_ACCEPT) &&
		    (S3I(s)->hs.state <= SSL3_ST_SW_HELLO_REQ_A) &&
		    (S3I(s)->hs.state >= SSL3_ST_SR_CLNT_HELLO_A)))) {
			S3I(s)->in_read_app_data = 2;
			return (-1);
		} else {
			al = SSL_AD_UNEXPECTED_MESSAGE;
			SSLerror(s, SSL_R_UNEXPECTED_RECORD);
			goto fatal_err;
		}
	}
	/* not reached */

 fatal_err:
	ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
	return (-1);
}

int
ssl3_do_change_cipher_spec(SSL *s)
{
	if (S3I(s)->hs.tls12.key_block == NULL) {
		if (s->session == NULL || s->session->master_key_length == 0) {
			/* might happen if dtls1_read_bytes() calls this */
			SSLerror(s, SSL_R_CCS_RECEIVED_EARLY);
			return (0);
		}

		s->session->cipher = S3I(s)->hs.cipher;
		if (!tls1_setup_key_block(s))
			return (0);
	}

	if (!tls1_change_read_cipher_state(s))
		return (0);

	/*
	 * We have to record the message digest at this point so we can get it
	 * before we read the finished message.
	 */
	if (!tls12_derive_peer_finished(s))
		return (0);

	return (1);
}

static int
ssl3_write_alert(SSL *s)
{
	if (SSL_is_dtls(s))
		return do_dtls1_write(s, SSL3_RT_ALERT, S3I(s)->send_alert,
		    sizeof(S3I(s)->send_alert));

	return do_ssl3_write(s, SSL3_RT_ALERT, S3I(s)->send_alert,
	    sizeof(S3I(s)->send_alert));
}

int
ssl3_send_alert(SSL *s, int level, int desc)
{
	/* If alert is fatal, remove session from cache. */
	if (level == SSL3_AL_FATAL)
		SSL_CTX_remove_session(s->ctx, s->session);

	S3I(s)->alert_dispatch = 1;
	S3I(s)->send_alert[0] = level;
	S3I(s)->send_alert[1] = desc;

	/*
	 * If data is still being written out, the alert will be dispatched at
	 * some point in the future.
	 */
	if (S3I(s)->wbuf.left != 0)
		return -1;

	return ssl3_dispatch_alert(s);
}

int
ssl3_dispatch_alert(SSL *s)
{
	int ret;

	S3I(s)->alert_dispatch = 0;
	if ((ret = ssl3_write_alert(s)) <= 0) {
		S3I(s)->alert_dispatch = 1;
		return ret;
	}

	/*
	 * Alert sent to BIO.  If it is important, flush it now.
	 * If the message does not get sent due to non-blocking IO,
	 * we will not worry too much.
	 */
	if (S3I(s)->send_alert[0] == SSL3_AL_FATAL)
		(void)BIO_flush(s->wbio);

	ssl_msg_callback(s, 1, SSL3_RT_ALERT, S3I(s)->send_alert, 2);

	ssl_info_callback(s, SSL_CB_WRITE_ALERT,
	    (S3I(s)->send_alert[0] << 8) | S3I(s)->send_alert[1]);

	return ret;
}
