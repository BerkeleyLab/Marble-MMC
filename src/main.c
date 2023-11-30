#include <string.h>
#include <stdio.h>
#include "marble_api.h"
#include "i2c_pm.h"
#include "i2c_fpga.h"
#include "mailbox.h"
#include "phy_mdio.h"
#include "rev.h"
#include "console.h"
#include "uart_fifo.h"
#include "ltm4673.h"
#include "watchdog.h"

#define LED_SNAKE
#ifdef MARBLE_V2
static void mgtclk_xpoint_en(void)
{
   if (xrp_ch_status(XRP7724, 1)) { // CH1: 3.3V
      adn4600_init();
   } else if (ltm4673_ch_status(LTM4673)) {
      printf("Using LTM4673 and adn4600_init\r\n");
      adn4600_init();
   } else {
      printf("Skipping adn4600_init\r\n");
   }
}
#endif
// CLOCK_USE_XTAL only used for marble-mini
#ifdef CLOCK_USE_XTAL
#define XTAL_IN_USE     "True"
#else
#define XTAL_IN_USE     "False"
#endif

#define FPGA_PUSH_DELAY_MS              (2)
unsigned int live_cnt=0;
unsigned int fpga_prog_cnt=0;
unsigned int fpga_net_prog_pend=0;
unsigned int fpga_done_tickval=0;
static volatile bool spi_update = false;
static uint32_t systimer_ms=1; // System timer interrupt period

static void fpga_done_handler(void)
{
   fpga_prog_cnt++;
   fpga_net_prog_pend=1;
   fpga_done_tickval = BSP_GET_SYSTICK();
   FPGAWD_DoneHandler();
   return;
}

static void timer_int_handler(void)
{
   const uint32_t SPI_MBOX_RATE = 2000;  // ms
   static uint32_t spi_ms_cnt=0;

   // SPI mailbox update flag; soft-realtime
   spi_ms_cnt += systimer_ms;
   //printf("%d\r\n", spi_ms_cnt);
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

int main(void) {
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

   // UART console service
   console_init();

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

   // Boot the power supply controller if needed
   pwr_autoboot();

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

   I2C_PM_init();

   while (1) {
      // Run all system update/monitoring tasks and only then handle console
      if (spi_update) {
         mbox_update(false);
         spi_update = false; // Clear flag
      }
      // TODO - fix encapsulation here
      if (fpga_net_prog_pend) {
        if (BSP_GET_SYSTICK() > fpga_done_tickval + FPGA_PUSH_DELAY_MS) {
          console_print_mac_ip();
          console_push_fpga_mac_ip();
          printf("DONE\r\n");
          fpga_net_prog_pend=0;
        }
      }
      console_service();
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
