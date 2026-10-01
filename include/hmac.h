#pragma once

#include "sha256.h"

/* HMAC-SHA256 (RFC 2104). out receives 32 bytes. */
static inline void hmac_sha256(
    const uint8_t* key,
    size_t klen,
    const uint8_t* msg,
    size_t mlen,
    uint8_t out[SIZE_OF_SHA_256_HASH]
)
{
    uint8_t k[SIZE_OF_SHA_256_CHUNK];        // 64-byte padded key
    uint8_t inner[SIZE_OF_SHA_256_HASH];     // 32-byte inner hash
    struct SHA256 s;
    size_t i;

    // 1. K' = key padded with zeros to 64 bytes (hashed first if longer)
    for (i = 0; i < SIZE_OF_SHA_256_CHUNK; i++)
        k[i] = 0;
    if (klen > SIZE_OF_SHA_256_CHUNK)
        calc_sha_256(k, key, klen);          // fills k[0..31], rest stays 0
    else
        for (i = 0; i < klen; i++)
            k[i] = key[i];

    // 2. inner = SHA256((K' ^ 0x36) || msg)
    for (i = 0; i < SIZE_OF_SHA_256_CHUNK; i++)
        k[i] ^= 0x36;
    sha_256_init(&s, inner);
    sha_256_write(&s, k, SIZE_OF_SHA_256_CHUNK);
    sha_256_write(&s, msg, mlen);
    sha_256_close(&s);

    // 3. out = SHA256((K' ^ 0x5C) || inner)
    for (i = 0; i < SIZE_OF_SHA_256_CHUNK; i++)
        k[i] ^= 0x36 ^ 0x5C;                 // flip inner pad into outer pad
    sha_256_init(&s, out);
    sha_256_write(&s, k, SIZE_OF_SHA_256_CHUNK);
    sha_256_write(&s, inner, SIZE_OF_SHA_256_HASH);
    sha_256_close(&s);

    // wipe secrets from the stack
    xmemset(k, 0, sizeof(k));
    xmemset(inner, 0, sizeof(inner));
}