/*
 * Based in part on (and therefore legally derived from):
 * Embedded Artists LPC1788 and LPC4088 Development Kit board support files
 *
 * which is Copyright(C) NXP Semiconductors, 2012 and
 * Copyright(C) NXP Semiconductors, 2014
 * All rights reserved.
 *
 * The licensing terms of that file require this file to contain the following:
 *
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "chip.h"
#include "stopwatch.h"
#include "marble_api.h"
#include "string.h"
#include <stdio.h>

/************
* Clocking
************/

/* System oscillator rate and RTC oscillator rate */
const uint32_t OscRateIn = 25000000;
const uint32_t RTCOscRateIn = 32768;

/************
* UART
************/

/* UART ring buffer and baud rate */
#define UART_BAUD_RATE 115200
STATIC RINGBUFF_T txring, rxring;

#define UART_SEND_RB_SZ 128
#define UART_RECV_RB_SZ 32
static uint8_t rxbuff[UART_RECV_RB_SZ], txbuff[UART_SEND_RB_SZ];

// Override default (weak) IRQHandler
void UART0_IRQHandler(void)
{
   Chip_UART_IRQRBHandler(LPC_UART0, &rxring, &txring);
}

/* Initialize UART pins */
void marble_UART_init(void)
{
   /* Initialize UART0 pin connect
    * P0.2: TXD
    * P0.3: RXD */
   Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 2, (IOCON_FUNC1 | IOCON_MODE_INACT));
   Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 3, (IOCON_FUNC1 | IOCON_MODE_INACT));

   // Setup UART built-in FIFOs
   Chip_UART_SetBaud(LPC_UART0, UART_BAUD_RATE);
   Chip_UART_ConfigData(LPC_UART0, (UART_LCR_WLEN8 | UART_LCR_SBS_1BIT));
   Chip_UART_SetupFIFOS(LPC_UART0, (UART_FCR_FIFO_EN | UART_FCR_TRG_LEV2));
   Chip_UART_TXEnable(LPC_UART0);

   // Before using the ring buffers, initialize them using the ring buffer init function
   RingBuffer_Init(&rxring, rxbuff, 1, UART_RECV_RB_SZ);
   RingBuffer_Init(&txring, txbuff, 1, UART_SEND_RB_SZ);

   /* Reset and enable FIFOs, FIFO trigger level 3 (14 chars) */
   Chip_UART_SetupFIFOS(LPC_UART0, (UART_FCR_FIFO_EN | UART_FCR_RX_RS |
                                    UART_FCR_TX_RS | UART_FCR_TRG_LEV3));

   /* Enable receive data and line status interrupt */
   Chip_UART_IntEnable(LPC_UART0, (UART_IER_RBRINT | UART_IER_RLSINT));

   /* preemption = 1, sub-priority = 1 */
   NVIC_SetPriority(UART0_IRQn, 1);
   NVIC_EnableIRQ(UART0_IRQn);
}

#define UART_WAIT 100000

/* Send \0 terminated string over UART. Returns number of bytes sent */
int marble_UART_send(const char *str, int size)
{
   int sent=0;
   do {
      sent += Chip_UART_SendRB(LPC_UART0, &txring, str+sent, size-sent);
      for (int wait_cnt = UART_WAIT; wait_cnt > 0; wait_cnt--) {} // Busy-wait
   } while (sent < size);

   return sent;
}

/* Read at most size-1 bytes (due to \0) from UART. Returns bytes read */
int marble_UART_recv(char *str, int size)
{
   return Chip_UART_ReadRB(LPC_UART0, &rxring, str, size);
}

/************
* LEDs
************/

#define MAXLEDS 3
static const uint8_t ledports[MAXLEDS] = {2, 2, 2};
static const uint8_t ledpins[MAXLEDS] = {21, 25, 24};

/* Initializes board LED(s) */
static void marble_LED_init(void)
{
   int i;

   /* Setup port direction and initial output state */
   for (i = 0; i < MAXLEDS; i++) {
      Chip_GPIO_WriteDirBit(LPC_GPIO, ledports[i], ledpins[i], true);
      Chip_GPIO_WritePortBit(LPC_GPIO, ledports[i], ledpins[i], true);
   }
}

/* Sets the state of a board LED to on or off */
void marble_LED_set(uint8_t led_num, bool on)
{
   if (led_num < MAXLEDS) {
      /* Set state, low is on, high is off */
      Chip_GPIO_SetPinState(LPC_GPIO, ledports[led_num], ledpins[led_num], !on);
   }
}

/* Returns the current state of a board LED */
bool marble_LED_get(uint8_t led_num)
{
   bool state = false;

   if (led_num < MAXLEDS) {
      state = Chip_GPIO_GetPinState(LPC_GPIO, ledports[led_num], ledpins[led_num]);
   }

   /* These LEDs are reverse logic. */
   return !state;
}

/* Toggles the current state of a board LED */
void marble_LED_toggle(uint8_t led_num)
{
   if (led_num < MAXLEDS) {
      Chip_GPIO_SetPinToggle(LPC_GPIO, ledports[led_num], ledpins[led_num]);
   }
}

/************
* FMC & PSU
************/

/* Set FMC power */
void marble_FMC_pwr(bool on)
{
   const uint8_t fmc_port = 1;
   // EN_FMC1_P12V
   const uint8_t fmc1_pin = 27;
   Chip_GPIO_WriteDirBit(LPC_GPIO, fmc_port, fmc1_pin, true);
   Chip_GPIO_WritePortBit(LPC_GPIO, fmc_port, fmc1_pin, on);

   // EN_FMC2_P12V
   const uint8_t fmc2_pin = 19;
   Chip_GPIO_WriteDirBit(LPC_GPIO, fmc_port, fmc2_pin, true);
   Chip_GPIO_WritePortBit(LPC_GPIO, fmc_port, fmc2_pin, on);
}

uint8_t marble_FMC_status(void)
{
   uint8_t status = 0;
   status = WRITE_BIT(status, M_FMC_STATUS_FMC1_PWR,  Chip_GPIO_ReadPortBit(LPC_GPIO, 1, 27));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC1_FUSE, Chip_GPIO_ReadPortBit(LPC_GPIO, 0, 23));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC2_PWR,  Chip_GPIO_ReadPortBit(LPC_GPIO, 1, 19));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC2_FUSE, Chip_GPIO_ReadPortBit(LPC_GPIO, 0, 24));
   return status;
}

void marble_PSU_pwr(bool on)
{
   Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 31, true);
   Chip_GPIO_WritePortBit(LPC_GPIO, 1, 31, on);
}

uint8_t marble_PWR_status(void)
{
   uint8_t status = 0;
   status = WRITE_BIT(status, M_PWR_STATUS_PSU_EN, Chip_GPIO_ReadPortBit(LPC_GPIO, 1, 31));
   status = WRITE_BIT(status, M_PWR_STATUS_POE,    Chip_GPIO_ReadPortBit(LPC_GPIO, 1, 0));
   status = WRITE_BIT(status, M_PWR_STATUS_OTEMP,  Chip_GPIO_ReadPortBit(LPC_GPIO, 0, 29));
   return status;
}


/************
* Switches and FPGA interrupt
************/

static void marble_SW_init(void)
{
   // SW1 and SW2
   Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 12, false);
}

bool marble_SW_get(void)
{
   // Pin pulled low on button press
   if (Chip_GPIO_ReadPortBit(LPC_GPIO, 2, 12) == 0x01) {
      return false;
   }
   return true;
}

bool marble_FPGAint_get(void)
{
   if (Chip_GPIO_ReadPortBit(LPC_GPIO, 0, 19) == 0x00) {
      return false;
   }
   return true;
}

void reset_fpga(void)
{
   /* Pulse the PROGRAM_B pin low; it's spelled PROG_B on schematic */
   const uint8_t rst_port = 0;
   const uint8_t rst_pin = 4;
   Chip_GPIO_WriteDirBit(LPC_GPIO, rst_port, rst_pin, true);
   for (int wait_cnt = 20; wait_cnt > 0; wait_cnt--) {
      /* Only the first call does anything, but the compiler
       * doesn't know that.  That keeps its optimizer from
       * eliminating the associated time delay.
       */
      Chip_GPIO_WritePortBit(LPC_GPIO, rst_port, rst_pin, false);
   }
   Chip_GPIO_WritePortBit(LPC_GPIO, rst_port, rst_pin, true);
   Chip_GPIO_WriteDirBit(LPC_GPIO, rst_port, rst_pin, false);
}

/************
* GPIO interrupt setup and user-defined handlers
************/
const uint8_t FPGA_DONE_INT_PIN = 5;
void FPGA_DONE_dummy(void) {}
void (*volatile marble_FPGA_DONE_handler)(void) = FPGA_DONE_dummy;

// Override default (weak) IRQHandler
void GPIO_IRQHandler(void)
{
   uint32_t gpioint_rs = Chip_GPIOINT_GetStatusRising(LPC_GPIOINT, GPIOINT_PORT0);
   /* FPGA_DONE P0[5] */
   Chip_GPIOINT_ClearIntStatus(LPC_GPIOINT, GPIOINT_PORT0, 1 << FPGA_DONE_INT_PIN);

   if ((gpioint_rs & (1 << FPGA_DONE_INT_PIN)) != 0) {
      if (marble_FPGA_DONE_handler) {
         marble_FPGA_DONE_handler();
      }
   }
}

void marble_GPIOint_init(void)
{
   /* FPGA_DONE P0[5] */
   /* Configure GPIO interrupt pin as input and rising-edge */
   Chip_GPIO_WriteDirBit(LPC_GPIO, GPIOINT_PORT0, FPGA_DONE_INT_PIN, false);
   Chip_GPIOINT_SetIntRising(LPC_GPIOINT, GPIOINT_PORT0, 1 << FPGA_DONE_INT_PIN);

   /* Enable interrupt in the NVIC */
   NVIC_ClearPendingIRQ(GPIO_IRQn);
   NVIC_EnableIRQ(GPIO_IRQn);
}

/* Register user-defined interrupt handlers */
void marble_GPIOint_handlers(void (*FPGA_DONE_handler)(void)) {
   marble_FPGA_DONE_handler = FPGA_DONE_handler;
}

/************
* I2C
************/
#define SPEED_100KHZ 100000
#define I2C_POLL 1

static void i2c_state_handling(I2C_ID_T id)
{
   if (Chip_I2C_IsMasterActive(id)) {
      Chip_I2C_MasterStateHandler(id);
   } else {
      Chip_I2C_SlaveStateHandler(id);
   }
}

// Override default (weak) IRQHandlers
void I2C0_IRQHandler(void)
{
   i2c_state_handling(I2C0);
}
void I2C1_IRQHandler(void)
{
   i2c_state_handling(I2C1);
}
void I2C2_IRQHandler(void)
{
   i2c_state_handling(I2C2);
}

static void marble_I2C_pins(I2C_ID_T id)
{
   switch (id) {
      case I2C0:
         Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 27, (IOCON_MODE_INACT | IOCON_FUNC1));
         Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 28, (IOCON_MODE_INACT | IOCON_FUNC1));
         // Primary I2C pins that are always open-drain
         break;
      case I2C1:
         Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 0, (IOCON_MODE_INACT | IOCON_OPENDRAIN_EN | IOCON_FUNC3));
         Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 1, (IOCON_MODE_INACT | IOCON_OPENDRAIN_EN | IOCON_FUNC3));
         break;
      case I2C2:
         Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 10, (IOCON_MODE_INACT | IOCON_OPENDRAIN_EN | IOCON_FUNC2));
         Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 11, (IOCON_MODE_INACT | IOCON_OPENDRAIN_EN | IOCON_FUNC2));
         break;
      default:
         break;  // unknown id, fail silently
   }
}

/* Initialize the I2C bus */
static void marble_I2C_init(I2C_ID_T id, int poll)
{
   // Configure pins
   marble_I2C_pins(id);

   /* Initialize I2C */
   Chip_I2C_Init(id);
   Chip_I2C_SetClockRate(id, SPEED_100KHZ);

   if (!poll) {
      Chip_I2C_SetMasterEventHandler(id, Chip_I2C_EventHandler);
      NVIC_EnableIRQ(id == I2C0 ? I2C0_IRQn : id == I2C1 ? I2C1_IRQn : I2C2_IRQn);
   } else {
      NVIC_DisableIRQ(id == I2C0 ? I2C0_IRQn : id == I2C1 ? I2C1_IRQn : I2C2_IRQn);
      Chip_I2C_SetMasterEventHandler(id, Chip_I2C_EventHandlerPolling);
   }
}

/* Non-destructive I2C probe function based on a single-byte read; Probe with no
   data payload, i.e. S+[A,RW]+P, is not supported by this chip.
*/
int marble_I2C_probe(I2C_BUS I2C_bus, uint8_t addr) {
   uint8_t data;
   addr = addr >> 1;
   return Chip_I2C_MasterRead(I2C_bus, addr, &data, 1) != 1;
}

/* Generic I2C send function with selectable I2C bus and 8-bit I2C addresses (R/W bit = 0) */
/* For compatiblity with STM32 code base (!?),
 * return 0 on success, 1 on failure */
int marble_I2C_send(I2C_BUS I2C_bus, uint8_t addr, const uint8_t *data, int size) {
   addr = addr >> 1;
   return Chip_I2C_MasterSend(I2C_bus, addr, data, size) != size;
}

int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size) {
   addr = addr >> 1;
   return Chip_I2C_MasterRead(I2C_bus, addr, data, size) != size;
}

int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
   addr = addr >> 1;
   return Chip_I2C_MasterCmdRead(I2C_bus, addr, cmd, data, size) != size;
}

int marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
   // Crude hack, prepending 8-bit cmd to the data array
   uint8_t ldata[8];
   if (size > 7) return 1;  // failure
   ldata[0] = cmd;
   memcpy(ldata+1, data, size);
   return marble_I2C_send(I2C_bus, addr, ldata, size+1);
}

int marble_I2C_cmdsend_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
   // Crude hack, prepending 16-bit cmd to the data array
   uint8_t ldata[8];
   if (size > 6) return 1;  // failure
   ldata[0] = cmd >> 8;
   ldata[1] = cmd & 0xff;
   memcpy(ldata+2, data, size);
   return marble_I2C_send(I2C_bus, addr, ldata, size+2);
}

/* Transmit two bytes and receive an array of bytes after a repeated start condition is generated */
int marble_I2C_cmdrecv_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
   // Swap cmd endianness
   uint8_t cmd_be[2];
   cmd_be[0] = cmd >> 8;
   cmd_be[1] = cmd & 0xff;

   // Setup low-level transfer
   // Based on Chip_I2C_MasterCmdRead in i2c_17xx_40xx.c
   I2C_XFER_T xfer = {0};
   xfer.slaveAddr = addr >> 1;
   xfer.txBuff = cmd_be;
   xfer.txSz = 2;
   xfer.rxBuff = data;
   xfer.rxSz = size;
   while (Chip_I2C_MasterTransfer(I2C_bus, &xfer) == I2C_STATUS_ARBLOST) {}
   // printf("marble_I2C_cmdrecv_a2: %u %u\n", i2cx, xfer.rxSz);
   return xfer.rxSz != 0;
}

/************
* SSP/SPI
************/
static void marble_SSP_pins(LPC_SSP_T *ssp)
{
   if (ssp == LPC_SSP1) {
      Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 7, (IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_DIGMODE_EN)); // SCK
      Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 6, (IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_DIGMODE_EN)); // SSEL
      Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 8, (IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_DIGMODE_EN)); // MISO
      Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 9, (IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_DIGMODE_EN)); // MOSI
   } else if (ssp == LPC_SSP0) {
      Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 15, (IOCON_FUNC2 | IOCON_MODE_INACT)); // SCK
      Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 16, (IOCON_FUNC2 | IOCON_MODE_INACT)); // SSEL
      Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 17, (IOCON_FUNC2 | IOCON_MODE_INACT)); // MISO
      Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 18, (IOCON_FUNC2 | IOCON_MODE_INACT)); // MOSI
   }
}

static void marble_SSP_init(LPC_SSP_T *ssp)
{
   marble_SSP_pins(ssp);

   CHIP_SYSCTL_CLOCK_T clkSSP;
   if (ssp == LPC_SSP1) {
      clkSSP = SYSCTL_CLOCK_SSP1;
   }
   else {
      clkSSP = SYSCTL_CLOCK_SSP0;
   }
   Chip_Clock_EnablePeriphClock(clkSSP);
   Chip_SSP_Set_Mode(ssp, SSP_MODE_MASTER);

   // 16-bit Frame, SPI, Clock Polarity High
   Chip_SSP_SetFormat(ssp, SSP_BITS_16, SSP_FRAMEFORMAT_SPI, SSP_CLOCK_CPHA0_CPOL1);
   Chip_SSP_SetBitRate(ssp, 100000);
   Chip_SSP_Enable(ssp);

   // No interrupt mode for now
   //NVIC_EnableIRQ(SSP_IRQ);
}

int marble_SSP_write16(SSP_PORT ssp, uint16_t *buffer, unsigned size)
{
   return Chip_SSP_WriteFrames_Blocking(ssp, (uint8_t*) buffer, size*2); // API expectes length in bytes
}

int marble_SSP_read16(SSP_PORT ssp, uint16_t *buffer, unsigned size)
{
   return Chip_SSP_ReadFrames_Blocking(ssp, (uint8_t*) buffer, size*2);
}

int marble_SSP_exch16(SSP_PORT ssp, uint16_t *tx_buf, uint16_t *rx_buf, unsigned size)
{
   Chip_SSP_DATA_SETUP_T set;
   set.tx_data = tx_buf;
   set.tx_cnt = 0;
   set.rx_data = rx_buf;
   set.rx_cnt = 0;
   set.length = size;
   return Chip_SSP_RWFrames_Blocking(ssp, &set);
}

/************
* MDIO to PHY
************/
void marble_MDIO_init(void)
{
   Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_ENET);

   /* Setup MII clock rate and PHY address */
   Chip_ENET_SetupMII(LPC_ETHERNET, Chip_ENET_FindMIIDiv(LPC_ETHERNET, 2500000), 0);
}

void marble_MDIO_write(uint16_t reg, uint32_t data)
{
   Chip_ENET_StartMIIWrite(LPC_ETHERNET, reg, data);
   while (Chip_ENET_IsMIIBusy(LPC_ETHERNET));
}

uint32_t marble_MDIO_read(uint16_t reg)
{
   Chip_ENET_StartMIIRead(LPC_ETHERNET, reg);
   while (Chip_ENET_IsMIIBusy(LPC_ETHERNET));
   return Chip_ENET_ReadMIIData(LPC_ETHERNET);
}

/************
* System Timer and Stopwatch
************/
void SysTick_Handler_dummy(void) {}
void (*volatile marble_SysTick_Handler)(void) = SysTick_Handler_dummy;

// Override default (weak) SysTick_Handler
void SysTick_Handler(void)
{
   if (marble_SysTick_Handler)
      marble_SysTick_Handler();
}

/* Register user-defined interrupt handlers */
void marble_SYSTIMER_handler(void (*handler)(void)) {
   marble_SysTick_Handler = handler;
}

/* Configures 24-bit count-down timer and enables systimer interrupt */
uint32_t marble_SYSTIMER_ms(uint32_t delay)
{
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
   StopWatch_DelayMs(delay);
}

void marble_SLEEP_us(uint32_t delay)
{
   StopWatch_DelayUs(delay);
}

/************
* Board Init
************/

uint32_t marble_init(bool use_xtal)
{
   // Must happen before any other clock manipulations:
   SystemCoreClockUpdate(); /* Update the value of SystemCoreClock */

   if (use_xtal) {
      Chip_SetupXtalClocking();
   } else {
      Chip_SetupIrcClocking(); // 120 MHz based on 12 MHz internal clock
   }

   SystemCoreClockUpdate(); /* Update the value of SystemCoreClock (120 MHz) */

   // Initialize system stopwatch
   StopWatch_Init();

   marble_LED_init();
   marble_SW_init();
   marble_UART_init();

   // Init GPIO interrupts
   marble_GPIOint_init();

   // Init I2C busses in interrupt mode
   marble_I2C_init(I2C0, !I2C_POLL);
   marble_I2C_init(I2C1, !I2C_POLL);
   marble_I2C_init(I2C2, !I2C_POLL);
   I2C_IPMB = I2C0;
   I2C_PM = I2C1;
   I2C_FPGA = I2C2;

   // Init SSP busses
   marble_SSP_init(LPC_SSP0);
   //marble_SSP_init(LPC_SSP1);
   SSP_FPGA = LPC_SSP0;
   SSP_PMOD = LPC_SSP1;

   marble_MDIO_init();

   return SystemCoreClock;
}
