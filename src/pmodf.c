/*
 * file: pmodf.c
 * desc: Forwarding bridge between mailbox and pmod
 */

#include <stdint.h>
#include "pmodf.h"
#include "marble_api.h"

#define PAGE_SIZE                         (16)

#define PMOD_CR0_TYPE_BYPASS            (0x00)
#define PMOD_CR0_TYPE_GPIO              (0x01)

static uint8_t pmodType;

static uint8_t dr0[PAGE_SIZE];

void pmodf_init(void) {
  pmodType = PMOD_CR0_TYPE_BYPASS;
  return;
}

void pmodf_handleConfig0(const uint8_t *pdata, int size) {
  if ((size == 0) || (size > 15)) {
    // TODO Error
    return;
  }
  if (pdata[0] == PMOD_CR0_TYPE_GPIO) {
    pmodType = PMOD_CR0_TYPE_GPIO;
    marble_pmod_set_dir(pdata[1]);
  }
}

void pmodf_handleData0(const uint8_t *pdata, int size) {
  int nmax = size > PAGE_SIZE ? PAGE_SIZE : size;
  for (int n = 0; n < nmax; n++) {
    dr0[n] = pdata[n];
  }
  if (pmodType == PMOD_CR0_TYPE_GPIO) {
    marble_pmod_set_gpio(dr0[0]);
  }
  return;
}

void pmodf_returnData0(uint8_t *pdata, int size) {
  int nmax = size > PAGE_SIZE ? PAGE_SIZE : size;
  if (pmodType == PMOD_CR0_TYPE_GPIO) {
    dr0[0] = marble_pmod_get_gpio();
  }
  for (int n = 0; n < nmax; n++) {
    pdata[n] = dr0[n];
  }
  return;
}

