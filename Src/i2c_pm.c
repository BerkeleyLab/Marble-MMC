/*
 * i2c_pm.c
 *
 *  Created on: 11.09.2020
 *      Author: micha
 */

#include "marble_api.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include "i2c_pm.h"
//#include "stm32f2xx_hal_def.h"



int set_max6639_reg(int regno, int value)
{
   uint8_t addr = MAX6639;
   uint8_t i2c_dat[4];
   i2c_dat[0] = regno;
   i2c_dat[1] = value;
   //int rc = HAL_I2C_Master_Transmit(&hi2c1, addr, i2c_dat, 2, HAL_MAX_DELAY);
   int rc = marble_I2C_send(I2C_PM, addr, i2c_dat, 2);
   return rc ;
}

int get_max6639_reg(int regno, int *value)
{
   uint8_t i2c_dat[4];
   uint8_t addr = MAX6639;
   int rc = marble_I2C_cmdrecv(I2C_PM, addr, regno, i2c_dat, 1) == 1;
   if (rc == HAL_OK) *value = i2c_dat[0];
   return rc;
}

void set_fans(int speed[2])
{
    set_max6639_reg(0x11, 2);  // Fan 1 PWM sign = 1
    set_max6639_reg(0x15, 2);  // Fan 2 PWM sign = 1
    set_max6639_reg(0x26, speed[0]);
    set_max6639_reg(0x27, speed[1]);
}

void print_max6639(void)
{
   char p_buf[40];
   int value;
   uint8_t addr = MAX6639;
   // ix is the MAX6639 register address
   for (unsigned ix=0; ix<64; ix++) {
      if (get_max6639_reg(ix, &value) != HAL_OK) {
          marble_UART_send("I2C fault!\r\n", 11);
          break;
      }
      snprintf(p_buf, 40, "  reg[%2.2x] = %2.2x", ix, value);
      marble_UART_send(p_buf, strlen(p_buf));
      if ((ix&0x3) == 0x3) marble_UART_send("\r\n", 2);
   }
   if (0) {
      int fan_speed[2];
      fan_speed[0] = 200;
      fan_speed[1] = 150;
      set_fans(fan_speed);
   }
}


/************
* LM75 Register interface
************/

static int LM75_readwrite(uint8_t dev, LM75_REG reg, int *data, bool rnw) {
   uint8_t i2c_buf[3];
   int i2c_stat=0;
   int i2c_expect=1;
   short temp;

   // Select register
   i2c_buf[0] = reg;
   i2c_stat = marble_I2C_send(I2C_PM, dev, i2c_buf, 1);
   switch (reg) {
      case LM75_TEMP:
      case LM75_HYST:
      case LM75_OS:

         if (rnw) {
            i2c_expect += 2;
            i2c_stat = marble_I2C_recv(I2C_PM, dev, i2c_buf, 2);
            // Signed Q7.1, i.e. resolution of 0.5 deg
            temp = ((i2c_buf[0]<<8) | (i2c_buf[1])) >> 7;
            *data = temp;
         } else {
            i2c_expect += 3;
            i2c_buf[0] = reg;
            i2c_buf[1] = (*data >> 1) & 0xff; // MSB first
            i2c_buf[2] = (*data & 0x1) << 7;
            i2c_stat = marble_I2C_send(I2C_PM, dev, i2c_buf, 3);
         }
         break;
      case LM75_CFG:
         if (rnw) {
            i2c_expect += 1;
            i2c_stat = marble_I2C_recv(I2C_PM, dev, i2c_buf, 1);
            *data = (i2c_buf[0]) & 0xff;
         } else {
            i2c_expect += 2;
            i2c_buf[0] = reg;
            i2c_buf[1] = (*data) & 0xff;
            i2c_stat = marble_I2C_send(I2C_PM, dev, i2c_buf, 2);
         }
         break;
   }
   return i2c_stat;
}

int LM75_read(uint8_t dev, LM75_REG reg, int *data) {
   return LM75_readwrite(dev, reg, data, true);
}

int LM75_write(uint8_t dev, LM75_REG reg, int data) {
   return LM75_readwrite(dev, reg, &data, false);
}

void LM75_print(uint8_t dev) {
   static const  uint8_t rlist[LM75_MAX] = {LM75_TEMP, LM75_CFG, LM75_HYST, LM75_OS};
   int i;
   int recv;
   const char ok_str[] = "> LM75 %x: [%d]: %d\r\n";
   const char fail_str[] = "> LM75 %x: [%d]: FAIL\r\n";
   char p_buf[40];

   for (i = 0; i < LM75_MAX; i++) {
      if (LM75_read(dev, rlist[i], &recv) == HAL_OK) {
         snprintf(p_buf, 40, ok_str, dev, rlist[i], recv);
      } else {
         snprintf(p_buf, 40, fail_str, dev, rlist[i]);
      }
      marble_UART_send(p_buf, strlen(p_buf));
   }
}

static const uint8_t i2c_list[I2C_NUM] = {LM75_0, LM75_1, MAX6639, XRP7724};

const char i2c_ok[] = "> Found I2C slave: %x\r\n";
const char i2c_nok[] = "> I2C slave not found: %x\r\n";
const char i2c_ret[] = "> %x\r\n";

/* Perform basic sanity check and print result to UART */
void I2C_PM_probe(void) {
   int i;
   int i2c_expect;
   int i2c_cnt=0;
   uint8_t i2c_dat[4];
   char p_buf[40];

   for (i = 0; i < I2C_NUM; i++) {
      switch (i2c_list[i]) {
         case LM75_0:
         case LM75_1:
            i2c_expect = 2;
            i2c_cnt = marble_I2C_recv(I2C_PM, i2c_list[i], i2c_dat, i2c_expect);
            break;
         case MAX6639:
            i2c_expect = 1;
            i2c_cnt = marble_I2C_recv(I2C_PM, i2c_list[i], i2c_dat, i2c_expect);
            break;
         case XRP7724:
            i2c_expect = 3;
            i2c_dat[0] = 0x9;
            i2c_cnt = marble_I2C_send(I2C_PM, i2c_list[i], i2c_dat, 1); // PWR_GET_STATUS
            i2c_cnt += marble_I2C_recv(I2C_PM, i2c_list[i], i2c_dat, 2);
            break;
      }
      if (i2c_cnt == HAL_OK) {
         snprintf(p_buf, 40, i2c_ok, i2c_list[i]);
      } else {
         snprintf(p_buf, 40, i2c_nok, i2c_list[i]);
      }
      marble_UART_send(p_buf, strlen(p_buf));
      snprintf(p_buf, 40, i2c_ret, *i2c_dat);
      marble_UART_send(p_buf, strlen(p_buf));
   }
}

/* XPR7724 is special
 * Seems that one-byte Std Commands documented in ANP-38 apply to
 * commands < 0x4F, but there are also two-byte commands (called addresses)
 * starting with 0x8000, discussed in ANP-39, and captured in hex files
 * used for runtime programming.  Even ANP-38 mentions one of these
 * in passing, YFLASHPGMDELAY (address = 0x8068).
 * Thus there are two types of I2C transactions:
 *  1-byte address  2-byte data, use marble_I2C_cmd{send,recv}().
 *  2-byte address  1-byte data, use marble_I2C_cmd{send,recv}_a2().
 * The former are (mostly) used in Flash programming, the latter are
 * (mostly) used in runtime (RAM) programming.
 * One special case not documented elsewhere: Exar's UnivPMIC project
 * instructs us not to write to register 0xD022.
 */
static int xrp_set2(uint8_t dev, uint16_t addr, uint8_t data)
{
   int rc = marble_I2C_cmdsend_a2(I2C_PM, dev, addr, &data, 1);
   if (rc != HAL_OK) {
      printf("xrp_set2: failure writing r[%4.4x] <= %2.2x\n", addr, data);
      return rc;
   }
   HAL_Delay(10);
   uint8_t chk = 0x55;
   rc = marble_I2C_cmdrecv_a2(I2C_PM, dev, addr, &chk, 1);
   if (rc != HAL_OK || data != chk) {
      printf("xrp_set2: r[%4.4x] <= %2.2x   readback %2.2x rc %d\n", addr, data, chk, rc);
   }
   return rc;
}

static int xrp_read2(uint8_t dev, uint16_t addr)
{
   uint8_t chk = 0x55;
   int rc = marble_I2C_cmdrecv_a2(I2C_PM, dev, addr, &chk, 1);
   if (rc != HAL_OK) {
      printf("xrp_set2: failure reading r[%4.4x]\n", addr);
      return 0;  // No good way to signal fault to caller
   }
   return chk;
}

void xrp_dump(uint8_t dev) {
   // https://www.maxlinear.com/appnote/anp-38.pdf
   printf("XRP7724 dump [%2.2x]\n", dev);
   struct {int a; char *n;} r_table[] = {
      {0x02, "HOST_STS"},
      {0x05, "FAULT_STATUS"},
      {0x09, "STATUS"},
      {0x0e, "CHIP_READY"},
      {0x10, "VOLTAGE_CH1"},
      {0x11, "VOLTAGE_CH2"},
      {0x12, "VOLTAGE_CH3"},
      {0x13, "VOLTAGE_CH4"},
      {0x14, "VOLTAGE_VIN"},
      {0x15, "TEMP_VTJ"},
      {0x16, "CURRENT_CH1"},
      {0x17, "CURRENT_CH2"},
      {0x18, "CURRENT_CH3"},
      {0x19, "CURRENT_CH4"},
      {0x30, "READ_GPIO"},
      {0x40, "FLASH_PROGRAM_ADDRESS"},
      {0x4E, "FLASH_PAGE_CLEAR"},
      {0x4F, "FLASH_PAGE_ERASE"}};

   const unsigned tlen = sizeof(r_table)/sizeof(r_table[0]);
   for (unsigned ix=0; ix<tlen; ix++) {
      uint8_t i2c_dat[4];
      int regno = r_table[ix].a;
      int rc = marble_I2C_cmdrecv(I2C_PM, dev, regno, i2c_dat, 2);
      if (rc == HAL_OK) {
          unsigned value = (((unsigned) i2c_dat[0]) << 8) | i2c_dat[1];
          printf("r[%2.2x] = 0x%4.4x = %5d   (%s)\n", regno, value, value, r_table[ix].n);
      } else {
          printf("r[%2.2x]    unread          (%s)\n", regno, r_table[ix].n);
      }
   }
}

static int xrp_reg_write(uint8_t dev, uint8_t regno, uint16_t d)
{
   uint8_t i2c_dat[4];
   i2c_dat[0] = (d>>8) & 0xff;
   i2c_dat[1] = d & 0xff;
   int rc = marble_I2C_cmdsend(I2C_PM, dev, regno, i2c_dat, 2);
   if (rc != HAL_OK) {
      printf("r[%2.2x] wrote 0x%4.4x,  rc = %d (want %d)\n", regno, d, rc, HAL_OK);
   }
   return rc;
}

static int xrp_reg_write_check(uint8_t dev, uint8_t regno, uint16_t d)
{
   xrp_reg_write(dev, regno, d);
   HAL_Delay(10);
   uint8_t i2c_dat[4];
   i2c_dat[0] = 0xde;
   i2c_dat[1] = 0xad;
   int rc = marble_I2C_cmdrecv(I2C_PM, dev, regno, i2c_dat, 2);
   if (rc == HAL_OK) {
      unsigned value = (((unsigned) i2c_dat[0]) << 8) | i2c_dat[1];
      printf("r[%2.2x] = 0x%4.4x = %5d  (hope for 0x%4.4x)\n", regno, value, value, d);
      return value == d;
   } else {
      printf("r[%2.2x]    unread\n", regno);
   }
   return 0;
}

static void xrp_print_reg(uint8_t dev, uint8_t regno)
{
   uint8_t i2c_dat[4];
   i2c_dat[0] = 0xde;
   i2c_dat[1] = 0xad;
   int rc = marble_I2C_cmdrecv(I2C_PM, dev, regno, i2c_dat, 2);
   if (rc == HAL_OK) {
      unsigned value = (((unsigned) i2c_dat[0]) << 8) | i2c_dat[1];
      printf("r[%2.2x] = 0x%4.4x\n", regno, value);
   } else {
      printf("r[%2.2x]    unread\n", regno);
   }
}

static int xrp_push(uint8_t dev, uint16_t d[], unsigned len)
{
   for (unsigned jx = 0; jx < len; jx++) {
      uint8_t i2c_dat[4];
      i2c_dat[0] = (d[jx]>>8) & 0xff;
      i2c_dat[1] = d[jx] & 0xff;
      // FLASH_PROGRAM_DATA_INC_ADDRESS (0x42)
      int rc = marble_I2C_cmdsend(I2C_PM, dev, 0x42, i2c_dat, 2);
      if (rc != HAL_OK) {
         printf(" Write Fault\n");
         return 0;
      }
      printf(".");
      HAL_Delay(50);
   }
   printf("\n");
   xrp_print_reg(dev, 0x40);
   return 1;
}

static int xrp_pull(uint8_t dev, unsigned len)
{
   for (unsigned jx = 0; jx < len; jx++) {
      uint8_t i2c_dat[4];
      int rc = marble_I2C_cmdrecv(I2C_PM, dev, 0x42, i2c_dat, 2);
      if (rc != HAL_OK) {
         printf(" Read Fault\n");
         return 0;
      }
      unsigned value = (((unsigned) i2c_dat[0]) << 8) | i2c_dat[1];
      printf(" %4.4x", value);
      HAL_Delay(10);
   }
   printf("\n");
   xrp_print_reg(dev, 0x40);
   return 1;
}

static int xrp_srecord(uint8_t dev, uint8_t data[])
{
   unsigned len = data[0];
   uint16_t addr = (((unsigned) data[1]) << 8) | data[2];
   unsigned rtype = data[3];  // record type
   if (rtype != 0) {
      printf("rtype %d\n", rtype);
      return 1;
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
   for (unsigned jx=4; jx<(len+4); jx++) {
      unsigned addr1 = addr + jx - 4;
      if (addr1 == 0xD022) continue;
      xrp_set2(dev, addr1, data[jx]);
   }
   // Double-check
   for (unsigned jx=4; jx<(len+4); jx++) {
      printf(".");
      int v = xrp_read2(dev, addr + jx - 4);
      if (v != data[jx]) {
          printf("fault %2.2x != %2.2x\n", v, data[jx]);
      }
   }
   printf(" OK\n");
   return 0;
}

// Data from python hex2c.py < Marble_runtime_1A.hex
// Temporary only (?); it would be much better to read hex records from UART
static int xrp_program_1A(uint8_t dev) {
   // each element of dd represents a hex record,
   // https://en.wikipedia.org/wiki/Intel_HEX
   // including length, address, record type, data, and checksum,
   // but without start code.
   uint8_t *dd[] = {
      "\x10\x80\x72\x00\x00\x02\x50\x00\x00\xFF\xFF\x32\x04\x32\x06\x32\x00\x00\x03\x00\x0B",
      "\x10\x80\x82\x00\x00\x00\x00\x00\x00\x00\x02\x00\x04\x00\x07\x00\x00\x00\x00\x64\x7D",
      "\x10\x80\x92\x00\x21\x64\x64\x64\x20\x64\x64\x64\x21\x64\x64\x64\x22\x64\x64\x0A\x04",
      "\x0B\x80\xA2\x00\x20\x0A\x05\x19\xFF\x00\x00\x00\x00\xFF\xFF\x8E",
      "\x10\x80\xAE\x00\x04\xFF\xFF\x64\x42\x50\x48\x18\x0C\x30\x18\x00\x00\x00\x00\x00\x16",
      "\x10\x80\xBE\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x0F\xAF",
      "\x04\x80\xCE\x00\x00\x00\x00\x1E\x90",
      "\x10\xC0\x00\x00\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x13\xE1\x14\x33\x0D\xE0",
      "\x10\xC0\x10\x00\x00\x00\x03\x00\x50\x00\x11\x02\x15\xEB\x4E\x20\x22\x3C\x14\x01\xD9",
      "\x10\xC0\x20\x00\x01\x01\x01\x01\x01\x1D\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB\xF6",
      "\x0F\xC0\x30\x00\x00\x58\x64\x3D\x3D\x14\x72\x59\x5F\x12\x3D\x21\x7F\x12\x28\x64",
      "\x10\xC1\x00\x00\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x1D\xE1\x14\x33\x0D\xD5",
      "\x10\xC1\x10\x00\x00\x00\x04\x00\x6A\x00\x16\x02\x0D\xF3\x5F\x20\x22\x3C\x0D\x04\xAB",
      "\x10\xC1\x20\x00\x04\x04\x04\x04\x04\x1C\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB\xE7",
      "\x0F\xC1\x30\x00\x00\x90\x42\x7B\x7B\x1F\xCF\x44\x68\x1B\xE6\x20\x00\x22\x28\x33",
      "\x10\xC2\x00\x00\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x27\xE1\x28\x33\x0D\xB6",
      "\x10\xC2\x10\x00\x00\x00\x03\x00\x40\x00\x0D\x02\x11\xEF\x58\x21\x22\x3C\x10\x80\x65",
      "\x10\xC2\x20\x00\x80\x80\x80\x80\x80\x1D\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB\x79",
      "\x0F\xC2\x30\x00\x00\x44\x50\x1E\x1E\x17\xC3\x52\x7A\x15\xCC\x2A\x76\x01\x18\xEF",
      "\x10\xC3\x00\x00\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x13\xE1\x28\x33\x0D\xC9",
      "\x10\xC3\x10\x00\x00\x00\x05\x00\x73\x00\x18\x02\x91\x6F\x5C\x21\x22\x3C\x16\x40\x5A",
      "\x10\xC3\x20\x00\x40\x40\x40\x40\x40\x1D\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB\xB8",
      "\x0F\xC3\x30\x00\x00\x29\x48\x3D\x3D\x15\x04\x59\x2C\x11\xEC\x20\x00\x11\x18\x2F",
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
   int rc;
   for (unsigned jx=0; jx<dd_size; jx++) {
      rc = xrp_srecord(dev, dd[jx]);
      if (rc) break;
   }
   return rc;
}

void xrp_flash(uint8_t dev) {
   printf("XRP7724 flash (software not yet written)\n");
   // if (xrp_reg_write_check(dev, 0x40, 0x8072)) {
   // printf("OK, trying to write\n");
   xrp_set2(dev, 0x8068, 0xff);  // YFLASHPGMDELAY
   xrp_reg_write_check(dev, 0x40, 0xC010);
}

void xrp_go(uint8_t dev) {
   printf("XRP7724 go (WIP) [%2.2x]\n", dev);
   // check that PWR_CHIP_READY (0E) reads back 0
   xrp_reg_write_check(dev, 0x0E, 0x0000);
   xrp_set2(dev, 0x8000, 0x93);  // write random byte to 0x8000
   int rc = xrp_program_1A(dev);
   // read random byte from 0x8000, should match
   int v = xrp_read2(dev, 0x8000);
   if (v != 0x93) {
      printf("write corrupted (0x93 != 0x%2.2x)\n", v);
      return;
   }
   printf("almost done\n");
   xrp_reg_write_check(dev, 0x0E, 0x0001);  // Set the XRP7724 to operate mode
}

void xrp_halt(uint8_t dev) {
   printf("XRP7724 halt [%2.2x]\n", dev);
   xrp_reg_write(dev, 0x1E, 0x0000);  // turn off Ch1
   xrp_reg_write(dev, 0x1E, 0x0100);  // turn off Ch2
   xrp_reg_write(dev, 0x1E, 0x0200);  // turn off Ch3
   xrp_reg_write(dev, 0x1E, 0x0300);  // turn off Ch4
   printf("DONE\n");
}
