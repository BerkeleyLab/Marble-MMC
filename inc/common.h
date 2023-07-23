/*
 * File: common.h
 * Desc: Common utilities
 */

#ifndef __COMMON_H
#define __COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

// Define this to enable decoding (most) errnos into their string name
//#define DEBUG_ENABLE_ERRNO_DECODE

// Make the compiler quit complaining about a particular unused parameter
#define _UNUSED(X)                 (void)(X)

// This silly hack is to allow __LINE__ to be converted to a string at compile time
#define __S(x) #x
#define __S_(x) __S(x)
#define S__LINE__ __S_(__LINE__)

// ======================= Generic Register Utilities =========================
#define WRITE_BIT(REG, BIT, VAL)  ((REG & ~(1<<BIT)) | (VAL<<BIT))
#define TEST_BIT(REG, BIT)        (REG & (1<<BIT))
#define BIT_MASK(START, END)      (((1 << (END+1)) - 1) ^ ((1 << (START)) -1))
#define SET_FIELD_MASK(REG, VAL, MASK)    ((REG & ~MASK) | (VAL & MASK))
#define SET_FIELD(REG, VAL, START, END)   SET_FIELD_MASK(REG, VAL, BIT_MASK(START, END))
#define EXTRACT_FIELD(VAL, START, END)      ((VAL & BIT_MASK(START, END)) >> START)

// ============================== Handy Macros ================================
// Printing multi-byte values
#define PRINT_MULTIBYTE_HEX(pdata, len, div) do { \
  for (int ix=0; ix<len-1; ix++) { printf("%x%c", pdata[ix], div); } \
  printf("%x\r\n", pdata[len-1]); \
} while (0);

#define PRINT_MULTIBYTE_DEC(pdata, len, div) do { \
  for (int ix=0; ix<len-1; ix++) { printf("%d%c", pdata[ix], div); } \
  printf("%d\r\n", pdata[len-1]); \
} while (0);

/* Usage Example:
  uint8_t ip[4] = {192, 168, 10, 0};
  PRINT_MULTIBYTE_DEC(ip, 4, '.');
  uint8_t mac[6] = {0x19, 0x69, 0xDE, 0xAF, 0xBE, 0xEF};
  PRINT_MULTIBYTE_HEX(mac, 6, ':');
*/

// ============================ Errno Decoding ================================
#ifdef DEBUG_ENABLE_ERRNO_DECODE
#include <errno.h>
// ==== Hack to get around strerrorname_np not found on libc 2.35 ====
// Returns string of errno name given an errno int
// E.g. decode_errno(EIO) return pointer to string "EIO"
const char *decode_errno(int err);
#define FOR_ALL_ERRNOS() \
  X(EPERM) \
  X(ENOENT) \
  X(ESRCH) \
  X(E2BIG) \
  X(ENOEXEC) \
  X(EBADF) \
  X(ECHILD) \
  X(EDEADLK) \
  X(ENOMEM) \
  X(EACCES) \
  X(EFAULT) \
  /*X(ENOTBLK)*/ \
  X(EBUSY) \
  X(EEXIST) \
  X(EXDEV) \
  X(ENODEV) \
  X(ENOTDIR) \
  X(EISDIR) \
  X(EMFILE) \
  X(ENFILE) \
  X(ENOTTY) \
  X(ETXTBSY) \
  X(EFBIG) \
  X(ENOSPC) \
  X(ESPIPE) \
  X(EROFS) \
  X(EMLINK) \
  X(EPIPE) \
  X(EDOM) \
  X(EAGAIN) \
  X(EINPROGRESS) \
  X(EALREADY) \
  X(ENOTSOCK) \
  X(EMSGSIZE) \
  X(EPROTOTYPE) \
  X(ENOPROTOOPT) \
  X(EPROTONOSUPPORT) \
  X(EOPNOTSUPP) \
  X(EAFNOSUPPORT) \
  X(EADDRINUSE) \
  X(EADDRNOTAVAIL) \
  X(ENETDOWN) \
  X(ENETUNREACH) \
  X(ENETRESET) \
  X(ECONNABORTED) \
  X(ENXIO) \
  X(EINTR) \
  X(EIO) \
  X(EINVAL)
// I'm not going to do the rest... there are too many
#endif // DEBUG_ENABLE_ERRNO_DECODE

#ifdef __cplusplus
}
#endif

#endif // __COMMON_H

