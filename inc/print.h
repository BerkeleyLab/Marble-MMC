#ifndef DEC_H
#define DEC_H

#include <stdint.h>
#include <stddef.h>

// Format integer `val` with `n` total digits and `dp` decimal places into `buf`.
void dec_dp(int32_t val, uint8_t n, uint8_t dp, char *buf);

// Format integer `val` with `nFract` fractional digits and `nDigits` total digits into `buf`.
void dec_fix(int32_t val, uint8_t nFract, uint8_t nDigits, char *buf);

#endif // DEC_H