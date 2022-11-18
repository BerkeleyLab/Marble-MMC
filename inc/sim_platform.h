/*
 * File: sim_platform.h
 * Desc: Simulated hardware API to run application on host
 */

#ifndef __SIM_PLATFORM_H
#define __SIM_PLATFORM_H
#ifdef SIMULATION

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define UART_QUEUE_ITEMS                         (100)
#define UART_MSG_TERMINATOR                     ('\n')

// ========= sim_platform.c ============
int sim_platform_service(void);
uint32_t marble_init(bool use_xtal);
void print_status_counters(void);
void marble_UART_init(void);

int marble_UART_send(const char *str, int size);

int marble_UART_recv(char *str, int size);

void UART_FIFO_ISR(void);

void marble_LED_set(uint8_t led_num, bool on);

bool marble_LED_get(uint8_t led_num);

void marble_LED_toggle(uint8_t led_num);

bool marble_SW_get(void);

bool marble_FPGAint_get(void);

void marble_FMC_pwr(bool on);

uint8_t marble_FMC_status(void);

void marble_PSU_pwr(bool on);

uint8_t marble_PWR_status(void);

void reset_fpga(void);

typedef void *SSP_PORT;

int marble_SSP_write16(SSP_PORT ssp, uint16_t *buffer, unsigned size);
int marble_SSP_read16(SSP_PORT ssp, uint16_t *buffer, unsigned size);
int marble_SSP_exch16(SSP_PORT ssp, uint16_t *tx_buf, uint16_t *rx_buf, unsigned size);

void marble_GPIOint_handlers(void (*FPGA_DONE_handler)(void));

void marble_MGTMUX_set(uint8_t mgt_num, bool on);
uint8_t marble_MGTMUX_status(void);

void marble_MDIO_write(uint16_t reg, uint32_t data);
uint32_t marble_MDIO_read(uint16_t reg);

uint32_t marble_SYSTIMER_ms(uint32_t delay);
void marble_SYSTIMER_handler(void (*handler)(void));

void marble_SLEEP_ms(uint32_t delay);
void marble_SLEEP_us(uint32_t delay);


// ========= sim_i2c.c ============
typedef void *I2C_BUS;
int marble_I2C_probe(I2C_BUS I2C_bus, uint8_t addr);
int marble_I2C_send(I2C_BUS I2C_bus, uint8_t addr, const uint8_t *data, int size);
int marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size);
int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size);
int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size);
int marble_I2C_cmdsend_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size);
int marble_I2C_cmdrecv_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size);


#ifdef __cplusplus
}
#endif

#endif /* SIMULATION */
#endif // __SIM_PLATFORM_H


