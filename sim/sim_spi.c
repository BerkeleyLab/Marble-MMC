/* A simulated FPGA pseudo-SPI mailbox memory
 */

#include <stdint.h>

//#define DEBUG_PRINT
#include <stdio.h>
#include "dbg.h"

typedef void *SSP_PORT;

#define MAILBOX_PAGES                 (32)

static uint8_t mailbox[MAILBOX_PAGES][16]; // Make sure this stays > max mailbox page

static unsigned int npage = 0;

// GLOBALS
SSP_PORT SSP_FPGA;

int marble_SSP_write16(SSP_PORT ssp, uint16_t *buffer, unsigned size) {
  if (ssp != SSP_FPGA) {
    return 0;
  }
  uint8_t upper = (uint8_t)((*buffer) >> 8);
  if (upper == 0x22) {  // Set page
    npage = (unsigned int)((*buffer) & 0x7f);
    printd("Set page %d\r\n", npage);
  } else if ((upper & 0xf0) == 0x50) {  // Mailbox write
    mailbox[npage][(upper & 0x0f)] = (uint8_t)((*buffer) & 0xff);
  } // Ignore IP/MAC/port config and enable/disable rx for now
  return 0;
}

int marble_SSP_read16(SSP_PORT ssp, uint16_t *buffer, unsigned size) {
  // Unused in application as of writing
  if (ssp != SSP_FPGA) {
    return 0;
  }
  uint8_t upper = (uint8_t)((*buffer) >> 8);
  if ((upper & 0xf0) == 0x40) {  // Mailbox read
    *buffer = (uint16_t)mailbox[npage][(upper & 0x0f)];
  }
  return 0;
}

int marble_SSP_exch16(SSP_PORT ssp, uint16_t *tx_buf, uint16_t *rx_buf, unsigned size) {
  if (ssp != SSP_FPGA) {
    return 0;
  }
  uint8_t upper = (uint8_t)((*tx_buf) >> 8);
  if ((upper & 0xf0) == 0x40) {  // Mailbox read
    *rx_buf = (uint16_t)mailbox[npage][(upper & 0x0f)];
  }
  return 0;
}


