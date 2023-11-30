/*
 * File: watchdog.h
 * Desc: FPGA Watchdog system for authenticating host and rebooting FPGA to
 *       golden image when needed.
 */

#ifndef __WATCHDOG_H
#define __WATCHDOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint32_t FPGAWD_GetRand(void);
void FPGAWD_DoneHandler(void);
void FPGAWD_HandleHash(uint32_t hash, int index);

#ifdef __cplusplus
}
#endif

#endif // __WATCHDOG_H

