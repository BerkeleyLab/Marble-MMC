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
#include "i2c_pm.h"
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

// Data from python hex2c.py < Marble_runtime.hex
// where Marble_runtime.hex is found in github.com/BerkeleyLab/Marble.git
// Temporary only (?); it would be much better to read hex records from UART
int xrp_program_static(uint8_t dev) {
   // each element of dd represents a hex record,
   // https://en.wikipedia.org/wiki/Intel_HEX
   // including length, address, record type, data, and checksum,
   // but without start code.
   char *dd[] = {
      "\x10\x80\x72\x00\x00\x02\x50\x00\x00\xFF\xFF\x32\x04\x32\x06\x32\x00\x00\x03\x00\x0B",
      "\x10\x80\x82\x00\x00\x00\x00\x00\x00\x00\x02\x00\x04\x00\x07\x00\x00\x00\x00\x64\x7D",
      "\x10\x80\x92\x00\x21\x64\x64\x64\x20\x64\x64\x64\x21\x64\x64\x64\x22\x64\x64\x0A\x04",
      "\x0B\x80\xA2\x00\x20\x0A\x05\x19\xFF\x00\x00\x00\x00\xFF\xFF\x8E",
      "\x10\x80\xAE\x00\x04\xFF\xFF\x64\x42\x50\x48\x18\x0C\x30\x18\x00\x00\x00\x00\x00\x16",
      "\x10\x80\xBE\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x0F\xAF",
      "\x04\x80\xCE\x00\x00\x00\x00\x1E\x90",
      "\x10\xC0\x00\x00\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x13\xE1\x14\x33\x0D\xE0",
      "\x10\xC0\x10\x00\x00\x00\x03\x00\x50\x00\x11\x02\x15\xEB\x4E\x2A\x2C\x3C\x14\x01\xC5",
      "\x10\xC0\x20\x00\x01\x01\x01\x01\x01\x1D\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB\xF6",
      "\x0F\xC0\x30\x00\x00\x58\x64\x3D\x3D\x14\x73\x59\x5D\x12\x3E\x21\x7F\x12\x28\x64",
      "\x10\xC1\x00\x00\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x1D\xE1\x14\x33\x0D\xD5",
      "\x10\xC1\x10\x00\x00\x00\x04\x00\x6A\x00\x16\x02\x0D\xF3\x5F\x36\x3B\x3C\x0D\x04\x7C",
      "\x10\xC1\x20\x00\x04\x04\x04\x04\x04\x1C\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB\xE7",
      "\x0F\xC1\x30\x00\x00\x90\x42\x7B\x7B\x1F\xD3\x44\x61\x1B\xE9\x20\x00\x22\x28\x33",
      "\x10\xC2\x00\x00\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x27\xE1\x28\x33\x0D\xB6",
      "\x10\xC2\x10\x00\x00\x00\x03\x00\x40\x00\x0D\x02\x11\xEF\x58\x4D\x56\x3C\x10\x80\x05",
      "\x10\xC2\x20\x00\x80\x80\x80\x80\x80\x1D\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB\x79",
      "\x0F\xC2\x30\x00\x00\x44\x50\x1E\x1E\x17\xD6\x52\x56\x15\xDD\x2A\x76\x01\x18\xEF",
      "\x10\xC3\x00\x00\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x13\xE1\x28\x33\x0D\xC9",
      "\x10\xC3\x10\x00\x00\x00\x05\x00\x73\x00\x18\x02\x91\x6F\x5C\x2F\x32\x3C\x16\x40\x3C",
      "\x10\xC3\x20\x00\x40\x40\x40\x40\x40\x1D\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB\xB8",
      "\x0F\xC3\x30\x00\x00\x29\x48\x3D\x3D\x15\x07\x59\x26\x11\xEE\x20\x00\x11\x18\x30",
      "\x10\xC4\x00\x00\x05\x00\x00\x4C\x4F\x4C\x1E\x1C\x00\x30\x01\x9F\x55\x02\x02\x00\xDD",
      "\x0F\xC4\x10\x00\x08\x16\x04\x0A\x10\x00\x00\x00\x00\x10\x0F\x17\x10\x17\x0F\x75",
      "\x07\xD0\x01\x00\x00\x00\x02\x00\x00\x37\x00\xEF",
      "\x10\xD0\x09\x00\x0F\x02\x02\x02\x03\x09\x09\x09\x0A\x00\x00\x00\x00\x0F\x00\x02\xC9",
      "\x0F\xD0\x19\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x04\x04\x00\x00\x00\xFF",
      "\x05\xD3\x02\x00\x62\x61\x61\x61\x61\x40",
      "\x01\xD3\x08\x00\x00\x24",
      "\x04\xD9\x00\x00\x62\x01\x62\xFA\x64",
      "\x01\xFF\xA4\x00\x80\xDC",
      "\x01\xFF\xA6\x00\x00\x5A",
      "\x01\xFF\xA9\x00\x00\x57",
      "\x01\xFF\xAB\x00\xFF\x56",
      "\x01\xFF\xAD\x00\x12\x41",
      "\x01\xFF\xAF\x00\x02\x4F",
      "\x01\xFF\xB2\x00\xE1\x6D",
      "\x01\xFF\xDC\x00\x00\x24",
      "\x00\x00\x00\x01\xFF"
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