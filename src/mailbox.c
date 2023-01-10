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

/* =========================== Static Prototypes ============================ */
static void mbox_handleI2CBusStatusMsg(uint8_t msg);

// XXX Including auto-generated source file! This is atypical, but works nicely.
#include "mailbox_def.c"

/* ========================== Function Definitions ========================== */
static void mbox_set_page(uint8_t page_no)
{
   uint16_t ssp_buf;
   ssp_buf = 0x2200 + page_no;
   marble_SSP_write16(SSP_FPGA, &ssp_buf, 1);
}

static void mbox_write_entry(uint8_t entry_no, uint8_t data) {
   uint16_t ssp_buf = 0x5000 + (entry_no<<8) + data;
   marble_SSP_write16(SSP_FPGA, &ssp_buf, 1);
}

static uint8_t mbox_read_entry(uint8_t entry_no) {
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

void mbox_handle_fmc_mgt_ctl(uint8_t fmc_mgt_cmd) {
  // Control of FMC power and MGT mux based on mailbox entry MB2_FMC_MGT_CTL
  // Currently addressed as 0x200020 = 2097184 in test_marble_family
  // [1] - FMC_SEL,      [0] - ON/OFF
  // [3] - MGT_MUX0_SEL, [2] - ON/OFF
  // [5] - MGT_MUX1_SEL, [4] - ON/OFF
  // [7] - MGT_MUX2_SEL, [6] - ON/OFF
  //if (verbose) printf("FMC/MGT command 0x%2.2x\r\n", fmc_mgt_cmd);

  if (fmc_mgt_cmd != 0) {  // clear mailbox entry
    mbox_set_page(2);
    mbox_write_entry(MB2_FMC_MGT_CTL, 0x00);
  }
  if (fmc_mgt_cmd & 2) {
    marble_FMC_pwr(fmc_mgt_cmd & 1);
  }
  unsigned v = fmc_mgt_cmd;
  for (unsigned kx=1; kx<4; kx++) {
    v = v >> 2;
    if (v & 2) {
      marble_MGTMUX_set(kx, v & 1);
    }
  }
  return;
}

void mbox_update(bool verbose)
{
  update_count++;
<<<<<<< HEAD
  mailbox_update_output();  // This function is auto-generated in src/mailbox_def.c
  mailbox_update_input();   // This function is auto-generated in src/mailbox_def.c
#if 0
=======
  mailbox_update_output();
  mailbox_update_input();
#if 1
  (void) verbose;  // not used
#else
>>>>>>> 05e2e1feb07d5ddd0974e61da76d80c0dc92e4fc
   // --------
   // Push data to remote mailbox
   // --------

   static uint16_t count=0;
   count++;
   { // new block makes it clear we can reuse memory
      uint8_t page3[MB3_SIZE];
      int lm75_0_temp, lm75_1_temp;
      LM75_read(LM75_0, LM75_TEMP, &lm75_0_temp);
      LM75_read(LM75_1, LM75_TEMP, &lm75_1_temp);

      page3[MB3_COUNT_HI] = count >> 8;
      page3[MB3_COUNT_LO] = count & 0xff;
      page3[MB3_PAD1] = 0xff;
      page3[MB3_PAD2] = 0x00;
      page3[MB3_LM75_0_HI] = lm75_0_temp >> 8;
      page3[MB3_LM75_0_LO] = lm75_0_temp & 0xff;
      page3[MB3_LM75_1_HI] = lm75_1_temp >> 8;
      page3[MB3_LM75_1_LO] = lm75_1_temp & 0xff;
      page3[MB3_FMC_ST] = marble_FMC_status();
      page3[MB3_PWR_ST] = marble_PWR_status();
#ifdef MARBLE_V2
      page3[MB3_MGTMUX_ST] = marble_MGTMUX_status();
#else
      page3[MB3_MGTMUX_ST] = 0xFF;
#endif
      page3[MB3_PAD3] = 0x00;
      for (unsigned i=0; i<4; i++) {
         page3[i+MB3_GIT32_4] = (GIT_REV_32BIT >> (32-8*(i+1))) & 0xff;
      }
      if (verbose) {
         printf("Writing mailbox page 3:");
         for (unsigned jx=0; jx<MB3_SIZE; jx++) printf(" %2.2x", page3[jx]);
         printf("\r\n");
      }
      // Write page
      mbox_write_page(3, MB3_SIZE, page3);
   }
   { // new block makes it clear we can reuse memory
      uint8_t page4[MB4_SIZE];
      //  MB4_MAX_T1_HI, 0x00
      //  MB4_MAX_T1_LO, 0x05
      //  MB4_MAX_T2_HI, 0x01
      //  MB4_MAX_T2_LO, 0x06
      //  MB4_MAX_F1_TACH, 0x20
      //  MB4_MAX_F2_TACH, 0x21
      //  MB4_MAX_F1_DUTY, 0x26
      // MB4_MAX_F2_DUTY, 0x27
      int max_addr[8] = {0x00, 0x05, 0x01, 0x06, 0x20, 0x21, 0x26, 0x27};
      for (unsigned ix=0; ix<8; ix++) {
          // XXX don't check for errors
          int max_val;
          get_max6639_reg(max_addr[ix], &max_val);
          page4[ix] = max_val;
      }
      page4[MB4_COUNT_HI] = count >> 8;
      page4[MB4_COUNT_LO] = count & 0xff;
      if (verbose) {
         printf("Writing mailbox page 4:");
         for (unsigned jx=0; jx<MB4_SIZE; jx++) printf(" %2.2x", page4[jx]);
         printf("\r\n");
      }
      // Write page
      mbox_write_page(4, MB4_SIZE, page4);
   }

   // --------
   // Retrieve and act on data from remote mailbox
   // --------
   // Read page
   { // new block makes it clear we can reuse memory
      uint8_t page2[MB2_SIZE];
      mbox_read_page(2, MB2_SIZE, page2);

      if (verbose) {
         printf("Reading mailbox page 2:");
         for (unsigned jx=0; jx<MB2_SIZE; jx++) printf(" %2.2x", page2[jx]);
         printf("\r\n");
      }

      // Control of FMC power and MGT mux based on page 2, entry 0
      // Currently addressed as 0x200020 = 2097184 in test_marble_family
      // [1] - FMC_SEL,      [0] - ON/OFF
      // [3] - MGT_MUX0_SEL, [2] - ON/OFF
      // [5] - MGT_MUX1_SEL, [4] - ON/OFF
      // [7] - MGT_MUX2_SEL, [6] - ON/OFF
      uint8_t fmc_mgt_cmd = page2[MB2_FMC_MGT_CTL];

      if (verbose) printf("FMC/MGT command 0x%2.2x\r\n", fmc_mgt_cmd);

      if (fmc_mgt_cmd != 0) {  // clear mailbox entry
         mbox_set_page(2);
         mbox_write_entry(MB2_FMC_MGT_CTL, 0x00);
      }
      if (fmc_mgt_cmd & 2) {
         marble_FMC_pwr(fmc_mgt_cmd & 1);
      }
      unsigned v = fmc_mgt_cmd;
      for (unsigned kx=1; kx<4; kx++) {
         v = v >> 2;
         if (v & 2) {
#ifdef MARBLE_V2
            marble_MGTMUX_set(kx, v & 1);
#endif
         }
      }
   }
#endif
}

void mbox_peek(void)
{
   uint8_t page2[MB2_SIZE];
   mbox_read_page(2, MB2_SIZE, page2);
   printf("Mailbox page 2:");
   for (unsigned jx=0; jx<MB2_SIZE; jx++) printf(" %2.2x", page2[jx]);
   printf("\r\n");

   uint8_t page3[MB3_SIZE];
   mbox_read_page(3, MB3_SIZE, page3);
   printf("Mailbox page 3:");
   for (unsigned jx=0; jx<MB3_SIZE; jx++) printf(" %2.2x", page3[jx]);
   printf("\r\n");
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
  if (msg & 0x01) {
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
