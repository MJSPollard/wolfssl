/* bio.c
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

#if !defined(WOLFSSL_BIO_INCLUDED)
    #ifndef WOLFSSL_IGNORE_FILE_WARN
        #warning bio.c does not need to be compiled separately from ssl.c
    #endif
#else


/* Helper function to decode a base64 input
 *
 * returns size of resulting buffer on success
 */
static int wolfSSL_BIO_BASE64_read(WOLFSSL_BIO* bio, void* buf, int len)
{
    word32 frmtSz = len;

    WOLFSSL_ENTER("wolfSSL_BIO_BASE64_read");

    if (Base64_Decode((const byte*)buf, (word32)len, (byte*)buf, &frmtSz) !=0) {
        WOLFSSL_MSG("Err doing base64 decode");
        return SSL_FATAL_ERROR;
    }

    (void)bio;
    return (int)frmtSz;
}


/* Helper function to read from WOLFSSL_BIO_BIO type
 *
 * returns amount in bytes read on success
 */
static int wolfSSL_BIO_BIO_read(WOLFSSL_BIO* bio, void* buf, int len)
{
    int   sz;
    char* pt;

    sz = wolfSSL_BIO_nread(bio, &pt, len);

    if (sz > 0) {
        XMEMCPY(buf, pt, sz);
    }

    return sz;
}


/* Handles reading from a memory type BIO and advancing the state.
 *
 * bio  WOLFSSL_BIO to read from
 * buf  buffer to put data from bio in
 * len  amount of data to be read
 *
 * returns size read on success
 */
static int wolfSSL_BIO_MEMORY_read(WOLFSSL_BIO* bio, void* buf, int len)
{
    int   sz;

    sz = wolfSSL_BIO_pending(bio);
    if (sz > 0) {
        const unsigned char* pt = NULL;
        int memSz;

        if (sz > len) {
            sz = len;
        }
        memSz = wolfSSL_BIO_get_mem_data(bio, (void*)&pt);
        if (memSz >= sz && pt != NULL) {
            byte* tmp;

            XMEMCPY(buf, (void*)pt, sz);
            if (memSz - sz > 0) {
                tmp = (byte*)XMALLOC(memSz-sz, bio->heap, DYNAMIC_TYPE_OPENSSL);
                if (tmp == NULL) {
                    WOLFSSL_MSG("Memory error");
                    return WOLFSSL_BIO_ERROR;
                }
                XMEMCPY(tmp, (void*)(pt + sz), memSz - sz);

                /* reset internal bio->mem */
                XFREE(bio->mem, bio->heap, DYNAMIC_TYPE_OPENSSL);
                bio->mem    = tmp;
                bio->memLen = memSz-sz;
                if (bio->mem_buf != NULL) {
                    bio->mem_buf->data = (char*)bio->mem;
                    bio->mem_buf->length = bio->memLen;
                }
            }
            bio->wrSz  -= sz;
        }
        else {
            WOLFSSL_MSG("Issue with getting bio mem pointer");
            return 0;
        }
    }
    else {
        return WOLFSSL_BIO_ERROR;
    }

    return sz;
}


/* Helper function to read from WOLFSSL_BIO_SSL type
 *
 * returns the number of bytes read on success
 */
static int wolfSSL_BIO_SSL_read(WOLFSSL_BIO* bio, void* buf,
        int len, WOLFSSL_BIO* front)
{
    int ret;

    WOLFSSL_ENTER("wolfSSL_BIO_SSL_read");

    /* already got eof, again is error */
    if ((front == NULL) || front->eof)
        return WOLFSSL_FATAL_ERROR;

    ret = wolfSSL_read(bio->ssl, buf, len);
    if (ret == 0)
        front->eof = 1;
    else if (ret < 0) {
        int err = wolfSSL_get_error(bio->ssl, 0);
        if ( !(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) )
            front->eof = 1;
    }

    return ret;
}



/* Used to read data from a WOLFSSL_BIO structure
 *
 * bio  structure to read data from
 * buf  buffer to hold the result
 * len  length of buf buffer
 *
 * returns the number of bytes read on success
 */
int wolfSSL_BIO_read(WOLFSSL_BIO* bio, void* buf, int len)
{
    int  ret = 0;
    WOLFSSL_BIO* front = bio;
    int  sz  = 0;

    WOLFSSL_ENTER("wolfSSL_BIO_read");

    /* start at end of list and work backwards */
    while ((bio != NULL) && (bio->next != NULL)) {
        bio = bio->next;
    }

    while (bio != NULL && ret >= 0) {
        /* formating data */
        if (bio->type == WOLFSSL_BIO_BASE64 && ret > 0 && sz > 0) {
            ret = wolfSSL_BIO_BASE64_read(bio, buf, sz);
        }

        /* write BIOs */
        if (bio && bio->type == WOLFSSL_BIO_BIO) {
            ret = wolfSSL_BIO_BIO_read(bio, buf, len);
        }

        if (bio && bio->type == WOLFSSL_BIO_MEMORY) {
            ret = wolfSSL_BIO_MEMORY_read(bio, buf, len);
        }

    #ifndef NO_FILESYSTEM
        if (bio && bio->type == WOLFSSL_BIO_FILE) {
            ret = (int)XFREAD(buf, 1, len, bio->file);
        }
    #endif

        if (bio && bio->type == WOLFSSL_BIO_SSL) {
            ret = wolfSSL_BIO_SSL_read(bio, buf, len, front);
        }

        /* case where front of list is done */
        if (bio == front) {
            break; /* at front of list so be done */
        }

        if (ret > 0) {
            sz = ret; /* adjust size for formating */
        }

        /* previous WOLFSSL_BIO in list working towards head of list */
        bio = bio->prev;
    }

    return ret;
}


/* Converts data into base64 output
 *
 * returns the resulting buffer size on success.
 */
static int wolfSSL_BIO_BASE64_write(WOLFSSL_BIO* bio, const void* data,
        word32 inLen, byte* out, word32* outLen)
{
    byte* tmp = NULL;
    int ret   = 0;

    WOLFSSL_ENTER("wolfSSL_BIO_BASE64_write");

    if (bio == NULL || data == NULL || out == NULL || outLen == NULL) {
        return BAD_FUNC_ARG;
    }

#if defined(WOLFSSL_BASE64_ENCODE)
    tmp = (byte*)XMALLOC(*outLen, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);
    if (tmp == NULL) {
        return WOLFSSL_FATAL_ERROR;
    }

    if ((bio->flags & WOLFSSL_BIO_FLAG_BASE64_NO_NL) ==
            WOLFSSL_BIO_FLAG_BASE64_NO_NL) {
        if (Base64_Encode_NoNl((const byte*)data, inLen,
                tmp, outLen) < 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    else {
        if (Base64_Encode((const byte*)data, inLen,
                tmp, outLen) < 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    if (ret != WOLFSSL_FATAL_ERROR) {
        ret = (int)*outLen;
        XMEMCPY(out, tmp, *outLen);

    }
    XFREE(tmp, bio->heap, DYNAMIC_TYPE_TMP_BUFFER);
#else
    (void)bio;
    (void)data;
    (void)inLen;
    (void)out;
    (void)outLen;
    (void)tmp;
    WOLFSSL_MSG("BASE64 encoding not compiled in");
#endif
    return ret;
}


/* Helper function for writing to a WOLFSSL_BIO_SSL type
 *
 * returns the amount written in bytes on success
 */
static int wolfSSL_BIO_SSL_write(WOLFSSL_BIO* bio, const void* data,
        int len, WOLFSSL_BIO* front)
{
    int ret;

    WOLFSSL_ENTER("wolfSSL_BIO_SSL_write");

    if (bio->ssl == 0) return BAD_FUNC_ARG;

    ret = wolfSSL_write(bio->ssl, data, len);
    if (ret == 0)
        front->eof = 1;
    else if (ret < 0) {
        int err = wolfSSL_get_error(bio->ssl, 0);
        if ( !(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) )
            front->eof = 1;
    }
    return ret;
}


/* Writes to a WOLFSSL_BIO_BIO type.
 *
 * returns the amount written on success
 */
static int wolfSSL_BIO_BIO_write(WOLFSSL_BIO* bio, const void* data,
        int len)
{
    int   sz;
    char* buf;

    WOLFSSL_ENTER("wolfSSL_BIO_BIO_write");

    /* adding in sanity checks for static analysis tools */
    if (bio == NULL || data == NULL) {
        return BAD_FUNC_ARG;
    }

    sz = wolfSSL_BIO_nwrite(bio, &buf, len);

    /* test space for write */
    if (sz <= 0) {
        WOLFSSL_MSG("No room left to write");
        return sz;
    }

    XMEMCPY(buf, data, sz);

    return sz;
}


/* for complete compatibility a bio memory write allocs its own memory
 * until the application runs out ....
 *
 * bio  structure to hold incoming data
 * data buffer holding the data to be written
 * len  length of data buffer
 *
 * returns the amount of data written on success and WOLFSSL_FAILURE or
 *         WOLFSSL_BIO_ERROR for failure cases.
 */
static int wolfSSL_BIO_MEMORY_write(WOLFSSL_BIO* bio, const void* data,
        int len)
{
    int   sz;
    const unsigned char* buf;

    WOLFSSL_ENTER("wolfSSL_BIO_MEMORY_write");

    if (bio == NULL || data == NULL) {
        return BAD_FUNC_ARG;
    }

    sz = wolfSSL_BIO_pending(bio);
    if (sz < 0) {
        WOLFSSL_MSG("Error getting memory data");
        return sz;
    }

    if (bio->mem == NULL) {
        bio->mem = (byte*)XMALLOC(len, bio->heap, DYNAMIC_TYPE_OPENSSL);
        if (bio->mem == NULL) {
            WOLFSSL_MSG("Error on malloc");
            return WOLFSSL_FAILURE;
        }
        bio->memLen = len;
        if (bio->mem_buf != NULL) {
            bio->mem_buf->data = (char*)bio->mem;
            bio->mem_buf->length = bio->memLen;
        }
    }

    /* check if will fit in current buffer size */
    if (wolfSSL_BIO_get_mem_data(bio, (void*)&buf) < 0) {
        return WOLFSSL_BIO_ERROR;
    }
    if (bio->memLen < sz + len) {
        bio->mem = (byte*)XREALLOC(bio->mem, sz + len, bio->heap,
            DYNAMIC_TYPE_OPENSSL);
        if (bio->mem == NULL) {
            WOLFSSL_MSG("Error on realloc");
            return WOLFSSL_FAILURE;
        }
        bio->memLen = sz + len;
        if (bio->mem_buf != NULL) {
            bio->mem_buf->data = (char*)bio->mem;
            bio->mem_buf->length = bio->memLen;
        }
    }

    XMEMCPY(bio->mem + sz, data, len);
    bio->wrSz += len;

    return len;
}


/* Writes data to a WOLFSSL_BIO structure
 *
 * bio  structure to write to
 * data holds the data to be written
 * len  length of data buffer
 *
 * returns the amount written in bytes on success
 */
int wolfSSL_BIO_write(WOLFSSL_BIO* bio, const void* data, int len)
{
    int  ret = 0;
    WOLFSSL_BIO* front = bio;
    void*  frmt   = NULL;
    word32 frmtSz = 0;

    WOLFSSL_ENTER("wolfSSL_BIO_write");

    while (bio != NULL && ret >= 0) {
        /* check for formating */
        if (bio && bio->type == WOLFSSL_BIO_BASE64) {
#if defined(WOLFSSL_BASE64_ENCODE)
            word32 sz = 0;

            if (bio->flags & WOLFSSL_BIO_FLAG_BASE64_NO_NL) {
                if (Base64_Encode_NoNl((const byte*)data, len, NULL,
                            &sz) != LENGTH_ONLY_E) {
                    WOLFSSL_MSG("Error with base 64 get length");
                    ret = SSL_FATAL_ERROR;
                }
            }
            else {
                if (Base64_Encode((const byte*)data, len, NULL, &sz) !=
                    LENGTH_ONLY_E) {
                    WOLFSSL_MSG("Error with base 64 get length");
                    ret = SSL_FATAL_ERROR;
                }
            }

            if (frmt == NULL && sz > 0 && ret != SSL_FATAL_ERROR) {
                frmt = (void*)XMALLOC(sz, front->heap,
                        DYNAMIC_TYPE_TMP_BUFFER);
                if (frmt == NULL) {
                    WOLFSSL_MSG("Memory error");
                    ret = SSL_FATAL_ERROR;
                }
                frmtSz = sz;
            }
            else if (sz > frmtSz) {
                frmt = (void*)XREALLOC(frmt, sz, front->heap,
                        DYNAMIC_TYPE_TMP_BUFFER);
                if (frmt == NULL) {
                    WOLFSSL_MSG("Memory error");
                    ret = SSL_FATAL_ERROR;
                }
                /* since frmt already existed then data should point to knew
                   formated buffer */
                data = frmt;
                len  = frmtSz;
                frmtSz = sz;
            }
#endif /* defined(WOLFSSL_BASE64_ENCODE) */

            if (ret >= 0) {
                /* change so that data is formated buffer */
                ret = wolfSSL_BIO_BASE64_write(bio, data, (word32)len,
                        (byte*)frmt, &frmtSz);
                data = frmt;
                len  = frmtSz;
            }
        }

        /* write bios */
        if (bio && bio->type == WOLFSSL_BIO_BIO) {
            ret = wolfSSL_BIO_BIO_write(bio, data, len);
        }

        if (bio && bio->type == WOLFSSL_BIO_MEMORY) {
            ret = wolfSSL_BIO_MEMORY_write(bio, data, len);
        }

    #ifndef NO_FILESYSTEM
        if (bio && bio->type == WOLFSSL_BIO_FILE) {
            ret = (int)XFWRITE(data, 1, len, bio->file);
        }
    #endif

        if (bio && bio->type == WOLFSSL_BIO_SSL) {
            /* already got eof, again is error */
            if (bio && front->eof) {
                ret = SSL_FATAL_ERROR;
            }
            else {
                ret = wolfSSL_BIO_SSL_write(bio, data, len, front);
            }
        }

        /* advance to the next bio in list */
        bio = bio->next;
    }

    if (frmt != NULL) {
        XFREE(frmt, front->heap, DYNAMIC_TYPE_TMP_BUFFER);
    }

    return ret;
}

/* Wrapper for other BIO type funcions, expected to grow as OpenSSL compatability
 * layer grows.
 * 
 * return info. specific to the cmd that is passed in.
 */
#if defined(OPENSSL_ALL)
long wolfSSL_BIO_ctrl(WOLFSSL_BIO *bio, int cmd, long larg, void *parg)
{
    long ret;

    (void)larg; /* not currently used */

    WOLFSSL_ENTER("wolfSSL_BIO_ctrl");

    switch(cmd) {
        case BIO_CTRL_WPENDING:
            ret = wolfSSL_BIO_ctrl_pending(bio);
            break;
        case BIO_CTRL_INFO:
            ret = wolfSSL_BIO_get_mem_data(bio, parg);
            break;
        default:
            WOLFSSL_MSG("CMD not yet implemented");
            ret = WOLFSSL_FAILURE;
            break;
    }
    return ret;
}
#endif


/* helper function for wolfSSL_BIO_gets
 * size till a newline is hit
 * returns the number of bytes including the new line character
 */
static int wolfSSL_getLineLength(char* in, int inSz)
{
    int i;

    for (i = 0; i < inSz; i++) {
        if (in[i] == '\n') {
            return i + 1; /* includes new line character */
        }
    }

    return inSz; /* rest of buffer is all one line */
}


/* Gets the next line from bio. Goes until a new line character or end of
 * buffer is reached.
 *
 * bio  the structure to read a new line from
 * buf  buffer to hold the result
 * sz   the size of "buf" buffer
 *
 * returns the size of the result placed in buf on success and a 0 or negative
 *         value in an error case.
 */
int wolfSSL_BIO_gets(WOLFSSL_BIO* bio, char* buf, int sz)
{
    int ret = WOLFSSL_BIO_UNSET;

    WOLFSSL_ENTER("wolfSSL_BIO_gets");

    if (bio == NULL || buf == NULL) {
        return WOLFSSL_FAILURE;
    }

    /* not enough space for character plus terminator */
    if (sz <= 1) {
        return 0;
    }

    switch (bio->type) {
#ifndef NO_FILESYSTEM
        case WOLFSSL_BIO_FILE:
            if (bio->file == XBADFILE) {
                return WOLFSSL_BIO_ERROR;
            }

            #if defined(MICRIUM) || defined(LSR_FS) || defined(EBSNET)
            WOLFSSL_MSG("XFGETS not ported for this system yet");
            ret = XFGETS(buf, sz, bio->file);
            #else
            if (XFGETS(buf, sz, bio->file) != NULL) {
                ret = (int)XSTRLEN(buf);
            }
            else {
                ret = WOLFSSL_BIO_ERROR;
            }
            #endif
            break;
#endif /* NO_FILESYSTEM */
        case WOLFSSL_BIO_MEMORY:
            {
                const byte* c;
                int   cSz;
                cSz = wolfSSL_BIO_pending(bio);
                if (cSz == 0) {
                    ret = 0; /* Nothing to read */
                    buf[0] = '\0';
                    break;
                }

                if (wolfSSL_BIO_get_mem_data(bio, (void*)&c) <= 0) {
                    ret = WOLFSSL_BIO_ERROR;
                    break;
                }

                cSz = wolfSSL_getLineLength((char*)c, cSz);
                /* check case where line was bigger then buffer and buffer
                 * needs end terminator */
                if (cSz >= sz) {
                    cSz = sz - 1;
                    buf[cSz] = '\0';
                }
                else {
                    /* not minus 1 here because placing terminator after
                       msg and have checked that sz is large enough */
                    buf[cSz] = '\0';
                }

                ret = wolfSSL_BIO_MEMORY_read(bio, (void*)buf, cSz);
                /* ret is read after the switch statement */
                break;
            }
        case WOLFSSL_BIO_BIO:
            {
                char* c;
                int   cSz;
                cSz = wolfSSL_BIO_nread0(bio, &c);
                if (cSz == 0) {
                    ret = 0; /* Nothing to read */
                    buf[0] = '\0';
                    break;
                }

                cSz = wolfSSL_getLineLength(c, cSz);
                /* check case where line was bigger then buffer and buffer
                 * needs end terminator */
                if (cSz >= sz) {
                    cSz = sz - 1;
                    buf[cSz] = '\0';
                }
                else {
                    /* not minus 1 here because placing terminator after
                       msg and have checked that sz is large enough */
                    buf[cSz] = '\0';
                }

                ret = wolfSSL_BIO_nread(bio, &c, cSz);
                if (ret > 0 && ret < sz) {
                    XMEMCPY(buf, c, ret);
                }
                break;
            }

        default:
            WOLFSSL_MSG("BIO type not supported yet with wolfSSL_BIO_gets");
    }

    return ret;
}


/* searches through bio list for a BIO of type "type"
 * returns NULL on failure to find a given type */
WOLFSSL_BIO* wolfSSL_BIO_find_type(WOLFSSL_BIO* bio, int type)
{
    WOLFSSL_BIO* local = NULL;
    WOLFSSL_BIO* current;

    WOLFSSL_ENTER("wolfSSL_BIO_find_type");

    if (bio == NULL) {
        return local;
    }

    current = bio;
    while (current != NULL) {
        if (current->type == type) {
            WOLFSSL_MSG("Found matching WOLFSSL_BIO type");
            local = current;
            break;
        }
        current = current->next;
    }

    return local;
}


/* returns a pointer to the next WOLFSSL_BIO in the chain on success.
 * If a failure case then NULL is returned */
WOLFSSL_BIO* wolfSSL_BIO_next(WOLFSSL_BIO* bio)
{
    WOLFSSL_ENTER("wolfSSL_BIO_next");

    if (bio == NULL) {
        WOLFSSL_MSG("Bad argument passed in");
        return NULL;
    }

    return bio->next;
}

/* BIO_wpending returns the number of bytes pending to be written. */
size_t wolfSSL_BIO_wpending(const WOLFSSL_BIO *bio)
{
    WOLFSSL_ENTER("BIO_wpending");

    if (bio == NULL)
        return 0;

    if (bio->ssl != NULL) {
        /* not supported case */
        return 0;
    }

    if (bio->type == WOLFSSL_BIO_MEMORY) {
        return bio->wrSz;
    }

    /* type BIO_BIO then check paired buffer */
    if (bio->type == WOLFSSL_BIO_BIO && bio->pair != NULL) {
        WOLFSSL_BIO* pair = bio->pair;
        return pair->wrIdx;
    }

    return 0;
}

/* Return the number of pending bytes in read and write buffers */
size_t wolfSSL_BIO_ctrl_pending(WOLFSSL_BIO *bio)
{
    WOLFSSL_ENTER("BIO_ctrl_pending");
    if (bio == NULL) {
        return 0;
    }

    if (bio->ssl != NULL) {
        return (long)wolfSSL_pending(bio->ssl);
    }

    if (bio->type == WOLFSSL_BIO_MEMORY) {
        return bio->wrSz;
    }

    /* type BIO_BIO then check paired buffer */
    if (bio->type == WOLFSSL_BIO_BIO && bio->pair != NULL) {
        WOLFSSL_BIO* pair = bio->pair;
        if (pair->wrIdx > 0 && pair->wrIdx <= pair->rdIdx) {
            /* in wrap around state where beginning of buffer is being
             * overwritten */
            return pair->wrSz - pair->rdIdx + pair->wrIdx;
        }
        else {
            /* simple case where has not wrapped around */
            return pair->wrIdx - pair->rdIdx;
        }
    }

    return 0;
}


long wolfSSL_BIO_get_mem_ptr(WOLFSSL_BIO *bio, WOLFSSL_BUF_MEM **ptr)
{
    WOLFSSL_ENTER("wolfSSL_BIO_get_mem_ptr");

    if (bio == NULL || ptr == NULL) {
        return WOLFSSL_FAILURE;
    }

    if (bio->type == WOLFSSL_BIO_FILE || bio->type == WOLFSSL_BIO_SOCKET) {
        WOLFSSL_MSG("NO memory buffer for FILE type");
        return SSL_FAILURE;
    }

    *ptr = bio->mem_buf;

    return SSL_SUCCESS;
}

WOLFSSL_API long wolfSSL_BIO_int_ctrl(WOLFSSL_BIO *bp, int cmd, long larg, int iarg)
{
    (void) bp;
    (void) cmd;
    (void) larg;
    (void) iarg;
    WOLFSSL_STUB("BIO_int_ctrl");
    return 0;
}


int wolfSSL_BIO_set_write_buf_size(WOLFSSL_BIO *bio, long size)
{
    WOLFSSL_ENTER("wolfSSL_BIO_set_write_buf_size");

    if (bio == NULL || bio->type != WOLFSSL_BIO_BIO || size < 0) {
        return WOLFSSL_FAILURE;
    }

    /* if already in pair then do not change size */
    if (bio->pair != NULL) {
        WOLFSSL_MSG("WOLFSSL_BIO is paired, free from pair before changing");
        return WOLFSSL_FAILURE;
    }

    bio->wrSz  = (int)size;
    if (bio->wrSz < 0) {
        WOLFSSL_MSG("Unexpected negative size value");
        return WOLFSSL_FAILURE;
    }

    if (bio->mem != NULL) {
        XFREE(bio->mem, bio->heap, DYNAMIC_TYPE_OPENSSL);
    }

    bio->mem = (byte*)XMALLOC(bio->wrSz, bio->heap, DYNAMIC_TYPE_OPENSSL);
    if (bio->mem == NULL) {
        WOLFSSL_MSG("Memory allocation error");
        return WOLFSSL_FAILURE;
    }
    bio->memLen = bio->wrSz;
    bio->wrIdx = 0;
    bio->rdIdx = 0;
    if (bio->mem_buf != NULL) {
        bio->mem_buf->data = (char*)bio->mem;
        bio->mem_buf->length = bio->memLen;
    }

    return WOLFSSL_SUCCESS;
}


/* Joins two BIO_BIO types. The write of b1 goes to the read of b2 and vise
 * versa. Creating something similar to a two way pipe.
 * Reading and writing between the two BIOs is not thread safe, they are
 * expected to be used by the same thread. */
int wolfSSL_BIO_make_bio_pair(WOLFSSL_BIO *b1, WOLFSSL_BIO *b2)
{
    WOLFSSL_ENTER("wolfSSL_BIO_make_bio_pair");

    if (b1 == NULL || b2 == NULL) {
        WOLFSSL_LEAVE("wolfSSL_BIO_make_bio_pair", BAD_FUNC_ARG);
        return WOLFSSL_FAILURE;
    }

    /* both are expected to be of type BIO and not already paired */
    if (b1->type != WOLFSSL_BIO_BIO || b2->type != WOLFSSL_BIO_BIO ||
        b1->pair != NULL || b2->pair != NULL) {
        WOLFSSL_MSG("Expected type BIO and not already paired");
        return WOLFSSL_FAILURE;
    }

    /* set default write size if not already set */
    if (b1->mem == NULL && wolfSSL_BIO_set_write_buf_size(b1,
                            WOLFSSL_BIO_SIZE) != WOLFSSL_SUCCESS) {
        return WOLFSSL_FAILURE;
    }

    if (b2->mem == NULL && wolfSSL_BIO_set_write_buf_size(b2,
                            WOLFSSL_BIO_SIZE) != WOLFSSL_SUCCESS) {
        return WOLFSSL_FAILURE;
    }

    b1->pair = b2;
    b2->pair = b1;

    return WOLFSSL_SUCCESS;
}


int wolfSSL_BIO_ctrl_reset_read_request(WOLFSSL_BIO *b)
{
    WOLFSSL_ENTER("wolfSSL_BIO_ctrl_reset_read_request");

    if (b == NULL || b->type == WOLFSSL_BIO_MEMORY) {
        return SSL_FAILURE;
    }

    b->readRq = 0;

    return WOLFSSL_SUCCESS;
}


/* Does not advance read index pointer */
int wolfSSL_BIO_nread0(WOLFSSL_BIO *bio, char **buf)
{
    WOLFSSL_ENTER("wolfSSL_BIO_nread0");

    if (bio == NULL || buf == NULL) {
        WOLFSSL_MSG("NULL argument passed in");
        return 0;
    }

    /* if paired read from pair */
    if (bio->pair != NULL) {
        WOLFSSL_BIO* pair = bio->pair;

        /* case where have wrapped around write buffer */
        *buf = (char*)pair->mem + pair->rdIdx;
        if (pair->wrIdx > 0 && pair->rdIdx >= pair->wrIdx) {
            return pair->wrSz - pair->rdIdx;
        }
        else {
            return pair->wrIdx - pair->rdIdx;
        }
    }

    return 0;
}


/* similar to wolfSSL_BIO_nread0 but advances the read index */
int wolfSSL_BIO_nread(WOLFSSL_BIO *bio, char **buf, int num)
{
    int sz = WOLFSSL_BIO_UNSET;

    WOLFSSL_ENTER("wolfSSL_BIO_nread");

    if (bio == NULL || buf == NULL) {
        WOLFSSL_MSG("NULL argument passed in");
        return WOLFSSL_FAILURE;
    }

    if (bio->type == WOLFSSL_BIO_MEMORY) {
        return SSL_FAILURE;
    }

    if (bio->pair != NULL) {
        /* special case if asking to read 0 bytes */
        if (num == 0) {
            *buf = (char*)bio->pair->mem + bio->pair->rdIdx;
            return 0;
        }

        /* get amount able to read and set buffer pointer */
        sz = wolfSSL_BIO_nread0(bio, buf);
        if (sz == 0) {
            return WOLFSSL_BIO_ERROR;
        }

        if (num < sz) {
            sz = num;
        }
        bio->pair->rdIdx += sz;

        /* check if have read to the end of the buffer and need to reset */
        if (bio->pair->rdIdx == bio->pair->wrSz) {
            bio->pair->rdIdx = 0;
            if (bio->pair->wrIdx == bio->pair->wrSz) {
                bio->pair->wrIdx = 0;
            }
        }

        /* check if read up to write index, if so then reset indexs */
        if (bio->pair->rdIdx == bio->pair->wrIdx) {
            bio->pair->rdIdx = 0;
            bio->pair->wrIdx = 0;
        }
    }

    return sz;
}


int wolfSSL_BIO_nwrite(WOLFSSL_BIO *bio, char **buf, int num)
{
    int sz = WOLFSSL_BIO_UNSET;

    WOLFSSL_ENTER("wolfSSL_BIO_nwrite");

    if (bio == NULL || buf == NULL) {
        WOLFSSL_MSG("NULL argument passed in");
        return 0;
    }

    if (bio->type != WOLFSSL_BIO_BIO) {
        return SSL_FAILURE;
    }

    if (bio->pair != NULL) {
        if (num == 0) {
            *buf = (char*)bio->mem + bio->wrIdx;
            return 0;
        }

        if (bio->wrIdx < bio->rdIdx) {
            /* if wrapped around only write up to read index. In this case
             * rdIdx is always greater then wrIdx so sz will not be negative. */
            sz = bio->rdIdx - bio->wrIdx;
        }
        else if (bio->rdIdx > 0 && bio->wrIdx == bio->rdIdx) {
            return WOLFSSL_BIO_ERROR; /* no more room to write */
        }
        else {
            /* write index is past read index so write to end of buffer */
            sz = bio->wrSz - bio->wrIdx;

            if (sz <= 0) {
                /* either an error has occured with write index or it is at the
                 * end of the write buffer. */
                if (bio->rdIdx == 0) {
                    /* no more room, nothing has been read */
                    return WOLFSSL_BIO_ERROR;
                }

                bio->wrIdx = 0;

                /* check case where read index is not at 0 */
                if (bio->rdIdx > 0) {
                    sz = bio->rdIdx; /* can write up to the read index */
                }
                else {
                    sz = bio->wrSz; /* no restriction other then buffer size */
                }
            }
        }

        if (num < sz) {
            sz = num;
        }
        *buf = (char*)bio->mem + bio->wrIdx;
        bio->wrIdx += sz;

        /* if at the end of the buffer and space for wrap around then set
         * write index back to 0 */
        if (bio->wrIdx == bio->wrSz && bio->rdIdx > 0) {
            bio->wrIdx = 0;
        }
    }

    return sz;
}


/* Reset BIO to initial state */
int wolfSSL_BIO_reset(WOLFSSL_BIO *bio)
{
    WOLFSSL_ENTER("wolfSSL_BIO_reset");

    if (bio == NULL) {
        WOLFSSL_MSG("NULL argument passed in");
        /* -1 is consistent failure even for FILE type */
        return WOLFSSL_BIO_ERROR;
    }

    switch (bio->type) {
        #ifndef NO_FILESYSTEM
        case WOLFSSL_BIO_FILE:
            XREWIND(bio->file);
            return 0;
        #endif

        case WOLFSSL_BIO_BIO:
            bio->rdIdx = 0;
            bio->wrIdx = 0;
            return 0;

        case WOLFSSL_BIO_MEMORY:
            bio->rdIdx = 0;
            bio->wrIdx = 0;
            bio->wrSz  = 0;
            XFREE(bio->mem, bio->heap, DYNAMIC_TYPE_OPENSSL);
            bio->mem = NULL;
            bio->memLen = 0;
            if (bio->mem_buf != NULL) {
                bio->mem_buf->data = (char*)bio->mem;
                bio->mem_buf->length = bio->memLen;
            }
            return 0;

        default:
            WOLFSSL_MSG("Unknown BIO type needs added to reset function");
    }

    return WOLFSSL_BIO_ERROR;
}

#ifndef NO_FILESYSTEM
long wolfSSL_BIO_set_fp(WOLFSSL_BIO *bio, XFILE fp, int c)
{
    WOLFSSL_ENTER("wolfSSL_BIO_set_fp");

    if (bio == NULL || fp == XBADFILE) {
        WOLFSSL_LEAVE("wolfSSL_BIO_set_fp", BAD_FUNC_ARG);
        return WOLFSSL_FAILURE;
    }

    if (bio->type != WOLFSSL_BIO_FILE) {
        return WOLFSSL_FAILURE;
    }

    bio->close = (byte)c;
    bio->file  = fp;

    return WOLFSSL_SUCCESS;
}


long wolfSSL_BIO_get_fp(WOLFSSL_BIO *bio, XFILE* fp)
{
    WOLFSSL_ENTER("wolfSSL_BIO_get_fp");

    if (bio == NULL || fp == XBADFILE) {
        return WOLFSSL_FAILURE;
    }

    if (bio->type != WOLFSSL_BIO_FILE) {
        return SSL_FAILURE;
    }

    *fp = bio->file;

    return WOLFSSL_SUCCESS;
}

/* overwrites file */
int wolfSSL_BIO_write_filename(WOLFSSL_BIO *bio, char *name)
{
    WOLFSSL_ENTER("wolfSSL_BIO_write_filename");

    if (bio == NULL || name == NULL) {
        return WOLFSSL_FAILURE;
    }

    if (bio->type == WOLFSSL_BIO_FILE) {
        if (bio->file != XBADFILE && bio->close == BIO_CLOSE) {
            XFCLOSE(bio->file);
        }

        bio->file = XFOPEN(name, "w");
        if (bio->file == XBADFILE) {
            return WOLFSSL_FAILURE;
        }
        bio->close = BIO_CLOSE;

        return WOLFSSL_SUCCESS;
    }

    return WOLFSSL_FAILURE;
}


int wolfSSL_BIO_seek(WOLFSSL_BIO *bio, int ofs)
{
      WOLFSSL_ENTER("wolfSSL_BIO_seek");

      if (bio == NULL) {
          return -1;
      }

      /* offset ofs from beginning of file */
      if (bio->type == WOLFSSL_BIO_FILE &&
              XFSEEK(bio->file, ofs, SEEK_SET) < 0) {
          return -1;
      }

      return 0;
}
#endif /* NO_FILESYSTEM */


long wolfSSL_BIO_set_mem_eof_return(WOLFSSL_BIO *bio, int v)
{
      WOLFSSL_ENTER("wolfSSL_BIO_set_mem_eof_return");

      if (bio != NULL) {
        bio->eof = v;
      }

      return 0;
}
#endif /* WOLFSSL_BIO_INCLUDED */

