/* pkcs7.h
 *
 * Copyright (C) 2006-2018 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* pkcs7.h for openSSL */


#ifndef WOLFSSL_PKCS7_H_
#define WOLFSSL_PKCS7_H_

#include <wolfssl/openssl/ssl.h>
#include <wolfssl/wolfcrypt/pkcs7.h>

#ifdef __cplusplus
    extern "C" {
#endif

#if defined(WOLFSSL_QT)
    #include <wolfssl/openssl/objects.h>
#endif

#if defined(OPENSSL_ALL) && defined(HAVE_PKCS7)

#define PKCS7_NOINTERN         0x0010
#define PKCS7_NOVERIFY         0x0020


typedef struct WOLFSSL_PKCS7
{
    PKCS7 pkcs7;
    unsigned char* data;
    int len;
} WOLFSSL_PKCS7;


WOLFSSL_API PKCS7* wolfSSL_PKCS7_new(void);
WOLFSSL_API void wolfSSL_PKCS7_free(PKCS7* p7);

WOLFSSL_API PKCS7* wolfSSL_d2i_PKCS7(PKCS7** p7, const unsigned char** in,
    int len);
WOLFSSL_API PKCS7* wolfSSL_d2i_PKCS7_bio(WOLFSSL_BIO* bio, PKCS7** p7);
WOLFSSL_API int wolfSSL_PKCS7_verify(PKCS7* p7, WOLFSSL_STACK* certs,
    WOLFSSL_X509_STORE* store, WOLFSSL_BIO* in, WOLFSSL_BIO* out, int flags);
WOLFSSL_API WOLFSSL_STACK* wolfSSL_PKCS7_get0_signers(PKCS7* p7,
    WOLFSSL_STACK* certs, int flags);

#define PKCS7_new                      wolfSSL_PKCS7_new 
#define PKCS7_free                     wolfSSL_PKCS7_free
#define d2i_PKCS7                      wolfSSL_d2i_PKCS7
#define d2i_PKCS7_bio                  wolfSSL_d2i_PKCS7_bio
#define PKCS7_verify                   wolfSSL_PKCS7_verify
#define PKCS7_get0_signers             wolfSSL_PKCS7_get0_signers

#endif /* OPENSSL_ALL && HAVE_PKCS7 */

#ifdef __cplusplus
    }  /* extern "C" */
#endif

#endif /* WOLFSSL_PKCS7_H_ */
