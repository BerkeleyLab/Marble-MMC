/*
 * FIXME Dummy file to allow marble_mini to compile
 *       This needs to be replaced with a suitable wrapper around the LCP's EEPROM
 *       memory controller.
 *       Might need to move st-eeprom.c functionality to board_support and expose
 *       a more general interface in the common src/inc directories.
 */
#include "common.h"
#include "flash.h"
#include "st-eeprom.h"

ee_frame eeprom0_base;    // Should be defined in linker file when implemented
ee_frame eeprom1_base;    // Should be defined in linker file when implemented

int fmc_flash_init(void) {
  return 0;
}

int fmc_flash_program(void *paddr, const void *pvalue, size_t count) {
  _UNUSED(paddr);
  _UNUSED(pvalue);
  _UNUSED(count);
  return 0;
}

int fmc_flash_erase_sector(unsigned sectorn) {
  _UNUSED(sectorn);
  return 0;
}

void fmc_flash_cache_flush_all(void) {
  return;
}

int restore_flash(void) {
  return 0;
}

