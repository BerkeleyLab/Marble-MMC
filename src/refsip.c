// Most of this file is pulled out of libsodium, up until the main test program.
// This is a simplified/toy SipHash for messages that are multiples of 8 bytes
// (64-bits) only, without the padding and length encoding step.
// Do not use unless that message already has some form of length encoding embedded.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "refsip.h"

#ifdef DEBUG
#define debug 1
#else
#define debug 0
#endif

#define STORE64_BE(DST, W) store64_be((DST), (W))
static inline void
store64_be(uint8_t dst[8], uint64_t w)
{
#ifdef NATIVE_BIG_ENDIAN
    memcpy(dst, &w, sizeof w);
#else
    dst[7] = (uint8_t) w; w >>= 8;
    dst[6] = (uint8_t) w; w >>= 8;
    dst[5] = (uint8_t) w; w >>= 8;
    dst[4] = (uint8_t) w; w >>= 8;
    dst[3] = (uint8_t) w; w >>= 8;
    dst[2] = (uint8_t) w; w >>= 8;
    dst[1] = (uint8_t) w; w >>= 8;
    dst[0] = (uint8_t) w;
#endif
}

#define LOAD64_BE(SRC) load64_be(SRC)
static inline uint64_t
load64_be(const uint8_t src[8])
{
#ifdef NATIVE_BIG_ENDIAN
    uint64_t w;
    memcpy(&w, src, sizeof w);
    return w;
#else
    uint64_t w = (uint64_t) src[7];
    w |= (uint64_t) src[6] <<  8;
    w |= (uint64_t) src[5] << 16;
    w |= (uint64_t) src[4] << 24;
    w |= (uint64_t) src[3] << 32;
    w |= (uint64_t) src[2] << 40;
    w |= (uint64_t) src[1] << 48;
    w |= (uint64_t) src[0] << 56;
    return w;
#endif
}

#define LOAD64_LE(SRC) load64_le(SRC)
static inline uint64_t
load64_le(const uint8_t src[8])
{
#ifdef NATIVE_LITTLE_ENDIAN
    uint64_t w;
    memcpy(&w, src, sizeof w);
    return w;
#else
    uint64_t w = (uint64_t) src[0];
    w |= (uint64_t) src[1] <<  8;
    w |= (uint64_t) src[2] << 16;
    w |= (uint64_t) src[3] << 24;
    w |= (uint64_t) src[4] << 32;
    w |= (uint64_t) src[5] << 40;
    w |= (uint64_t) src[6] << 48;
    w |= (uint64_t) src[7] << 56;
    return w;
#endif
}

#define ROTL64(X, B) rotl64((X), (B))
static inline uint64_t
rotl64(const uint64_t x, const int b)
{
    return (x << b) | (x >> (64 - b));
}

#define SIPROUND             \
    do {                     \
        if (debug) printf("sipround in  %16.16llx %16.16llx %16.16llx %16.16llx\n", v0, v1, v2, v3); \
        v0 += v1;            \
        v1 = ROTL64(v1, 13); \
        v1 ^= v0;            \
        v0 = ROTL64(v0, 32); \
        v2 += v3;            \
        v3 = ROTL64(v3, 16); \
        v3 ^= v2;            \
        v0 += v3;            \
        v3 = ROTL64(v3, 21); \
        v3 ^= v0;            \
        v2 += v1;            \
        v1 = ROTL64(v1, 17); \
        v1 ^= v2;            \
        v2 = ROTL64(v2, 32); \
        if (debug) printf("sipround out %16.16llx %16.16llx %16.16llx %16.16llx\n", v0, v1, v2, v3); \
    } while (0)

void core_siphash(unsigned char *out, const unsigned char *in,
	unsigned long inlen, const unsigned char *k)
{
    /* "somepseudorandomlygeneratedbytes" */
    uint64_t       v0 = 0x736f6d6570736575ULL;
    uint64_t       v1 = 0x646f72616e646f6dULL;
    uint64_t       v2 = 0x6c7967656e657261ULL;
    uint64_t       v3 = 0x7465646279746573ULL;
    uint64_t       b;
    uint64_t       k0 = LOAD64_LE(k);
    uint64_t       k1 = LOAD64_LE(k + 8);
    uint64_t       m;
    const uint8_t *end  = in + inlen - (inlen % sizeof(uint64_t));
    v3 ^= k1;
    v2 ^= k0;
    v1 ^= k1;
    v0 ^= k0;
    for (; in != end; in += 8) {
        // Use big-endian because that's network byte order?
        m = LOAD64_BE(in);
        if (debug) printf("siphash m    %16.16llx\n", m);
        v3 ^= m;
        SIPROUND;
        SIPROUND;
        v0 ^= m;
    }
    v2 ^= 0xff;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    b = v0 ^ v1 ^ v2 ^ v3;
    if (debug) printf("siphash b    %16.16llx\n", b);
    STORE64_BE(out, b);
}

// end of material snarfed from libsodium

// Not provided here: sip_sign() sip_check() usage() main()
// See refsip_test.c
