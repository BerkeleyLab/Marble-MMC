/*
 * File: sim_platform.c
 * Desc: Simulated hardware API to run application on host
 */

#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h> // Needed for struct timeval
#include <unistd.h>   // For STDIN_FILENO
#include <fcntl.h>    // For fcntl()
#include "marble_api.h"
#include "console.h"
#include "uart_fifo.h"
#include "st-eeprom.h"

/*
 * On the simulated platform, the "UART" console process will be the following:
 *  * In main loop:
 *  *   Shift in all bytes currently waiting in stdin into the FIFO
 *  *   If any byte is '\n', set msgReady flag
 *  *   If msgReady, Shift message out of FIFO
 *  *   handleMsg(msg);
 */

#define DEBUG_TX_OUT
#define SIM_FPGA_DONE_DELAY_MS        (2)

typedef struct {
  int toExit;
  int msgReady;
} sim_console_state_t;

// GLOBALS
SSP_PORT SSP_FPGA;
uint32_t _timeStart;
int _fpgaDonePend;
static sim_console_state_t sim_console_state;
void FPGA_DONE_dummy(void) {}
void (*volatile marble_FPGA_DONE_handler)(void) = FPGA_DONE_dummy;

static int shiftMessage(void);
static void _sigHandler(int c);

// Emulate UART_RXNE_ISR() from marble_board.c but with keyboard input from stdin
// Also emulate USART_TXE_ISR() for printf()
int sim_platform_service(void) {
  uint8_t outByte;
  shiftMessage();
  if (sim_console_state.msgReady) {
    console_pend_msg();
    sim_console_state.msgReady = 0;
  }
  // If char queue not empty
  if (UARTTXQUEUE_Get(&outByte) != UARTTX_QUEUE_EMPTY) {
    // Write new char to DR
    putchar((char)outByte);
  }
  // If enough time has elapsed, simulate the FPGA_DONE signal arrival
  if (_fpgaDonePend) {
    if (BSP_GET_SYSTICK() - _timeStart > SIM_FPGA_DONE_DELAY_MS) {
      marble_FPGA_DONE_handler();
      _fpgaDonePend = 0;
    }
  }
  return sim_console_state.toExit;
}

static void _sigHandler(int c) {
  printf("Exiting...\r\n");
  sim_console_state.toExit = 1;
  return;
}

static int shiftMessage(void) {
  fd_set rset;        // A file-descriptor set for read mode
  int ri, rval;
  char rc;
  struct timeval timeout;
  FD_ZERO(&rset);     // Initialize the data to 0s
  FD_SET(STDIN_FILENO, &rset);    // Add STDIN to the read set
  // NOTE: select() MODIFIES TIMEOUT!!!   Need to reinitialize every time.
  timeout.tv_sec = 0;
  timeout.tv_usec = 1000;  // 1ms timeout
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

uint32_t marble_init(bool initFlash) {
  _timeStart = BSP_GET_SYSTICK();
  _fpgaDonePend = 1;
  signal(SIGINT, _sigHandler);
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
  sim_console_state.toExit = 0;
  sim_console_state.msgReady = 0;
  eeprom_init(initFlash);
  return 0;
}

Marble_PCB_Rev_t marble_get_pcb_rev(void) {
  return Marble_Simulator;
}

void marble_print_pcb_rev(void) {
  printf("PCB Rev: Marble Simulator\r\n");
  return;
}

uint8_t marble_get_board_id(void) {
  return (uint8_t)BOARD_TYPE_SIMULATION;
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
  // TODO - Do I want fake LEDs?
  return;
}

bool marble_LED_get(uint8_t led_num) {
  // TODO - Do I want fake LEDs?
  return 0;
}

void marble_LED_toggle(uint8_t led_num) {
  // TODO - Do I want fake LEDs?
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

uint8_t marble_PWR_status(void) {
  return 0;
}

void reset_fpga(void) {
  return;
}

void enable_fpga(void) {
  return;
}

typedef void *SSP_PORT;

int marble_SSP_write16(SSP_PORT ssp, uint16_t *buffer, unsigned size) {
  // TODO - What is SSP?
  return 0;
}

int marble_SSP_read16(SSP_PORT ssp, uint16_t *buffer, unsigned size) {
  return 0;
}

int marble_SSP_exch16(SSP_PORT ssp, uint16_t *tx_buf, uint16_t *rx_buf, unsigned size) {
  return 0;
}

void marble_GPIOint_handlers(void (*FPGA_DONE_handler)(void)) {
  marble_FPGA_DONE_handler = FPGA_DONE_handler;
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
  return 0;
}

void marble_SYSTIMER_handler(void (*handler)(void)) {
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

void FPGAWD_set_period(uint16_t preload) {
  _UNUSED(preload);
  return;
}

void FPGAWD_pet(void) {
  return;
}

void FPGAWD_ISR(void) {
  return;
}

