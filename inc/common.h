/*
 * File: common.h
 * Desc: Common utilities
 */

#ifndef __COMMON_H
#define __COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#define _UNUSED(X)                 (void)(X)

// ======================= Generic Register Utilities =========================
#define WRITE_BIT(REG, BIT, VAL)  ((REG & ~(1<<BIT)) | (VAL<<BIT))
#define TEST_BIT(REG, BIT)        (REG & (1<<BIT))
#define BIT_MASK(START, END)      (((1 << (END+1)) - 1) ^ ((1 << (START)) -1)) 
#define SET_FIELD_MASK(REG, VAL, MASK)    ((REG & ~MASK) | (VAL & MASK))
#define SET_FIELD(REG, VAL, START, END)   SET_FIELD_MASK(REG, VAL, BIT_MASK(START, END))
#define EXTRACT_FIELD(VAL, START, END)      ((VAL & BIT_MASK(START, END)) >> START)

#ifdef __cplusplus
}
#endif

#endif // __COMMON_H

