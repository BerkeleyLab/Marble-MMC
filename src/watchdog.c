/*
 * File: watchdog.c
 * Desc: FPGA Watchdog system for authenticating host and rebooting FPGA to
 *       golden image when needed.
 */

#include "watchdog.h"
#include "marble_api.h"

// Debug
#include <stdio.h>

/* ============================= Helper Macros ============================== */
#define HASH_SIZE         (3)

/* ============================ Static Variables ============================ */
typedef enum {
  STATE_BOOT,     // Waiting for first DONE rising edge     (disable watchdog)
  STATE_GOLDEN,   // Assumed to be running golden image     (disable watchdog)
  STATE_USER,     // Assumed to be running user image       (enable watchdog)
  STATE_RESET,    // MMC is holding the FPGA reset low      (disable watchdog)
} FPGAWD_State_t;

static uint32_t remote_hash[HASH_SIZE] = {0};
static uint32_t local_hash[HASH_SIZE] = {0};
static uint32_t rand;
static int rng_status = 0;
static int poll_counter = 0;
static int max_poll_counts = 10; // TODO - Get from non-volatile
static FPGAWD_State_t fpga_state = STATE_BOOT;

/* =========================== Static Prototypes ============================ */
static void compute_hash(void);
static int vet_hash(void);
static void fpga_reset_callback(void);

/* ========================== Function Definitions ========================== */
uint32_t FPGAWD_GetRand(void) {
  rng_status = get_hw_rnd(&rand);
  return rand;
}

void FPGAWD_set_period(int period) {
  max_poll_counts = period;
  return;
}

void FPGAWD_Poll(void) {
  if (max_poll_counts == 0) {
    return;
  }
  printf("poll_counter = %d\r\n", poll_counter);
  if (++poll_counter > max_poll_counts) {
    reset_fpga_with_callback(fpga_reset_callback);
    fpga_state = STATE_RESET;
  }
  return;
}

void FPGAWD_DoneHandler(void) {
  switch (fpga_state) {
    case STATE_BOOT:
      // First rising edge after boot is assumed to be the golden image
      printf("Going to STATE_GOLDEN\r\n");
      fpga_state = STATE_GOLDEN;
      break;
    case STATE_GOLDEN:
      // Must assume we are booting to user image
      printf("Going to STATE_USER\r\n");
      fpga_state = STATE_USER;
      break;
    case STATE_USER:
      // Remain in state USER for subsequent boots
      printf("Stay in STATE_USER\r\n");
      break;
    case STATE_RESET:
      // This should not happen
      printf("YIKES!!!\r\n");
      break;
    default:
      break;
  }
  return;
}

/* Encapsulation necessitates this kind of structure
 */
static void fpga_reset_callback(void) {
  printf("reset_callback\r\n");
  if (fpga_state == STATE_RESET) {
    fpga_state = STATE_BOOT;
    poll_counter = 0;
  }
  return;
}

void FPGAWD_HandleHash(uint32_t hash, int index) {
  // Hash should be read in order from highest index to 0. See mbox.def
  if (index < HASH_SIZE) {
    remote_hash[index] = hash;
  }
  if (index == 0) {
    if (vet_hash()) {
      printf("Hash vetted\r\n");
      poll_counter = 0;
    }
  }
  return;
}

static void compute_hash(void) {
  // TODO - For now we're just comparing the lower 32-bits with the random number
  //        In the future, this should be an actual hash function.
  for (int n = 0; n < HASH_SIZE; n++) {
    local_hash[n] = 0;
  }
  local_hash[0] = rand;
  return;
}

static int vet_hash(void) {
  compute_hash();
  int match = 1;
  for (int n = 0; n < HASH_SIZE; n++) {
    if (remote_hash[n] != local_hash[n]) {
      match = 0;
    }
  }
  return match;
}
