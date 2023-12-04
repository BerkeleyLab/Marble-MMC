/*
 * File: watchdog.c
 * Desc: FPGA Watchdog system for authenticating host and rebooting FPGA to
 *       golden image when needed.
 */

#include "watchdog.h"
#include "marble_api.h"
#include "refsip.h"
#include "common.h"
#include <stdio.h>

//#define DEBUG_PRINT
#include "dbg.h"

/* ============================= Helper Macros ============================== */
// TODO - Get this from mailbox.h or marble_api.h
#define MAILBOX_UPDATE_PERIOD_SECONDS (SPI_MAILBOX_PERIOD_MS/1000)
#define MAX_WATCHDOG_TIMEOUT_PERIODS  (255)
#define MAX_WATCHDOG_TIMEOUT_S        (MAX_WATCHDOG_TIMEOUT_PERIODS*MAILBOX_UPDATE_PERIOD_SECONDS)
#define HASH_SIZE                     (2)

/* ============================ Static Variables ============================ */
typedef enum {
  STATE_BOOT,     // Waiting for first DONE rising edge     (disable watchdog)
  STATE_GOLDEN,   // Assumed to be running golden image     (disable watchdog)
  STATE_USER,     // Assumed to be running user image       (enable watchdog)
  STATE_RESET,    // MMC is holding the FPGA reset low      (disable watchdog)
} FPGAWD_State_t;

static uint32_t remote_hash[HASH_SIZE] = {0};
static uint32_t local_hash[HASH_SIZE] = {0};
static uint32_t entropy[HASH_SIZE] = {0};
static int rng_status = 0;
static unsigned int poll_counter = 0;
static unsigned int max_poll_counts = 10;
static FPGAWD_State_t fpga_state = STATE_BOOT;

/* =========================== Static Prototypes ============================ */
static void compute_hash(void);
static int vet_hash(void);
static void fpga_reset_callback(void);
static void pet_wdog(void);

/* ========================== Function Definitions ========================== */
static const char* state_str(FPGAWD_State_t ss) {
  switch (ss) {
    case STATE_BOOT:   return "BOOT";
    case STATE_GOLDEN: return "GOLDEN";
    case STATE_USER:   return "USER";
    case STATE_RESET:  return "RESET";
    default: break;
  }
  return "BAD";
}

uint32_t FPGAWD_GetNonce(unsigned int index) {
  if (index < HASH_SIZE) return local_hash[index];
  else return 0;
}

int FPGAWD_SetPeriod(unsigned int period) {
  period = MIN(period, MAX_WATCHDOG_TIMEOUT_PERIODS);
  printd("period = %d\r\n", period);
  max_poll_counts = (unsigned)MAX((int)((period/MAILBOX_UPDATE_PERIOD_SECONDS)-1), 0);
  if (max_poll_counts == 0) {
    printf("Disabling watchdog\r\n");
  } else {
    printf("Setting watchdog timeout to %d seconds.\r\n", (max_poll_counts+1)*MAILBOX_UPDATE_PERIOD_SECONDS);
  }
  printd("Setting poll_counter to %d\r\n", max_poll_counts);
  poll_counter = max_poll_counts;
  return period;
}

int FPGAWD_GetPeriod(void) {
  if (max_poll_counts == 0) {
    return 0;
  } else {
    return (max_poll_counts+1)*MAILBOX_UPDATE_PERIOD_SECONDS;
  }
}

void FPGAWD_Poll(void) {
  if (poll_counter == 0) return;
  if (--poll_counter == 0) {
    printd("poll_counter reached 0\r\n");
    if (fpga_state == STATE_USER) {
      reset_fpga_with_callback(fpga_reset_callback);
      fpga_state = STATE_RESET;
    }
  }
  return;
}

void FPGAWD_DoneHandler(void) {
  FPGAWD_State_t old = fpga_state;
  switch (fpga_state) {
    case STATE_BOOT:
      // First rising edge after boot is assumed to be the golden image
      fpga_state = STATE_GOLDEN;
      break;
    case STATE_GOLDEN:
      // Must assume we are booting to user image
      fpga_state = STATE_USER;
      pet_wdog();
      break;
    case STATE_USER:
      // Remain in state USER for subsequent boots
      pet_wdog();
      break;
    case STATE_RESET:
      // This should not happen
      printf("DoneHandler invalid transition. Consider reboot.\r\n");
      break;
    default:
      break;
  }
  printf("DoneHandler transition %s -> %s\r\n", state_str(old), state_str(fpga_state));
  return;
}

/* Encapsulation necessitates this kind of structure
 */
static void fpga_reset_callback(void) {
  printf("Watchdog timeout: resetting to golden image.\r\n");
  if (fpga_state == STATE_RESET) {
    fpga_state = STATE_BOOT;
    poll_counter = 0;
  }
  return;
}

static void pet_wdog(void) {
  printd("Watchdog pet\n");
  poll_counter = max_poll_counts;
  for (unsigned ux=0; ux < HASH_SIZE; ux++) {
    // STM32 has a 4-entry FIFO for this feature, right?
    rng_status = get_hw_rnd(&(entropy[ux]));
    printd("rnd %d %d %8.8lx\r\n", ux, rng_status, entropy[ux]);
  }
  compute_hash();
}

void FPGAWD_HandleHash(uint32_t hash, unsigned int index) {
  // Hash should be read in order from highest index to 0. See mbox.def
  if (index < HASH_SIZE) remote_hash[index] = hash;
  if (index == 0 && vet_hash()) pet_wdog();
  return;
}

static void compute_hash(void) {
  // For now, we just copy bits from the entropy pool.
  // In the future, this can involve some fancy math.
  for (int n = 0; n < HASH_SIZE; n++) {
    local_hash[n] = entropy[n];
  }
  return;
}

static const unsigned char *get_auth_key(void) {
  return (const unsigned char *) "super secret key";  // trailing nul ignored
}

static int vet_hash(void) {
  uint32_t desired_mac[HASH_SIZE];
  const unsigned char *key = get_auth_key();
  int match = 1;
  for (int n = 0; n < HASH_SIZE; n++) {
    if (remote_hash[n] != 0) match = 0;
  }
  if (match) return 0;  // all zeros means disabled
  core_siphash((unsigned char *) desired_mac, (unsigned char *) local_hash, 8, key);
  match = 1;
  for (int n = 0; n < HASH_SIZE; n++) {
    if (remote_hash[n] != desired_mac[n]) match = 0;
  }
  if (0) {
    printf("local_hash  = %8.8lx %8.8lx\r\n", local_hash[0], local_hash[1]);
    printf("desired_mac = %8.8lx %8.8lx\r\n", desired_mac[0], desired_mac[1]);
    printf("remote_hash = %8.8lx %8.8lx\r\n", remote_hash[0], remote_hash[1]);
    printf("vet_hash match = %d\r\n", match);
  }
  return match;
}

void FPGAWD_ShowState(void) {
  uint32_t desired_mac[HASH_SIZE];
  const unsigned char *key = get_auth_key();
  core_siphash((unsigned char *) desired_mac, (unsigned char *) local_hash, 8, key);
  printf("poll_counter = %d\r\n", poll_counter);
  printf("FPGA state  = %s\r\n", state_str(fpga_state));
  printf("local_hash  = %8.8lx %8.8lx\r\n", local_hash[0], local_hash[1]);
  printf("desired_mac = %8.8lx %8.8lx\r\n", desired_mac[0], desired_mac[1]);
  printf("remote_hash = %8.8lx %8.8lx\r\n", remote_hash[0], remote_hash[1]);
}
