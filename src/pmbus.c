/* A set of Power Management Bus (PMBus) conversion utilities
 */

#include "pmbus.h"

// FIXME DELETEME
#include <stdio.h>

static double _shift(double f, int ord);
// =========================== Runtime Conversion =============================

#ifndef INT_MAX
//#define INT_MAX 0x7fffffff
// Silly little two-step hack to keep the compiler from warning about
// -0x80000000 rolling over to 0x7fffffff with the -1 step
#define INT_MAX   ((((1 << (8*sizeof(int)-2)) - 1) << 1) | 1)
#endif
#ifndef INT_MIN
//#define INT_MIN -0x80000000
#define INT_MIN   (1 << (8*sizeof(int)-1))
#endif

uint16_t v_to_l11_int(int v) {
  // Start most-negative exponent (largest mantissa) possible
  int n = 0;
  while ((v < INT_MAX/2) && (v > INT_MIN/2)) {
    v = v << 1;
    n--;
  }
  //v = v << 16;
  while ((v > 1023) || (v < -1024)) {
    v = v >> 1;
    if (++n > 14) { // Enforce clipping in case too large v
      break;
    }
  }
  return (uint16_t)(((n & 0x1f) << 11) | (v & 0x7ff));
}

uint16_t v_to_l11_float(float v) {
  int n = -16;
  v = v * (1 << 16);
  while ((v > 1023) || (v < -1024)) {
    v = v/2;
    if (++n > 14) { // Enforce clipping in case too large v
      break;
    }
  }
  return (uint16_t)(((n & 0x1f) << 11) | ((int)v & 0x7ff));
}

uint16_t v_to_l11_double(double v) {
  int n = -16;
  v = v * (1 << 16);
  while ((v > 1023) || (v < -1024)) {
    v = v/2;
    if (++n > 14) { // Enforce clipping in case too large v
      break;
    }
  }
  return (uint16_t)(((n & 0x1f) << 11) | ((int)v & 0x7ff));
}

uint16_t mv_to_l11_int(int mv) {
  // Start most-negative exponent (largest mantissa) possible
  int n = 0;
  while ((mv < INT_MAX/2) && (mv > INT_MIN/2)) {
    mv = mv << 1;
    n--;
  }
  mv = mv/1000;
  while ((mv > 1023) || (mv < -1024)) {
    mv = mv >> 1;
    if (++n > 14) { // Enforce clipping in case too large v
      break;
    }
  }
  return (uint16_t)(((n & 0x1f) << 11) | (mv & 0x7ff));
}

uint16_t mv_to_l11_float(float mv) {
  int n = -16;
  mv = mv*(1 << 16)/1000;
  while ((mv > 1023) || (mv < -1024)) {
    mv = mv/2;
    if (++n > 14) { // Enforce clipping in case too large v
      break;
    }
  }
  return (uint16_t)(((n & 0x1f) << 11) | ((int)mv & 0x7ff));
}

uint16_t mv_to_l11_double(double mv) {
  int n = -16;
  mv = mv*(1 << 16)/1000;
  while ((mv > 1023) || (mv < -1024)) {
    mv = mv/2;
    if (++n > 14) { // Enforce clipping in case too large v
      break;
    }
  }
  return (uint16_t)(((n & 0x1f) << 11) | ((int)mv & 0x7ff));
}

int l11_to_v_int(uint16_t l) {
  int a, n;
  // Avoid relying on platform-dependent sign-extension
  n = ((l >> 11) & 0xf);
  a = (l & 0x3ff);
  if (l & 0x400) {
    a -= 0x400;
  }
  if (l & 0x8000) {
    return (a >> (16-n));
  }
  return (a << n);
}

int l11_to_mv_int(uint16_t l) {
  int a, n;
  // Avoid relying on platform-dependent sign-extension
  n = ((l >> 11) & 0xf);
  a = (l & 0x3ff);
  if (l & 0x400) {
    a -= 0x400;
  }
  if (l & 0x8000) {
    return ((1000*a) >> (16-n));
  }
  return ((1000*a) << n);
}

double l11_to_v_double(uint16_t l) {
  double a;
  int n;
  // Avoid relying on platform-dependent sign-extension
  n = ((l >> 11) & 0xf);
  a = (double)(l & 0x3ff);
  if (l & 0x400) {
    a -= 0x400;
  }
  if (l & 0x8000) {
    return _shift(a, n-16);
  }
  return (a*(1 << n));
}

float l11_to_v_float(uint16_t l) {
  return (float)l11_to_v_double(l);
}

double l11_to_mv_double(uint16_t l) {
  double a;
  int n;
  // Avoid relying on platform-dependent sign-extension
  n = ((l >> 11) & 0xf);
  a = (double)(l & 0x3ff);
  if (l & 0x400) {
    a -= 0x400;
  }
  if (l & 0x8000) {
    return _shift(1000*a, n-16);
  }
  return (1000*a*(1 << n));
}

int l16_to_v_int(uint16_t l) {
  return (int)(l>>13);
}

float l11_to_mv_float(uint16_t l) {
  return (float)l11_to_mv_double(l);
}

double l16_to_v_double(uint16_t l) {
  double lf = (double)l;
  //return _shift(lf, -13);
  return lf*0.0001220703125;  // 2**(-13)
}

float l16_to_v_float(uint16_t l) {
  return (float)l16_to_v_double(l);
}

int l16_to_mv_int(uint16_t l) {
  return (1000*((int)l)>>13);
}

double l16_to_mv_double(uint16_t l) {
  return 1000*l16_to_v_double(l);
}

float l16_to_mv_float(uint16_t l) {
  return 1000*l16_to_v_float(l);
}

static double _shift(double f, int ord) {
  while (ord > 0) {
    f = f*2;
    --ord;
  };
  while (ord < 0) {
    f = f/2;
    ++ord;
  };
  return f;
}
