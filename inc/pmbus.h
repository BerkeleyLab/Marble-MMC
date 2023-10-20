#ifndef _PMBUS_H_
#define _PMBUS_H_

/* A set of Power Management Bus (PMBus) format conversion utilities
==== Linear11 (L11) signed type ====
  https://en.wikipedia.org/wiki/Power_Management_Bus#Linear11_Floating-Point_Format
  |--------- 16-bit ---------|
  | N (5 bits) | Y (11 bits) |
  Value = Y*2^N
  N and Y are both signed!
  -16 <= N <= 15
  -1024 <= Y <= 1023
  Therefore, the L11 format can represent positive numbers from 1*2^(-16) ~ 15.26u
  to ((2^10)-1)*(2^15) ~ 33.52M and negative numbers from -1*2^(-16) ~ -15.26u
  to -(2^10)*(2^15) = -2^25 = -33554432 ~ -33.55M

==== Linear16 (L16) unsigned type ====
  Much simpler than L11
  |        Y (16 bits)       |
  Value = Y*2^N where N is fixed on a per-component basis
  Example (LTM4673): N = -13
    In this case, the L16 format can represent numbers from 2^-13 ~ 1.2207m
    to ((2^16)-1)*2^-13 ~ 8 (7.99988)
*/

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>

// ========================= Compile Time Conversion ==========================
/* Ok, I admit these macros look ridiculous, but I promise they exist in this
 * format for a good reason.  The runtime conversion functions work well and
 * are easy to understand, but cannot be used for populating constants in an
 * initialization table.  This is only a minor drawback for read/write variables
 * (destined for the .data or .bss sections) where you would just move your
 * initialization to a function body.  But for compile-time constants, without
 * these macros you would need to pre-compute the L11/L16 encoded values and
 * store them as cryptic constants whose physical relevance is nearly impossible
 * to see without decoding.
 *
 * For example, which equivalent statement is easier to read?
 *  const uint16_t setpts[] = {0xbb00, 0xc240, 0xc34c, 0xd500};
 *  const uint16_t setpts[] = {V_TO_L11(1.5), V_TO_L11(2.25),
 *                             V_TO_L11(3.3), V_TO_L11(-12)};
 *
 * Both of the above declarations create an array of 4 16-bit numbers in L11
 * format, but the first form hides their true nature - i.e. 1.5V, 2.25V, 3.3V
 * and -12V (obviously these don't need to be voltages and could just as easily
 * represent current in amps, temperature in degC, etc).
 *
 * Note that the macros work with floating-point literal arguments yet resolve
 * to 16-bit integers at compile time (and so do not rely on any floating-point
 * capabilities of the target).  Using the "V_TO_L11" and "V_TO_L16" macros with
 * floating-point arguments is the best way to go (at least on gcc & arm-gcc).
 * The other variants accepting arguments scaled by 10^-3 (i.e. MV_TO_L11) and
 * 10^-6 (i.e. UV_TO_L11) respectively should only be used when the literal to
 * be converted is only available as an integer for some reason.  Care has been
 * taken to try to prevent integer overflow in the macros, but that can only be
 * valid for a limited range of arguments.
 *
 * Note also that these macros have been optimized for 32-bit integers.
 */

// Linear16 (L16) unsigned type
#define _L16_EXPONENT                                                      (13)

#define V_TO_L16(v)                                      (v*(1<<_L16_EXPONENT))
#define L16_TO_V(l)                                      (v/(1<<_L16_EXPONENT))

#define MV_TO_L16(v)                                (v*(1<<_L16_EXPONENT)/1000)
#define L16_TO_MV(l)                                (1000*l/(1<<_L16_EXPONENT))

#define UV_TO_L16(v)                             (v*(1<<_L16_EXPONENT)/1000000)
#define L16_TO_UV(l)                             (1000000*l/(1<<_L16_EXPONENT))

// Linear11 (L11) signed type
// Unity-scaled units (i.e. Volts, Amps, degC, etc) to L11 encoding
#define V_TO_L11(v) \
  ((v*(1<<16)) < 1024) && ((v*(1<<16)) > -1025) ? ((0x10 << 11) | ((uint16_t)(v*(1<<16)) & 0x7ff)) : \
  ((v*(1<<15)) < 1024) && ((v*(1<<15)) > -1025) ? ((0x11 << 11) | ((uint16_t)(v*(1<<15)) & 0x7ff)) : \
  ((v*(1<<14)) < 1024) && ((v*(1<<14)) > -1025) ? ((0x12 << 11) | ((uint16_t)(v*(1<<14)) & 0x7ff)) : \
  ((v*(1<<13)) < 1024) && ((v*(1<<13)) > -1025) ? ((0x13 << 11) | ((uint16_t)(v*(1<<13)) & 0x7ff)) : \
  ((v*(1<<12)) < 1024) && ((v*(1<<12)) > -1025) ? ((0x14 << 11) | ((uint16_t)(v*(1<<12)) & 0x7ff)) : \
  ((v*(1<<11)) < 1024) && ((v*(1<<11)) > -1025) ? ((0x15 << 11) | ((uint16_t)(v*(1<<11)) & 0x7ff)) : \
  ((v*(1<<10)) < 1024) && ((v*(1<<10)) > -1025) ? ((0x16 << 11) | ((uint16_t)(v*(1<<10)) & 0x7ff)) : \
  ((v*(1<< 9)) < 1024) && ((v*(1<< 9)) > -1025) ? ((0x17 << 11) | ((uint16_t)(v*(1<< 9)) & 0x7ff)) : \
  ((v*(1<< 8)) < 1024) && ((v*(1<< 8)) > -1025) ? ((0x18 << 11) | ((uint16_t)(v*(1<< 8)) & 0x7ff)) : \
  ((v*(1<< 7)) < 1024) && ((v*(1<< 7)) > -1025) ? ((0x19 << 11) | ((uint16_t)(v*(1<< 7)) & 0x7ff)) : \
  ((v*(1<< 6)) < 1024) && ((v*(1<< 6)) > -1025) ? ((0x1a << 11) | ((uint16_t)(v*(1<< 6)) & 0x7ff)) : \
  ((v*(1<< 5)) < 1024) && ((v*(1<< 5)) > -1025) ? ((0x1b << 11) | ((uint16_t)(v*(1<< 5)) & 0x7ff)) : \
  ((v*(1<< 4)) < 1024) && ((v*(1<< 4)) > -1025) ? ((0x1c << 11) | ((uint16_t)(v*(1<< 4)) & 0x7ff)) : \
  ((v*(1<< 3)) < 1024) && ((v*(1<< 3)) > -1025) ? ((0x1d << 11) | ((uint16_t)(v*(1<< 3)) & 0x7ff)) : \
  ((v*(1<< 2)) < 1024) && ((v*(1<< 2)) > -1025) ? ((0x1e << 11) | ((uint16_t)(v*(1<< 2)) & 0x7ff)) : \
  ((v*(1<< 1)) < 1024) && ((v*(1<< 1)) > -1025) ? ((0x1f << 11) | ((uint16_t)(v*(1<< 1)) & 0x7ff)) : \
  ((v*(1<< 0)) < 1024) && ((v/(1<< 0)) > -1025) ? ((0x10 << 11) | ((uint16_t)(v*(1<< 0)) & 0x7ff)) : \
  ((v/(1<< 1)) < 1024) && ((v/(1<< 1)) > -1025) ? ((0x11 << 11) | ((uint16_t)(v/(1<< 1)) & 0x7ff)) : \
  ((v/(1<< 2)) < 1024) && ((v/(1<< 2)) > -1025) ? ((0x12 << 11) | ((uint16_t)(v/(1<< 2)) & 0x7ff)) : \
  ((v/(1<< 3)) < 1024) && ((v/(1<< 3)) > -1025) ? ((0x13 << 11) | ((uint16_t)(v/(1<< 3)) & 0x7ff)) : \
  ((v/(1<< 4)) < 1024) && ((v/(1<< 4)) > -1025) ? ((0x14 << 11) | ((uint16_t)(v/(1<< 4)) & 0x7ff)) : \
  ((v/(1<< 5)) < 1024) && ((v/(1<< 5)) > -1025) ? ((0x15 << 11) | ((uint16_t)(v/(1<< 5)) & 0x7ff)) : \
  ((v/(1<< 6)) < 1024) && ((v/(1<< 6)) > -1025) ? ((0x16 << 11) | ((uint16_t)(v/(1<< 6)) & 0x7ff)) : \
  ((v/(1<< 7)) < 1024) && ((v/(1<< 7)) > -1025) ? ((0x17 << 11) | ((uint16_t)(v/(1<< 7)) & 0x7ff)) : \
  ((v/(1<< 8)) < 1024) && ((v/(1<< 8)) > -1025) ? ((0x18 << 11) | ((uint16_t)(v/(1<< 8)) & 0x7ff)) : \
  ((v/(1<< 9)) < 1024) && ((v/(1<< 9)) > -1025) ? ((0x19 << 11) | ((uint16_t)(v/(1<< 9)) & 0x7ff)) : \
  ((v/(1<<10)) < 1024) && ((v/(1<<10)) > -1025) ? ((0x1a << 11) | ((uint16_t)(v/(1<<10)) & 0x7ff)) : \
  ((v/(1<<11)) < 1024) && ((v/(1<<11)) > -1025) ? ((0x1b << 11) | ((uint16_t)(v/(1<<11)) & 0x7ff)) : \
  ((v/(1<<12)) < 1024) && ((v/(1<<12)) > -1025) ? ((0x1c << 11) | ((uint16_t)(v/(1<<12)) & 0x7ff)) : \
  ((v/(1<<13)) < 1024) && ((v/(1<<13)) > -1025) ? ((0x1d << 11) | ((uint16_t)(v/(1<<13)) & 0x7ff)) : \
  ((v/(1<<14)) < 1024) && ((v/(1<<14)) > -1025) ? ((0x1e << 11) | ((uint16_t)(v/(1<<14)) & 0x7ff)) : \
                                                  ((0x0f << 11) | ((uint16_t)(v/(1<<15)) & 0x7ff))

// L11 encoding to unity-scaled units (i.e. Volts, Amps, degC, etc)
#define L11_TO_V(l) \
  ((l & 0x400) ? \
    (l & 0x8000) ? \
      (-1*((~l & 0x3ff) + 1) >> ((~(l >> 11) & 0xf) + 1)) \
    : \
      (-1*((~l & 0x3ff) + 1) << ((l >> 11) & 0xf)) \
  : \
    (l & 0x8000) ? \
      ((l & 0x3ff) >> (~(l >> 11) & 0xf) + 1) \
    : \
      ((l & 0x3ff) << ((l >> 11) & 0xf)) \
  )

// Units scaled by 10^3 (i.e. millivolts, milliamps, milliseconds, etc) to L11 encoding
#define MV_TO_L11(v) \
  ((v*(1<<16)/(1000 <<  0)) < 1024) && ((v*(1<<16)/(1000 <<  0)) > -1025) ? ((0x10 << 11) | ((uint16_t)(v*(1<<16)/(1000 <<  0)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 <<  1)) < 1024) && ((v*(1<<16)/(1000 <<  1)) > -1025) ? ((0x11 << 11) | ((uint16_t)(v*(1<<16)/(1000 <<  1)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 <<  2)) < 1024) && ((v*(1<<16)/(1000 <<  2)) > -1025) ? ((0x12 << 11) | ((uint16_t)(v*(1<<16)/(1000 <<  2)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 <<  3)) < 1024) && ((v*(1<<16)/(1000 <<  3)) > -1025) ? ((0x13 << 11) | ((uint16_t)(v*(1<<16)/(1000 <<  3)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 <<  4)) < 1024) && ((v*(1<<16)/(1000 <<  4)) > -1025) ? ((0x14 << 11) | ((uint16_t)(v*(1<<16)/(1000 <<  4)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 <<  5)) < 1024) && ((v*(1<<16)/(1000 <<  5)) > -1025) ? ((0x15 << 11) | ((uint16_t)(v*(1<<16)/(1000 <<  5)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 <<  6)) < 1024) && ((v*(1<<16)/(1000 <<  6)) > -1025) ? ((0x16 << 11) | ((uint16_t)(v*(1<<16)/(1000 <<  6)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 <<  7)) < 1024) && ((v*(1<<16)/(1000 <<  7)) > -1025) ? ((0x17 << 11) | ((uint16_t)(v*(1<<16)/(1000 <<  7)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 <<  8)) < 1024) && ((v*(1<<16)/(1000 <<  8)) > -1025) ? ((0x18 << 11) | ((uint16_t)(v*(1<<16)/(1000 <<  8)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 <<  9)) < 1024) && ((v*(1<<16)/(1000 <<  9)) > -1025) ? ((0x19 << 11) | ((uint16_t)(v*(1<<16)/(1000 <<  9)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 10)) < 1024) && ((v*(1<<16)/(1000 << 10)) > -1025) ? ((0x1a << 11) | ((uint16_t)(v*(1<<16)/(1000 << 10)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 11)) < 1024) && ((v*(1<<16)/(1000 << 11)) > -1025) ? ((0x1b << 11) | ((uint16_t)(v*(1<<16)/(1000 << 11)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 12)) < 1024) && ((v*(1<<16)/(1000 << 12)) > -1025) ? ((0x1c << 11) | ((uint16_t)(v*(1<<16)/(1000 << 12)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 13)) < 1024) && ((v*(1<<16)/(1000 << 13)) > -1025) ? ((0x1d << 11) | ((uint16_t)(v*(1<<16)/(1000 << 13)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 14)) < 1024) && ((v*(1<<16)/(1000 << 14)) > -1025) ? ((0x1e << 11) | ((uint16_t)(v*(1<<16)/(1000 << 14)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 15)) < 1024) && ((v*(1<<16)/(1000 << 15)) > -1025) ? ((0x1f << 11) | ((uint16_t)(v*(1<<16)/(1000 << 15)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 16)) < 1024) && ((v*(1<<16)/(1000 << 16)) > -1025) ? ((0x00 << 11) | ((uint16_t)(v*(1<<16)/(1000 << 16)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 17)) < 1024) && ((v*(1<<16)/(1000 << 17)) > -1025) ? ((0x01 << 11) | ((uint16_t)(v*(1<<16)/(1000 << 17)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 18)) < 1024) && ((v*(1<<16)/(1000 << 18)) > -1025) ? ((0x02 << 11) | ((uint16_t)(v*(1<<16)/(1000 << 18)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 19)) < 1024) && ((v*(1<<16)/(1000 << 19)) > -1025) ? ((0x03 << 11) | ((uint16_t)(v*(1<<16)/(1000 << 19)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 20)) < 1024) && ((v*(1<<16)/(1000 << 20)) > -1025) ? ((0x04 << 11) | ((uint16_t)(v*(1<<16)/(1000 << 20)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 21)) < 1024) && ((v*(1<<16)/(1000 << 21)) > -1025) ? ((0x05 << 11) | ((uint16_t)(v*(1<<16)/(1000 << 21)) & 0x7ff)) : \
  ((v*(1<<16)/(1000 << 22)) < 1024) && ((v*(1<<16)/(1000 << 22)) > -1025) ? ((0x06 << 11) | ((uint16_t)(v*(1<<16)/(1000 << 22)) & 0x7ff)) : \
  ((v*(1<<15)/(1000 << 22)) < 1024) && ((v*(1<<15)/(1000 << 22)) > -1025) ? ((0x07 << 11) | ((uint16_t)(v*(1<<15)/(1000 << 22)) & 0x7ff)) : \
  ((v*(1<<14)/(1000 << 22)) < 1024) && ((v*(1<<14)/(1000 << 22)) > -1025) ? ((0x08 << 11) | ((uint16_t)(v*(1<<14)/(1000 << 22)) & 0x7ff)) : \
  ((v*(1<<13)/(1000 << 22)) < 1024) && ((v*(1<<13)/(1000 << 22)) > -1025) ? ((0x09 << 11) | ((uint16_t)(v*(1<<13)/(1000 << 22)) & 0x7ff)) : \
  ((v*(1<<12)/(1000 << 22)) < 1024) && ((v*(1<<12)/(1000 << 22)) > -1025) ? ((0x0a << 11) | ((uint16_t)(v*(1<<12)/(1000 << 22)) & 0x7ff)) : \
  ((v*(1<<11)/(1000 << 22)) < 1024) && ((v*(1<<11)/(1000 << 22)) > -1025) ? ((0x0b << 11) | ((uint16_t)(v*(1<<11)/(1000 << 22)) & 0x7ff)) : \
  ((v*(1<<10)/(1000 << 22)) < 1024) && ((v*(1<<10)/(1000 << 22)) > -1025) ? ((0x0c << 11) | ((uint16_t)(v*(1<<10)/(1000 << 22)) & 0x7ff)) : \
  ((v*(1<< 9)/(1000 << 22)) < 1024) && ((v*(1<< 9)/(1000 << 22)) > -1025) ? ((0x0d << 11) | ((uint16_t)(v*(1<< 9)/(1000 << 22)) & 0x7ff)) : \
  ((v*(1<< 8)/(1000 << 22)) < 1024) && ((v*(1<< 8)/(1000 << 22)) > -1025) ? ((0x0e << 11) | ((uint16_t)(v*(1<< 8)/(1000 << 22)) & 0x7ff)) : \
                                                                            ((0x0f << 11) | ((uint16_t)(v*(1<< 7)/(1000 << 22)) & 0x7ff))

// L11 encoding to units scaled by 10^3 (i.e. millivolts, milliamps, milliseconds, etc)
#define L11_TO_MV(l) \
  ((l & 0x400) ? \
    (l & 0x8000) ? \
      (-1000*((~l & 0x3ff) + 1) >> ((~(l >> 11) & 0xf) + 1)) \
    : \
      (-1000*((~l & 0x3ff) + 1) << ((l >> 11) & 0xf)) \
  : \
    (l & 0x8000) ? \
      (1000*(l & 0x3ff) >> (~(l >> 11) & 0xf) + 1) \
    : \
      (1000*(l & 0x3ff) << ((l >> 11) & 0xf)) \
  )

// Units scaled by 10^6 (i.e. microvolts, microamps, microseconds, etc) to L11 encoding
#define UV_TO_L11(v) \
  ((v*(1<<16)/(1000000 <<  0)) < 1024) && ((v*(1<<16)/(1000000 <<  0)) > -1025) ? ((0x10 << 11) | ((uint16_t)(v*(1<<16)/(1000000 <<  0)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 <<  1)) < 1024) && ((v*(1<<16)/(1000000 <<  1)) > -1025) ? ((0x11 << 11) | ((uint16_t)(v*(1<<16)/(1000000 <<  1)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 <<  2)) < 1024) && ((v*(1<<16)/(1000000 <<  2)) > -1025) ? ((0x12 << 11) | ((uint16_t)(v*(1<<16)/(1000000 <<  2)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 <<  3)) < 1024) && ((v*(1<<16)/(1000000 <<  3)) > -1025) ? ((0x13 << 11) | ((uint16_t)(v*(1<<16)/(1000000 <<  3)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 <<  4)) < 1024) && ((v*(1<<16)/(1000000 <<  4)) > -1025) ? ((0x14 << 11) | ((uint16_t)(v*(1<<16)/(1000000 <<  4)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 <<  5)) < 1024) && ((v*(1<<16)/(1000000 <<  5)) > -1025) ? ((0x15 << 11) | ((uint16_t)(v*(1<<16)/(1000000 <<  5)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 <<  6)) < 1024) && ((v*(1<<16)/(1000000 <<  6)) > -1025) ? ((0x16 << 11) | ((uint16_t)(v*(1<<16)/(1000000 <<  6)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 <<  7)) < 1024) && ((v*(1<<16)/(1000000 <<  7)) > -1025) ? ((0x17 << 11) | ((uint16_t)(v*(1<<16)/(1000000 <<  7)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 <<  8)) < 1024) && ((v*(1<<16)/(1000000 <<  8)) > -1025) ? ((0x18 << 11) | ((uint16_t)(v*(1<<16)/(1000000 <<  8)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 <<  9)) < 1024) && ((v*(1<<16)/(1000000 <<  9)) > -1025) ? ((0x19 << 11) | ((uint16_t)(v*(1<<16)/(1000000 <<  9)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 << 10)) < 1024) && ((v*(1<<16)/(1000000 << 10)) > -1025) ? ((0x1a << 11) | ((uint16_t)(v*(1<<16)/(1000000 << 10)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 << 11)) < 1024) && ((v*(1<<16)/(1000000 << 11)) > -1025) ? ((0x1b << 11) | ((uint16_t)(v*(1<<16)/(1000000 << 11)) & 0x7ff)) : \
  ((v*(1<<16)/(1000000 << 12)) < 1024) && ((v*(1<<16)/(1000000 << 12)) > -1025) ? ((0x1c << 11) | ((uint16_t)(v*(1<<16)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<<15)/(1000000 << 12)) < 1024) && ((v*(1<<15)/(1000000 << 12)) > -1025) ? ((0x1d << 11) | ((uint16_t)(v*(1<<15)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<<14)/(1000000 << 12)) < 1024) && ((v*(1<<14)/(1000000 << 12)) > -1025) ? ((0x1e << 11) | ((uint16_t)(v*(1<<14)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<<13)/(1000000 << 12)) < 1024) && ((v*(1<<13)/(1000000 << 12)) > -1025) ? ((0x1f << 11) | ((uint16_t)(v*(1<<13)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<<12)/(1000000 << 12)) < 1024) && ((v*(1<<12)/(1000000 << 12)) > -1025) ? ((0x00 << 11) | ((uint16_t)(v*(1<<12)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<<11)/(1000000 << 12)) < 1024) && ((v*(1<<11)/(1000000 << 12)) > -1025) ? ((0x01 << 11) | ((uint16_t)(v*(1<<11)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<<10)/(1000000 << 12)) < 1024) && ((v*(1<<10)/(1000000 << 12)) > -1025) ? ((0x02 << 11) | ((uint16_t)(v*(1<<10)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<< 9)/(1000000 << 12)) < 1024) && ((v*(1<< 9)/(1000000 << 12)) > -1025) ? ((0x03 << 11) | ((uint16_t)(v*(1<< 9)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<< 8)/(1000000 << 12)) < 1024) && ((v*(1<< 8)/(1000000 << 12)) > -1025) ? ((0x04 << 11) | ((uint16_t)(v*(1<< 8)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<< 7)/(1000000 << 12)) < 1024) && ((v*(1<< 7)/(1000000 << 12)) > -1025) ? ((0x05 << 11) | ((uint16_t)(v*(1<< 7)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<< 6)/(1000000 << 12)) < 1024) && ((v*(1<< 6)/(1000000 << 12)) > -1025) ? ((0x06 << 11) | ((uint16_t)(v*(1<< 6)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<< 5)/(1000000 << 12)) < 1024) && ((v*(1<< 5)/(1000000 << 12)) > -1025) ? ((0x07 << 11) | ((uint16_t)(v*(1<< 5)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<< 4)/(1000000 << 12)) < 1024) && ((v*(1<< 4)/(1000000 << 12)) > -1025) ? ((0x08 << 11) | ((uint16_t)(v*(1<< 4)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<< 3)/(1000000 << 12)) < 1024) && ((v*(1<< 3)/(1000000 << 12)) > -1025) ? ((0x09 << 11) | ((uint16_t)(v*(1<< 3)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<< 2)/(1000000 << 12)) < 1024) && ((v*(1<< 2)/(1000000 << 12)) > -1025) ? ((0x0a << 11) | ((uint16_t)(v*(1<< 2)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<< 1)/(1000000 << 12)) < 1024) && ((v*(1<< 1)/(1000000 << 12)) > -1025) ? ((0x0b << 11) | ((uint16_t)(v*(1<< 1)/(1000000 << 12)) & 0x7ff)) : \
  ((v*(1<< 0)/(1000000 << 12)) < 1024) && ((v*(1<< 0)/(1000000 << 12)) > -1025) ? ((0x0c << 11) | ((uint16_t)(v*(1<< 0)/(1000000 << 12)) & 0x7ff)) : \
  ((v/(1<< 1)/(1000000 << 12)) < 1024) && ((v/(1<< 1)/(1000000 << 12)) > -1025) ? ((0x0d << 11) | ((uint16_t)(v/(1<< 1)/(1000000 << 12)) & 0x7ff)) : \
  ((v/(1<< 2)/(1000000 << 12)) < 1024) && ((v/(1<< 2)/(1000000 << 12)) > -1025) ? ((0x0e << 11) | ((uint16_t)(v/(1<< 2)/(1000000 << 12)) & 0x7ff)) : \
                                                                                  ((0x0f << 11) | ((uint16_t)(v/(1<< 3)/(1000000 << 12)) & 0x7ff))

// L11 encoding to units scaled by 10^6 (i.e. microvolts, microamps, microseconds, etc)
#define L11_TO_UV(l) \
  (l & 0x400) ? \
    (l & 0x8000) ? \
      (-1000000*((~l & 0x3ff) + 1) >> ((~(l >> 11) & 0xf) + 1)) \
    : \
      (-1000000*((~l & 0x3ff) + 1) << ((l >> 11) & 0xf)) \
  : \
    (l & 0x8000) ? \
      (1000000*(l & 0x3ff) >> (~(l >> 11) & 0xf) + 1) \
    : \
      (1000000*(l & 0x3ff) << ((l >> 11) & 0xf))

// =========================== Runtime Conversion =============================
/* These runtime functions should be used in the program body instead of the macros.
 * The macros will still work, but will create wasteful arithmetic stages for your
 * compiler to optimize away and may end up slower than the associated functions.
 * Of course with these functions you need to be a bit more careful with the types
 * of your arguments, which is why there are so many functions below.  The collection
 * below is really just a 4D matrix of 2 encodings (L11/L16), 2 directions (to/from)
 * 3 types (int/float/double), and 3 unit scales (V/mV/uV), yielding 2*2*3*3 = 36
 * functions.
 */

// To L11 encoding
uint16_t v_to_l11_int(int v);
uint16_t v_to_l11_float(float v);
uint16_t v_to_l11_double(double v);
uint16_t mv_to_l11_int(int mv);
uint16_t mv_to_l11_float(float mv);
uint16_t mv_to_l11_double(double mv);
uint16_t uv_to_l11_int(int mv);
uint16_t uv_to_l11_float(float mv);
uint16_t uv_to_l11_double(double mv);

// From L11 encoding
int l11_to_v_int(uint16_t l);
float l11_to_v_float(uint16_t l);
double l11_to_v_double(uint16_t l);
int l11_to_mv_int(uint16_t l);
float l11_to_mv_float(uint16_t l);
double l11_to_mv_double(uint16_t l);
int l11_to_uv_int(uint16_t l);
float l11_to_uv_float(uint16_t l);
double l11_to_uv_double(uint16_t l);

// To L16 encoding
uint16_t v_to_l16_int(int v);
uint16_t v_to_l16_float(float v);
uint16_t v_to_l16_double(double v);
uint16_t mv_to_l16_int(int mv);
uint16_t mv_to_l16_float(float mv);
uint16_t mv_to_l16_double(double mv);
uint16_t uv_to_l16_int(int uv);
uint16_t uv_to_l16_float(float uv);
uint16_t uv_to_l16_double(double uv);

// From L16 encoding
int l16_to_v_int(uint16_t l);
float l16_to_v_float(uint16_t l);
double l16_to_v_double(uint16_t l);
int l16_to_mv_int(uint16_t l);
float l16_to_mv_float(uint16_t l);
double l16_to_mv_double(uint16_t l);
int l16_to_uv_int(uint16_t l);
float l16_to_uv_float(uint16_t l);
double l16_to_uv_double(uint16_t l);

#ifdef __cplusplus
}
#endif

#endif // _PMBUS_H_
