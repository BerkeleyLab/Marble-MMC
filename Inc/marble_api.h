/*
 * MARBLE_API.h
 * API consisting of wrapper functions providing access to low-level features
 * of Marble Mini (v1 and v2) and Marble.
 * The functions defined herein are necessarily uController-agnostic.
 */

#ifndef _MARBLE_API_H_
#define _MARBLE_API_H_

#include <stdbool.h>
#include <stdint.h>

/* Initialize uC and peripherals before main code can run. */
void marble_init(bool use_xtal);

/****
* UART
****/
void marble_UART_init(void);

int marble_UART_send(const char *str, int size);

int marble_UART_recv(char *str, int size);

/****
* LED
****/
void marble_LED_set(uint8_t led_num, bool on);

bool marble_LED_get(uint8_t led_num);

void marble_LED_toggle(uint8_t led_num);

/****
* Push-Buttons
****/
bool marble_SW_get(void);

/****
* FPGA int
****/
bool marble_FPGAint_get(void);

/****
* FMC & PSU
****/
void marble_FMC_pwr(bool on);

void marble_PSU_pwr(bool on);

/****
* FPGA reset/init control
* Central to this whole job, may need refinement
****/
void reset_fpga(void);

/****
* SPI/SSP
****/
typedef enum {
   SSP_FPGA,
   SSP_PMOD
} SSP_PORT;

int marble_SSP_write(SSP_PORT ssp, uint8_t *buffer, int size);
int marble_SSP_read(SSP_PORT ssp, uint8_t *buffer, int size);

/****
* I2C
****/
typedef enum {
   I2C_PM,
   I2C_IPMB,
   I2C_FPGA,
} I2C_BUS;

int marble_I2C_send(I2C_BUS I2C_bus, uint8_t addr, const uint8_t *data, int size);
marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size);
int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size);
int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size);

/************
* MDIO to PHY
************/
void marble_MDIO_write(uint16_t reg, uint32_t data);
uint32_t marble_MDIO_read(uint8_t reg);

#endif /* _MARBLE_API_H_ */
