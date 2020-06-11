#include "marble_api.h"
#include "stdio.h"
#include "string.h"
#include "i2c_pm.h"

/************
* LM75 Register interface
************/

int LM75_readwrite(uint8_t dev, LM75_REG reg, int *data, bool rnw) {
   uint8_t i2c_buf[2];
   int i2c_cnt=0;
   int i2c_expect=1;
   short temp;

   // Select register
   i2c_buf[0] = reg;
   i2c_cnt += marble_I2C_send(I2C_PM, dev, i2c_buf, 1);
   switch (reg) {
      case LM75_TEMP:
      case LM75_HYST:
      case LM75_OS:
         i2c_expect += 2;

         if (rnw) {
            i2c_cnt += marble_I2C_recv(I2C_PM, dev, i2c_buf, 2);
            // Signed Q7.1, i.e. resolution of 0.5 deg
            temp = ((i2c_buf[0]<<8) | (i2c_buf[1])) >> 7;
            *data = temp;
         } else {
            i2c_buf[0] = (*data >> 1) & 0xff; // MSB first
            i2c_buf[1] = (*data & 0x1) << 7;
            i2c_cnt += marble_I2C_send(I2C_PM, dev, i2c_buf, 2);
         }
         break;
      case LM75_CFG:
         i2c_expect += 1;

         if (rnw) {
            i2c_cnt += marble_I2C_recv(I2C_PM, dev, i2c_buf, 1);
            *data = (i2c_buf[0]) & 0xff;
         } else {
            i2c_buf[0] = (*data) & 0xff;
            i2c_cnt += marble_I2C_send(I2C_PM, dev, i2c_buf, 1);
         }
         break;
   }
   return (i2c_expect == i2c_cnt);
}

void LM75_print(uint8_t dev) {
   static const  uint8_t rlist[LM75_MAX] = {LM75_TEMP, LM75_CFG, LM75_HYST, LM75_OS};
   int i;
   int recv;
   const char ok_str[] = "> LM75 %x: [%d]: %d\r\n";
   const char fail_str[] = "> LM75 %x: [%d]: FAIL\r\n";
   char p_buf[40];

   for (i = 0; i < LM75_MAX; i++) {
      if (LM75_readwrite(dev, rlist[i], &recv, true)) {
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
   int i2c_cnt;
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
      if (i2c_cnt == i2c_expect) {
         snprintf(p_buf, 40, i2c_ok, i2c_list[i]);
      } else {
         snprintf(p_buf, 40, i2c_nok, i2c_list[i]);
      }
      marble_UART_send(p_buf, strlen(p_buf));
      snprintf(p_buf, 40, i2c_ret, *i2c_dat);
      marble_UART_send(p_buf, strlen(p_buf));
   }
}


