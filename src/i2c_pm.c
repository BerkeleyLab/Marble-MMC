#include "marble_api.h"
#define HAL_OK (0U)
#include <stdio.h>
#include <string.h>
#include "i2c_pm.h"

void I2C_PM_scan(void)
{
   printf("Scanning I2C_PM bus:\r\n");
   for (unsigned i = 1; i < 128; i++)
   {
      // Using 8-bit I2C addresses
      if (marble_I2C_probe(I2C_PM, (uint8_t) (i<<1)) != HAL_OK) {
         printf("."); // No ACK received at that address
      } else {
         printf("0x%02X", i << 1); // Received an ACK at that address
      }
   }
   printf("\r\n");
}

static int set_max6639_reg(int regno, int value)
{
   uint8_t addr = MAX6639;
   uint8_t i2c_dat[4];
   i2c_dat[0] = regno;
   i2c_dat[1] = value;
   int rc = marble_I2C_send(I2C_PM, addr, i2c_dat, 2);
   return rc;
}

static int get_max6639_reg(int regno, int *value)
{
   uint8_t i2c_dat[4];
   uint8_t addr = MAX6639;
   int rc = marble_I2C_cmdrecv(I2C_PM, addr, regno, i2c_dat, 1);
   if ((rc==0) && value) *value = i2c_dat[0];
   return rc;
}

static void set_fans(int speed[2])
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
   // ix is the MAX6639 register address
   for (unsigned ix=0; ix<64; ix++) {
      if (get_max6639_reg(ix, &value) != 0) {
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

static int LM75_readwrite(uint8_t dev, LM75_REG reg, int *data, bool rnw)
{
   uint8_t i2c_buf[3];
   int i2c_stat;
   short temp;

   // Select register
   i2c_buf[0] = reg;
   i2c_stat = marble_I2C_send(I2C_PM, dev, i2c_buf, 1);
   switch (reg) {
      case LM75_TEMP:
      case LM75_HYST:
      case LM75_OS:

         if (rnw) {
            i2c_stat = marble_I2C_recv(I2C_PM, dev, i2c_buf, 2);
            // Signed Q7.1, i.e. resolution of 0.5 deg
            temp = ((i2c_buf[0]<<8) | (i2c_buf[1])) >> 7;
            *data = temp;
         } else {
            i2c_buf[0] = reg;
            i2c_buf[1] = (*data >> 1) & 0xff; // MSB first
            i2c_buf[2] = (*data & 0x1) << 7;
            i2c_stat = marble_I2C_send(I2C_PM, dev, i2c_buf, 3);
         }
         break;
      case LM75_CFG:
         if (rnw) {
            i2c_stat = marble_I2C_recv(I2C_PM, dev, i2c_buf, 1);
            *data = (i2c_buf[0]) & 0xff;
         } else {
            i2c_buf[0] = reg;
            i2c_buf[1] = (*data) & 0xff;
            i2c_stat = marble_I2C_send(I2C_PM, dev, i2c_buf, 2);
         }
         break;
      default:
         break;
   }
   return i2c_stat;
}

int LM75_read(uint8_t dev, LM75_REG reg, int *data)
{
   return LM75_readwrite(dev, reg, data, true);
}

int LM75_write(uint8_t dev, LM75_REG reg, int data)
{
   return LM75_readwrite(dev, reg, &data, false);
}

void LM75_print(uint8_t dev)
{
   static const uint8_t rlist[LM75_MAX] = {LM75_TEMP, LM75_CFG, LM75_HYST, LM75_OS};
   int i;
   int recv;
   const char ok_str[] = "> LM75 %x: [%d]: %d\r\n";
   const char fail_str[] = "> LM75 %x: [%d]: FAIL\r\n";
   char p_buf[40];

   for (i = 0; i < LM75_MAX; i++) {
      if (LM75_read(dev, rlist[i], &recv) == 0) {
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
void I2C_PM_probe(void)
{
   int i;
   int i2c_stat=0;
   uint8_t i2c_dat[4];
   char p_buf[40];

   for (i = 0; i < I2C_NUM; i++) {
      switch (i2c_list[i]) {
         case LM75_0:
         case LM75_1:
            i2c_stat = marble_I2C_recv(I2C_PM, i2c_list[i], i2c_dat, 2);
            break;
         case MAX6639:
            i2c_stat = marble_I2C_recv(I2C_PM, i2c_list[i], i2c_dat, 1);
            break;
         case XRP7724:
            // Needs work
            i2c_dat[0] = 0x9;
            i2c_stat = marble_I2C_send(I2C_PM, i2c_list[i], i2c_dat, 1); // PWR_GET_STATUS
            i2c_stat = marble_I2C_recv(I2C_PM, i2c_list[i], i2c_dat, 2);
            break;
      }
      if (i2c_stat == 0) {
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
int xrp_set2(uint8_t dev, uint16_t addr, uint8_t data)
{
   int rc = marble_I2C_cmdsend_a2(I2C_PM, dev, addr, &data, 1);
   if (rc != HAL_OK) {
      printf("xrp_set2: failure writing r[%4.4x] <= %2.2x\n", addr, data);
      return rc;
   }
   marble_SLEEP_ms(10);
   uint8_t chk = 0x55;
   rc = marble_I2C_cmdrecv_a2(I2C_PM, dev, addr, &chk, 1);
   if (rc != HAL_OK || data != chk) {
      printf("xrp_set2: r[%4.4x] <= %2.2x   readback %2.2x rc %d\n", addr, data, chk, rc);
   }
   return rc;
}

int xrp_read2(uint8_t dev, uint16_t addr)
{
   uint8_t chk = 0x55;
   int rc = marble_I2C_cmdrecv_a2(I2C_PM, dev, addr, &chk, 1);
   if (rc != HAL_OK) {
      printf("xrp_set2: failure reading r[%4.4x]\n", addr);
      return 0;  // No good way to signal fault to caller
   }
   return chk;
}

void xrp_dump(uint8_t dev)
{
   // https://www.maxlinear.com/appnote/anp-38.pdf
   printf("XRP7724 dump [%2.2x]\n", dev);
   struct {int a; const char *n;} r_table[] = {
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

/* Returns fault (0) and in-regulation (1) status for XRP channels 1-4.
 * Invalid channel numbers will return fault (1).
 */
int xrp_ch_status(uint8_t dev, uint8_t chn)
{
   const uint8_t XRP_STS = 0x9;
   uint8_t i2c_dat[2];
   if (HAL_OK == marble_I2C_cmdrecv(I2C_PM, dev, XRP_STS, i2c_dat, 2)) {
      if ((i2c_dat[0] & ~i2c_dat[1]) & (1<<(chn-1)))
         return 1;
   }
   return 0;
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
   marble_SLEEP_ms(10);
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

#if 0
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
#endif

// Sending data to flash, see ANP-38
int xrp_push_low(uint8_t dev, uint16_t addr, const uint8_t data[], unsigned len)
{
   printf("xrp_push_low WIP 0x%4.4x\n", addr);
   int rc;
   if (len & 1) return 1;  // Odd length not allowed
   // FLASH_PROGRAM_ADDRESS (0x40)
   rc = xrp_reg_write_check(dev, 0x40, addr);
   if (rc == 0) {
      printf("can't set flash program address\n");
      return 1;
   }
   printf("start ");
   for (unsigned jx = 0; jx < len; jx+=2) {
      marble_SLEEP_ms(12);
      // Can argue that copying is stupid -- just pass data+jx to marble_I2C_cmdsend
      uint8_t i2c_dat[4];
      i2c_dat[0] = data[jx];
      i2c_dat[1] = data[jx+1];
      // FLASH_PROGRAM_DATA (0x41)
      rc = marble_I2C_cmdsend(I2C_PM, dev, 0x41, i2c_dat, 2);
      if (rc != HAL_OK) {
         printf(" Write Fault\n");
         return 1;
      }
      printf(".");
      marble_SLEEP_ms(12);

      uint8_t i2c_rd[4];
      // FLASH_PROGRAM_DATA_INC_ADDRESS (0x42); N.B.: Read-Write pointer is incremented here
      rc = marble_I2C_cmdrecv(I2C_PM, dev, 0x42, i2c_rd, 2);
      marble_SLEEP_ms(12);
      if (rc != HAL_OK || i2c_rd[0] != data[jx] || i2c_rd[1] != data[jx+1]) {
         printf("readback fail: rc=%d  0x%2.2x:0x%2.2x  0x%2.2x:0x%2.2x\n",
            rc, i2c_rd[0], data[jx], i2c_rd[1], data[jx+1]);
         return 1;
      }
   }
   printf(" Page Done\n");
   marble_SLEEP_ms(12);
   rc = xrp_reg_write_check(dev, 0x40, addr);
   if (rc == 0) {
      printf("can't set flash program address\n");
      return 1;
   }
   printf("Double-check\n");
   for (unsigned jx = 0; jx < len; jx+=2) {
      marble_SLEEP_ms(10);
      uint8_t i2c_dat[4];
      // FLASH_PROGRAM_DATA_INC_ADDRESS (0x42)
      rc = marble_I2C_cmdrecv(I2C_PM, dev, 0x42, i2c_dat, 2);
      if (rc != HAL_OK || i2c_dat[0] != data[jx] || i2c_dat[1] != data[jx+1]) {
         printf("readback fail: rc=%d  0x%2.2x:0x%2.2x  0x%2.2x:0x%2.2x\n",
            rc, i2c_dat[0], data[jx], i2c_dat[1], data[jx+1]);
         return 1;
      }
      printf(".");
   }
   printf(" OK\n");
   return 0;  // Success!
}

#if 0
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
      marble_SLEEP_ms(10);
   }
   printf("\n");
   xrp_print_reg(dev, 0x40);
   return 1;
}
#endif

// See Figures 3 and 4 of ANP-38
// Figure 3:  cmd is FLASH_PAGE_CLEAR (0x4E),  mode is 1,  dwell is 10
// Figure 4:  cmd is FLASH_PAGE_ERASE (0x4F),  mode is 5,  dwell is 50
static int xrp_process_flash(uint8_t dev, int page_no, int cmd, int mode, int dwell)
{
   int rc;
   uint8_t i2c_dat[4];
   for (unsigned retry=0; retry<5; retry++) {
      xrp_set2(dev, 0x8068, 0xff);  // YFLASHPGMDELAY
      // FLASH_INIT (0x4D)
      i2c_dat[0] = 0;  i2c_dat[1] = mode;
      rc = marble_I2C_cmdsend(I2C_PM, dev, 0x4D, i2c_dat, 2);
      if (rc != HAL_OK) return 1;
      marble_SLEEP_ms(50);
      int outer, status, busy;
      for (outer=0; outer < 10; outer++) {
         i2c_dat[0] = 0;  i2c_dat[1] = page_no;
         rc = marble_I2C_cmdsend(I2C_PM, dev, cmd, i2c_dat, 2);
         if (rc != HAL_OK) return 1;
         marble_SLEEP_ms(500);
         int poll;
         for (poll=0; poll < 20; poll++) {
            rc = marble_I2C_cmdrecv(I2C_PM, dev, cmd, i2c_dat, 2);
            if (rc != HAL_OK) return 1;
            status = i2c_dat[0];
            busy = i2c_dat[1];
            if (busy == 0) break;
            marble_SLEEP_ms(dwell);
         }
         printf("page_no %d: %d polls, status 0x%2.2x\n", page_no, poll, status);
         if (busy == 1) {
            printf("Timeout!\n");
            return 1;
         }
         if (status != 0xff) break;
      }
      if (status == 0xff) {
         printf("Status stuck at 0xFF!\n");
         return 1;
      }
      printf("Status OK\n");
      // final check
      int v = xrp_read2(dev, 0x8068);  // YFLASHPGMDELAY
      if (v == 0xff) {
         printf("Page %d complete\n", page_no);
         return 0;  // Success
      }
      printf("YFLASHPGMDELAY = 0x%2.2x after programming; Fault %d!\n", v, retry);
   }
   return 1;  // "Abort - Erasing the Flash has failed"
}

static int xrp_program_page(uint8_t dev, unsigned page_no, uint8_t data[], unsigned len)
{
   printf("FLASH_PAGE_CLEAR %u\n", page_no);
   if (xrp_process_flash(dev, page_no, 0x4E, 1, 10)) return 1;
   printf("FLASH_PAGE_ERASE %u\n", page_no);
   if (xrp_process_flash(dev, page_no, 0x4F, 5, 50)) return 1;
   //
   // On to Figure 5: Program Flash Image
   xrp_set2(dev, 0x8068, 0xff);  // YFLASHPGMDELAY
   marble_SLEEP_ms(12);
   int v = xrp_read2(dev, 0x8068);  // YFLASHPGMDELAY
   if (v != 0xff) {
      printf("YFLASHPGMDELAY = 0x%2.2x before programming; Fault!\n", v);
      return 1;
   }
   marble_SLEEP_ms(12);
   int rc = xrp_reg_write(dev, 0x4D, 1);  // FLASH_INIT (0x4D), mode=1
   if (rc != HAL_OK) return 1;
   marble_SLEEP_ms(10);
   if (xrp_push_low(dev, page_no*64, data, len)) return 1;
   v = xrp_read2(dev, 0x8068);  // YFLASHPGMDELAY
   if (v != 0xff) {
      printf("YFLASHPGMDELAY = 0x%2.2x after programming; Fault!\n", v);
      return 1;
   }
   return 0;  // Success?
}

// Temporarily abandon hex record concept
void xrp_flash(uint8_t dev)
{

#ifdef MARBLEM_V1
   // Data originally based on python hex2c.py < MarbleMini.hex
   // Pure copy of 7 x 64-byte pages, spannning addresses 0x0000 to 0x01bf
   uint8_t dd[] = {
      "\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x27\xE1\x28\x33\x0D"
      "\x00\x00\x03\x00\x56\x00\x12\x02\x69\x97\x4A\x26\x28\x3C\x20\x01"
      "\x01\x01\x01\x01\x01\x1E\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB"
      "\x00\x12\x6C\x1E\x1E\x0C\x2E\x69\x98\x0A\x4D\x20\x00\x00\x08\xC4"
      "\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x13\xE1\x14\x33\x0D"
      "\x00\x00\x03\x00\x50\x00\x11\x02\x65\x9B\x4E\x24\x26\x3C\x14\x08"
      "\x08\x08\x08\x08\x08\x1D\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB"
      "\x00\x3E\x64\x7B\x7B\x1D\xF2\x48\x05\x1A\x26\x20\x00\x11\x18\x02"
      "\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x09\xE1\x14\x33\x0D"
      "\x00\x00\x04\x00\x6A\x00\x16\x02\x45\xBB\x5F\x2D\x31\x3C\x1A\x02"
      "\x02\x02\x02\x02\x02\x1C\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB"
      "\x00\x35\x42\x7B\x7B\x16\x48\x57\xE5\x12\x07\x20\x00\x21\x18\x07"
      "\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x27\xE1\x28\x33\x0D"
      "\x00\x00\x03\x00\x40\x00\x0D\x02\x51\xAF\x58\x3F\x45\x3C\x10\x40"
      "\x40\x40\x40\x40\x40\x1E\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB"
      "\x00\x43\x50\x1E\x1E\x19\x5C\x4F\x6F\x17\x40\x29\x77\x01\x18\xFC"
      "\x05\x0F\xFF\x4C\x4F\x4C\x1E\x1C\x00\x30\x01\x9F\x55\x04\x04\x00"
      "\x10\x16\x04\x0A\x10\x00\x00\x00\x00\x20\x0F\x17\x10\x17\x0F\x6C"
      "\x64\x42\x50\x30\x18\x0C\x30\x00\x00\x00\x00\x00\x00\x00\x00\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x0F\x00\x00\x00\x71"
      "\x00\x00\x00\x02\x00\x00\x37\x00\x00\x0F\x04\x02\x06\x02\x0A\x09"
      "\x0B\x09\x00\x00\x00\x00\x0F\x00\x02\x00\x00\x00\x00\x01\x00\x00"
      "\x00\x00\x00\x04\x04\x00\x00\x00\x61\x62\x61\x61\x61\x00\x00\x62"
      "\x01\x62\xFA\x00\x80\x00\x00\xFF\x12\x02\xE2\x00\x1D\x00\x00\xF3"
      "\x00\x02\x50\x00\x00\xFF\xFF\xFA\x06\xFA\x04\xFA\x02\x96\x01\x00"
      "\x00\x00\x00\x00\x00\x00\x02\x00\x04\x00\x07\x00\x00\x00\x00\x64"
      "\x21\x64\x64\x64\x21\x64\x64\x64\x21\x64\x64\x64\x21\x64\x64\x0A"
      "\x20\x0A\x05\x19\x00\xFF\x00\x00\x00\xFF\xFF\x00\x04\xFF\xFF\xCF"
   };
#elif MARBLE_V2
   // Data originally based on python hex2c.py < Marble.hex
   // Pure copy of 7 x 64-byte pages, spannning addresses 0x0000 to 0x01bf
   uint8_t dd[] = {
      "\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x13\xE1\x14\x33\x0D"
      "\x00\x00\x03\x00\x50\x00\x11\x02\x15\xEB\x4E\x2A\x2C\x3C\x14\x01"
      "\x01\x01\x01\x01\x01\x1D\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB"
      "\x00\x58\x64\x3D\x3D\x14\x73\x59\x5D\x12\x3E\x21\x7F\x12\x28\x1E"
      "\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x1D\xE1\x14\x33\x0D"
      "\x00\x00\x04\x00\x6A\x00\x16\x02\x0D\xF3\x5F\x36\x3B\x3C\x0D\x04"
      "\x04\x04\x04\x04\x04\x1C\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB"
      "\x00\x90\x42\x7B\x7B\x1F\xD3\x44\x61\x1B\xE9\x20\x00\x22\x28\x62"
      "\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x27\xE1\x28\x33\x0D"
      "\x00\x00\x03\x00\x40\x00\x0D\x02\x11\xEF\x58\x4D\x56\x3C\x10\x80"
      "\x80\x80\x80\x80\x80\x1D\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB"
      "\x00\x44\x50\x1E\x1E\x17\xD6\x52\x56\x15\xDD\x2A\x76\x01\x18\x76"
      "\xFF\xC5\x00\x0A\x03\x41\x00\xFA\x02\xDA\x20\x13\xE1\x28\x33\x0D"
      "\x00\x00\x05\x00\x73\x00\x18\x02\x91\x6F\x5C\x2F\x32\x3C\x16\x40"
      "\x40\x40\x40\x40\x40\x1D\x1E\xCE\x04\xB0\x0D\x00\x40\x00\x40\xCB"
      "\x00\x29\x48\x3D\x3D\x15\x07\x59\x26\x11\xEE\x20\x00\x11\x18\x73"
      "\x05\x00\x00\x4C\x4F\x4C\x1E\x1C\x00\x30\x01\x9F\x55\x02\x02\x00"
      "\x08\x16\x04\x0A\x10\x00\x00\x00\x00\x10\x0F\x17\x10\x17\x0F\x64"
      "\x42\x50\x48\x18\x0C\x30\x18\x00\x00\x00\x00\x00\x00\x00\x00\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x0F\x00\x00\x00\xE1"
      "\x00\x00\x00\x02\x00\x00\x37\x00\x00\x0F\x02\x02\x02\x03\x09\x09"
      "\x09\x0A\x00\x00\x00\x00\x0F\x00\x02\x00\x00\x00\x00\x01\x00\x00"
      "\x00\x00\x00\x04\x04\x00\x00\x00\x62\x61\x61\x61\x61\x00\x00\x62"
      "\x01\x62\xFA\x00\x80\x00\x00\xFF\x12\x02\xE1\x00\x1E\x00\x00\x34"
      "\x00\x02\x50\x00\x00\xFF\xFF\x32\x04\x32\x06\x32\x00\x00\x03\x00"
      "\x00\x00\x00\x00\x00\x00\x02\x00\x04\x00\x07\x00\x00\x00\x00\x64"
      "\x21\x64\x64\x64\x20\x64\x64\x64\x21\x64\x64\x64\x22\x64\x64\x0A"
      "\x20\x0A\x05\x19\xFF\x00\x00\x00\x00\xFF\xFF\x00\x04\xFF\xFF\x12"
   };
#endif

   const unsigned dd_size = sizeof(dd) / sizeof(dd[0]);
   const unsigned pages = 7;
   if (dd_size != pages*64+1) {  // account for trailing "\0"
      printf("bad setup, dd_size=%u, pages=%u\n", dd_size, pages);
      return;
   }
   printf("XRP7724 flash (WIP)\n");
   for (unsigned page_no=0; page_no < pages; page_no++) {
      if (xrp_program_page(dev, page_no, dd+page_no*64, 64)) return;
   }
   printf("flash programming complete!?\n");
}

void xrp_go(uint8_t dev)
{
   printf("XRP7724 go [%2.2x]\n", dev);
   // check that PWR_CHIP_READY (0E) reads back 0
   xrp_reg_write_check(dev, 0x0E, 0x0000);
   xrp_set2(dev, 0x8000, 0x93);  // write random byte to 0x8000
   int rc = xrp_program_static(dev);
   printf("xrp_program_static rc = %d\n", rc);
   if (rc) return;
   // read random byte from 0x8000, should match
   int v = xrp_read2(dev, 0x8000);
   if (v != 0x93) {
      printf("write corrupted (0x93 != 0x%2.2x)\n", v);
      return;
   }
   printf("almost done\n");
   if (1) {
      xrp_reg_write_check(dev, 0x0E, 0x0001);  // Set the XRP7724 to operate mode
   }
}

void xrp_hex_in(uint8_t dev)
{
   printf("XRP7724 hex in (WIP) [%2.2x]\n", dev);
   printf("Do not use for flash programming! (yet)\n");
   // check that PWR_CHIP_READY (0E) reads back 0
   xrp_reg_write_check(dev, 0x0E, 0x0000);
   xrp_set2(dev, 0x8000, 0x95);  // write random byte to 0x8000
   int rc = xrp_file(dev);  // XXX test result
   printf("xrp_file rc = %d\n", rc);
   if (rc) return;
   // read random byte from 0x8000, should match
   int v = xrp_read2(dev, 0x8000);
   if (v != 0x95) {
      printf("write corrupted (0x95 != 0x%2.2x)\n", v);
      return;
   }
   printf("almost done\n");
   if (1) {
      xrp_reg_write_check(dev, 0x0E, 0x0001);  // Set the XRP7724 to operate mode
   }
}

// Didn't work when tested; why?
#if 0
void xrp_halt(uint8_t dev)
{
   printf("XRP7724 halt [%2.2x]\n", dev);
   xrp_reg_write(dev, 0x1E, 0x0000);  // turn off Ch1
   xrp_reg_write(dev, 0x1E, 0x0100);  // turn off Ch2
   xrp_reg_write(dev, 0x1E, 0x0200);  // turn off Ch3
   xrp_reg_write(dev, 0x1E, 0x0300);  // turn off Ch4
   printf("DONE\n");
}
#endif
