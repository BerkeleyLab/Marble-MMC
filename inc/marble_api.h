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

#define WRITE_BIT(REG, BIT, VAL) ((REG & ~(1<<BIT)) | (VAL<<BIT))
#define TEST_BIT(REG, BIT) (REG & (1<<BIT))

/* Initialize uC and peripherals before main code can run. */
uint32_t marble_init(bool use_xtal);

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

typedef enum {
   M_FMC_STATUS_FMC1_PWR = 0,
   M_FMC_STATUS_FMC1_FUSE,
   M_FMC_STATUS_FMC2_PWR,
   M_FMC_STATUS_FMC2_FUSE,
} M_FMC_STATUS;

uint8_t marble_FMC_status(void);

void marble_PSU_pwr(bool on);

typedef enum {
   M_PWR_STATUS_PSU_EN = 0,
   M_PWR_STATUS_POE,
   M_PWR_STATUS_OTEMP
} M_PWR_STATUS;

uint8_t marble_PWR_status(void);

/****
* FPGA reset/init control
* Central to this whole job, may need refinement
****/
void reset_fpga(void);

/****
* SPI/SSP
****/
typedef void *SSP_PORT;
SSP_PORT SSP_FPGA;
SSP_PORT SSP_PMOD;

int marble_SSP_write16(SSP_PORT ssp, uint16_t *buffer, unsigned size);
int marble_SSP_read16(SSP_PORT ssp, uint16_t *buffer, unsigned size);
int marble_SSP_exch16(SSP_PORT ssp, uint16_t *tx_buf, uint16_t *rx_buf, unsigned size);

/************
* GPIO user-defined handlers
************/
void marble_GPIOint_handlers(void (*FPGA_DONE_handler)(void));

/************
* MGT Multiplexer
************/
#ifdef MARBLE_V2
void marble_MGTMUX_set(uint8_t mgt_num, bool on);

uint8_t marble_MGTMUX_status(void);
#endif

/****
* I2C
****/
#ifdef MARBLE_LPC1776
typedef int I2C_BUS;
#elif MARBLE_STM32F207
typedef void *I2C_BUS;
#else
#error Marble microcontroller API not defined!
#endif
I2C_BUS I2C_PM;
I2C_BUS I2C_IPMB;
I2C_BUS I2C_FPGA;

int marble_I2C_probe(I2C_BUS I2C_bus, uint8_t addr);
int marble_I2C_send(I2C_BUS I2C_bus, uint8_t addr, const uint8_t *data, int size);
int marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size);
int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size);
int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size);
int marble_I2C_cmdsend_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size);
int marble_I2C_cmdrecv_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size);

/************
* MDIO to PHY
************/
void marble_MDIO_write(uint16_t reg, uint32_t data);
uint32_t marble_MDIO_read(uint16_t reg);

/************
* System Timer and Stopwatch
************/
uint32_t marble_SYSTIMER_ms(uint32_t delay);
void marble_SYSTIMER_handler(void (*handler)(void));

void marble_SLEEP_ms(uint32_t delay);
void marble_SLEEP_us(uint32_t delay);

#endif /* _MARBLE_API_H_ */
