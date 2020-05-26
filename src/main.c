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

#include "chip.h" // TODO: Remove after wrapping SSP
#include "marble_api.h"
#include "marble1.h"

// Setup UART strings
const char demo_str[] = "Marble Mini v1 UART DEMO\r\n";
const char lb_str[] = "Loopback... ESC to exit\r\n";
const char menu_str[] = "\r\nMenu:\r\n0) Loopback\r\n1) FPGA IP/MAC update\r\n2) I2C monitor\r\n3) MDIO status\r\n";
const char unk_str[] = "> Unknown option\r\n";

int main (void) {

   // Initialize Marble(mini) board with IRC
   marble_init(false);

   /* Turn on LEDs */
   marble_LED_set(0, true);
   marble_LED_set(1, true);
   marble_LED_set(2, true);

   // Power FMCs
   marble_FMC_pwr(true);

   // Initialize UART
   marble_UART_init();

   /* Configure the SysTick for 1 s interrupts */
   SysTick_Config(SystemCoreClock * 1);

   // TODO: Refactor ssp_init() into marble_board.c
   ssp_init();

   // Send demo string over UART at 115200 BAUD
   marble_UART_send(demo_str, strlen(demo_str));
   char rx_ch;
   while (true) {
      marble_UART_send(menu_str, strlen(menu_str));
      // Wait for user selection
      while(marble_UART_recv(&rx_ch, 1) == 0);
      switch (rx_ch) {
         case '0':
            marble_UART_send(lb_str, strlen(lb_str));
            do {
               if (marble_UART_recv(&rx_ch, 1) != 0) {
                  marble_UART_send(&rx_ch, 1);
               }
            } while (rx_ch != 27);
            marble_UART_send("\r\n", 2);
            break;
         case '1':
         case '2':
         case '3':
         default:
            marble_UART_send(unk_str, strlen(unk_str));
            break;
      }
   }

   unsigned char mac_ip_data[10] = {
      18, 85, 85, 0, 1, 46,  // MAC (locally managed)
      192, 168, 19, 31   // IP
   };
   int push_button=1;
   while (1) {
      int push_button_new = marble_SW_get();  // SW2 or SW3
      int fpga_int = marble_FPGAint_get();  // debug hook
      if (fpga_int || (!push_button_new && push_button)) {  // falling edge detect
         push_fpga_mac_ip(mac_ip_data);
         for (unsigned ix=0; ix<6000; ix++) { (void) LPC_SSP0->SR; }  // doze
      }
      push_button = push_button_new;
      // No debounce, might trigger on button release as well
  }
}

extern void SysTick_Handler(void) {
   marble_LED_toggle(0);
   marble_LED_toggle(1);
   marble_LED_toggle(2);
}
