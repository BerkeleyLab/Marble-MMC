#include <string.h>
#include <stdio.h>
#include "marble_api.h"
#include "sim_platform.h"
#include "i2c_pm.h"
#include "i2c_fpga.h"
#include "mailbox.h"
#include "phy_mdio.h"
#include "rev.h"
#include "console.h"
#include "uart_fifo.h"

//#define LED_SNAKE

#ifdef MARBLE_V2
static void mgtclk_xpoint_en(void)
{
   if (xrp_ch_status(XRP7724, 1)) { // CH1: 3.3V
      adn4600_init();
   }
}
#endif

unsigned int live_cnt=0;
unsigned int fpga_prog_cnt=0;
unsigned int fpga_net_prog_pend=0;
static volatile bool spi_update = false;
static uint32_t systimer_ms=1; // System timer interrupt period

static void fpga_done_handler(void)
{
   fpga_prog_cnt++;
   fpga_net_prog_pend=1;
}

static void timer_int_handler(void)
{
   const uint32_t SPI_MBOX_RATE = 2000;  // ms
   static uint32_t spi_ms_cnt=0;

   // SPI mailbox update flag; soft-realtime
   spi_ms_cnt += systimer_ms;
   if (spi_ms_cnt > SPI_MBOX_RATE) {
      spi_update = true;
      spi_ms_cnt = 0;
      // Use LED2 for SPI heartbeat
      marble_LED_toggle(2);
   }

   // Snake-pattern LEDs on two LEDs
#ifdef LED_SNAKE
   static uint16_t led_cnt = 0;
   if(led_cnt == 330)
      marble_LED_toggle(0);
   else if(led_cnt == 660)
      marble_LED_toggle(1);
   led_cnt = (led_cnt + 1) % 1000;
#endif /* LED_SNAKE */
   live_cnt++;
}

void print_status_counters(void) {
  printf("Live counter: %u\r\n", live_cnt);
  printf("FPGA prog counter: %d\r\n", fpga_prog_cnt);
  printf("FMC status: %x\r\n", marble_FMC_status());
  printf("PWR status: %x\r\n", marble_PWR_status());
#ifdef MARBLE_V2
  printf("MGT CLK Mux: %x\r\n", marble_MGTMUX_status());
#endif
  return;
}

#ifdef SIMULATION
int main(int argc, char *argv[]) {
  bool initialize = false;
  for (int n = 1; n < argc; n++) {
    if ((argv[n][0] == '-') && (argv[n][1] == 'i')) {
      initialize = true;
    }
  }
#else
int main(void) {
#endif
   UARTQUEUE_Init();
#ifdef MARBLEM_V1
   // Initialize Marble(mini) board with IRC, so it works even when
   // the XRP7724 isn't running, keeping the final 25 MHz source away.
   const bool use_xtal = false;
   uint32_t sysclk_freq = marble_init(use_xtal);
   printf("marble_init with use_xtal = %d\r\n", use_xtal);
   printf("system clock = %lu Hz\r\n", sysclk_freq);
#else
#ifdef MARBLE_V2
   marble_init(0);
#endif
#endif

#ifdef SIMULATION
   console_init();
   if (initialize) {
     printf("Initializing flash\r\n");
   } else {
     printf("Restoring flash\r\n");
   }
   marble_init(initialize);
#else
   // UART console service
   console_init();
#endif

   /* Turn on LEDs */
   marble_LED_set(0, true);   // LD15
   marble_LED_set(1, true);   // LD11
   marble_LED_set(2, true);   // LD12

   // Register GPIO interrupt handlers
   marble_GPIOint_handlers(fpga_done_handler);

   /* Configure the System Timer for 200 Hz interrupts (if supported) */
   systimer_ms = marble_SYSTIMER_ms(5);

   // Register System Timer interrupt handler
   marble_SYSTIMER_handler(timer_int_handler);

#ifdef XRP_AUTOBOOT
   printf("XRP_AUTOBOOT\r\n");
   marble_SLEEP_ms(300);
   xrp_boot();
#endif

#ifdef MARBLE_V2
   // Enable MGT clock cross-point switch if 3.3V rail is ON
   mgtclk_xpoint_en();
#endif

   // Power FMCs
   marble_FMC_pwr(true);

   if (1) {
      printf("** Policy: reset FPGA on MMC reset.  Doing it now. **\r\n");
      reset_fpga();
      printf("**\r\n");
   }

   // Send demo string over UART at 115200 BAUD
   marble_UART_send(DEMO_STRING, strlen(DEMO_STRING));
   //printf("%s", DEMO_STRING);

   while (1) {
      // Run all system update/monitoring tasks and only then handle console
      if (spi_update) {
         mbox_update(false);
         spi_update = false; // Clear flag
      }
      // TODO - fix encapsulation here
      if (fpga_net_prog_pend) {
         console_print_mac_ip();
         console_push_fpga_mac_ip();
         printf("DONE\r\n");
         fpga_net_prog_pend=0;
      }
#ifdef SIMULATION
      if (sim_platform_service()) {
        break;
      }
#endif
      console_service();
   }
}

// This probably belongs in some other file, but which one?
int __io_putchar(int ch);
int __io_putchar(int ch)
{
  marble_UART_send((const char *)&ch, 1);
  return ch;
}
