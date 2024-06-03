/*
 * File: sim_platform.c
 * Desc: Simulated hardware API to run application on host
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h> // Needed for struct timeval
#include <unistd.h>   // For STDIN_FILENO
#include <stdlib.h>   // For posix_openpt et al
#include <fcntl.h>    // For fcntl()
#include "marble_api.h"
#include "console.h"
#include "uart_fifo.h"
#include "st-eeprom.h"
#include "sim_api.h"
#include "sim_lass.h"

/*
 * On the simulated platform, the "UART" console process will be the following:
 *  * In main loop:
 *  *   Shift in all bytes currently waiting in stdin into the FIFO
 *  *   If any byte is '\n', set msgReady flag
 *  *   If msgReady, Shift message out of FIFO
 *  *   handleMsg(msg);
 */

#define DEBUG_TX_OUT
#define BOARD_SERVICE_SLEEP_MS       (50)
#define SIM_FPGA_DONE_DELAY_MS      (100)
#define SIM_FPGA_RESETS               (0)

// Defined in sim_i2c.c; declared here to avoid creating a "real" i2c_init function in marble_api.h
//void init_sim_ltm4673(void);

typedef struct {
  int toExit;
  int msgReady;
} sim_console_state_t;

// Local static variables
static void dummy_handler(void) {}
static int32_t _fpgaDoneTimeStart;
static int32_t _systickIrqTimeStart;
static int _fpgaDonePend;
static void (*volatile marble_FPGA_DONE_handler)(void) = dummy_handler;
static sim_console_state_t sim_console_state;
static void (*volatile marble_SysTick_Handler)(void) = dummy_handler;
static uint32_t sim_systick_period_ms = 1;
static int fpga_resets = 0;
static int fpga_enabled = 1;

// Static Prototypes
static int shiftMessage(void);
static void _sigHandler(int c);

void disable_all_IRQs(void) {
  return;
}

#define MAILBOX_PORT      (8003)
uint32_t marble_init(void) {
  _fpgaDoneTimeStart = BSP_GET_SYSTICK();
  _systickIrqTimeStart = BSP_GET_SYSTICK();
  _fpgaDonePend = 1;
  signal(SIGINT, _sigHandler);
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
  sim_console_state.toExit = 0;
  sim_console_state.msgReady = 0;
  eeprom_init();
  init_sim_ltm4673();
  if (lass_init(MAILBOX_PORT) < 0) {
    return 1;
  }
  sim_spi_init();
  printf("Listening on port %d\r\n", MAILBOX_PORT);
  return 0;
}

void board_init(void) {
  return;
}

void marble_print_status(void) {
  printf("Board Status: Simulation\r\n");
  return;
}

int marble_pwr_good(void) {
  return 1;
}

// Emulate USART_RXNE_ISR() from marble_board.c but with keyboard input from stdin
// Also emulate USART_TXE_ISR() for printf()
int board_service(void) {
  uint8_t outByte;
  shiftMessage();
  if (sim_console_state.msgReady) {
    console_pend_msg();
    sim_console_state.msgReady = 0;
  }
  // If char queue not empty
  if (UARTTXQUEUE_Get(&outByte) != UARTTX_QUEUE_EMPTY) {
    putchar((char)outByte);
  }
  // If enough time has elapsed, simulate the FPGA_DONE signal arrival
  uint32_t now = BSP_GET_SYSTICK();
  if (_fpgaDonePend) {
    if (now - _fpgaDoneTimeStart > SIM_FPGA_DONE_DELAY_MS) {
      marble_FPGA_DONE_handler();
      if (fpga_resets++ < SIM_FPGA_RESETS) {
        _fpgaDonePend = 1;
        _fpgaDoneTimeStart = now;
      } else {
        _fpgaDonePend = 0;
      }
    }
  }
  if (now - _systickIrqTimeStart >= sim_systick_period_ms) {
    //printf("now-start = %d\r\n", now - _systickIrqTimeStart);
    marble_SysTick_Handler();
    _systickIrqTimeStart = now;
  }
  lass_service();

  // Keep the system responsive, but don't hog resources
  sleep(BOARD_SERVICE_SLEEP_MS/1000);
  return sim_console_state.toExit;
}

static void _sigHandler(int c) {
  printf("Exiting...\r\n");
  sim_console_state.toExit = 1;
  return;
}

void cleanup(void) {
  return;
}

static int shiftMessage(void) {
  fd_set rset;        // A file-descriptor set for read mode
  int ri, rval;
  char rc;
  struct timeval timeout;
  FD_ZERO(&rset);     // Initialize the data to 0s
  // NOTE: select() MODIFIES TIMEOUT!!!   Need to reinitialize every time.
  timeout.tv_sec = 0;
  timeout.tv_usec = 1000;  // 1ms timeout
  FD_SET(STDIN_FILENO, &rset);    // Add STDIN to the read set
  rval = select(STDIN_FILENO+1, &rset, NULL, NULL, &timeout);
  int n = UART_QUEUE_ITEMS;
  if (rval) {
    while (n--) {
      ri = fgetc(stdin);
      if ((ri == EOF) || (ri == 0)) {
        break;
      }
      rc = (char)(ri & 0xff);
      UARTQUEUE_Add((uint8_t *)&rc);
      if (rc == UART_MSG_TERMINATOR) {
        sim_console_state.msgReady = 1;
        //sim_console_state.inputMsg[sim_console_state.mPtr++] = '\0';
        break;
      } else if (rc == UART_MSG_ABORT) {
        UARTQUEUE_Clear();
        break;
      }
    }
  }
  return 0;
}

void pwr_autoboot(void) {
  return;
}

Marble_PCB_Rev_t marble_get_pcb_rev(void) {
  return Marble_Simulator;
}

void marble_print_pcb_rev(void) {
  printf("PCB Rev: Marble Simulator\r\n");
  return;
}

uint8_t marble_get_board_id(void) {
  // Emulating Marble v1.4
  return ((uint8_t)Marble_v1_4 & 0xf) | (uint8_t)BOARD_TYPE_SIMULATION;
}

void marble_UART_init(void) {
  return;
}

int marble_UART_send(const char *str, int size) {
#ifdef DEBUG_TX_OUT
  USART_Tx_LL_Queue((char *)str, size);
#else
  char termStr[size+1];
  memcpy(termStr, str, size);
  termStr[size] = '\0';
  printf("%s", termStr);
#endif
  return 0;
}

int marble_UART_recv(char *str, int size) {
  uint8_t outByte;
  if (UARTTXQUEUE_Get(&outByte) == UARTTX_QUEUE_EMPTY) {
    return -1;
  }
  *str = (char)outByte;
  return 0;
}

void marble_LED_set(uint8_t led_num, bool on) {
  return;
}

bool marble_LED_get(uint8_t led_num) {
  return 0;
}

void marble_LED_toggle(uint8_t led_num) {
  return;
}

void marble_Pmod3_5_write(bool on) {
  _UNUSED(on);
  return;
}

bool marble_SW_get(void) {
  return 0;
}

bool marble_FPGAint_get(void) {
  return 0;
}

void marble_FMC_pwr(bool on) {
  return;
}

uint8_t marble_FMC_status(void) {
  return 0;
}

void marble_PSU_pwr(bool on) {
  return;
}

void marble_PSU_reset_write(bool on) {
  return;
}

uint8_t marble_PWR_status(void) {
  return 0;
}

void marble_print_GPIO_status(void) {
  printf("Sim print_GPIO_status\r\n");
  return;
}

void marble_list_GPIOs(void) {
  printf("Sim list_GPIOs\r\n");
  return;
}

void reset_fpga(void) {
  enable_fpga();
  return;
}

void enable_fpga(void) {
  fpga_enabled = 1;
  _fpgaDonePend = 1;
  _fpgaDoneTimeStart = BSP_GET_SYSTICK();
  return;
}

void disable_fpga(void) {
  fpga_enabled = 0;
  return;
}

void marble_GPIOint_handlers(void (*FPGA_DONE_handler)(void)) {
  marble_FPGA_DONE_handler = FPGA_DONE_handler;
  return;
}

void marble_MGTMUX_config(uint8_t mgt_msg, uint8_t store, uint8_t print) {
  _UNUSED(mgt_msg);
  _UNUSED(store);
  _UNUSED(print);
  return;
}

void marble_MGTMUX_set(uint8_t mgt_num, bool on) {
  return;
}

uint8_t marble_MGTMUX_status(void) {
  return 0;
}

void marble_MDIO_write(uint16_t reg, uint32_t data) {
  return;
}

uint32_t marble_MDIO_read(uint16_t reg) {
  return 0;
}

uint32_t marble_SYSTIMER_ms(uint32_t delay) {
  sim_systick_period_ms = BOARD_SERVICE_SLEEP_MS > delay ? BOARD_SERVICE_SLEEP_MS : delay;
  return sim_systick_period_ms;
}

void marble_SYSTIMER_handler(void (*handler)(void)) {
  marble_SysTick_Handler = handler;
  return;
}

void marble_SLEEP_ms(uint32_t delay) {
  // TODO - Implement sleep function?
  return;
}

void marble_SLEEP_us(uint32_t delay) {
  // TODO - Implement sleep function?
  return;
}

uint32_t marble_get_tick(void) {
  return BSP_GET_SYSTICK();
}

uint8_t fsynthGetAddr(void) {
  return 0;
}

uint8_t fsynthGetConfig(void) {
  return 0;
}

uint32_t fsynthGetFreq(void) {
  return 0;
}

int get_hw_rnd(uint32_t *result) {
  // Using stdlib's random()
  *result = (uint32_t)random();
  return 0;
}

void marble_MGTMUX_set_all(uint8_t mgt_cfg) {
  _UNUSED(mgt_cfg);
}
