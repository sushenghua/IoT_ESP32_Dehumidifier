/*
 * Crypto
 * Copyright (c) 2018 Shenghua Su
 *
 */

#include "Crypto.h"

#include "Config.h"
#include "mbedtls/base64.h"
#include "mbedtls/aes.h"
#include "esp_log.h"
#include "AppLog.h"
#include <string.h>


#ifdef DEBUG_CRYPT
#include "esp_log.h"
void printBlock(void *data, size_t size)
{
  APP_LOGC("[printBlock]", "print block of length: %d", size);
  ESP_LOG_BUFFER_HEXDUMP("PLAIN", data, size, ESP_LOG_INFO);
}
#endif


#define BUFSIZE 256
unsigned char _encodedBuf[BUFSIZE];
unsigned char _decodedBuf[BUFSIZE];
unsigned char _iv[16];

bool encode(const unsigned char *bytes, size_t size, unsigned char *out, size_t *outLen)
{
  if (size > BUFSIZE) return false;
  if (0 != mbedtls_base64_encode(_encodedBuf, BUFSIZE, outLen, bytes, size))
    return false;
  memcpy(out, _encodedBuf, *outLen);
  out[*outLen] = '\0';
  return true;
}

bool decode(const unsigned char *bytes, size_t size, unsigned char *out, size_t *outLen)
{
  if (size > BUFSIZE) return false;
  memcpy(_encodedBuf, bytes, size);
  if (0 != mbedtls_base64_decode(_decodedBuf, BUFSIZE, outLen, _encodedBuf, size))
    return false;
  memcpy(out, _decodedBuf, *outLen);
  out[*outLen] = '\0';
  return true;
}

bool _decryptCbcInternal(mbedtls_aes_context *ctx, const unsigned char *src, size_t srcLen,
                         const unsigned char *iv, unsigned char *output, size_t *outLen)
{
  int ret;
  ret = mbedtls_base64_decode(_decodedBuf, BUFSIZE, outLen, src, srcLen);
#ifdef DEBUG_CRYPT_DETAIL
  APP_LOGC("[decryptCrt]", "mbedtls_base64_decode ret: %d, oLen: %d", ret, *outLen);
#endif
  if (ret != 0) return false;

  memcpy(_iv, iv, 16);
  ret = mbedtls_aes_crypt_cbc(ctx, MBEDTLS_AES_DECRYPT, *outLen, _iv, _decodedBuf, output);
#ifdef DEBUG_CRYPT_DETAIL
  APP_LOGC("[decryptCrt]", "mbedtls_aes_crypt_cbc ret: %d", ret);
  // APP_LOGC("[decryptCrt]", "mbedtls_aes_crypt_cbc ret: %d, iv: %.*s", ret, 16, iv);
#endif
  if (ret != 0) return false;

  return true;
}


unsigned char _segmentBuf[BUFSIZE];
unsigned char _decryptedBuf[BUFSIZE];

void decryptBase64(const char *src, size_t length, char segmentSplit,
                   const unsigned char *key, const unsigned char *iv, char *result, size_t *size)
{
  if (!src || !key || !iv || !result) return;

  size_t _decryptedLen = 0;

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);

  int ret = mbedtls_aes_setkey_dec(&ctx, key, strlen((const char*)key)*8);
#ifdef DEBUG_CRYPT
  APP_LOGC("[decryptCrt]", "mbedtls_aes_setkey_dec ret: %d", ret);
#endif
  if (ret == 0) {
    *size = 0;
    size_t searchLen = length;
    const char *segmentStart = src;
    const char *splitCh;
    char *dest = result;
    bool process;

    splitCh = (char *)memchr(segmentStart, segmentSplit, searchLen);
    process = (splitCh != NULL);

    // single segment
    if ( !process && (length > 16 && length < BUFSIZE) ) {// aes-cbc cipher >= 16, base64 of cipher > 16
      process = true;
      splitCh = src + length;
    }

    while (process) {
      // copy source line
      size_t segmentLen = splitCh - segmentStart;
      memcpy(_segmentBuf, segmentStart, segmentLen);
      // _segmentBuf[segmentLen] = '\0';

      // memset(_decryptedBuf, 0, BUFSIZE);
      if (!_decryptCbcInternal(&ctx, _segmentBuf, segmentLen, iv, _decryptedBuf, &_decryptedLen))
        break;
#ifdef DEBUG_CRYPT
      // APP_LOGC("[decryptCrt]", "_decryptInternal OK, searchLen: %d, src: %.*s, decrypt len: %d",
      //               searchLen, segmentLen, segmentStart, _decryptedLen);
      printBlock(_decryptedBuf, _decryptedLen);
#endif
      // remove padding
      _decryptedLen -= _decryptedBuf[_decryptedLen - 1];

#ifdef DEBUG_CRYPT
      // APP_LOGC("[decryptCrt]", "decrypt: %.*s", _decryptedLen, _decryptedBuf);
#endif
      // copy to output buffer and move output buffer pointer foreward
      memcpy(dest, _decryptedBuf, _decryptedLen);
      dest[_decryptedLen] = segmentSplit;
      dest += _decryptedLen + 1;
      *size += _decryptedLen + 1;

      // check next line
      segmentStart = splitCh + 1;
      searchLen -= segmentLen + 1;
      if (searchLen <= 0 || searchLen > length) { // size_t value minus operation may overflow
        dest[0] = '\0';
        break;
      }
      splitCh = (char *)memchr(segmentStart, segmentSplit, searchLen);
      process = (splitCh != NULL);
    }
  };

  mbedtls_aes_free(&ctx);
}
