/*
 * Crypto.h
 * Copyright (c) 2018 Shenghua Su
 *
 */

#ifndef _CRYPTO_H_INCLUDED
#define _CRYPTO_H_INCLUDED

#include <cstddef>

bool encode(const unsigned char *bytes, size_t size, unsigned char *out, size_t *outLen);
bool decode(const unsigned char *bytes, size_t size, unsigned char *out, size_t *outLen);
void decryptBase64(const char *src, size_t length, char segmentSplit,
                   const unsigned char *key, const unsigned char *iv, char *result, size_t *size);

#endif // _CRYPTO_H_INCLUDED
