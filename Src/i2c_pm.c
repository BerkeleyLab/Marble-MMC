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

