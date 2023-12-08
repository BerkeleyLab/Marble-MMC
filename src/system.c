/* File: system.c
 * Desc: Non-board-specific routines that are not part of a particular subsystem
 */

#include "marble_api.h"
#include "console.h"
#include "st-eeprom.h"
#include "i2c_pm.h"
#include "mailbox.h"
#include "rev.h"
#include "console.h"
#include "ltm4673.h"
#include "watchdog.h"

#include <stdio.h>

#define LED_SNAKE
#define FPGA_PUSH_DELAY_MS              (2)
#define FPGA_RESET_DURATION_MS         (50)
unsigned int live_cnt=0;
unsigned int fpga_prog_cnt=0;
unsigned int fpga_done_tickval=0;
unsigned int fpga_net_prog_pend=0;
static uint32_t fpga_disabled_time = 0;
static int fpga_reset = 0;
static void (*fpga_reset_callback)(void) = NULL;
static volatile bool spi_update = false;
static uint32_t systimer_ms=1; // System timer interrupt period

static void system_apply_params(void);

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
   static uint32_t spi_ms_cnt=0;

   // SPI mailbox update flag; soft-realtime
   spi_ms_cnt += systimer_ms;
   //printf("%d\r\n", spi_ms_cnt);
   if (spi_ms_cnt > SPI_MAILBOX_PERIOD_MS) {
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

void system_init(void) {
  // Initialize non-volatile memory
  eeprom_init();
  // Apply parameters from non-volatile memory
  system_apply_params();

  // Register GPIO interrupt handlers
  marble_GPIOint_handlers(fpga_done_handler);

  /* Configure the System Timer for 200 Hz interrupts (if supported) */
  systimer_ms = marble_SYSTIMER_ms(5);

  // Register System Timer interrupt handler
  marble_SYSTIMER_handler(timer_int_handler);

  // UART console service
  console_init();

  // Initialize subsystems
  I2C_PM_init();
  return;
}

void system_service(void) {
  // Run all system update/monitoring tasks and only then handle console
  // Handle Mailbox Updates
  if (spi_update) {
     mbox_update(false);
     spi_update = false; // Clear flag
  }
  // Handle delayed action in response to FPGA's DONE pin asserting
  if ((fpga_net_prog_pend) && (BSP_GET_SYSTICK() > fpga_done_tickval + FPGA_PUSH_DELAY_MS)) {
    console_print_mac_ip();
    console_push_fpga_mac_ip();
    printf("DONE\r\n");
    fpga_net_prog_pend=0;
  }
  // Handle re-enabling FPGA after scheduled reset
  // NOTE! Timing depends on BSP_GET_SYSTICK returning ms
  if ((fpga_reset) && (BSP_GET_SYSTICK() - fpga_disabled_time >= FPGA_RESET_DURATION_MS)) {
    enable_fpga();
    if (fpga_reset_callback != NULL) {
      fpga_reset_callback();
      fpga_reset_callback = NULL;
    }
    fpga_reset = 0;
  }
  console_service();
  return;
}

/*
 * static void system_apply_params(void);
 *    Apply parameters stored in non-volatile memory.
 *    TODO - This is not board-specific.  Move to "system" file.
 */
static void system_apply_params(void) {
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
  // Watchdog period
  if (eeprom_read_wd_period(&val, 1)) {
    printf("Could not read watchdog period.\r\n");
  } else {
    FPGAWD_SetPeriod((int)val);
  }
  return;
}

void reset_fpga_with_callback(void (*cb)(void)) {
  fpga_reset_callback = cb;
  disable_fpga();
  fpga_reset = 1;
  fpga_disabled_time = BSP_GET_SYSTICK();
  return;
}

void print_status_counters(void) {
  printf("Live counter: %u\r\n", live_cnt);
  printf("FPGA prog counter: %d\r\n", fpga_prog_cnt);
  FPGAWD_ShowState();
  printf("FMC status: %x\r\n", marble_FMC_status());
  printf("PWR status: %x\r\n", marble_PWR_status());
#ifdef MARBLE_V2
  printf("MGT CLK Mux: %x\r\n", marble_MGTMUX_status());
#endif
  return;
}
