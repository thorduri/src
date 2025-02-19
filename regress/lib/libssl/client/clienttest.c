/*	$OpenBSD: clienttest.c,v 1.41 2023/07/11 08:31:34 tb Exp $ */
/*
 * Copyright (c) 2015 Joel Sing <jsing@openbsd.org>
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

#include <openssl/ssl.h>

#include <openssl/dtls1.h>
#include <openssl/ssl3.h>

#include <err.h>
#include <stdio.h>
#include <string.h>

#define DTLS_HM_OFFSET (DTLS1_RT_HEADER_LENGTH + DTLS1_HM_HEADER_LENGTH)
#define DTLS_RANDOM_OFFSET (DTLS_HM_OFFSET + 2)
#define DTLS_CIPHER_OFFSET (DTLS_HM_OFFSET + 38)

#define SSL3_HM_OFFSET (SSL3_RT_HEADER_LENGTH + SSL3_HM_HEADER_LENGTH)
#define SSL3_RANDOM_OFFSET (SSL3_HM_OFFSET + 2)
#define SSL3_CIPHER_OFFSET (SSL3_HM_OFFSET + 37)

#define TLS13_HM_OFFSET (SSL3_RT_HEADER_LENGTH + SSL3_HM_HEADER_LENGTH)
#define TLS13_RANDOM_OFFSET (TLS13_HM_OFFSET + 2)
#define TLS13_SESSION_OFFSET (TLS13_HM_OFFSET + 34)
#define TLS13_CIPHER_OFFSET (TLS13_HM_OFFSET + 69)
#define TLS13_KEY_SHARE_OFFSET (TLS13_HM_OFFSET + 188)
#define TLS13_ONLY_KEY_SHARE_OFFSET (TLS13_HM_OFFSET + 98)

#define TLS1_3_VERSION_ONLY (TLS1_3_VERSION | 0x10000)

int tlsext_linearize_build_order(SSL *);

static const uint8_t cipher_list_dtls1[] = {
	0xc0, 0x14, 0xc0, 0x0a, 0x00, 0x39, 0xff, 0x85,
	0x00, 0x88, 0x00, 0x81, 0x00, 0x35, 0x00, 0x84,
	0xc0, 0x13, 0xc0, 0x09, 0x00, 0x33, 0x00, 0x45,
	0x00, 0x2f, 0x00, 0x41, 0xc0, 0x12, 0xc0, 0x08,
	0x00, 0x16, 0x00, 0x0a, 0x00, 0xff,
};

static const uint8_t client_hello_dtls1[] = {
	0x16, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x74, 0x01, 0x00, 0x00,
	0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x68, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0xc0,
	0x14, 0xc0, 0x0a, 0x00, 0x39, 0xff, 0x85, 0x00,
	0x88, 0x00, 0x81, 0x00, 0x35, 0x00, 0x84, 0xc0,
	0x13, 0xc0, 0x09, 0x00, 0x33, 0x00, 0x45, 0x00,
	0x2f, 0x00, 0x41, 0xc0, 0x12, 0xc0, 0x08, 0x00,
	0x16, 0x00, 0x0a, 0x00, 0xff, 0x01, 0x00, 0x00,
	0x18, 0x00, 0x0b, 0x00, 0x02, 0x01, 0x00, 0x00,
	0x0a, 0x00, 0x0a, 0x00, 0x08, 0x00, 0x1d, 0x00,
	0x17, 0x00, 0x18, 0x00, 0x19, 0x00, 0x23, 0x00,
	0x00,
};

static const uint8_t cipher_list_dtls12_aes[] = {
	0xc0, 0x30, 0xc0, 0x2c, 0xc0, 0x28, 0xc0, 0x24,
	0xc0, 0x14, 0xc0, 0x0a, 0x00, 0x9f, 0x00, 0x6b,
	0x00, 0x39, 0xcc, 0xa9, 0xcc, 0xa8, 0xcc, 0xaa,
	0xff, 0x85, 0x00, 0xc4, 0x00, 0x88, 0x00, 0x81,
	0x00, 0x9d, 0x00, 0x3d, 0x00, 0x35, 0x00, 0xc0,
	0x00, 0x84, 0xc0, 0x2f, 0xc0, 0x2b, 0xc0, 0x27,
	0xc0, 0x23, 0xc0, 0x13, 0xc0, 0x09, 0x00, 0x9e,
	0x00, 0x67, 0x00, 0x33, 0x00, 0xbe, 0x00, 0x45,
	0x00, 0x9c, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0xba,
	0x00, 0x41, 0xc0, 0x12, 0xc0, 0x08, 0x00, 0x16,
	0x00, 0x0a, 0x00, 0xff
};

static const uint8_t cipher_list_dtls12_chacha[] = {
	0xcc, 0xa9, 0xcc, 0xa8, 0xcc, 0xaa, 0xc0, 0x30,
	0xc0, 0x2c, 0xc0, 0x28, 0xc0, 0x24, 0xc0, 0x14,
	0xc0, 0x0a, 0x00, 0x9f, 0x00, 0x6b, 0x00, 0x39,
	0xff, 0x85, 0x00, 0xc4, 0x00, 0x88, 0x00, 0x81,
	0x00, 0x9d, 0x00, 0x3d, 0x00, 0x35, 0x00, 0xc0,
	0x00, 0x84, 0xc0, 0x2f, 0xc0, 0x2b, 0xc0, 0x27,
	0xc0, 0x23, 0xc0, 0x13, 0xc0, 0x09, 0x00, 0x9e,
	0x00, 0x67, 0x00, 0x33, 0x00, 0xbe, 0x00, 0x45,
	0x00, 0x9c, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0xba,
	0x00, 0x41, 0xc0, 0x12, 0xc0, 0x08, 0x00, 0x16,
	0x00, 0x0a, 0x00, 0xff,
};

static const uint8_t client_hello_dtls12[] = {
	0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xbe, 0x01, 0x00, 0x00,
	0xb2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xb2, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0xc0,
	0x30, 0xc0, 0x2c, 0xc0, 0x28, 0xc0, 0x24, 0xc0,
	0x14, 0xc0, 0x0a, 0x00, 0x9f, 0x00, 0x6b, 0x00,
	0x39, 0xcc, 0xa9, 0xcc, 0xa8, 0xcc, 0xaa, 0xff,
	0x85, 0x00, 0xc4, 0x00, 0x88, 0x00, 0x81, 0x00,
	0x9d, 0x00, 0x3d, 0x00, 0x35, 0x00, 0xc0, 0x00,
	0x84, 0xc0, 0x2f, 0xc0, 0x2b, 0xc0, 0x27, 0xc0,
	0x23, 0xc0, 0x13, 0xc0, 0x09, 0x00, 0x9e, 0x00,
	0x67, 0x00, 0x33, 0x00, 0xbe, 0x00, 0x45, 0x00,
	0x9c, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0xba, 0x00,
	0x41, 0xc0, 0x12, 0xc0, 0x08, 0x00, 0x16, 0x00,
	0x0a, 0x00, 0xff, 0x01, 0x00, 0x00, 0x34, 0x00,
	0x0b, 0x00, 0x02, 0x01, 0x00, 0x00, 0x0a, 0x00,
	0x0a, 0x00, 0x08, 0x00, 0x1d, 0x00, 0x17, 0x00,
	0x18, 0x00, 0x19, 0x00, 0x23, 0x00, 0x00, 0x00,
	0x0d, 0x00, 0x18, 0x00, 0x16, 0x08, 0x06, 0x06,
	0x01, 0x06, 0x03, 0x08, 0x05, 0x05, 0x01, 0x05,
	0x03, 0x08, 0x04, 0x04, 0x01, 0x04, 0x03, 0x02,
	0x01, 0x02, 0x03,
};

static const uint8_t cipher_list_tls10[] = {
	0xc0, 0x14, 0xc0, 0x0a, 0x00, 0x39, 0xff, 0x85,
	0x00, 0x88, 0x00, 0x81, 0x00, 0x35, 0x00, 0x84,
	0xc0, 0x13, 0xc0, 0x09, 0x00, 0x33, 0x00, 0x45,
	0x00, 0x2f, 0x00, 0x41, 0xc0, 0x11, 0xc0, 0x07,
	0x00, 0x05, 0xc0, 0x12, 0xc0, 0x08, 0x00, 0x16,
	0x00, 0x0a, 0x00, 0xff,
};

static const uint8_t client_hello_tls10[] = {
	0x16, 0x03, 0x01, 0x00, 0x71, 0x01, 0x00, 0x00,
	0x6d, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0xc0, 0x14,
	0xc0, 0x0a, 0x00, 0x39, 0xff, 0x85, 0x00, 0x88,
	0x00, 0x81, 0x00, 0x35, 0x00, 0x84, 0xc0, 0x13,
	0xc0, 0x09, 0x00, 0x33, 0x00, 0x45, 0x00, 0x2f,
	0x00, 0x41, 0xc0, 0x11, 0xc0, 0x07, 0x00, 0x05,
	0xc0, 0x12, 0xc0, 0x08, 0x00, 0x16, 0x00, 0x0a,
	0x00, 0xff, 0x01, 0x00, 0x00, 0x18, 0x00, 0x0b,
	0x00, 0x02, 0x01, 0x00, 0x00, 0x0a, 0x00, 0x0a,
	0x00, 0x08, 0x00, 0x1d, 0x00, 0x17, 0x00, 0x18,
	0x00, 0x19, 0x00, 0x23, 0x00, 0x00,
};

static const uint8_t cipher_list_tls11[] = {
	0xc0, 0x14, 0xc0, 0x0a, 0x00, 0x39, 0xff, 0x85,
	0x00, 0x88, 0x00, 0x81, 0x00, 0x35, 0x00, 0x84,
	0xc0, 0x13, 0xc0, 0x09, 0x00, 0x33, 0x00, 0x45,
	0x00, 0x2f, 0x00, 0x41, 0xc0, 0x11, 0xc0, 0x07,
	0x00, 0x05, 0xc0, 0x12, 0xc0, 0x08, 0x00, 0x16,
	0x00, 0x0a, 0x00, 0xff,
};

static const uint8_t client_hello_tls11[] = {
	0x16, 0x03, 0x01, 0x00, 0x71, 0x01, 0x00, 0x00,
	0x6d, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0xc0, 0x14,
	0xc0, 0x0a, 0x00, 0x39, 0xff, 0x85, 0x00, 0x88,
	0x00, 0x81, 0x00, 0x35, 0x00, 0x84, 0xc0, 0x13,
	0xc0, 0x09, 0x00, 0x33, 0x00, 0x45, 0x00, 0x2f,
	0x00, 0x41, 0xc0, 0x11, 0xc0, 0x07, 0x00, 0x05,
	0xc0, 0x12, 0xc0, 0x08, 0x00, 0x16, 0x00, 0x0a,
	0x00, 0xff, 0x01, 0x00, 0x00, 0x18, 0x00, 0x0b,
	0x00, 0x02, 0x01, 0x00, 0x00, 0x0a, 0x00, 0x0a,
	0x00, 0x08, 0x00, 0x1d, 0x00, 0x17, 0x00, 0x18,
	0x00, 0x19, 0x00, 0x23, 0x00, 0x00,
};

static const uint8_t cipher_list_tls12_aes[] = {
	0xc0, 0x30, 0xc0, 0x2c, 0xc0, 0x28, 0xc0, 0x24,
	0xc0, 0x14, 0xc0, 0x0a, 0x00, 0x9f, 0x00, 0x6b,
	0x00, 0x39, 0xcc, 0xa9, 0xcc, 0xa8, 0xcc, 0xaa,
	0xff, 0x85, 0x00, 0xc4, 0x00, 0x88, 0x00, 0x81,
	0x00, 0x9d, 0x00, 0x3d, 0x00, 0x35, 0x00, 0xc0,
	0x00, 0x84, 0xc0, 0x2f, 0xc0, 0x2b, 0xc0, 0x27,
	0xc0, 0x23, 0xc0, 0x13, 0xc0, 0x09, 0x00, 0x9e,
	0x00, 0x67, 0x00, 0x33, 0x00, 0xbe, 0x00, 0x45,
	0x00, 0x9c, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0xba,
	0x00, 0x41, 0xc0, 0x11, 0xc0, 0x07, 0x00, 0x05,
	0xc0, 0x12, 0xc0, 0x08, 0x00, 0x16, 0x00, 0x0a,
	0x00, 0xff,
};

static const uint8_t cipher_list_tls12_chacha[] = {
	0xcc, 0xa9, 0xcc, 0xa8, 0xcc, 0xaa, 0xc0, 0x30,
	0xc0, 0x2c, 0xc0, 0x28, 0xc0, 0x24, 0xc0, 0x14,
	0xc0, 0x0a, 0x00, 0x9f, 0x00, 0x6b, 0x00, 0x39,
	0xff, 0x85, 0x00, 0xc4, 0x00, 0x88, 0x00, 0x81,
	0x00, 0x9d, 0x00, 0x3d, 0x00, 0x35, 0x00, 0xc0,
	0x00, 0x84, 0xc0, 0x2f, 0xc0, 0x2b, 0xc0, 0x27,
	0xc0, 0x23, 0xc0, 0x13, 0xc0, 0x09, 0x00, 0x9e,
	0x00, 0x67, 0x00, 0x33, 0x00, 0xbe, 0x00, 0x45,
	0x00, 0x9c, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0xba,
	0x00, 0x41, 0xc0, 0x11, 0xc0, 0x07, 0x00, 0x05,
	0xc0, 0x12, 0xc0, 0x08, 0x00, 0x16, 0x00, 0x0a,
	0x00, 0xff,
};

static const uint8_t client_hello_tls12[] = {
	0x16, 0x03, 0x01, 0x00, 0xbb, 0x01, 0x00, 0x00,
	0xb7, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x5a, 0xc0, 0x30,
	0xc0, 0x2c, 0xc0, 0x28, 0xc0, 0x24, 0xc0, 0x14,
	0xc0, 0x0a, 0x00, 0x9f, 0x00, 0x6b, 0x00, 0x39,
	0xcc, 0xa9, 0xcc, 0xa8, 0xcc, 0xaa, 0xff, 0x85,
	0x00, 0xc4, 0x00, 0x88, 0x00, 0x81, 0x00, 0x9d,
	0x00, 0x3d, 0x00, 0x35, 0x00, 0xc0, 0x00, 0x84,
	0xc0, 0x2f, 0xc0, 0x2b, 0xc0, 0x27, 0xc0, 0x23,
	0xc0, 0x13, 0xc0, 0x09, 0x00, 0x9e, 0x00, 0x67,
	0x00, 0x33, 0x00, 0xbe, 0x00, 0x45, 0x00, 0x9c,
	0x00, 0x3c, 0x00, 0x2f, 0x00, 0xba, 0x00, 0x41,
	0xc0, 0x11, 0xc0, 0x07, 0x00, 0x05, 0xc0, 0x12,
	0xc0, 0x08, 0x00, 0x16, 0x00, 0x0a, 0x00, 0xff,
	0x01, 0x00, 0x00, 0x34, 0x00, 0x0b, 0x00, 0x02,
	0x01, 0x00, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x08,
	0x00, 0x1d, 0x00, 0x17, 0x00, 0x18, 0x00, 0x19,
	0x00, 0x23, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x18,
	0x00, 0x16, 0x08, 0x06, 0x06, 0x01, 0x06, 0x03,
	0x08, 0x05, 0x05, 0x01, 0x05, 0x03, 0x08, 0x04,
	0x04, 0x01, 0x04, 0x03, 0x02, 0x01, 0x02, 0x03,
};

static const uint8_t cipher_list_tls13_aes[] = {
	0x13, 0x02, 0x13, 0x03, 0x13, 0x01, 0xc0, 0x30,
	0xc0, 0x2c, 0xc0, 0x28, 0xc0, 0x24, 0xc0, 0x14,
	0xc0, 0x0a, 0x00, 0x9f, 0x00, 0x6b, 0x00, 0x39,
	0xcc, 0xa9, 0xcc, 0xa8, 0xcc, 0xaa, 0xff, 0x85,
	0x00, 0xc4, 0x00, 0x88, 0x00, 0x81, 0x00, 0x9d,
	0x00, 0x3d, 0x00, 0x35, 0x00, 0xc0, 0x00, 0x84,
	0xc0, 0x2f, 0xc0, 0x2b, 0xc0, 0x27, 0xc0, 0x23,
	0xc0, 0x13, 0xc0, 0x09, 0x00, 0x9e, 0x00, 0x67,
	0x00, 0x33, 0x00, 0xbe, 0x00, 0x45, 0x00, 0x9c,
	0x00, 0x3c, 0x00, 0x2f, 0x00, 0xba, 0x00, 0x41,
	0xc0, 0x11, 0xc0, 0x07, 0x00, 0x05, 0xc0, 0x12,
	0xc0, 0x08, 0x00, 0x16, 0x00, 0x0a, 0x00, 0xff,
};

static const uint8_t cipher_list_tls13_chacha[] = {
	0x13, 0x03, 0x13, 0x02, 0x13, 0x01, 0xcc, 0xa9,
	0xcc, 0xa8, 0xcc, 0xaa, 0xc0, 0x30, 0xc0, 0x2c,
	0xc0, 0x28, 0xc0, 0x24, 0xc0, 0x14, 0xc0, 0x0a,
	0x00, 0x9f, 0x00, 0x6b, 0x00, 0x39, 0xff, 0x85,
	0x00, 0xc4, 0x00, 0x88, 0x00, 0x81, 0x00, 0x9d,
	0x00, 0x3d, 0x00, 0x35, 0x00, 0xc0, 0x00, 0x84,
	0xc0, 0x2f, 0xc0, 0x2b, 0xc0, 0x27, 0xc0, 0x23,
	0xc0, 0x13, 0xc0, 0x09, 0x00, 0x9e, 0x00, 0x67,
	0x00, 0x33, 0x00, 0xbe, 0x00, 0x45, 0x00, 0x9c,
	0x00, 0x3c, 0x00, 0x2f, 0x00, 0xba, 0x00, 0x41,
	0xc0, 0x11, 0xc0, 0x07, 0x00, 0x05, 0xc0, 0x12,
	0xc0, 0x08, 0x00, 0x16, 0x00, 0x0a, 0x00, 0xff,
};

static const uint8_t client_hello_tls13[] = {
	0x16, 0x03, 0x03, 0x01, 0x14, 0x01, 0x00, 0x01,
	0x10, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x13, 0x03,
	0x13, 0x02, 0x13, 0x01, 0xcc, 0xa9, 0xcc, 0xa8,
	0xcc, 0xaa, 0xc0, 0x30, 0xc0, 0x2c, 0xc0, 0x28,
	0xc0, 0x24, 0xc0, 0x14, 0xc0, 0x0a, 0x00, 0x9f,
	0x00, 0x6b, 0x00, 0x39, 0xff, 0x85, 0x00, 0xc4,
	0x00, 0x88, 0x00, 0x81, 0x00, 0x9d, 0x00, 0x3d,
	0x00, 0x35, 0x00, 0xc0, 0x00, 0x84, 0xc0, 0x2f,
	0xc0, 0x2b, 0xc0, 0x27, 0xc0, 0x23, 0xc0, 0x13,
	0xc0, 0x09, 0x00, 0x9e, 0x00, 0x67, 0x00, 0x33,
	0x00, 0xbe, 0x00, 0x45, 0x00, 0x9c, 0x00, 0x3c,
	0x00, 0x2f, 0x00, 0xba, 0x00, 0x41, 0xc0, 0x11,
	0xc0, 0x07, 0x00, 0x05, 0xc0, 0x12, 0xc0, 0x08,
	0x00, 0x16, 0x00, 0x0a, 0x00, 0xff, 0x01, 0x00,
	0x00, 0x67, 0x00, 0x2b, 0x00, 0x05, 0x04, 0x03,
	0x04, 0x03, 0x03, 0x00, 0x33, 0x00, 0x26, 0x00,
	0x24, 0x00, 0x1d, 0x00, 0x20, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00,
	0x02, 0x01, 0x00, 0x00, 0x0a, 0x00, 0x0a, 0x00,
	0x08, 0x00, 0x1d, 0x00, 0x17, 0x00, 0x18, 0x00,
	0x19, 0x00, 0x23, 0x00, 0x00, 0x00, 0x0d, 0x00,
	0x18, 0x00, 0x16, 0x08, 0x06, 0x06, 0x01, 0x06,
	0x03, 0x08, 0x05, 0x05, 0x01, 0x05, 0x03, 0x08,
	0x04, 0x04, 0x01, 0x04, 0x03, 0x02, 0x01, 0x02,
	0x03,
};

static const uint8_t cipher_list_tls13_only_aes[] = {
	0x13, 0x02, 0x13, 0x03, 0x13, 0x01,
};

static const uint8_t cipher_list_tls13_only_chacha[] = {
	0x13, 0x03, 0x13, 0x02, 0x13, 0x01,
};

static const uint8_t client_hello_tls13_only[] = {
	0x16, 0x03, 0x03, 0x00, 0xb6, 0x01, 0x00, 0x00,
	0xb2, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x13, 0x03,
	0x13, 0x02, 0x13, 0x01, 0x00, 0xff, 0x01, 0x00,
	0x00, 0x61, 0x00, 0x2b, 0x00, 0x03, 0x02, 0x03,
	0x04, 0x00, 0x33, 0x00, 0x26, 0x00, 0x24, 0x00,
	0x1d, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x02, 0x01,
	0x00, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x08, 0x00,
	0x1d, 0x00, 0x17, 0x00, 0x18, 0x00, 0x19, 0x00,
	0x23, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x14, 0x00,
	0x12, 0x08, 0x06, 0x06, 0x01, 0x06, 0x03, 0x08,
	0x05, 0x05, 0x01, 0x05, 0x03, 0x08, 0x04, 0x04,
	0x01, 0x04, 0x03,
};

struct client_hello_test {
	const char *desc;
	const int protocol;
	const size_t random_start;
	const size_t session_start;
	const size_t key_share_start;
	const SSL_METHOD *(*ssl_method)(void);
	const long ssl_options;
	int connect_fails;
};

static const struct client_hello_test client_hello_tests[] = {
	{
		.desc = "DTLSv1 client method",
		.protocol = DTLS1_VERSION,
		.random_start = DTLS_RANDOM_OFFSET,
		.ssl_method = DTLSv1_client_method,
		.connect_fails = 1,
	},
	{
		.desc = "DTLSv1.2 client method",
		.protocol = DTLS1_2_VERSION,
		.random_start = DTLS_RANDOM_OFFSET,
		.ssl_method = DTLSv1_2_client_method,
	},
	{
		.desc = "DTLS client method",
		.protocol = DTLS1_2_VERSION,
		.random_start = DTLS_RANDOM_OFFSET,
		.ssl_method = DTLS_client_method,
	},
	{
		.desc = "DTLS client method (no DTLSv1.2)",
		.protocol = DTLS1_VERSION,
		.random_start = DTLS_RANDOM_OFFSET,
		.ssl_method = DTLS_client_method,
		.ssl_options = SSL_OP_NO_DTLSv1_2,
		.connect_fails = 1,
	},
	{
		.desc = "DTLS client method (no DTLSv1.0)",
		.protocol = DTLS1_2_VERSION,
		.random_start = DTLS_RANDOM_OFFSET,
		.ssl_method = DTLS_client_method,
		.ssl_options = SSL_OP_NO_DTLSv1,
	},
	{
		.desc = "TLSv1 client method",
		.protocol = TLS1_VERSION,
		.random_start = SSL3_RANDOM_OFFSET,
		.ssl_method = TLSv1_client_method,
		.connect_fails = 1,
	},
	{
		.desc = "TLSv1_1 client method",
		.protocol = TLS1_1_VERSION,
		.random_start = SSL3_RANDOM_OFFSET,
		.ssl_method = TLSv1_1_client_method,
		.connect_fails = 1,
	},
	{
		.desc = "TLSv1_2 client method",
		.protocol = TLS1_2_VERSION,
		.random_start = SSL3_RANDOM_OFFSET,
		.ssl_method = TLSv1_2_client_method,
	},
	{
		.desc = "SSLv23 default",
		.protocol = TLS1_3_VERSION,
		.random_start = TLS13_RANDOM_OFFSET,
		.session_start = TLS13_SESSION_OFFSET,
		.key_share_start = TLS13_KEY_SHARE_OFFSET,
		.ssl_method = SSLv23_client_method,
		.ssl_options = 0,
	},
	{
		.desc = "SSLv23 default (no TLSv1.3)",
		.protocol = TLS1_2_VERSION,
		.random_start = SSL3_RANDOM_OFFSET,
		.ssl_method = SSLv23_client_method,
		.ssl_options = SSL_OP_NO_TLSv1_3,
	},
	{
		.desc = "SSLv23 (no TLSv1.2)",
		.protocol = TLS1_3_VERSION_ONLY,
		.random_start = TLS13_RANDOM_OFFSET,
		.session_start = TLS13_SESSION_OFFSET,
		.key_share_start = TLS13_ONLY_KEY_SHARE_OFFSET,
		.ssl_method = SSLv23_client_method,
		.ssl_options = SSL_OP_NO_TLSv1_2,
	},
	{
		.desc = "SSLv23 (no TLSv1.1)",
		.protocol = TLS1_3_VERSION,
		.random_start = TLS13_RANDOM_OFFSET,
		.session_start = TLS13_SESSION_OFFSET,
		.key_share_start = TLS13_KEY_SHARE_OFFSET,
		.ssl_method = SSLv23_client_method,
		.ssl_options = SSL_OP_NO_TLSv1_1,
	},
	{
		.desc = "TLS default",
		.protocol = TLS1_3_VERSION,
		.random_start = TLS13_RANDOM_OFFSET,
		.session_start = TLS13_SESSION_OFFSET,
		.key_share_start = TLS13_KEY_SHARE_OFFSET,
		.ssl_method = TLS_client_method,
		.ssl_options = 0,
	},
	{
		.desc = "TLS (no TLSv1.3)",
		.protocol = TLS1_2_VERSION,
		.random_start = SSL3_RANDOM_OFFSET,
		.ssl_method = TLS_client_method,
		.ssl_options = SSL_OP_NO_TLSv1_3,
	},
	{
		.desc = "TLS (no TLSv1.2)",
		.protocol = TLS1_3_VERSION_ONLY,
		.random_start = TLS13_RANDOM_OFFSET,
		.session_start = TLS13_SESSION_OFFSET,
		.key_share_start = TLS13_ONLY_KEY_SHARE_OFFSET,
		.ssl_method = TLS_client_method,
		.ssl_options = SSL_OP_NO_TLSv1_2,
	},
	{
		.desc = "TLS (no TLSv1.1)",
		.protocol = TLS1_3_VERSION,
		.random_start = TLS13_RANDOM_OFFSET,
		.session_start = TLS13_SESSION_OFFSET,
		.key_share_start = TLS13_KEY_SHARE_OFFSET,
		.ssl_method = TLS_client_method,
		.ssl_options = SSL_OP_NO_TLSv1_1,
	},
#if 0
	/* XXX - build client hello with explicit versions extension. */
	{
		.desc = "TLS (no TLSv1.0, no TLSv1.1)",
		.protocol = TLS1_3_VERSION,
		.random_start = TLS13_RANDOM_OFFSET,
		.session_start = TLS13_SESSION_OFFSET,
		.key_share_start = TLS13_KEY_SHARE_OFFSET,
		.ssl_method = TLS_client_method,
		.ssl_options = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1,
	},
#endif
	{
		.desc = "TLS (no TLSv1.0, no TLSv1.1, no TLSv1.2)",
		.protocol = TLS1_3_VERSION_ONLY,
		.random_start = TLS13_RANDOM_OFFSET,
		.session_start = TLS13_SESSION_OFFSET,
		.key_share_start = TLS13_ONLY_KEY_SHARE_OFFSET,
		.ssl_method = TLS_client_method,
		.ssl_options = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2,
	},
};

#define N_CLIENT_HELLO_TESTS \
    (sizeof(client_hello_tests) / sizeof(*client_hello_tests))

static void
hexdump(const uint8_t *buf, size_t len, const uint8_t *compare)
{
	const char *mark = "";
	size_t i;

	for (i = 1; i <= len; i++) {
		if (compare != NULL)
			mark = (buf[i - 1] != compare[i - 1]) ? "*" : " ";
		fprintf(stderr, " %s0x%02hhx,%s", mark, buf[i - 1],
		    i % 8 && i != len ? "" : "\n");
	}
	fprintf(stderr, "\n");
}

static inline int
ssl_aes_is_accelerated(void)
{
#if defined(__i386__) || defined(__x86_64__)
	return ((OPENSSL_cpu_caps() & (1ULL << 57)) != 0);
#else
	return (0);
#endif
}

static int
make_client_hello(int protocol, char **out, size_t *outlen)
{
	size_t client_hello_len, cipher_list_len, cipher_list_offset;
	const uint8_t *client_hello, *cipher_list;
	char *p;

	*out = NULL;
	*outlen = 0;

	switch (protocol) {
	case DTLS1_VERSION:
		client_hello = client_hello_dtls1;
		client_hello_len = sizeof(client_hello_dtls1);
		cipher_list = cipher_list_dtls1;
		cipher_list_len = sizeof(cipher_list_dtls1);
		cipher_list_offset = DTLS_CIPHER_OFFSET;
		break;

	case DTLS1_2_VERSION:
		client_hello = client_hello_dtls12;
		client_hello_len = sizeof(client_hello_dtls12);
		cipher_list = cipher_list_dtls12_chacha;
		cipher_list_len = sizeof(cipher_list_dtls12_chacha);
		if (ssl_aes_is_accelerated()) {
			cipher_list = cipher_list_dtls12_aes;
			cipher_list_len = sizeof(cipher_list_dtls12_aes);
		}
		cipher_list_offset = DTLS_CIPHER_OFFSET;
		break;

	case TLS1_VERSION:
		client_hello = client_hello_tls10;
		client_hello_len = sizeof(client_hello_tls10);
		cipher_list = cipher_list_tls10;
		cipher_list_len = sizeof(cipher_list_tls10);
		cipher_list_offset = SSL3_CIPHER_OFFSET;
		break;

	case TLS1_1_VERSION:
		client_hello = client_hello_tls11;
		client_hello_len = sizeof(client_hello_tls11);
		cipher_list = cipher_list_tls11;
		cipher_list_len = sizeof(cipher_list_tls11);
		cipher_list_offset = SSL3_CIPHER_OFFSET;
		break;

	case TLS1_2_VERSION:
		client_hello = client_hello_tls12;
		client_hello_len = sizeof(client_hello_tls12);
		cipher_list = cipher_list_tls12_chacha;
		cipher_list_len = sizeof(cipher_list_tls12_chacha);
		if (ssl_aes_is_accelerated()) {
			cipher_list = cipher_list_tls12_aes;
			cipher_list_len = sizeof(cipher_list_tls12_aes);
		}
		cipher_list_offset = SSL3_CIPHER_OFFSET;
		break;

	case TLS1_3_VERSION:
		client_hello = client_hello_tls13;
		client_hello_len = sizeof(client_hello_tls13);
		cipher_list = cipher_list_tls13_chacha;
		cipher_list_len = sizeof(cipher_list_tls13_chacha);
		if (ssl_aes_is_accelerated()) {
			cipher_list = cipher_list_tls13_aes;
			cipher_list_len = sizeof(cipher_list_tls13_aes);
		}
		cipher_list_offset = TLS13_CIPHER_OFFSET;
		break;

	case TLS1_3_VERSION_ONLY:
		client_hello = client_hello_tls13_only;
		client_hello_len = sizeof(client_hello_tls13_only);
		cipher_list = cipher_list_tls13_only_chacha;
		cipher_list_len = sizeof(cipher_list_tls13_only_chacha);
		if (ssl_aes_is_accelerated()) {
			cipher_list = cipher_list_tls13_only_aes;
			cipher_list_len = sizeof(cipher_list_tls13_only_aes);
		}
		cipher_list_offset = TLS13_CIPHER_OFFSET;
		break;

	default:
		return (-1);
	}

	if ((p = malloc(client_hello_len)) == NULL)
		return (-1);

	memcpy(p, client_hello, client_hello_len);
	memcpy(p + cipher_list_offset, cipher_list, cipher_list_len);

	*out = p;
	*outlen = client_hello_len;

	return (0);
}

static int
client_hello_test(int testno, const struct client_hello_test *cht)
{
	BIO *rbio = NULL, *wbio = NULL;
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	char *client_hello = NULL;
	size_t client_hello_len;
	size_t session_len;
	char *wbuf, rbuf[1];
	int ret = 1;
	long len;

	fprintf(stderr, "Test %d - %s\n", testno, cht->desc);

	/* Providing a small buf causes *_get_server_hello() to return. */
	if ((rbio = BIO_new_mem_buf(rbuf, sizeof(rbuf))) == NULL) {
		fprintf(stderr, "Failed to setup rbio\n");
		goto failure;
	}
	if ((wbio = BIO_new(BIO_s_mem())) == NULL) {
		fprintf(stderr, "Failed to setup wbio\n");
		goto failure;
	}

	if ((ssl_ctx = SSL_CTX_new(cht->ssl_method())) == NULL) {
		fprintf(stderr, "SSL_CTX_new() returned NULL\n");
		goto failure;
	}

	SSL_CTX_set_options(ssl_ctx, cht->ssl_options);

	if ((ssl = SSL_new(ssl_ctx)) == NULL) {
		fprintf(stderr, "SSL_new() returned NULL\n");
		goto failure;
	}

	if (!tlsext_linearize_build_order(ssl)) {
		fprintf(stderr, "failed to linearize build order");
		goto failure;
	}

	BIO_up_ref(rbio);
	BIO_up_ref(wbio);
	SSL_set_bio(ssl, rbio, wbio);

	if (SSL_connect(ssl) != 0) {
		if (cht->connect_fails)
			goto done;
		fprintf(stderr, "SSL_connect() returned non-zero\n");
		goto failure;
	}

	len = BIO_get_mem_data(wbio, &wbuf);

	if (make_client_hello(cht->protocol, &client_hello,
	    &client_hello_len) != 0)
		errx(1, "failed to make client hello");

	if ((size_t)len != client_hello_len) {
		fprintf(stderr, "FAIL: test returned ClientHello length %ld, "
		    "want %zu\n", len, client_hello_len);
		fprintf(stderr, "received:\n");
		hexdump(wbuf, len, NULL);
		fprintf(stderr, "test data:\n");
		hexdump(client_hello, client_hello_len, NULL);
		fprintf(stderr, "\n");
		goto failure;
	}

	/* We expect the client random to differ. */
	if (memcmp(&client_hello[cht->random_start], &wbuf[cht->random_start],
	    SSL3_RANDOM_SIZE) == 0) {
		fprintf(stderr, "FAIL: ClientHello has zeroed random\n");
		goto failure;
	}

	memset(&wbuf[cht->random_start], 0, SSL3_RANDOM_SIZE);

	if (cht->session_start > 0) {
		session_len = wbuf[cht->session_start];
		if (session_len > 0)
			memset(&wbuf[cht->session_start + 1], 0, session_len);
	}
	if (cht->key_share_start > 0)
		memset(&wbuf[cht->key_share_start], 0, 32);

	if (memcmp(client_hello, wbuf, client_hello_len) != 0) {
		fprintf(stderr, "FAIL: ClientHello differs:\n");
		fprintf(stderr, "received:\n");
		hexdump(wbuf, len, client_hello);
		fprintf(stderr, "test data:\n");
		hexdump(client_hello, client_hello_len, wbuf);
		fprintf(stderr, "\n");
		goto failure;
	}

 done:
	ret = 0;

 failure:
	SSL_CTX_free(ssl_ctx);
	SSL_free(ssl);

	BIO_free(rbio);
	BIO_free(wbio);

	free(client_hello);

	return (ret);
}

int
main(int argc, char **argv)
{
	int failed = 0;
	size_t i;

	SSL_library_init();

	for (i = 0; i < N_CLIENT_HELLO_TESTS; i++)
		failed |= client_hello_test(i, &client_hello_tests[i]);

	return (failed);
}
