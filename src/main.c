/* 
 * Demo C Application: Toggles an output at 20Hz.
 * Copyright (C) 2013  Richard Meadows
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "chip.h"
#include "marble1.h"

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/* System oscillator rate and RTC oscillator rate */
const uint32_t OscRateIn = 25000000;
const uint32_t RTCOscRateIn = 32768;

int main (void) {

  /* Update the value of SystemCoreClock */
  SystemCoreClockUpdate(); // Expect 120 MHz
  Chip_SystemInit(); // Must come after SystemCoreClockUpdate()

  /* Set an LED output */
  // LPC_GPIO0->DIR |= 1 << 22;
  LPC_GPIO2->DIR |= 1 << 21; // LD 15
  LPC_GPIO2->SET |= (1 << 21);  // LD15 on

  LPC_GPIO2->DIR |= 1 << 25;
  LPC_GPIO2->SET |= (1 << 25);  // LD13 on

  LPC_GPIO2->DIR |= 1 << 24;
  LPC_GPIO2->SET |= (1 << 24);  // LD14 on

  LPC_GPIO1->DIR |= 1 << 27;
  LPC_GPIO1->SET |= (1 << 27);  // EN_FMC1_P12V

  LPC_GPIO1->DIR |= 1 << 19;
  LPC_GPIO1->SET |= (1 << 19);  // EN_FMC2_P12V

  /* Configure the SysTick for 1 s interrupts */
  SysTick_Config(SystemCoreClock / 1);

  ssp_init();
  unsigned char mac_ip_data[10] = {
      18, 85, 85, 0, 1, 46,  // MAC (locally managed)
      192, 168, 19, 31   // IP
  };
  int push_button=1;
  while (1) {
      int push_button_new = (LPC_GPIO2->PIN >> 12) & 1;  // SW2 or SW3
      int fpga_int = (LPC_GPIO->PIN >> 19) & 1;  // debug hook
      if (fpga_int || (!push_button_new && push_button)) {  // falling edge detect
          push_fpga_mac_ip(mac_ip_data);
          for (unsigned ix=0; ix<6000; ix++) { (void) LPC_SSP0->SR; }  // doze
      }
      push_button = push_button_new;
      // No debounce, might trigger on button release as well
  }
}

extern void SysTick_Handler(void) {
  /* Toggle LEDs */
  LPC_GPIO2->PIN ^= 1 << 21;
  LPC_GPIO2->PIN ^= 1 << 25;
  LPC_GPIO2->PIN ^= 1 << 24;
}
