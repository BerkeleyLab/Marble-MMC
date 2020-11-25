#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef SELFTEST
int xrp_push_low(uint8_t dev, uint16_t addr, uint8_t data[], unsigned len);
int xrp_set2(uint8_t dev, uint16_t addr, uint8_t data);
int xrp_read2(uint8_t dev, uint16_t addr);
int xrp_srecord(uint8_t dev, uint8_t data[]);
int xrp_program_static(uint8_t dev);
int xrp_file(uint8_t dev);
int marble_UART_recv(char *str, int size);
#else
#include "marble_api.h"
#endif

// Sending data to runtime memory, see ANP-39
static int xrp_push_high(uint8_t dev, uint16_t addr, uint8_t data[], unsigned len)
{
   int rc = 0;
   for (unsigned jx=0; jx<len; jx++) {
      unsigned addr1 = addr + jx;
      if (addr1 == 0xD022) continue;  // Mentioned in Exar's UnivPMIC github code base
      xrp_set2(dev, addr1, data[jx]);
   }
   // Double-check
   for (unsigned jx=0; jx<len; jx++) {
      printf(".");
      unsigned addr1 = addr + jx;
      if (addr1 == 0xD022) continue;
      if (addr1 == 0xFFAD) continue;  // Undocumented, ask MaxLinear about this
      int v = xrp_read2(dev, addr1);
      if (v != data[jx]) {
          printf(" fault %2.2x != %2.2x\n", v, data[jx]);
          rc = 1;
      }
   }
   if (rc == 0) printf(" OK\n");
   return rc;
}

// Return codes:
//   0  OK, continue
//   1  Fault, abort?
//   2  End of file detected
int xrp_srecord(uint8_t dev, uint8_t data[])
{
   unsigned len = data[0];
   uint16_t addr = (((unsigned) data[1]) << 8) | data[2];
   unsigned rtype = data[3];  // record type
   if (rtype != 0) {
      printf("rtype %d\n", rtype);
      return 2;
   }
   if (addr < 0x8000) {
      printf("Flash programming not yet handled.\n");
      return 1;
   }
   unsigned sum = 0;
   for (unsigned jx=0; jx<(len+5); jx++) sum = sum + data[jx];
   sum = sum & 0xff;
   if (sum != 0) {
      printf("Hex format checksum fault %2.2x\n", sum);
      return 1;
   }
   int rc;
   if (addr & 0x8000) {
      rc = xrp_push_high(dev, addr, data+4, len);
   } else {
      rc = xrp_push_low(dev, addr, data+4, len);
   }
   return rc;
}

// Data from python hex2c.py < foo.hex
// foo.hex is board-dependent
int xrp_program_static(uint8_t dev) {
   // each element of dd represents a hex record,
   // https://en.wikipedia.org/wiki/Intel_HEX
   // including length, address, record type, data, and checksum,
   // but without start code.
   char *dd[] = {
   };
   const unsigned dd_size = sizeof(dd) / sizeof(dd[0]);
   int rc=1;
   for (unsigned jx=0; jx<dd_size; jx++) {
      rc = xrp_srecord(dev, (uint8_t *) dd[jx]);
      if (rc) break;
   }
   return rc&1;  // Turn 2 (End of file detected) into 0
}

// hexdig_fun() is borrowed from newlib gdtoa-gethex.c.
// Possible author is David M. Gay, Copyright (C) 1998 by Lucent Technologies,
// licensed with Historical Permission Notice and Disclaimer (HPND).
static int hexdig_fun(unsigned char c)
{
   if (c>='0' && c<='9') return c-'0'+0x10;
   else if (c>='a' && c<='f') return c-'a'+0x10+10;
   else if (c>='A' && c<='F') return c-'A'+0x10+10;
   else return 0;
}

// Return codes:
//   0  OK, normal end of file reached
//   1  Fault, abort
int xrp_file(uint8_t dev) {
   printf("XRP7724 hex record file input [%2.2x]\n", dev);
   char rx_ch;
   int mode = 0;
   unsigned byte = 0;
   unsigned jx = 0;
   uint8_t record[70];
   do {
      if (marble_UART_recv(&rx_ch, 1) != 0) {
         // printf("%d %u %d\n", mode, rx_ch, jx);
         if (rx_ch == 27) return 1;
         int a = hexdig_fun(rx_ch);
         switch (mode) {
            case 0:
               if (rx_ch == ':') {
                 mode = 1;
                 jx = 0;
               }
               break;
            case 1:
               if (a>0) {
                  byte = a;
                  mode = 2;
               } else {
                  // printf("record %d %d\n", jx, record[0]);
                  if (jx > 4) {
                     int rc = xrp_srecord(dev, record);
                     if (rc) return (rc&1);  // at end or fault
                  }
                  mode = 0;
               }
               break;
            case 2:
               if (a>0) {
                  byte = ((byte << 4) | (a&0xf)) & 0xff;
                  if (jx>70) {
                     printf("bad2\n");
                     return 1;
                  }
                  record[jx++] = byte;
                  mode = 1;
               } else {
                  printf("bad1\n");
                  mode = 0;
               }
         }
      }
   } while (1);
}

#ifdef SELFTEST
// Sending data to flash, see ANP-38
int xrp_push_low(uint8_t dev, uint16_t addr, uint8_t data[], unsigned len)
{
   printf("xrp_push_low not part of test framework.\n");
   (void) dev;
   (void) addr;
   (void) data;
   (void) len;
   return 1;
}

int marble_UART_recv(char *str, int size)
{
   int ch;
   if (!str) return 0;
   for (int jx=0; jx < size; jx++) {
      ch = getchar();
      if (str) *str++ = ch;
   }
   return 1;
}

// vmem size is excessive, it could be much sparser.
// But nobody cares on a Real Workstation.
uint8_t vmem[32768];  // global
int actual;  // global

int xrp_set2(uint8_t dev, uint16_t addr, uint8_t data)
{
   (void) dev;  // unused in test framework
   // printf("r[%4.4x] <= %.2x\n", addr, data);
   if (addr >= 32768) {
       vmem[addr-32768] = data;
       actual++;
   }
   return 0;
}

int xrp_read2(uint8_t dev, uint16_t addr)
{
   (void) dev;  // unused in test framework
   int rv = 0;
   if (addr > 32768) rv = vmem[addr-32768];
   return rv;
}

int main(int argc, char *argv[])
{
   int wish = 0;
   //
   xrp_program_static(0);
   if (actual != 433) {
      printf("%d is not 433!\n", actual);
      return 1;
   }
   actual = 0;  // reset global
   //
   if (argc > 1) wish = atoi(argv[1]);
   for (unsigned jx=0; jx<32768; jx++) vmem[jx] = 0;
   int rc = xrp_file(0);
   printf("actual %d wish %d", actual, wish);
   if (actual != wish) {
      printf("  MISMATCH");
      rc = 1;
   }
   printf("\n");
   return rc;
}
#endif
