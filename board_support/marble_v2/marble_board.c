/*
 * Based in part on (and therefore legally derived from):
 * STM32 HAL Library
 *
 * which is COPYRIGHT(c) 2017 STMicroelectronics
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include "stm32f2xx_hal.h"
#include "marble_api.h"
#include "string.h"
#include "uart_fifo.h"
#include "console.h"
#include "st-eeprom.h"
#include "i2c_pm.h"
#include "mailbox.h"

#define UART_ECHO

#define PRINT_POWER_STATE(subs, on) do {\
   char s[3] = {'f', 'f', '\0'}; \
   if (on) {s[0] = 'n'; s[1] = '\0';} \
   printf(subs " Power O%s\r\n", s); } while (0)

void Error_Handler(void) {}
#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif /* USE_FULL_ASSERT */

void SystemClock_Config_HSI(void);

ETH_HandleTypeDef heth;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c3;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart_console;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;  // Used for nucleo

static Marble_PCB_Rev_t marble_pcb_rev;

static int i2cBusStatus = 0;
// Moved here from marble_api.h
SSP_PORT SSP_FPGA;
SSP_PORT SSP_PMOD;
I2C_BUS I2C_FPGA;
I2C_BUS I2C_PM;
/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ETH_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C3_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
//static void MX_USART1_UART_Init(void);
static void CONSOLE_USART_Init(void);
static void MX_USART2_UART_Init(void);
static void USART_RXNE_ISR(void);
static void USART_TXE_ISR(void);
static void USART_Erase_Echo(void);
static void USART_Erase(int n);
static void marble_apply_params(void);
static void marble_read_pcb_rev(void);
static int marble_MGTMUX_store(void);
static void _SPI1_switch_to_MISO(void);
static void _SPI1_switch_to_EXTI(void);
static void marble_enable_MISO_ISR(void);
static void marble_disable_MISO_ISR(void);

/* Initialize UART pins */
void marble_UART_init(void)
{
   /* PA9, PA10 - MMC_CONS_PROG */
   //MX_USART1_UART_Init();
   CONSOLE_USART_Init();
   /* PD5, PD6 - UART4 (Pmod3_7/3_6) */
   MX_USART2_UART_Init();
   i2cBusStatus = 0;
}

/* Send string over UART. Returns number of bytes sent */
int marble_UART_send(const char *str, int size)
{
  //HAL_UART_Transmit(&huart_console, (const uint8_t *) str, size, 1000);
  int txnum = USART_Tx_LL_Queue((char *)str, size);
  // Kick off the transmission if the TX buffer is empty
  if ((CONSOLE_USART->SR) & USART_SR_TXE) {
    SET_BIT(CONSOLE_USART->CR1, USART_CR1_TXEIE);
  }
  return txnum;
}

int marble_UART_recv(char *str, int size) {
  return USART_Rx_LL_Queue((volatile char *)str, size);
}

void CONSOLE_USART_ISR(void) {
  USART_RXNE_ISR();   // Handle RX interrupts first
  USART_TXE_ISR();  // Then handle TX interrupts
  return;
}

/*
 * void USART_RXNE_ISR(void);
 *  A low-level ISR to pre-empt the STM32_HAL handler to catch
 *  received bytes (TxE IRQ passes to HAL handler)
 */
void USART_RXNE_ISR(void) {
  uint8_t c = 0;
  //if (__HAL_UART_GET_FLAG(&huart_console, UART_FLAG_RXNE) == SET) {
  if ((CONSOLE_USART->SR & USART_SR_RXNE) == USART_SR_RXNE) {
    // Don't clear flags; the RXNE flag is cleared automatically by read from DR
    //c = (uint8_t)(huart_console.Instance->DR & (uint8_t)0x00FF);
    c = (uint8_t)(CONSOLE_USART->DR & (uint8_t)0x00FF);
    // Look for control characters first
    if (c == UART_MSG_ABORT) {
#ifdef UART_ECHO
      USART_Erase_Echo();
#endif
      // clear queue
      UARTQUEUE_Clear();
    } else if (c == UART_MSG_BKSP) {
#ifdef UART_ECHO
      USART_Erase(1);
#endif
      UARTQUEUE_Rewind(1);
    } else {
      if (UARTQUEUE_Add(&c) == UART_QUEUE_FULL) {
        UARTQUEUE_SetDataLost(UART_DATA_LOST);
        // Clear QUEUE at this point?
      }
      if (c == UART_MSG_TERMINATOR) {
        console_pend_msg();
      }
#ifdef UART_ECHO
      marble_UART_send((const char *)&c, 1);
#endif
    }
  }
  return;
}

/*
 * void USART_Erase_Echo(void);
 *  Print 1 backspace for every char in the queue
 */
static void USART_Erase_Echo(void) {
  USART_Erase(0);
  return;
}

static void USART_Erase(int n) {
  int fill = UARTQUEUE_FillLevel();
  // If n = 0, erase all in queue
  if (n == 0) {
    n = fill;
  } else {
    // n = min(n, fill)
    n = n > fill ? fill : n;
  }
  // First issue backspaces
  char bksps[n];
  for (int m = 0; m < n; m++) {
    bksps[m] = UART_MSG_BKSP;
  }
  marble_UART_send((const char *)bksps, n);
  // Then overwrite with spaces
  for (int m = 0; m < n; m++) {
    bksps[m] = ' ';
  }
  marble_UART_send((const char *)bksps, n);
  // Then issue backspaces again
  for (int m = 0; m < n; m++) {
    bksps[m] = UART_MSG_BKSP;
  }
  marble_UART_send((const char *)bksps, n);
  return;
}

void USART_TXE_ISR(void) {
  uint8_t outByte;
  // If char queue not empty
  //if (__HAL_UART_GET_FLAG(&huart_console, UART_FLAG_TXE) == SET) {
  if ((CONSOLE_USART->SR & USART_SR_TXE) == USART_SR_TXE) {
    if (UARTTXQUEUE_Get(&outByte) != UARTTX_QUEUE_EMPTY) {
      // Write new char to DR
      //huart_console.Instance->DR = (uint32_t)outByte;
      CONSOLE_USART->DR = (uint32_t)outByte;
    } else {
      // If the queue is empty, disable the TXE interrupt
      CLEAR_BIT(CONSOLE_USART->CR1, USART_CR1_TXEIE);
    }
  }
  return;
}


/************
* LEDs
************/

#define MAXLEDS 3
#ifdef NUCLEO
#define LED_GPIO  GPIOB
static const uint16_t ledpins[MAXLEDS] = {GPIO_PIN_0, GPIO_PIN_7, GPIO_PIN_14};
#else
static const uint16_t ledpins[MAXLEDS] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2};
#define LED_GPIO  GPIOE
#endif
// NOTE:  ledpins[0] = PE0 = "LD15"
//        ledpins[1] = PE1 = "LD11"
//        ledpins[2] = PE2 = "LD12"

/* Initializes board LED(s) */
static void marble_LED_init(void)
{
   // Handled in MX_GPIO_Init
   return;
}

/* Sets the state of a board LED to on or off */
void marble_LED_set(uint8_t led_num, bool on)
{
  bool state;
#ifdef NUCLEO
  /* On Nucleo, GPIO low is off, high is on */
  state = on;
#else
  /* On Marble, GPIO low is on, high is off */
  state = !on;
#endif
   if (led_num < MAXLEDS) {
      HAL_GPIO_WritePin(LED_GPIO, ledpins[led_num], state);
   }
}

/* Returns the current state of a board LED */
bool marble_LED_get(uint8_t led_num)
{
   bool state = false;

   if (led_num < MAXLEDS) {
      state = HAL_GPIO_ReadPin(LED_GPIO, ledpins[led_num]);
   }

   /* These LEDs are reverse logic. */
   return !state;
}

/* Toggles the current state of a board LED */
void marble_LED_toggle(uint8_t led_num)
{
   if (led_num < MAXLEDS) {
      HAL_GPIO_TogglePin(LED_GPIO, ledpins[led_num]);
   }
}

/* Debug purposes */
void marble_Pmod3_5_write(bool on) {
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
  return;
}

/************
* FMC & PSU
************/

/* Set FMC power */
void marble_FMC_pwr(bool on)
{
   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, on);
   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, on);
   return;
}

uint8_t marble_FMC_status(void)
{
   uint8_t status = 0;
   status = WRITE_BIT(status, M_FMC_STATUS_FMC1_PWR,  HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_10));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC1_FUSE, HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_10));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC2_PWR,  HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_9));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC2_FUSE, HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11));
   return status;
}

void marble_PSU_pwr(bool on)
{
   if (on == false) {
      SystemClock_Config_HSI(); // switch to internal clock source, external clock is powered from 3V3!
      marble_SLEEP_ms(50);
      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, on);
   } else {
      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, on);
      SystemClock_Config(); // switch back to external clock source
   }
   return;
}

uint8_t marble_PWR_status(void)
{
   uint8_t status = 0;
   status = WRITE_BIT(status, M_PWR_STATUS_PSU_EN, HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_11));
   status = WRITE_BIT(status, M_PWR_STATUS_POE,    HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8));
   status = WRITE_BIT(status, M_PWR_STATUS_OTEMP,  HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7));
   return status;
}

void marble_print_GPIO_status(void) {
  // FMC power
  printf("FMC power = ");
  int state = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_9);
  if (state) {
    printf("On\r\n");
  } else {
    printf("Off\r\n");
  }
  state = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_11);
  printf("PSU power = ");
  if (state) {
    printf("On\r\n");
  } else {
    printf("Off\r\n");
  }
  state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15);
  printf("Pmod3_5 = %d", state);
  printf("\r\n");
  return;
}



/************
* Switches and FPGA interrupt
************/

static void marble_SW_init(void)
{
   // SW1 and SW2
   //Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 12, false);
}

bool marble_SW_get(void)
{
   // Pin pulled low on button press
   //if (Chip_GPIO_ReadPortBit(LPC_GPIO, 2, 12) == 0x01) {
   //   return false;
   //}
   return true;
}

bool marble_FPGAint_get(void)
{
   if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3)) {
      return false;
   }
   return true;
}

void reset_fpga(void)
{
   /* Pull the PROGRAM_B pin low; it's spelled PROG_B on schematic */
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, false);
   marble_SLEEP_ms(50);
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, true);
}

void enable_fpga(void) {
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, true);
  return;
}

/************
* GPIO interrupt setup and user-defined handlers
************/
void FPGA_DONE_dummy(void) {}
void (*volatile marble_FPGA_DONE_handler)(void) = FPGA_DONE_dummy;


// Override default (weak) IRQHandler and redirect to HAL shim
void EXTI0_IRQHandler(void)
{
   HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

// Override default (weak) callback
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
   if (GPIO_Pin == GPIO_PIN_0) { // Handles any interrupt on line 0 (e.g. PA0, PB0, PC0, PD0)
      marble_FPGA_DONE_handler();
   }
}

void marble_GPIOint_init(void)
{
   /*Configure GPIO pin : PD0 - FPGA_DONE rising edge interrupt */
   GPIO_InitTypeDef GPIO_InitStruct = {0};
   GPIO_InitStruct.Pin = GPIO_PIN_0;
   GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

   /* Enable interrupt in the NVIC */
   HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
   HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/* Register user-defined interrupt handlers */
void marble_GPIOint_handlers(void (*FPGA_DONE_handler)(void)) {
   marble_FPGA_DONE_handler = FPGA_DONE_handler;
}

/************
* MGT Multiplexer
************/
static const uint16_t mgtmux_pins[MGT_MAX_PINS] = {GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_10};

void marble_MGTMUX_config(uint8_t mgt_msg, uint8_t store, uint8_t print) {
  // Control of FMC power and MGT mux based on mailbox entry MB2_FMC_MGT_CTL
  // Currently addressed as 0x200020 = 2097184 in test_marble_family
  // [1] - FMC_SEL,      [0] - ON/OFF
  // [3] - MGT_MUX1_SEL, [2] - ON/OFF
  // [5] - MGT_MUX2_SEL, [4] - ON/OFF
  // [7] - MGT_MUX3_SEL, [6] - ON/OFF
  if ((mgt_msg & 0xaa) == 0) {
    // Nothing to do; early exit
    return;
  }
  if (print) {
    printf("Setting ");
  }
  if (mgt_msg & 0x2) {
    if (print) {
      printf("FMC_Pwr=");
      if (mgt_msg & 0x1) {
        printf("ON ");
      } else {
        printf("OFF ");
      }
    }
    marble_FMC_pwr(mgt_msg & 1);
  }
  uint8_t v;
  if (mgt_msg & 0x8) {
    v = ((mgt_msg & 0x4) >> 2);
    if (print) {
      printf("MUX1=%d ", v);
    }
    HAL_GPIO_WritePin(GPIOE, mgtmux_pins[0], v);
    //marble_MGTMUX_set(1, v);
  }
  if (mgt_msg & 0x20) {
    v = ((mgt_msg & 0x10) >> 4);
    if (print) {
      printf("MUX2=%d ", v);
    }
    //marble_MGTMUX_set(2, v);
    HAL_GPIO_WritePin(GPIOE, mgtmux_pins[1], v);
  }
  if (mgt_msg & 0x80) {
    v = ((mgt_msg & 0x40) >> 6);
    if (print) {
      printf("MUX3=%d ", v);
    }
    //marble_MGTMUX_set(3, v & 1);
    HAL_GPIO_WritePin(GPIOE, mgtmux_pins[2], v);
  }
  // Store to EEPROM
  if (store) {
    marble_MGTMUX_store();
  }
  if (print) {
    printf("\r\n");
  }
  return;
}

void marble_MGTMUX_set(uint8_t mgt_num, bool on) {
  mgt_num -= 1;
  if (mgt_num < MGT_MAX_PINS) {
     HAL_GPIO_WritePin(GPIOE, mgtmux_pins[mgt_num], on);
  }
  // Store to EEPROM
  marble_MGTMUX_store();
  return;
}

uint8_t marble_MGTMUX_status(void) {
   uint8_t mgt_cfg = 0;
   for (unsigned i=0; i < MGT_MAX_PINS; i++) {
      mgt_cfg |= HAL_GPIO_ReadPin(GPIOE, mgtmux_pins[i])<<i;
   }
   return mgt_cfg;
}

void marble_MGTMUX_set_all(uint8_t mgt_cfg) {
  // Set the state of all MGT_MUX pins simultaneously.
  // This function uses the same bit format as marble_MGTMUX_status()
  // I.e. marble_MGTMUX_set_all(marble_MGTMUX_status()) is no-op.

  // Note! Non-portable function depends on MGT_MUX pins being consecutive in the GPIO.ODR register
  // mgt_cfg bit  Signal
  // -------------------
  // Bit0         MUX1
  // Bit1         MUX2
  // Bit2         MUX3
  uint32_t odr = GPIOE->ODR;
  uint32_t mask = (uint32_t)(7 << 8);
  GPIOE->ODR = (odr & ~mask) | ((mgt_cfg << 8) & mask);
  // NOTE! Does not store to EEPROM because this is used exclusively to apply stored value at startup
  return;
}

// Store to non-volatile memory
static int marble_MGTMUX_store(void) {
  uint8_t mgt_cfg = marble_MGTMUX_status();
  int rval = eeprom_store_mgt_mux((const uint8_t *)&mgt_cfg, 1);
  if (rval) {
    printf("Failed to store MGTMUX to EEPROM. Error = %d\r\n", rval);
  }
  return rval;
}



/************
* I2C
************/
#define SPEED_100KHZ 100000


/* Non-destructive I2C probe function based on empty data command, i.e. S+[A,RW]+P */
int marble_I2C_probe(I2C_BUS I2C_bus, uint8_t addr) {
   int rc = HAL_I2C_IsDeviceReady(I2C_bus, addr, 2, 2);
   i2cBusStatus |= rc;
   return rc;
}

/* Generic I2C send function with selectable I2C bus and 8-bit I2C addresses (R/W bit = 0) */
/* 1-byte register addresses */
int marble_I2C_send(I2C_BUS I2C_bus, uint8_t addr, const uint8_t *data, int size) {
   int rc = HAL_I2C_Master_Transmit(I2C_bus, (uint16_t)addr, data, size, HAL_MAX_DELAY);
   i2cBusStatus |= rc;
   return rc;
}

int marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
   int rc = HAL_I2C_Mem_Write(I2C_bus, (uint16_t)addr, cmd, 1, data, size, HAL_MAX_DELAY);
   i2cBusStatus |= rc;
   return rc;
}

int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size) {
   int rc = HAL_I2C_Master_Receive(I2C_bus, (uint16_t)addr, data, size, HAL_MAX_DELAY);
   i2cBusStatus |= rc;
   return rc;
}

int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
   int rc = HAL_I2C_Mem_Read(I2C_bus, (uint16_t)addr, cmd, 1, data, size, HAL_MAX_DELAY);
   i2cBusStatus |= rc;
   return rc;
}

/* Same but 2-byte register addresses */
int marble_I2C_cmdsend_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
   int rc = HAL_I2C_Mem_Write(I2C_bus, (uint16_t)addr, cmd, 2, data, size, HAL_MAX_DELAY);
   i2cBusStatus |= rc;
   return rc;
}
int marble_I2C_cmdrecv_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
   int rc = HAL_I2C_Mem_Read(I2C_bus, (uint16_t)addr, cmd, 2, data, size, HAL_MAX_DELAY);
   i2cBusStatus |= rc;
   return rc;
}

/************
* SSP/SPI
************/
static void SPI_CSB_SET(SSP_PORT ssp, bool set);

int marble_SSP_write16(SSP_PORT ssp, uint16_t *buffer, unsigned size)
{
   SPI_CSB_SET(ssp, false);
   int rc = HAL_SPI_Transmit(ssp, (uint8_t*) buffer, size, HAL_MAX_DELAY);
   SPI_CSB_SET(ssp, true);
   return rc;
}

int marble_SSP_read16(SSP_PORT ssp, uint16_t *buffer, unsigned size)
{
   SPI_CSB_SET(ssp, false);
   int rc = HAL_SPI_Receive(ssp, (uint8_t*) buffer, size, HAL_MAX_DELAY);
   SPI_CSB_SET(ssp, true);
   return rc;
}

int marble_SSP_exch16(SSP_PORT ssp, uint16_t *tx_buf, uint16_t *rx_buf, unsigned size)
{
   SPI_CSB_SET(ssp, false);
   int rc = HAL_SPI_TransmitReceive(ssp, (uint8_t*) tx_buf, (uint8_t*) rx_buf,size, HAL_MAX_DELAY);
   SPI_CSB_SET(ssp, true);
   return rc;
}

/************
* MDIO to PHY
************/
void marble_MDIO_init(void)
{
   //Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_ENET);

   /* Setup MII clock rate and PHY address */
   //Chip_ENET_SetupMII(LPC_ETHERNET, Chip_ENET_FindMIIDiv(LPC_ETHERNET, 2500000), 0);
}

void marble_MDIO_write(uint16_t reg, uint32_t data)
{
   //Chip_ENET_StartMIIWrite(LPC_ETHERNET, reg, data);
   HAL_ETH_WritePHYRegister(&heth, reg, data);
   //while (Chip_ENET_IsMIIBusy(LPC_ETHERNET));
}

uint32_t marble_MDIO_read(uint16_t reg)
{
   uint32_t value;
   printf("\r\nPHYread Stat: %d\r\n", HAL_ETH_ReadPHYRegister(&heth, reg, &value));
   return value;
}

/************
* System Timer and Stopwatch
************/
void SysTick_Handler_dummy(void) {}
void (*volatile marble_SysTick_Handler)(void) = SysTick_Handler_dummy;

// Override default (weak) SysTick_Handler
void SysTick_Handler(void)
{
   HAL_IncTick(); // Advances HAL timebase used in HAL_Delay
   if (marble_SysTick_Handler)
      marble_SysTick_Handler();
}

uint32_t marble_get_tick(void) {
  return (uint32_t)HAL_GetTick();
}

/* Register user-defined interrupt handlers */
void marble_SYSTIMER_handler(void (*handler)(void)) {
   marble_SysTick_Handler = handler;
}

/* Configures 24-bit count-down timer and enables systimer interrupt */
uint32_t marble_SYSTIMER_ms(uint32_t delay)
{
   // WARNING: Hardcoded to 1 ms since this is what increments HAL_IncTick() and
   // enables HAL_Delay
   delay = 1; // TODO: Consider decoupling stopwatch from system timer

   const uint32_t MAX_TICKS = (1<<24)-1;
   const uint32_t MAX_DELAY_MS = (SystemCoreClock * 1000U) / MAX_TICKS;
   uint32_t ticks = (SystemCoreClock / 1000U) * delay;
   if (delay > MAX_DELAY_MS) {
      ticks = MAX_TICKS;
      delay = MAX_DELAY_MS;
   }
   SysTick_Config(ticks);
   return delay;
}

void marble_SLEEP_ms(uint32_t delay)
{
   HAL_Delay(delay); // TODO: Consider replacing with stopwatch built from timer peripheral
}

void marble_SLEEP_us(uint32_t delay)
{
   (void) delay;
   return; // XXX Not available unless HAL weak definitions are overridden
   // Good thing nobody depends on this (yet)
}


/************
* Board Init
************/

uint32_t marble_init(bool use_xtal)
{
  (void) use_xtal;  // feature not yet supported with this chip

  // Must happen before any other clock manipulations:
  HAL_Init();
  SystemClock_Config_HSI();

  MX_GPIO_Init();

  // Configure GPIO interrupts
  marble_GPIOint_init();

  marble_PSU_pwr(true);
  MX_ETH_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();

  marble_LED_init();
  marble_SW_init();
  marble_UART_init();

  LM75_Init();
  eeprom_init(0);
  marble_apply_params();
  marble_read_pcb_rev();

  printf("** Marble init done **\r\n");
  marble_print_pcb_rev();

  // Init SSP busses
  //marble_SSP_init(LPC_SSP0);
  //marble_SSP_init(LPC_SSP1);

  //marble_MDIO_init();
  return 0;
}

/*
 * static void marble_apply_params(void);
 *    Apply parameters stored in non-volatile memory.
 */
static void marble_apply_params(void) {
  // Fan speed
  uint8_t val;
  if (eeprom_read_fan_speed(&val, 1)) {
    printf("Could not read current fan speed.\r\n");
  } else {
    max6639_set_fans((int)val);
  }
  // Over-temperature threshold
  if (eeprom_read_overtemp(&val, 1)) {
    printf("Could not read over-temperature threshold.\r\n");
  } else {
    max6639_set_overtemp(val);
    LM75_set_overtemp((int)val);
  }
  // MGT MUX
  if (eeprom_read_mgt_mux(&val, 1)) {
    printf("Could not read MGT MUX config.\r\n");
  } else {
    marble_MGTMUX_set_all(val);
  }
  return;
}

void marble_print_pcb_rev(void) {
#ifdef NUCLEO
  printf("PCB Rev: Nucleo\r\n");
#else
  switch (marble_pcb_rev) {
    case Marble_v1_3:
      printf("PCB Rev: Marble v1.3\r\n");
      break;
    default:
      printf("PCB Rev: Marble v1.2\r\n");
      break;
  }
#endif
}

Marble_PCB_Rev_t marble_get_pcb_rev(void) {
  return marble_pcb_rev;
}

uint8_t marble_get_board_id(void) {
  return ((uint8_t)(marble_pcb_rev & 0x0F) | BOARD_TYPE_MARBLE);
}

// This macro yields a number in the range 0-15 inclusive where the original
// bits 12-15 end up reversed and shifted, as in: (MSB->LSB) |b12|b13|b14|b15|
#define MARBLE_PCB_REV_XFORM(gpio_idr)    ((__RBIT(gpio_idr) >> 16) & 0xF)
static void marble_read_pcb_rev(void) {
  uint32_t pcbid = MARBLE_PCB_REV_XFORM(GPIOD->IDR);
  // Explicit case check rather than simple cast to catch unenumerated values
  // in 'default'
  switch ((Marble_PCB_Rev_t)pcbid) {
    case Marble_v1_3:
      marble_pcb_rev = Marble_v1_3;
      break;
    case Marble_v1_4: // Future support
      marble_pcb_rev = Marble_v1_4;
      break;
    case Marble_v1_5: // Future support
      marble_pcb_rev = Marble_v1_5;
      break;
    default:
      marble_pcb_rev = Marble_v1_2;
      break;
  }
  return;
}

static void SystemClock_Config(void)
{
   RCC_OscInitTypeDef RCC_OscInitStruct = {0};
   RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

   /** Initializes the CPU, AHB and APB busses clocks */
   RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
   RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
   RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
   RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
   RCC_OscInitStruct.PLL.PLLM = CONFIG_CLK_PLLM;
   RCC_OscInitStruct.PLL.PLLN = CONFIG_CLK_PLLN;
   RCC_OscInitStruct.PLL.PLLP = CONFIG_CLK_PLLP;
   RCC_OscInitStruct.PLL.PLLQ = 4;
   if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
   {
      Error_Handler();
   }
   /** Initializes the CPU, AHB and APB busses clocks */
   RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                               |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
   RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
   RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
   RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
   RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

   if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
   {
      Error_Handler();
   }
}

void SystemClock_Config_HSI(void)
{
   RCC_OscInitTypeDef RCC_OscInitStruct = {0};
   RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

   /** Initializes the CPU, AHB and APB busses clocks */
   RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
   RCC_OscInitStruct.HSIState = RCC_HSI_ON;
   RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
   RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
   RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
   RCC_OscInitStruct.PLL.PLLM = 13;
   RCC_OscInitStruct.PLL.PLLN = 195;
   RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
   RCC_OscInitStruct.PLL.PLLQ = 4;
   if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
   {
     Error_Handler();
   }
   /** Initializes the CPU, AHB and APB busses clocks */
   RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                               |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
   RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
   RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
   RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
   RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

   if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
   {
      Error_Handler();
   }
}

static void MX_ETH_Init(void)
{
   uint8_t MACAddr[6] ;

  heth.Instance = ETH;
  heth.Init.AutoNegotiation = ETH_AUTONEGOTIATION_DISABLE;
  heth.Init.PhyAddress = PHY_USER_NAME_PHY_ADDRESS;
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;
  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.RxMode = ETH_RXPOLLING_MODE;
  heth.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
  heth.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;

  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_I2C1_Init(void)
{
   hi2c1.Instance = I2C1;
   hi2c1.Init.ClockSpeed = SPEED_100KHZ;
   hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
   hi2c1.Init.OwnAddress1 = 0;
   hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
   hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
   hi2c1.Init.OwnAddress2 = 0;
   hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
   hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
   if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
      Error_Handler();
   }
   I2C_FPGA = &hi2c1;  // set global
}

static void MX_I2C3_Init(void)
{
   hi2c3.Instance = I2C3;
   hi2c3.Init.ClockSpeed = SPEED_100KHZ;
   hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
   hi2c3.Init.OwnAddress1 = 0;
   hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
   hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
   hi2c3.Init.OwnAddress2 = 0;
   hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
   hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
   if (HAL_I2C_Init(&hi2c3) != HAL_OK)
   {
      Error_Handler();
   }
   I2C_PM = &hi2c3;  // set global
}

static void MX_SPI1_Init(void)
{
   hspi1.Instance = SPI1;
   hspi1.Init.Mode = SPI_MODE_MASTER;
   hspi1.Init.Direction = SPI_DIRECTION_2LINES;
   hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
   hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
   hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
   hspi1.Init.NSS = SPI_NSS_SOFT;
   hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
   hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
   hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
   hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
   hspi1.Init.CRCPolynomial = 10;
   if (HAL_SPI_Init(&hspi1) != HAL_OK)
   {
      Error_Handler();
   }
   SSP_FPGA = &hspi1;
}

static void MX_SPI2_Init(void)
{
   hspi2.Instance = SPI2;
   hspi2.Init.Mode = SPI_MODE_MASTER;
   hspi2.Init.Direction = SPI_DIRECTION_2LINES;
   hspi2.Init.DataSize = SPI_DATASIZE_16BIT;
   hspi2.Init.CLKPolarity = SPI_POLARITY_HIGH;
   hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
   hspi2.Init.NSS = SPI_NSS_SOFT;
   hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
   hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
   hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
   hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
   hspi2.Init.CRCPolynomial = 10;
   if (HAL_SPI_Init(&hspi2) != HAL_OK)
   {
      Error_Handler();
   }
   SSP_PMOD = &hspi2;
}

static void SPI_CSB_SET(SSP_PORT ssp, bool set)
{
   if (ssp == SSP_FPGA) {
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, set ? GPIO_PIN_SET : GPIO_PIN_RESET);
   } else {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, set ? GPIO_PIN_SET : GPIO_PIN_RESET);
   }
}

/*
static void MX_USART1_UART_Init(void)
{
   huart_console.Instance = USART1;
   huart_console.Init.BaudRate = 115200;
   huart_console.Init.WordLength = UART_WORDLENGTH_8B;
   huart_console.Init.StopBits = UART_STOPBITS_1;
   huart_console.Init.Parity = UART_PARITY_NONE;
   huart_console.Init.Mode = UART_MODE_TX_RX;
   huart_console.Init.HwFlowCtl = UART_HWCONTROL_NONE;
   huart_console.Init.OverSampling = UART_OVERSAMPLING_16;
   if (HAL_UART_Init(&huart_console) != HAL_OK)
   {
      Error_Handler();
   }
   // Enable RXNE, TXE interrupts
   SET_BIT(huart_console.Instance->CR1, USART_CR1_RXNEIE);
}
*/

static void CONSOLE_USART_Init(void) {
#ifdef NUCLEO
  huart_console.Instance = USART3;
#else
  huart_console.Instance = USART1;
#endif
  huart_console.Init.BaudRate = 115200;
  huart_console.Init.WordLength = UART_WORDLENGTH_8B;
  huart_console.Init.StopBits = UART_STOPBITS_1;
  huart_console.Init.Parity = UART_PARITY_NONE;
  huart_console.Init.Mode = UART_MODE_TX_RX;
  huart_console.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart_console.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart_console) != HAL_OK)
  {
     Error_Handler();
  }
  // Enable RXNE, TXE interrupts
  SET_BIT(CONSOLE_USART->CR1, USART_CR1_RXNEIE);
  return;
}

static void MX_USART2_UART_Init(void)
{
   huart2.Instance = USART2;
   huart2.Init.BaudRate = 115200;
   huart2.Init.WordLength = UART_WORDLENGTH_8B;
   huart2.Init.StopBits = UART_STOPBITS_1;
   huart2.Init.Parity = UART_PARITY_NONE;
   huart2.Init.Mode = UART_MODE_TX_RX;
   huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
   huart2.Init.OverSampling = UART_OVERSAMPLING_16;
   if (HAL_UART_Init(&huart2) != HAL_OK)
   {
      Error_Handler();
   }
}

static void MX_GPIO_Init(void)
{
   GPIO_InitTypeDef GPIO_InitStruct = {0};

   /* GPIO Ports Clock Enable */
   __HAL_RCC_GPIOE_CLK_ENABLE();
   __HAL_RCC_GPIOH_CLK_ENABLE();
   __HAL_RCC_GPIOC_CLK_ENABLE();
   __HAL_RCC_GPIOA_CLK_ENABLE();
   __HAL_RCC_GPIOB_CLK_ENABLE();
   __HAL_RCC_GPIOD_CLK_ENABLE();

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOE,GPIO_PIN_0|GPIO_PIN_1| GPIO_PIN_2|GPIO_PIN_5|GPIO_PIN_8|GPIO_PIN_9
                           |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                           |GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_RESET);

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_1
                           |GPIO_PIN_2, GPIO_PIN_RESET);

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);

   /*Configure GPIO pins : PE2 PE8 PE9
                            PE10 PE11 PE12 PE13
                            PE14 PE15 PE0 PE1 */
   GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_8|GPIO_PIN_9
                           |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                           |GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

   /*Configure GPIO pins : PE5 (FPGA_RESETn) */
   GPIO_InitStruct.Pin = GPIO_PIN_5;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, true);
   HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

   /*Configure GPIO pins : PC7 PC8 */
   GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_8;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

   /*Configure GPIO pins : PC0 - PROG_B */
   GPIO_InitStruct.Pin = GPIO_PIN_0;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

   // N.B.: Order matters here; GPIO state must be set before it's init'd
   // so that there's no temporary glitch that pulls low PROG_B and thus
   // resets the FPGA
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, true);
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

   /*Configure GPIO pin : PA0, PA1, PA7 */
   GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

   /*Configure GPIO pin : PA3 - FPGA_INT (just an input for now; not an interrupt) */
   GPIO_InitStruct.Pin = GPIO_PIN_3;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

   /*Configure GPIO pins : PB0 PB1 PB2 */
   GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

   /*Configure GPIO pin : PB14 */
   GPIO_InitStruct.Pin = GPIO_PIN_14;
   GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

   /*Configure GPIO pin : PB15 */
   GPIO_InitStruct.Pin = GPIO_PIN_15;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;  // TEST
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

   /* Configure GPIO pins : PD9 PD10 PD11 PD1 PD2 */
   // GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_1|GPIO_PIN_2;
   GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_1
                           |GPIO_PIN_2;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

   /* Configure GPIO pins : Marble PCB Rev ID (PD12 PD13 PD14 PD15) */
   GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_PULLUP;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

   /* Configure GPIO pins : PD3 PD4 */
   GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

   /* Configure GPIO pin : PC12 */
   GPIO_InitStruct.Pin = GPIO_PIN_12;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

#if 0
static void TIM9_Init(void) {
  // Only one general-purpose FPGA input: PA3
  // Using TIM9 Ch2 automatic timer reset on input trigger
  // Enable clock to TIM9 via RCC APB2 clock enable register (APB2ENR)
  RCC->APB2ENR |= RCC_APB2ENR_TIM9EN;
  // GOTCHA! There's a sneaky /2 prescaler enabled whenever PCLK2 prescaler
  // is not /1 (as it is in this case).  So the input to TIM9 is probably
  // pclk2_freq_hz/2
  uint32_t psc = HAL_RCC_GetPCLK2Freq();  // Get PCLK2 freq
  psc = (psc/FPGA_WATCHDOG_CLK_FREQ) - 1; // Compute prescaler value
  TIM9->PSC = (uint16_t)(psc & 0xFFFF);
  TIM9->ARR = 0xFFFF;               // Max ARR for init; is configurable
  TIM9->CR1 &= ~TIM_CR1_CEN;        // counter disable

  TIM9->CCER &= ~TIM_CCER_CC2E_Msk; // disable ch2
  // SMCR
  uint32_t reg = TIM9->SMCR;
  reg = SET_FIELD_MASK(reg, 0 << TIM_SMCR_MSM_Pos, TIM_SMCR_MSM_Msk); // no synchronization needed
  reg = SET_FIELD_MASK(reg, 5 << TIM_SMCR_TS_Pos, TIM_SMCR_TS_Msk);   // Filtered Timer Input 2 (TI2FP2)
  reg = SET_FIELD_MASK(reg, 4 << TIM_SMCR_SMS_Pos, TIM_SMCR_SMS_Msk); // reset mode
  TIM9->SMCR = reg;

  // CR1
  reg = TIM9->CR1 | TIM_CR1_ARPE_Msk | TIM_CR1_URS_Msk; // Auto-reload preload enable and prevent
  TIM9->CR1 = reg;                                      // IRQ on setting of UG bit (software pet)

  // DIER
  TIM9->DIER |= TIM_DIER_UIE_Msk;   // Enable update interrupt?

  // CCMR1
  reg = TIM9->CCMR1;
  reg = SET_FIELD_MASK(reg, 0xF << TIM_CCMR1_IC2F_Pos, TIM_CCMR1_IC2F_Msk); // f_sample = f_dts/32; N = 8
  reg = SET_FIELD_MASK(reg, 0 << TIM_CCMR1_IC2PSC_Pos, TIM_CCMR1_IC2PSC_Msk); // capture on every edge
  reg = SET_FIELD_MASK(reg, 1 << TIM_CCMR1_CC2S_Pos, TIM_CCMR1_CC2S_Msk); // ch2 input -> TI2
  TIM9->CCMR1 = reg;

  // CCER
  reg = TIM9->CCER;
  reg = SET_FIELD_MASK(reg, 0 << TIM_CCER_CC2P_Pos, TIM_CCER_CC2P_Msk); // non-inverted input, reset on rising edge
  reg = SET_FIELD_MASK(reg, 1 << TIM_CCER_CC2E_Pos, TIM_CCER_CC2E_Msk); // enable ch2
  TIM9->CCER = reg;

  //TIM9->CR1 |= TIM_CR1_CEN;         // counter enable. Do not enable at startup for now.

  // Enable TIM1_BRK_TIM9_IRQn;
  HAL_NVIC_SetPriority(TIM1_BRK_TIM9_IRQn, 7, 7);
  HAL_NVIC_EnableIRQ(TIM1_BRK_TIM9_IRQn);
  return;
}
#endif

void FPGAWD_set_period(uint16_t preload) {
  if (preload) {
  // disable timer, set preload, and restart timer
    TIM9->CR1 &= ~TIM_CR1_CEN; // disable counter
    TIM9->ARR = preload;
    TIM9->CR1 |= TIM_CR1_CEN;  // enable counter
    HAL_NVIC_EnableIRQ(TIM1_BRK_TIM9_IRQn);
  } else {
  // If preload == 0, disable timer and interrupt
    TIM9->ARR = 0xFFFF;
    TIM9->CR1 &= ~TIM_CR1_CEN; // disable counter
    HAL_NVIC_DisableIRQ(TIM1_BRK_TIM9_IRQn);
  }
  return;
}

void FPGAWD_pet(void) {
  // Software pet the watchdog
  TIM9->EGR |= TIM_EGR_UG;
  return;
}

void FPGAWD_ISR(void) {
  // Reset the FPGA (if preload == 0, this should never fire)
  /* Pull the PROGRAM_B pin low; it's spelled PROG_B on schematic */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, false);
  console_pend_FPGA_enable();
  return;
}

int getI2CBusStatus(void) {
  return i2cBusStatus;
}

void resetI2CBusStatus(void) {
  i2cBusStatus = 0;
  return;
}

/*
 * void marble_mailbox_mode_IRQ(void);
 *  Switch mailbox pin mode to listen for interrupt requests on the
 *  MISO line.
 */
void marble_mailbox_mode_IRQ(void) {
  _SPI1_switch_to_EXTI();
  return;
}

/*
 * void marble_mailbox_mode_control(void);
 *  Switch mailbox pin mode to normal SPI controller mode.  This
 *  disables the MISO IRQ functionality and is required for
 *  successful SPI reads.
 */
void marble_mailbox_mode_control(void) {
  _SPI1_switch_to_MISO();
  return;
}

static void marble_enable_MISO_ISR(void) {
  NVIC_SetPriority(EXTI9_5_IRQn, 0);
  NVIC_EnableIRQ(EXTI9_5_IRQn);
  return;
}

static void marble_disable_MISO_ISR(void) {
  NVIC_DisableIRQ(EXTI9_5_IRQn);
  return;
}

static void _SPI1_switch_to_MISO(void) {
  // Set alternate function
  LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_6, LL_GPIO_AF_5);
  // Set PA6 as AF-push-pull mode
  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_6, LL_GPIO_MODE_ALTERNATE);
  // Remove any pull
  LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_6, LL_GPIO_PULL_NO);
  marble_disable_MISO_ISR();
  return;
}

static void _SPI1_switch_to_EXTI(void) {
  // Alternate function ignored
  // Set pin mode to input
  LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_6, LL_GPIO_MODE_INPUT);
  // Set pulldown
  LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_6, LL_GPIO_PULL_DOWN);
  // Ensure EXTI6 is configured to get ISRs from PORTA in EXTICR2
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  uint32_t temp = SYSCFG->EXTICR[1];  // EXTICR2
  SYSCFG->EXTICR[1] = temp & 0xf0ff;  // Easy enough bit mask
  // Configure trigger in EXTI
  LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_6);
  LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_6);
  LL_EXTI_DisableFallingTrig_0_31(LL_EXTI_LINE_6);
  // Enable EXTI6 ISR in NVIC
  marble_enable_MISO_ISR();
  return;
}

void marble_mbox_MISO_ISR(void) {
  if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_6)) {
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_6);
    mbox_pend_request();
  }
  return;
}

uint8_t marble_pmod_get_gpio(void) {
  // TODO - Write to GPIO
  return 0;
}

void marble_pmod_set_gpio(uint8_t data) {

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */

#ifdef  USE_FULL_ASSERT
/**
   * @brief  Reports the name of the source file and the source line number
   *         where the assert_param error has occurred.
   * @param  file: pointer to the source file name
   * @param  line: assert_param error line source number
   * @retval None
*/
void assert_failed(uint8_t *file, uint32_t line)
{
   /* USER CODE BEGIN 6 */
   /* User can add his own implementation to report the file name and line number,
      tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
   /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

