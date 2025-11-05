#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#ifndef SIMULATION
#include "chip.h"
#else
#include "sim_api.h"
#endif
#include "eeprom.h"
#include "eeprom_emu.h"

//#define DEBUG_PRINT
#include "dbg.h"

/* The LPC1776 has an actual EEPROM built in which behaves quite differently from the fake EEPROM emulated
 * in flash memory for the STM32F205.  The EEPROM memory is not memory-mapped but is managed as a separate
 * peripheral controlled via a set of memory-mapped registers.  Also unlike flash memory, writing to the
 * EEPROM requires the very slow "program" step which applies to a 64-byte page.
 * The number of differences means we should have a separate implementation for handling non-volatile
 * parameters between the LPC and the STM32.
 * For this implementation, we will:
 *     * Store 8 "frames" (8-byte structs) per page
 *     * Load all pages in use into RAM at reset
 *     * Cast reads/writes to non-volatile params as reads/write from/to pointers into the pages stored in RAM
 *     * Maintain a "dirty" bit for each page, indicating that any value has changed in RAM.
 *     * In the main loop, check for dirty bits and perform the required EEPROM page write
 */

#define EEPROM_BYTES_USED ((unsigned int)(ee_NUM_TAGS*sizeof(ee_frame)))
// Ceiling integer division
#define EEPROM_PAGES_USED ((EEPROM_BYTES_USED/EEPROM_PAGE_SIZE) + ((EEPROM_BYTES_USED % EEPROM_PAGE_SIZE) != 0))

#define FRAME_SIZE (sizeof(ee_frame)/sizeof(uint8_t))

static uint8_t ram_copy[EEPROM_PAGES_USED*EEPROM_PAGE_SIZE];
#define PAGE_OFFSET(page_num)  (page_num*EEPROM_PAGE_SIZE)
#define PAGE_POINTER(page_num) (ram_copy+PAGE_OFFSET(page_num))

static uint32_t dirty_flag;
#define SET_DIRTY(page_num)   (dirty_flag |= (1<<page_num))
#define GET_DIRTY(page_num)   ((dirty_flag >> page_num) & 1)
#define CLEAR_DIRTY(page_num) (dirty_flag &= ~(1<<page_num))
#define RESET_DIRTY()         (dirty_flag = 0)

// 'tag' is used as offset into memory structure
#define FRAME_OFFSET(tag)  ((tag-1)*FRAME_SIZE)
#define FRAME_POINTER(tag) ((ee_frame *)(ram_copy + FRAME_OFFSET(tag)))
#define PAGE_NUMBER(tag)   (FRAME_OFFSET(tag)/EEPROM_PAGE_SIZE)

#ifndef SIMULATION
#define EEPROM_INIT()               (Chip_EEPROM_Init(LPC_EEPROM))
#define EEPROM_READ_ALL(dest, size) (Chip_EEPROM_Read(LPC_EEPROM, 0, 0, dest, EEPROM_RWSIZE_8BITS, size))
#define EEPROM_ERASE(pagenum)       (Chip_EEPROM_Erase(LPC_EEPROM, pagenum))
#define EEPROM_WRITE(pagenum, src, size)  (Chip_EEPROM_Write(LPC_EEPROM, 0, pagenum, src, EEPROM_RWSIZE_8BITS, size))
#else
#define EEPROM_INIT()               (Sim_EEPROM_Init(EEPROM_BYTES_USED))
#define EEPROM_READ_ALL(dest, size) (Sim_EEPROM_Read(dest, size))
//#define EEPROM_ERASE(pagenum)       (Sim_EEPROM_Erase(PAGE_OFFSET(pagenum), EEPROM_PAGE_SIZE))
#define EEPROM_ERASE(pagenum)
#define EEPROM_WRITE(pagenum, src, size)  (Sim_EEPROM_Write(src, PAGE_OFFSET(pagenum), size))

#ifndef SUCCESS
#define SUCCESS                     (0)
#endif // ifndef SUCCESS
#endif // ifndef SIMULATION

#ifdef DEBUG_PRINT
static void debug_print_frame(ee_frame *frame);
#define DEBUG_PRINT_FRAME(frame) debug_print_frame(frame)
#else
#define DEBUG_PRINT_FRAME(frame)
#endif

int eeprom_system_init(void) {
  EEPROM_INIT();
  // Read all EEPROM pages used into ram_copy
  EEPROM_READ_ALL(ram_copy, EEPROM_BYTES_USED);
  dirty_flag = 0;
  printd("EEPROM_PAGES_USED = %u\r\n", EEPROM_PAGES_USED);
  printd("EEPROM_BYTES_USED = %u\r\n", EEPROM_BYTES_USED);
  return 0;
}

// This should be called periodically in the main loop
void eeprom_update(void) {
  if (!dirty_flag) return;
  printd("dirty_flag = %x\r\n", dirty_flag);
  for (unsigned int n = 0; n < EEPROM_PAGES_USED; n++) {
    if (GET_DIRTY(n)) {
      // Update page number n
      EEPROM_ERASE(n);

      if (EEPROM_WRITE(n, PAGE_POINTER(n), EEPROM_PAGE_SIZE) == SUCCESS) {
        printd("Successfully stored page %u\r\n", n);
      } else {
        printd("Failed to store page %u\r\n", n);
      }
    }
  }
  RESET_DIRTY();
  return;
}

// Read data corresponding to tag 'tag' into 'val'
int fmc_ee_read(ee_tags_t tag, ee_val_t val) {
  ee_frame *frame = FRAME_POINTER(tag);
  if ((frame->tag == 0) || (frame->tag == 0xff)) {
    printd("Empty location where tag %u should be\r\n", tag);
    return -ENOENT;
  }
  if (frame->tag != (uint8_t)tag) {
    printd("Incorrect tag. Found %u, expected %u\r\n", frame->tag, (uint8_t)tag);
    return -EIO;
  }
  memcpy(val, frame->val, (sizeof(ee_val_t)/sizeof(uint8_t)));
  return 0;
}

int fmc_ee_write(ee_tags_t tag, const ee_val_t val) {
  ee_val_t existing_val;
  // If the value is the same that's currently in RAM, exit early
  if (!fmc_ee_read(tag, existing_val)) {
    if (!strncmp((const char *)existing_val, (const char *)val, (sizeof(ee_val_t)/sizeof(uint8_t)))) {
      printd("Ignoring write to tag %u because value has not changed\r\n", tag);
      return 0;
    }
  }
  printd("Storing val to tag %u...\r\n", tag);
  ee_frame *frame = FRAME_POINTER(tag);
  frame->tag = tag;
  memcpy(frame->val, val, (sizeof(ee_val_t)/sizeof(uint8_t)));
  DEBUG_PRINT_FRAME(frame);
  SET_DIRTY(PAGE_NUMBER(tag));
  printd("dirty_flag = %x\r\n", dirty_flag);
  return 0;
}

#ifdef DEBUG_PRINT
static void debug_print_frame(ee_frame *frame) {
  printf("  FRAME: %u: {%x, %x, %x, %x, %x, %x}\r\n", frame->tag,
         frame->val[0], frame->val[1], frame->val[2],
         frame->val[3], frame->val[4], frame->val[5]);
  return;
}
#endif
