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
#include "string.h"
#include <stdio.h>

/* System oscillator rate and RTC oscillator rate */
const uint32_t OscRateIn = 25000000;
const uint32_t RTCOscRateIn = 32768;

/* UART ring buffer and othersetup */
#define UART_BAUD_RATE 115200
STATIC RINGBUFF_T txring, rxring;

#define UART_SEND_RB_SZ 128
#define UART_RECV_RB_SZ 32
static uint8_t rxbuff[UART_RECV_RB_SZ], txbuff[UART_SEND_RB_SZ];

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

/* Send \0 terminated string over UART. Returns number of bytes sent */
int marble_UART_send(char *str, int size)
{
   return Chip_UART_SendRB(LPC_UART0, &txring, str, size);
}

/* Read at most size-1 bytes (due to \0) from UART. Returns bytes read */
int marble_UART_recv(char *str, int size)
{
   return Chip_UART_ReadRB(LPC_UART0, &rxring, str, size);
}

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

void marble_init(bool use_xtal)
{
   // Must happen before any other clock manipulations:
   SystemCoreClockUpdate(); /* Update the value of SystemCoreClock */

   if (use_xtal) {
      Chip_SetupXtalClocking();
   } else {
      Chip_SetupIrcClocking(); // 120 MHz based on 12 MHz internal clock
   }

   marble_LED_init();
   marble_SW_init();
}

