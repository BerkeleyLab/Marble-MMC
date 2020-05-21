/*
 * Based in part on (and therefore legally derived from):
 * Embedded Artists LPC1788 and LPC4088 Development Kit board file (board.c)
 *
 * which is Copyright(C) NXP Semiconductors, 2012
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

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/* System oscillator rate and RTC oscillator rate */
const uint32_t OscRateIn = 25000000;
const uint32_t RTCOscRateIn = 32768;

/* Initialize UART pins */
static void marble_UART_init(void)
{
   /*
    * Initialize UART0 pin connect
    * P0.2: TXD
    * P0.3: RXD
   */
   Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 2, (IOCON_FUNC1 | IOCON_MODE_INACT));
   Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 3, (IOCON_FUNC1 | IOCON_MODE_INACT));
}

/* Sends a character on the UART */
void marble_UART_putch(char ch)
{
   while ((Chip_UART_ReadLineStatus(LPC_UART0) & UART_LSR_THRE) == 0) {}
   Chip_UART_SendByte(LPC_UART0, (uint8_t) ch);
}

/* Gets a character from the UART, returns EOF if no character is ready */
int marble_UART_getch(void)
{
   if (Chip_UART_ReadLineStatus(LPC_UART0) & UART_LSR_RDR) {
      return (int) Chip_UART_ReadByte(LPC_UART0);
   }
   return EOF;
}

/* Outputs a string on the debug UART */
void marble_UART_putstr(char *str)
{
   while (*str != '\0') {
      Board_UARTPutChar(*str++);
   }
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
   if (Chip_GPIO_ReadPortBit(LPC_GPIO, 2, 12) == 0x00) {
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

   marble_UART_init();
   marble_LED_init();
   marble_SW_init();
}

