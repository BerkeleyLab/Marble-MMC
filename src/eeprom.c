#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "flash.h"
#include "common.h"
#include "marble_api.h"

//#define DEBUG_PRINT
#include "dbg.h"

#include "eeprom.h"
#include "eeprom_emu.h"

static int eeprom_read_val(ee_tags_t tag, volatile uint8_t *paddr, int len);
static int eeprom_store_val(ee_tags_t tag, const uint8_t *paddr, int len);
static int eeprom_populate_val(ee_tags_t tag, const uint8_t *paddr, int len);
static int eeprom_restore_all(void);

int eeprom_init(void) {
  eeprom_system_init();
  // Write default values of all missing tags
  eeprom_restore_all();
  // Get those restored values written ASAP
  eeprom_update();
  return 0;
}

static int eeprom_read_val(ee_tags_t tag, volatile uint8_t *paddr, int len) {
  ee_val_t eeval;
  len = MIN(len, (int)(sizeof(ee_val_t)/sizeof(uint8_t)));
  int rval = fmc_ee_read(tag, eeval);
  if (!rval) {
    for (int n = 0; n < len; n++) {
      paddr[n] = eeval[n];
    }
  } else {
#ifdef DEBUG_ENABLE_ERRNO_DECODE
    const char *errname = decode_errno(-rval);
    printf("eeprom_read_val: rval = %d (%s)\r\n", rval, errname);
#else
    printf("eeprom_read_val: rval = %d\r\n", rval);
#endif
  }
  return rval;
}

static int eeprom_store_val(ee_tags_t tag, const uint8_t *paddr, int len) {
  ee_val_t eeval;
  len = MIN(len, (int)(sizeof(ee_val_t)/sizeof(uint8_t)));
  for (int n = 0; n < len; n++) {
    eeval[n] = paddr[n];
  }
  int rval = fmc_ee_write(tag, eeval);
  if (!rval) {
    printf("Success\r\n");
  } else {
#ifdef DEBUG_ENABLE_ERRNO_DECODE
    const char *errname = decode_errno(-rval);
    printf("eeprom_store_val: rval = %d (%s)\r\n", rval, errname);
#else
    printf("eeprom_store_val: rval = %d\r\n", rval);
#endif
  }
  return rval;
}

/*
 * static int eeprom_populate_val(ee_tags_t tag, const uint8_t *paddr, int len);
 *    Ensure a given tag is present in nonvolatile memory.  If the tag is not
 *    found, a new copy is stored.
 */
static int eeprom_populate_val(ee_tags_t tag, const uint8_t *paddr, int len) {
  ee_val_t eeval;
  len = MIN(len, (int)(sizeof(ee_val_t)/sizeof(uint8_t)));
  // Note: fmc_ee_read() wastes a bit of ops compared to ee_find(), but I like the
  // encapsulation, plus stack var eeval would need to exist for fmc_ee_write() anyhow
  int rval = fmc_ee_read(tag, eeval);
  if (rval) {
    for (int n = 0; n < len; n++) {
      eeval[n] = paddr[n];
    }
    // Avoid leaking bytes from stack into eeval
    for (int n = len; n < (int)(sizeof(ee_val_t)/sizeof(uint8_t)); n++) {
      eeval[n] = 0;
    }
    rval = fmc_ee_write(tag, eeval);
    if (!rval) {
      printf("Default stored\r\n");
      return 1;
    } else {
      printf("Failed to store default\r\n");
      return rval;
    }
  } else {
    printd("Found\r\n");
  }
  return 0;
}

/*
 * static int eeprom_restore_all(void);
 *    Write default values for all missing tags in non-volatile memory.
 */
static int eeprom_restore_all(void) {
#define X(N, NAME, TYPE, SIZE, ...) \
  uint8_t pdata_ ## NAME[SIZE] = __VA_ARGS__; \
  eeprom_populate_val(ee_ ## NAME, pdata_ ## NAME, SIZE);
  FOR_ALL_EETAGS()
#undef X
  return 0;
}

/* (see eeprom.h)
 */
#define X(N, NAME, TYPE, SIZE, ...) \
int eeprom_store_ ## NAME(const uint8_t *pdata, int len) { return eeprom_store_val(ee_ ## NAME, pdata, len); } \
int eeprom_read_ ## NAME(volatile uint8_t *pdata, int len) { return eeprom_read_val(ee_ ## NAME, pdata, len); }
FOR_ALL_EETAGS()
#undef X

/* int eeprom_read_wd_key(volatile uint8_t *pdata, int len);
 *  The whole eeprom scheme is built around an 8-byte structure with a 6-byte
 *  payload, so this bespoke hack is necessary for any larger structures.
 */
int eeprom_read_wd_key(volatile uint8_t *pdata, int len) {
  _UNUSED(len);
  // Read part 0 (6 bytes)
  if (eeprom_read_wd_key_0(pdata, 6) < 0) return -1;
  // Read part 1 (6 bytes)
  if (eeprom_read_wd_key_1((pdata+6), 6) < 0) return -1;
  // Read part 2 (6 bytes)
  if (eeprom_read_wd_key_2((pdata+12), 4) < 0) return -1;
  return 0;
}

/* int eeprom_store_wd_key(const uint8_t *pdata, int len);
 *  The whole eeprom scheme is built around an 8-byte structure with a 6-byte
 *  payload, so this bespoke hack is necessary for any larger structures.
 */
int eeprom_store_wd_key(const uint8_t *pdata, int len) {
  _UNUSED(len);
  // Store part 0 (6 bytes)
  if (eeprom_store_wd_key_0(pdata, 6) < 0) return -1;
  // Store part 1 (6 bytes)
  if (eeprom_store_wd_key_1((pdata+6), 6) < 0) return -1;
  // Store part 2 (6 bytes)
  if (eeprom_store_wd_key_2((pdata+12), 4) < 0) return -1;
  return 0;
}
