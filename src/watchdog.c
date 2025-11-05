/*
 * File: watchdog.c
 * Desc: FPGA Watchdog system for authenticating host and rebooting FPGA to
 *       golden image when needed.
 */

#include "watchdog.h"
#include "marble_api.h"
#include "refsip.h"
<<<<<<< HEAD
=======
#include "common.h"
#include "string.h"
#include "eeprom.h"
>>>>>>> master
#include <stdio.h>

//#define DEBUG_PRINT
#include "dbg.h"

/* ============================= Helper Macros ============================== */
<<<<<<< HEAD
// TODO - Get this from mailbox.h or marble_api.h
#define MAILBOX_UPDATE_PERIOD_SECONDS (SPI_MAILBOX_PERIOD_MS/1000)
#define MAX_WATCHDOG_TIMEOUT      (255*MAILBOX_UPDATE_PERIOD_SECONDS)
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
=======
#define MAILBOX_UPDATE_PERIOD_SECONDS (SPI_MAILBOX_PERIOD_MS/1000)
#define MAX_WATCHDOG_TIMEOUT_PERIODS  (255)
#define MAX_WATCHDOG_TIMEOUT_S        (MAX_WATCHDOG_TIMEOUT_PERIODS*MAILBOX_UPDATE_PERIOD_SECONDS)
// Size of hash in bytes
#define HASH_SIZE                     (8)
#define HASH_SIZE_32                  (HASH_SIZE/4)

/* ============================ Static Variables ============================ */
static uint8_t remote_hash[HASH_SIZE] = {0};
static uint8_t local_nonce[HASH_SIZE] = {0};
static uint32_t entropy[HASH_SIZE_32] = {0};
>>>>>>> master
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

<<<<<<< HEAD
uint32_t FPGAWD_GetNonce(unsigned int index) {
  if (index < HASH_SIZE) return local_hash[index];
  else return 0;
}

int FPGAWD_SetPeriod(unsigned int period) {
  period = period > MAX_WATCHDOG_TIMEOUT ? MAX_WATCHDOG_TIMEOUT : period;
  max_poll_counts = (period/MAILBOX_UPDATE_PERIOD_SECONDS)-1 > 0 ? (period/MAILBOX_UPDATE_PERIOD_SECONDS)-1 : 0;
  if (max_poll_counts == 0) {
    printf("Disabling watchdog\r\n");
  } else {
    printf("Setting watchdog timeout to %d seconds.\r\n", (max_poll_counts+1)*MAILBOX_UPDATE_PERIOD_SECONDS);
  }
=======
void FPGAWD_GetNonce(uint8_t *pdata) {
  memcpy(pdata, local_nonce, HASH_SIZE);
  return;
}

int FPGAWD_SetPeriod(unsigned int period) {
  period = MIN(period, MAX_WATCHDOG_TIMEOUT_PERIODS);
  printd("period = %d\r\n", period);
  max_poll_counts = (unsigned)MAX((int)((period/MAILBOX_UPDATE_PERIOD_SECONDS)-1), 0);
  if (max_poll_counts == 0) {
    printf("Disabling watchdog\r\n");
  } else {
    printf("Setting watchdog timeout to %u seconds.\r\n", (max_poll_counts+1)*MAILBOX_UPDATE_PERIOD_SECONDS);
  }
  printd("Setting poll_counter to %d\r\n", max_poll_counts);
>>>>>>> master
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
<<<<<<< HEAD
      reset_fpga_with_callback(fpga_reset_callback);
      fpga_state = STATE_RESET;
=======
      printf("Watchdog timeout: resetting to golden image.\r\n");
      FPGAWD_SelfReset();
>>>>>>> master
    }
  }
  return;
}

<<<<<<< HEAD
=======
void FPGAWD_SelfReset(void) {
  reset_fpga_with_callback(fpga_reset_callback);
  fpga_state = STATE_RESET;
  return;
}

>>>>>>> master
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
<<<<<<< HEAD
  printf("Watchdog timeout: resetting to golden image.\r\n");
  if (fpga_state == STATE_RESET) {
    fpga_state = STATE_BOOT;
    poll_counter = 0;
  }
=======
  fpga_state = STATE_BOOT;
  poll_counter = 0;
>>>>>>> master
  return;
}

static void pet_wdog(void) {
  printd("Watchdog pet\n");
  poll_counter = max_poll_counts;
<<<<<<< HEAD
  for (unsigned ux=0; ux < HASH_SIZE; ux++) {
    // STM32 has a 4-entry FIFO for this feature, right?
    rng_status = get_hw_rnd(&(entropy[ux]));
    printd("rnd %d %d %8.8lx\r\n", ux, rng_status, entropy[ux]);
=======
  for (unsigned int ux=0; ux < HASH_SIZE_32; ux++) {
    // STM32 has a 4-entry FIFO for this feature, right?
    rng_status = get_hw_rnd(&(entropy[ux]));
    printd("rnd %d %d %8.8"PRIx32"\r\n", ux, rng_status, entropy[ux]);
>>>>>>> master
  }
  compute_hash();
}

<<<<<<< HEAD
void FPGAWD_HandleHash(uint32_t hash, unsigned int index) {
  // Hash should be read in order from highest index to 0. See mbox.def
  if (index < HASH_SIZE) remote_hash[index] = hash;
  if (index == 0 && vet_hash()) pet_wdog();
=======
void FPGAWD_HandleHash(const uint8_t *hash) {
  // Hash should be read in order from highest index to 0. See mbox.def
  memcpy(remote_hash, hash, HASH_SIZE);
  if (vet_hash()) pet_wdog();
>>>>>>> master
  return;
}

static void compute_hash(void) {
  // For now, we just copy bits from the entropy pool.
  // In the future, this can involve some fancy math.
  for (int n = 0; n < HASH_SIZE; n++) {
<<<<<<< HEAD
    local_hash[n] = entropy[n];
=======
    // Yuck.  Is there a better (safe/correct) way of copying two 32-bit ints
    // to eight bytes?
    local_nonce[n] = (entropy[(n >> 2)] >> 8*(n % 4)) & 0xff;
>>>>>>> master
  }
  return;
}

<<<<<<< HEAD
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
=======
#define KEY_SIZE                (16)

#ifdef KEY_BYPASS
#define eeprom_read_wd_key get_fake_key
int get_fake_key(volatile uint8_t *pdata, int len) {
  uint8_t fake[] = "super secret key";  // trailing nul ignored
  if (len != KEY_SIZE) return -1;
  memcpy((uint8_t *) pdata, fake, KEY_SIZE);
  return 0;
}
#endif

static void print64(const char *header, const uint8_t *data, unsigned len)
{
  printf("%s",header);
  for (unsigned jx=0; jx<len; jx++) printf("%2.2x", data[jx]);
  printf("\r\n");
}

static int vet_hash(void) {
  uint8_t desired_mac[HASH_SIZE];
  unsigned char key[KEY_SIZE];
  int match = eeprom_read_wd_key((volatile uint8_t *)key, KEY_SIZE);
  if (match != 0) {
    // Failed to retrieve key
    return -1;
  }
  match = 1;
  for (int n = 0; n < HASH_SIZE; n++) {
    match &= (remote_hash[n] == 0);
  }
  if (match) return 0;  // all zeros means disabled
  core_siphash((unsigned char *) desired_mac, (unsigned char *) local_nonce, 8, key);
  memset(key, 0xcc, KEY_SIZE);  // Clobber key in RAM
  match = 1;
  for (int n = 0; n < HASH_SIZE; n++) {
    match &= (remote_hash[n] == desired_mac[n]);
  }
  if (0) {
    print64("local_nonce = ", local_nonce, HASH_SIZE);
    print64("desired_mac = ", desired_mac, HASH_SIZE);
    print64("remote_hash = ", remote_hash, HASH_SIZE);
>>>>>>> master
    printf("vet_hash match = %d\r\n", match);
  }
  return match;
}

void FPGAWD_ShowState(void) {
<<<<<<< HEAD
  uint32_t desired_mac[HASH_SIZE];
  const unsigned char *key = get_auth_key();
  core_siphash((unsigned char *) desired_mac, (unsigned char *) local_hash, 8, key);
  printf("poll_counter = %d\r\n", poll_counter);
  printf("FPGA state  = %s\r\n", state_str(fpga_state));
  printf("local_hash  = %8.8lx %8.8lx\r\n", local_hash[0], local_hash[1]);
  printf("desired_mac = %8.8lx %8.8lx\r\n", desired_mac[0], desired_mac[1]);
  printf("remote_hash = %8.8lx %8.8lx\r\n", remote_hash[0], remote_hash[1]);
=======
  uint8_t desired_mac[HASH_SIZE];
  //const unsigned char *key = get_auth_key();
  unsigned char key[KEY_SIZE];
  printf("poll_counter = %u\r\n", poll_counter);
  printf("FPGA state  = %s\r\n", state_str(fpga_state));
  print64("local_nonce = ", local_nonce, HASH_SIZE);
  int rval = eeprom_read_wd_key((volatile uint8_t *)key, KEY_SIZE);
  if (rval != 0) {
    // Failed to retrieve key
    printf("desired_mac unknown -- no key\r\n");
  } else {
    if (0) {
      printf("key");
      for (unsigned jx=0; jx<KEY_SIZE; jx++) printf(" %2.2x", key[jx]);
      printf("\r\n");
    }
    core_siphash((unsigned char *) desired_mac, (unsigned char *) local_nonce, 8, key);
    memset(key, 0xcc, KEY_SIZE);  // Clobber key in RAM
    print64("desired_mac = ", desired_mac, HASH_SIZE);
  }
  print64("remote_hash = ", remote_hash, HASH_SIZE);
}

FPGAWD_State_t FPGAWD_GetState(void) {
  return fpga_state;
>>>>>>> master
}
