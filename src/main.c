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
//   printf("\r\nInitializing:\r\n");
   printf("+++++++++++++++++++++++++++ | Initializing MMC | ++++++++++++++++++++++++++++\r\n");
#ifdef MARBLEM_V1
   uint32_t sysclk_freq = marble_init();
   // Initialize Marble(mini) board with IRC, so it works even when
   // the XRP7724 isn't running, keeping the final 25 MHz source away.
   printf("marble_init with use_xtal = %s\r\n", XTAL_IN_USE);
   printf("system clock = %lu Hz\r\n", sysclk_freq);
#else
   marble_init();
#endif
   //~ marble_PSU_pwr(false);
   //~ printf("Let's wait a second and turn on the psu");
   marble_SLEEP_ms(300);
   //~ marble_PSU_pwr(true);
   printf("+++++++++++++++++++++ | Initializing System Functions | +++++++++++++++++++++\r\n");


   system_init();
   printf("++++++++++++++++++++++++ | Initializing Peripherals | +++++++++++++++++++++++\r\n");

   /* Turn on LEDs */
   marble_LED_set(0, true);   // LD15
   marble_LED_set(1, true);   // LD11
   marble_LED_set(2, true);   // LD12

   // Boot the power supply controller if needed
   pwr_autoboot();

   // Initialize off-chip components
   board_init();
   printf("++++++++++++++++++++++++ | Initialization Complete | ++++++++++++++++++++++++\r\n");

   marble_print_ID_status();
   marble_SLEEP_ms(200); // settle and print

   // Power FMCs
   marble_FMC_pwr(true);

   if (1) {
      printf("+++++++++++++++++++++++++++++ | Resetting FPGA | ++++++++++++++++++++++++++++\r\n");
      FPGAWD_SelfReset();
      fflush(stdout);
      marble_SLEEP_ms(100);
      system_service();
      marble_SLEEP_ms(500);
      system_service();
   }

   printf("++++++++++++++++++++++++++++ | Starting Console | +++++++++++++++++++++++++++\r\n");
   fflush(stdout);

   marble_SLEEP_ms(10);
   UARTQUEUE_Init(); // Flush the bus before console starts

   printf("Enter command or '?' for help\r\n> ");
   fflush(stdout);

   // Send demo string over UART at 115200 BAUD
   // marble_UART_send(DEMO_STRING, strlen(DEMO_STRING));

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