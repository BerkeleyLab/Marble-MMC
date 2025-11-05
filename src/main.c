#include <string.h>
#include <stdio.h>
#include "marble_api.h"
#include "uart_fifo.h"
#include "i2c_pm.h"
#include "i2c_fpga.h"
#include "ltm4673.h"
#include "watchdog.h"

#define LED_SNAKE

// CLOCK_USE_XTAL only used for marble-mini
#ifdef CLOCK_USE_XTAL
#define XTAL_IN_USE     "True"
#else
#define XTAL_IN_USE     "False"
#endif

int main(void) {
   // Start by disabling interrupts just in case
   disable_all_IRQs();

   UARTQUEUE_Init();
#ifdef MARBLEM_V1
   uint32_t sysclk_freq = marble_init();
   // Initialize Marble(mini) board with IRC, so it works even when
   // the XRP7724 isn't running, keeping the final 25 MHz source away.
   printf("marble_init with use_xtal = %s\r\n", XTAL_IN_USE);
   printf("system clock = %lu Hz\r\n", sysclk_freq);
#else
   marble_init();
#endif
   system_init();

   /* Turn on LEDs */
   marble_LED_set(0, true);   // LD15
   marble_LED_set(1, true);   // LD11
   marble_LED_set(2, true);   // LD12

   // Boot the power supply controller if needed
   pwr_autoboot();

   // Initialize off-chip components
   board_init();

   // Power FMCs
   marble_FMC_pwr(true);

   if (1) {
      printf("** Policy: reset FPGA on MMC reset.  Doing it now. **\r\n");
      FPGAWD_SelfReset();
      printf("**\r\n");
   }

   // Send demo string over UART at 115200 BAUD
   marble_UART_send(DEMO_STRING, strlen(DEMO_STRING));

   while (1) {
      // Service system (application logic)
      system_service();
      // Service platform-specific functionality
      if (board_service()) {
        // This exit is only used in simulation
        break;
      }
   }
   cleanup(); // Only used for simulation
}

// This probably belongs in some other file, but which one?
int __io_putchar(int ch);
int __io_putchar(int ch)
{
  marble_UART_send((const char *)&ch, 1);
  return ch;
}
