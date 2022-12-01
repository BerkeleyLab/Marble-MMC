/*
 * File: sim_flash.c
 * Desc: Simulated flash memory interface for EEPROM-emulator
 */

#include <stdio.h>
#include <string.h>
#include "sim_platform.h"
#include "flash.h"
#include "st-eeprom.h"

#define FLASH_SECTOR_SIZE_WORDS         (FLASH_SECTOR_SIZE/4)
#define FLASH_SECTOR_SIZE_STRAY_BYTES   (FLASH_SECTOR_SIZE%4)

static int store_flash(void);

size_t eeprom_count = EEPROM_COUNT;
ee_frame eeprom0_base[EEPROM_COUNT];
ee_frame eeprom1_base[EEPROM_COUNT];

bool need_flush = false;

int fmc_flash_init(void) {
  need_flush = false;
  return 0;
}

int fmc_flash_program(void *paddr, const void *pvalue, size_t count)
{
  memcpy(paddr, pvalue, count);
  store_flash();
  return 0;
}

int fmc_flash_erase_sector(unsigned sectorn)
{
  if(sectorn==1) {
    memset(eeprom0_base, 0xff, sizeof(eeprom0_base));
  } else if(sectorn==2) {
    memset(eeprom1_base, 0xff, sizeof(eeprom1_base));
  } else {
    return -1;
  }
  need_flush = true;
  store_flash();
  return 0;
}

void fmc_flash_cache_flush_all(void)
{
  need_flush = false;
  return;
}

static int store_flash(void) {
  //printf("Writing sizeof(eeprom0_base) = %ld\r\n", sizeof(eeprom0_base));
  FILE *pFile = fopen(SIM_FLASH_FILENAME, "wb");
  if (!pFile) {
    printf("Cannot open %s for writing.\r\n", SIM_FLASH_FILENAME);
    return -1;
  }
  // Write by ee_frame
  // First eeprom0
  size_t rval = fwrite((const void *)eeprom0_base, sizeof(ee_frame), EEPROM_COUNT, pFile);
  // Then eeprom1
  rval += fwrite((const void *)eeprom1_base, sizeof(ee_frame), EEPROM_COUNT, pFile);
  fclose(pFile);
  //printf("Num writes = %ld\r\n", rval);
  return 0;
}

int restore_flash(void) {
  //printf("Reading...\r\n");
  FILE *pFile = fopen(SIM_FLASH_FILENAME, "rb");
  if (!pFile) {
    printf("Cannot open %s for reading.\r\n", SIM_FLASH_FILENAME);
    return -1;
  }
  // Read by ee_frame
  // First eeprom0
  size_t rval = fread((void *)eeprom0_base, sizeof(ee_frame), EEPROM_COUNT, pFile);
  // Then eeprom1
  rval += fread((void *)eeprom1_base, sizeof(ee_frame), EEPROM_COUNT, pFile);
  fclose(pFile);
  //printf("Num reads = %ld\r\n", rval);
  return 0;
}
