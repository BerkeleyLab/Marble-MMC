#ifndef __REFSIP_H
#define __REFSIP_H

#ifdef __cplusplus
extern "C" {
#endif

void core_siphash(unsigned char *out, const unsigned char *in,
        unsigned long inlen, const unsigned char *k);

#ifdef __cplusplus
}
#endif

#endif // __REFSIP_H
