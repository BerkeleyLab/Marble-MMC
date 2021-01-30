#include "marble_api.h"
#include <stdio.h>
#include "i2c_pm.h"
#include "mailbox.h"
#include "rev.h"

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

static void mbox_write_page(uint8_t page_no, uint8_t page_sz, const uint8_t page[]) {
   // Write at most 16 bytes to page
   if (page_sz > 16) page_sz = 16;
   mbox_set_page(page_no);
   for (unsigned jx=0; jx<page_sz; jx++) {
      mbox_write_entry(jx, page[jx]);
   }
}

static void mbox_read_page(uint8_t page_no, uint8_t page_sz, uint8_t *page) {
   // Write at most 16 bytes to page
   if (page_sz > 16) page_sz = 16;
   mbox_set_page(page_no);
   for (unsigned jx=0; jx<page_sz; jx++) {
      page[jx] = mbox_read_entry(jx);
   }
}

void mbox_update(bool verbose)
{
   // --------
   // Push data to remote mailbox
   // --------
   uint8_t page3[MB3_SIZE];

   static uint16_t count=0;
   count++;
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
      printf("\n");
   }
   // Write page
   mbox_write_page(3, MB3_SIZE, page3);

   // --------
   // Retrieve and act on data from remote mailbox
   // --------
   // Read page
   uint8_t page2[MB2_SIZE];
   mbox_read_page(2, MB2_SIZE, page2);

   if (verbose) {
      printf("Reading mailbox page 2:");
      for (unsigned jx=0; jx<MB2_SIZE; jx++) printf(" %2.2x", page2[jx]);
      printf("\n");
   }

   // Control of FMC power and MGT mux based on page 2, entry 0
   // Currently addressed as 0x200020 = 2097184 in test_marble_family
   // [1] - FMC_SEL,      [0] - ON/OFF
   // [3] - MGT_MUX0_SEL, [2] - ON/OFF
   // [5] - MGT_MUX1_SEL, [4] - ON/OFF
   // [7] - MGT_MUX2_SEL, [6] - ON/OFF
   uint8_t fmc_mgt_cmd = page2[MB2_FMC_MGT_CTL];

   if (verbose) printf("FMC/MGT command 0x%2.2x\n", fmc_mgt_cmd);

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
int push_fpga_mac_ip(unsigned char data[10])
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
