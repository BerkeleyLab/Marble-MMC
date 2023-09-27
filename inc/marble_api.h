/*
 * MARBLE_API.h
 * API consisting of wrapper functions providing access to low-level features
 * of Marble Mini (v1 and v2) and Marble.
 * The functions defined herein are necessarily uController-agnostic.
 */

#ifndef _MARBLE_API_H_
#define _MARBLE_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "common.h"

#ifdef SIMULATION
#include <stddef.h>
#include <errno.h>
#include <time.h>

#define SIM_FLASH_FILENAME                           "flash.bin"
#define FLASH_SECTOR_SIZE                                  (256)
#define EEPROM_COUNT ((size_t)FLASH_SECTOR_SIZE/sizeof(ee_frame))

#define DEMO_STRING                 "Marble UART Simulation\r\n"
#define BSP_GET_SYSTICK()     (uint32_t)((uint64_t)clock()/1000)

# define MGT_MAX_PINS 0
#else

#ifdef MARBLEM_V1
# define MGT_MAX_PINS 0
# define DEMO_STRING           "Marble Mini v1 UART Console\r\n"
#else
# ifdef MARBLE_V2
#  define MGT_MAX_PINS 3
#  define DEMO_STRING               "Marble v2 UART Console\r\n"
# endif /* MARBLE_V2 */
#endif /* MARBLEM_V1 */

#ifdef NUCLEO
#define CONSOLE_USART                                    USART3
#else
#define CONSOLE_USART                                    USART1
#endif

#ifdef RTEMS_SUPPORT
#include <rtems.h>
// Optionally be more specific and only include the file where these are defined
//#include <rtems/rtems/intr.h>
#define INTERRUPTS_DISABLE()  do { \
  rtems_interrupt_level level; \
  rtems_interrupt_disable(level); \
} while (0)
#define INTERRUPTS_ENABLE()   do { \
  rtems_interrupt_enable(level); \
} while (0)
#define BSP_GET_SYSTICK()       (0) // TODO
#else /* ndef RTEMS_SUPPORT */
//#define INTERRUPTS_DISABLE                     __disable_irq
#define INTERRUPTS_DISABLE()                    __set_PRIMASK(1)
//#define INTERRUPTS_ENABLE                       __enable_irq
#define INTERRUPTS_ENABLE()                     __set_PRIMASK(0)
#define BSP_GET_SYSTICK()                      marble_get_tick()
#endif  /* RTEMS_SUPPORT */

#endif /* SIMULATION */

#define FLASH_VOLTAGE_MV                              (3300)
#define FPGA_WATCHDOG_CLK_FREQ                        (3276)

/* = = = = System Clock Configuration = = = = */
// System Clock Frequency = 120 MHz
#ifdef NUCLEO
// HSE input is 8 MHz from onboard ST-Link MCO
#define CONFIG_CLK_PLLM              (8)
#define CONFIG_CLK_PLLN            (240)
#define CONFIG_CLK_PLLP    RCC_PLLP_DIV2
#else
// HSE input on Marble is 25 MHz from WhiteRabbit module
#define CONFIG_CLK_PLLM             (20)
#define CONFIG_CLK_PLLN            (192)
#define CONFIG_CLK_PLLP    RCC_PLLP_DIV2
#endif

#define UART_MSG_TERMINATOR                           ('\n')
#define UART_MSG_ABORT                                (27)  // esc
#define UART_MSG_BKSP                                 (8)   // backspace
#define UART_MSG_DEL                                  (128) // del

// OR'd with PCB revision to fully identify hardware
#define BOARD_TYPE_SIMULATION       (0x00)
#define BOARD_TYPE_MARBLE           (0x10)
#define BOARD_TYPE_MARBLE_MINI      (0x20)

// Enum for identifying Marble PCB revisions
typedef enum {
  Marble_v1_2 = 0,
  Marble_v1_3,
  Marble_v1_4,
  Marble_v1_5,  // Future support
  Marble_v1_6,  // Future support
  Marble_v1_7,  // Future support
  Marble_v1_8,  // Future support
  Marble_v1_9,  // Future support
  Marble_Nucleo,  // Nucleo-F207ZG dev board
  Marble_Simulator
} Marble_PCB_Rev_t;

/* Initialize uC and peripherals before main code can run. */
#ifndef SIMULATION
uint32_t marble_init(bool use_xtal);
#else
uint32_t marble_init(bool initFlash);
int sim_platform_service(void);
#endif

Marble_PCB_Rev_t marble_get_pcb_rev(void);

void marble_print_pcb_rev(void);

// The Board ID is (PCB revision) | (BOARD_TYPE_...)
uint8_t marble_get_board_id(void);

// TODO - Move these for encapsulation
void print_status_counters(void);

uint32_t marble_get_tick(void);

/****
* UART
****/
void marble_UART_init(void);

int marble_UART_send(const char *str, int size);

int marble_UART_recv(char *str, int size);

void CONSOLE_USART_ISR(void);

/****
* LED
****/
void marble_LED_set(uint8_t led_num, bool on);

bool marble_LED_get(uint8_t led_num);

void marble_LED_toggle(uint8_t led_num);

void marble_Pmod3_5_write(bool on);

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

void marble_PSU_reset_write(bool on);

typedef enum {
   M_PWR_STATUS_PSU_EN = 0,
   M_PWR_STATUS_POE,
   M_PWR_STATUS_OTEMP
} M_PWR_STATUS;

uint8_t marble_PWR_status(void);

void marble_print_GPIO_status(void);

/****
* FPGA reset/init control
* Central to this whole job, may need refinement
****/
void reset_fpga(void);

void enable_fpga(void);

/****
* SPI/SSP
****/
typedef void *SSP_PORT;

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
// Set one MGTMUX pin
// Stores to EEPROM
void marble_MGTMUX_set(uint8_t mgt_num, bool on);

// Set multiple MGTMUX pins
// Stores to EEPROM
void marble_MGTMUX_config(uint8_t mgt_msg, uint8_t store, uint8_t print);

// Get status in same bit format as argument to marble_MGTMUX_set_all()
uint8_t marble_MGTMUX_status(void);

// Set all MGTMUX pins simultaneously using same bit format as return value of marble_MGTMUX_status()
void marble_MGTMUX_set_all(uint8_t mgt_cfg);

/****
* I2C
****/
#ifdef MARBLE_LPC1776
typedef int I2C_BUS;
#elif defined MARBLE_STM32F207
typedef void *I2C_BUS;
#else
#ifdef SIMULATION
typedef void *I2C_BUS;
#else
#error Marble microcontroller API not defined!
#endif /* SIMULATION */
#endif

int marble_I2C_probe(I2C_BUS I2C_bus, uint8_t addr);
int marble_I2C_send(I2C_BUS I2C_bus, uint8_t addr, const uint8_t *data, int size);
int marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size);
int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size);
int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size);
int marble_I2C_cmdsend_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size);
int marble_I2C_cmdrecv_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size);
int getI2CBusStatus(void);
void resetI2CBusStatus(void);

/************
* Freq. Synthesizer (si570)
************/
#define FSYNTH_ASSEMBLE(data, addr, freq, config) \
  do { \
    data[0] = (uint8_t)(addr & 0xff); \
    data[1] = (uint8_t)(config & 0xff); \
    data[2] = (uint8_t)((freq >> 24) & 0xff); \
    data[3] = (uint8_t)((freq >> 16) & 0xff); \
    data[4] = (uint8_t)((freq >> 8) & 0xff); \
    data[5] = (uint8_t)(freq & 0xff); \
  } while (0)
#define FSYNTH_GET_ADDR(data) data[0]
#define FSYNTH_GET_CONFIG(data) data[1]
#define FSYNTH_GET_FREQ(data) (int)((data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5])
uint8_t fsynthGetAddr(void);
uint8_t fsynthGetConfig(void);
uint32_t fsynthGetFreq(void);

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

/************
* FPGA Software/Hardware Watchdog
************/
// Set period to 0 to disable
void FPGAWD_set_period(uint16_t preload);
// Software pet
void FPGAWD_pet(void);
// ISR pends FPGA reset
void FPGAWD_ISR(void);

#ifdef __cplusplus
}
#endif

#endif /* _MARBLE_API_H_ */
