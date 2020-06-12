#include "marble_api.h"

int push_fpga_mac_ip(unsigned char data[10])
{
   uint16_t ssp_buf;
   int ssp_expect=0;
   int ssp_cnt=0;

   static unsigned short test_only = 0;  // use 0x4000 to disable

   ssp_buf = 0x2000 | test_only; // disable FPGA Ethernet

   ssp_expect += 2;
   ssp_cnt += marble_SSP_write(SSP_FPGA, (uint8_t*) &ssp_buf, 2);

   for (unsigned ix = 0; ix < 10; ix++) {
      ssp_buf = data[ix] | (ix<<8) | 0x1000 | test_only;
      ssp_expect += 2;
      ssp_cnt += marble_SSP_write(SSP_FPGA, (uint8_t*) &ssp_buf, 2);
   }
   ssp_buf = 0x2001 | test_only;  // enable FPGA Ethernet
   ssp_expect += 2;
   ssp_cnt += marble_SSP_write(SSP_FPGA, (uint8_t*) &ssp_buf, 2);

   return (ssp_cnt == ssp_expect);
}
