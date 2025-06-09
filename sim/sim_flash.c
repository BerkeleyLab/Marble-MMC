/*
 * File: sim_flash.c
 * Desc: Simulated flash memory interface for EEPROM-emulator
 */

#include <stdio.h>
#include <string.h>
#include "marble_api.h"
#include "sim_api.h"
#include "flash.h"
#include "eeprom.h"

//#define DEBUG_PRINT
#include "dbg.h"

// ======================= Marble Flash Memory Emulation =====================
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

#define SIM_EEPROM_FILENAME            "eeprom.bin"
#define SIM_MAX_EEPROM_SIZE           (64*64)
// ======================= Marble-Mini EEPROM Emulation ======================
int eeprom_initialized=0;
void Sim_EEPROM_Init(int size) {
  if (size > SIM_MAX_EEPROM_SIZE) {
    printf("ERROR: %d > %d. Refusing to initialize simulated EEPROM\r\n", size, SIM_MAX_EEPROM_SIZE);
    return;
  }
  printd("size = %d\r\n", size);
  eeprom_initialized = 1;
  FILE *pFile = fopen(SIM_EEPROM_FILENAME, "rb");
  if (!pFile) {
    // If file doesn't exist, create it
    pFile = fopen(SIM_EEPROM_FILENAME, "wb");
    fclose(pFile);
    if (Sim_EEPROM_Erase(0, size)) {
      printf("ERROR: Could not initialize EEPROM file with zeros\r\n");
    }
  } else {
    fclose(pFile);
  }
  pFile = fopen(SIM_EEPROM_FILENAME, "rb+");
  // If file size is < EEPROM_BYTES_USED, pad with zeros
  if (fseek(pFile, 0L, SEEK_END)) {
    printf("ERROR: Could not seek to end of EEPROM file?!\r\n");
    fclose(pFile);
    return;
  }
  long file_end = ftell(pFile);
  fclose(pFile);
  if (file_end < (long)size) {
    if (Sim_EEPROM_Erase((int)file_end, (size-file_end))) {
      printf("ERROR: Could not initialize EEPROM file with zeros\r\n");
    }
  }
  return;
}

int Sim_EEPROM_Read(volatile uint8_t *dest, int size_bytes) {
  if (!eeprom_initialized) {
    return -1;
  }
  FILE *pFile = fopen(SIM_EEPROM_FILENAME, "rb");
  if (!pFile) {
    printf("ERROR: Cannot open %s for reading.\r\n", SIM_EEPROM_FILENAME);
    return -1;
  }
  size_t rval = fread((void *)dest, sizeof(uint8_t), (size_t)size_bytes, pFile);
  fclose(pFile);
  if (rval != (size_t)size_bytes) {
    printf("ERROR: Failed to read the requested %d bytes from EEPROM file; only read %d bytes.\r\n", size_bytes, (int)rval);
    return 1;
  }
  return 0;
}

int Sim_EEPROM_Erase(int offset, int size) {
  if (!eeprom_initialized) {
    return -1;
  }
  FILE *pFile = fopen(SIM_EEPROM_FILENAME, "rb+");
  if (!pFile) {
    printf("ERROR: Cannot open %s for writing.\r\n", SIM_EEPROM_FILENAME);
    return -1;
  }
  if (fseek(pFile, (long)offset, SEEK_SET)) {
    printf("ERROR: Cannot seek to location %d in EEPROM file.\r\n", offset);
  }
  // Just write by byte since we're not guaranteed any block alignment
  uint8_t zero = 0;
  for (int n=0; n<size; n++) {
    fwrite((const void *)&zero, sizeof(uint8_t), 1, pFile);
  }
  fclose(pFile);
  return 0;
}

int Sim_EEPROM_Write(const uint8_t *src, int offset, int size) {
  if (!eeprom_initialized) {
    return -1;
  }
  printd("Writing %d bytes from %d offset\r\n  ", size, offset);
#ifdef DEBUG_PRINT
  for (int n = 0; n < size; n++) {
    printf("%x ", src[n]);
    if ((n % 16) == 0) printf("\r\n  ");
  }
  printf("\r\n");
#endif
  FILE *pFile = fopen(SIM_EEPROM_FILENAME, "rb+");
  if (!pFile) {
    printf("ERROR: Cannot open %s for writing.\r\n", SIM_EEPROM_FILENAME);
    return -1;
  }
  if (fseek(pFile, (long)offset, SEEK_SET)) {
    printf("ERROR: Cannot seek to location %d in EEPROM file.\r\n", offset);
  }
  size_t rval = fwrite((const void *)src, sizeof(uint8_t), (size_t)size, pFile);
  if (rval != (size_t)size) {
    printf("ERROR: Requested write of %d bytes, but wrote %d bytes.\r\n", size, (int)rval);
  }
  fclose(pFile);
  return 0;
}
