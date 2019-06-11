/* sniffer.h
 *
 * Copyright (C) 2006-2019 wolfSSL Inc.
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



#ifndef WOLFSSL_SNIFFER_H
#define WOLFSSL_SNIFFER_H

#include <wolfssl/wolfcrypt/settings.h>

#ifdef _WIN32
    #ifdef SSL_SNIFFER_EXPORTS
        #define SSL_SNIFFER_API __declspec(dllexport)
    #else
        #define SSL_SNIFFER_API __declspec(dllimport)
    #endif
#else
    #define SSL_SNIFFER_API
#endif /* _WIN32 */


#ifdef __cplusplus
    extern "C" {
#endif

/* @param typeK: (formerly keyType) was shadowing a global declaration in
 *                wolfssl/wolfcrypt/asn.h line 175
 */
WOLFSSL_API
SSL_SNIFFER_API int ssl_SetPrivateKey(const char* address, int port,
                                      const char* keyFile, int typeK,
                                      const char* password, char* error);

WOLFSSL_API
SSL_SNIFFER_API int ssl_SetNamedPrivateKey(const char* name,
                                           const char* address, int port,
                                           const char* keyFile, int typeK,
                                           const char* password, char* error);

WOLFSSL_API
SSL_SNIFFER_API int ssl_DecodePacket(const unsigned char* packet, int length,
                                     unsigned char** data, char* error);

WOLFSSL_API
SSL_SNIFFER_API int ssl_FreeDecodeBuffer(unsigned char** data, char* error);

WOLFSSL_API
SSL_SNIFFER_API int ssl_FreeZeroDecodeBuffer(unsigned char** data, int sz,
                                             char* error);

WOLFSSL_API
SSL_SNIFFER_API int ssl_Trace(const char* traceFile, char* error);

WOLFSSL_API
SSL_SNIFFER_API int ssl_EnableRecovery(int onOff, int maxMemory, char* error);

WOLFSSL_API
SSL_SNIFFER_API int ssl_GetSessionStats(unsigned int* active,
                                        unsigned int* total,
                                        unsigned int* peak,
                                        unsigned int* maxSessions,
                                        unsigned int* missedData,
                                        unsigned int* reassemblyMemory,
                                        char* error);

WOLFSSL_API void ssl_InitSniffer(void);

WOLFSSL_API void ssl_FreeSniffer(void);


/* ssl_SetPrivateKey typeKs */
enum {
    FILETYPE_PEM = 1,
    FILETYPE_DER = 2,
};


/*
 * New Sniffer API that provides read-only access to the TLS and cipher
 * information associated with the SSL session.
 */

#if defined(__GNUC__)
    #define WOLFSSL_PACK __attribute__ ((packed))
#else
    #define WOLFSSL_PACK
#endif


typedef struct SSLInfo
{
    unsigned char  isValid;
            /* indicates if the info in this struct is valid: 0 = no, 1 = yes */
    unsigned char  protocolVersionMajor;    /* SSL Version: major */
    unsigned char  protocolVersionMinor;    /* SSL Version: minor */
    unsigned char  serverCipherSuite0;      /* first byte, normally 0 */
    unsigned char  serverCipherSuite;       /* second byte, actual suite */
    unsigned char  serverCipherSuiteName[256];
            /* cipher name, e.g., "TLS_RSA_..." */
    unsigned char  serverNameIndication[128];
    unsigned int   keySize;
} WOLFSSL_PACK SSLInfo;


WOLFSSL_API
SSL_SNIFFER_API int ssl_DecodePacketWithSessionInfo(
                        const unsigned char* packet, int length,
                        unsigned char** data, SSLInfo* sslInfo, char* error);

typedef void (*SSLConnCb)(const void* session, SSLInfo* info, void* ctx);

WOLFSSL_API
SSL_SNIFFER_API int ssl_SetConnectionCb(SSLConnCb cb);

WOLFSSL_API
SSL_SNIFFER_API int ssl_SetConnectionCtx(void* ctx);


typedef struct SSLStats
{
    unsigned int sslStandardConns; /* X */
    unsigned int sslRehandshakeConns; /* unsupported */
    unsigned int sslClientAuthConns; /* X */
    unsigned int sslResumedConns; /* X */
    unsigned int sslResumedRehandshakeConns; /* unsupported */
    unsigned int sslClientAuthRehandshakeConns; /* unsupported */
    unsigned int sslEphemeralMisses;
    unsigned int sslResumeMisses; /* X */
    unsigned int sslCiphersUnsupported; /* X */
    unsigned int sslKeysUnmatched; /* X */
    unsigned int sslKeyFails;
    unsigned int sslDecodeFails;
    unsigned int sslAlerts; /* X */
    unsigned int sslDecryptedBytes;
    unsigned int sslEncryptedBytes;
    unsigned int sslEncryptedPackets;
    unsigned int sslDecryptedPackets;
    unsigned int sslEncryptedConnsPerSecond;
    unsigned int sslKeyMatches; /* X */
    unsigned int sslActiveEncryptedConnsPerSecond;
    unsigned int sslActiveFlowsPerSecond;
} SSLStats;


WOLFSSL_API
SSL_SNIFFER_API int ssl_ResetStatistics(void);


WOLFSSL_API
SSL_SNIFFER_API int ssl_ReadStatistics(SSLStats* stats);


#ifdef __cplusplus
    }  /* extern "C" */
#endif

#endif /* wolfSSL_SNIFFER_H */

