#include "marble_api.h"
#include <stdio.h>
#include "i2c_pm.h"
#include "mailbox.h"
#include "max6639.h"
#include "rev.h"

/* ============================= Helper Macros ============================== */
// Define SPI_SWITCH to re-route SPI bound for FPGA to PMOD for debugging
//#define SPI_SWITCH
#ifdef SPI_SWITCH
#define SSP_TARGET        SSP_PMOD
#else
#define SSP_TARGET        SSP_FPGA
#endif

/* ============================ Static Variables ============================ */
extern SSP_PORT SSP_FPGA;
extern SSP_PORT SSP_PMOD;
uint16_t update_count = 0;
static int mbox_is_disabled = 0;

/* =========================== Static Prototypes ============================ */
static void mbox_handleI2CBusStatusMsg(uint8_t msg);

// XXX Including auto-generated source file! This is atypical, but works nicely.
#include "mailbox_def.c"

/* ========================== Function Definitions ========================== */

void mbox_enable(void) {
  mbox_is_disabled = 0;
  return;
}

void mbox_disable(void) {
  mbox_is_disabled = 1;
  return;
}

int mbox_get_enable(void) {
  if (mbox_is_disabled) {
    return 0;
  }
  return 1;
}

static void mbox_set_page(uint8_t page_no)
{
   uint16_t ssp_buf;
   ssp_buf = 0x2200 + page_no;
   marble_SSP_write16(SSP_FPGA, &ssp_buf, 1);
}

void mbox_write_entry(uint8_t entry_no, uint8_t data) {
   uint16_t ssp_buf = 0x5000 + (entry_no<<8) + data;
   marble_SSP_write16(SSP_FPGA, &ssp_buf, 1);
}

uint8_t mbox_read_entry(uint8_t entry_no) {
   uint16_t ssp_recv, ssp_buf = 0x4000 + (entry_no<<8);
   marble_SSP_exch16(SSP_FPGA, &ssp_buf, &ssp_recv, 1);
   return (ssp_recv & 0xff);
}

void mbox_write_page(uint8_t page_no, uint8_t page_sz, const uint8_t page[]) {
   // Write at most 16 bytes to page
   if (page_sz > 16) page_sz = 16;
   mbox_set_page(page_no);
   for (unsigned jx=0; jx<page_sz; jx++) {
      mbox_write_entry(jx, page[jx]);
   }
}

void mbox_read_page(uint8_t page_no, uint8_t page_sz, uint8_t *page) {
   // Write at most 16 bytes to page
   if (page_sz > 16) page_sz = 16;
   mbox_set_page(page_no);
   for (unsigned jx=0; jx<page_sz; jx++) {
      page[jx] = mbox_read_entry(jx);
   }
}

void mbox_update(bool verbose)
{
  if (mbox_is_disabled) {
    return;
  }
  _UNUSED(verbose);
  update_count++;
  // Note! Input function must come before output function or any input values will
  // be clobbered by their output value before reading.
  mailbox_update_input();   // This function is auto-generated in src/mailbox_def.c
  mailbox_update_output();  // This function is auto-generated in src/mailbox_def.c
  return;
}

// Pseudo-mailbox interaction
int push_fpga_mac_ip(mac_ip_data_t *pmac_ip_data)
{
   uint16_t ssp_buf;
   int ssp_expect=0;
   int ssp_cnt=0;

   static unsigned short test_only = 0;  // use 0x4000 to disable

   ssp_buf = 0x2000 | test_only; // disable FPGA Ethernet

   ssp_expect += 2;
   ssp_cnt += marble_SSP_write16(SSP_TARGET, &ssp_buf, 1);

   // MAC first
   for (unsigned ix = 0; ix < MAC_LENGTH; ix++) {
      ssp_buf = pmac_ip_data->mac[ix] | (ix<<8) | 0x1000 | test_only;
      ssp_expect += 2;
      ssp_cnt += marble_SSP_write16(SSP_TARGET, &ssp_buf, 1);
   }

   // IP second
   for (unsigned ix = 0; ix < IP_LENGTH; ix++) {
      ssp_buf = pmac_ip_data->ip[ix] | ((ix+MAC_LENGTH)<<8) | 0x1000 | test_only;
      ssp_expect += 2;
      ssp_cnt += marble_SSP_write16(SSP_TARGET, &ssp_buf, 1);
   }

   ssp_buf = 0x2001 | test_only;  // enable FPGA Ethernet
   ssp_expect += 2;
   ssp_cnt += marble_SSP_write16(SSP_TARGET, &ssp_buf, 1);

   return (ssp_cnt == ssp_expect);
}

uint16_t mbox_get_update_count(void) {
  return update_count;
}

void mbox_reset_update_count(void) {
  update_count = 0;
  return;
}

// Handle input from FPGA across mailbox interface
// A '1' in bit position 0 clears the I2C bus status register
static void mbox_handleI2CBusStatusMsg(uint8_t msg) {
  //printf("I2C bus msg = 0x%x\r\n", msg);
  if (msg == 0x01) {
    printf("Resetting I2C bus status\r\n");
    resetI2CBusStatus();
  }
  return;
}

#if 0
int OLDpush_fpga_mac_ip(unsigned char data[10])
{
   uint16_t ssp_buf;
   int ssp_expect=0;
   int ssp_cnt=0;

   static unsigned short test_only = 0;  // use 0x4000 to disable

   ssp_buf = 0x2000 | test_only; // disable FPGA Ethernet

   ssp_expect += 2;
   ssp_cnt += marble_SSP_write16(SSP_FPGA, &ssp_buf, 1);

   for (unsigned ix = 0; ix < 10; ix++) {
      ssp_buf = data[ix] | (ix<<8) | 0x1000 | test_only;
      ssp_expect += 2;
      ssp_cnt += marble_SSP_write16(SSP_FPGA, &ssp_buf, 1);
   }
   ssp_buf = 0x2001 | test_only;  // enable FPGA Ethernet
   ssp_expect += 2;
   ssp_cnt += marble_SSP_write16(SSP_FPGA, &ssp_buf, 1);

   return (ssp_cnt == ssp_expect);
}
#endif
