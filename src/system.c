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

#ifdef MARBLE_V2
// NOTE - Currently only supported on Marble (not Marble-mini)
#include "display.h"
#endif

#include <stdio.h>


#define LED_SNAKE
#define FPGA_PUSH_DELAY_MS              (2)
#define FPGA_RESET_DURATION_MS         (50)
/* ============================ Static Variables ============================ */
static unsigned int live_cnt=0;
static unsigned int fpga_prog_cnt=0;
static unsigned int fpga_done_tickval=0;
static unsigned int fpga_net_prog_pend=0;
static uint32_t fpga_disabled_time = 0;
static int fpga_reset = 0;
static void (*fpga_reset_callback)(void) = NULL;
static volatile bool spi_update = false;
static uint32_t systimer_ms=1; // System timer interrupt period
static pmod_mode_t pmod_mode = PMOD_MODE_DISABLED;

/* ====================== Static Function Prototypes ======================== */
static void pmod_subsystem_init(void);
static void pmod_subsystem_service(void);
static void fpga_done_handler(void);
static void timer_int_handler(void);
static void system_apply_internal_params(void);
static void system_apply_external_params(void);
static void system_pmod_mode_disabled(void);
static void system_pmod_mode_ui_board(void);
static void system_pmod_mode_led(void);

/* =========================== Exported Functions =========================== */
/* void system_init(void);
 *  Initialization of internal chip-agnostic MMC functionality.  Nothing that
 *  relies on an external component should be included here.
 *  This can be called immediately upon startup.
 */
void system_init(void) {
  // Initialize non-volatile memory
  eeprom_init();

  // Apply parameters from non-volatile memory
  system_apply_internal_params();
  //system_apply_params();

  // Register GPIO interrupt handlers
  marble_GPIOint_handlers(fpga_done_handler);

  // Configure the System Timer for 200 Hz interrupts (if supported)
  systimer_ms = marble_SYSTIMER_ms(5);

  // Register System Timer interrupt handler
  marble_SYSTIMER_handler(timer_int_handler);

  // UART console service
  console_init();

  return;
}

/* void system_off_chip_init(void);
 *  Initialization of subsystems either external to the MCU or relying on
 *  external components (i.e. power good) but which are still platform-agnostic.
 *  This should only be called after power is confirmed good.
 */
void system_off_chip_init(void) {
  // Read and apply any non-volatile parameters destined for off-chip components
  system_apply_external_params();

  // Pmod subsystem (UI Board, LEDs, GPIOs, etc)
  pmod_subsystem_init();

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

  pmod_subsystem_service();
  return;
}

/*
 * void system_apply_params(void);
 *    Apply parameters stored in non-volatile memory.
 */
void system_apply_params(void) {
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
  // Mailbox enable
  if (eeprom_read_mbox_en(&val, 1)) {
    printf("Could not read mailbox enable setting.\r\n");
  } else {
    mbox_set_enable(val);
  }
  // Pmod mode
  if (eeprom_read_pmod_mode(&val, 1)) {
    printf("Could not read Pmod mode setting.\r\n");
  } else {
    //system_set_pmod_mode((pmod_mode_t)val);
    pmod_mode = (pmod_mode_t)val;
  }
  return;
}

static void system_apply_internal_params(void) {
  uint8_t val;
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
  // Mailbox enable
  if (eeprom_read_mbox_en(&val, 1)) {
    printf("Could not read mailbox enable setting.\r\n");
  } else {
    mbox_set_enable(val);
  }
  // Pmod mode
  if (eeprom_read_pmod_mode(&val, 1)) {
    printf("Could not read Pmod mode setting.\r\n");
  } else {
    //system_set_pmod_mode((pmod_mode_t)val);
    pmod_mode = (pmod_mode_t)val;
  }
  return;
}

static void system_apply_external_params(void) {
  uint8_t val;
  // Fan speed
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
  marble_print_status();
  printf("Live counter: %u\r\n", live_cnt);
  printf("FPGA prog counter: %u\r\n", fpga_prog_cnt);
  FPGAWD_ShowState();
  printf("FMC status: %x\r\n", marble_FMC_status());
  printf("PWR status: %x\r\n", marble_PWR_status());
#ifdef MARBLE_V2
  printf("MGT CLK Mux: %x\r\n", marble_MGTMUX_status());
#endif
  return;
}

int system_set_pmod_mode(pmod_mode_t mode) {
  pmod_mode = mode;
  int rval = eeprom_store_pmod_mode((const uint8_t *)&mode, 1);
  if (rval == 0) {
    pmod_subsystem_init();
  }
  return rval;
}

pmod_mode_t system_get_pmod_mode(void) {
  return pmod_mode;
}

static void system_pmod_mode_disabled(void) {
  marble_pmod_config_inputs();
  marble_pmod_timer_disable();
  return;
}

static void system_pmod_mode_ui_board(void) {
#ifdef MARBLE_V2
  display_init();
#endif
  marble_pmod_timer_disable();
  return;
}

static void system_pmod_mode_led(void) {
  marble_pmod_config_outputs();
  marble_pmod_timer_config();
  marble_pmod_timer_enable();
  return;
}

static void pmod_subsystem_init(void) {
  switch (pmod_mode) {
    case PMOD_MODE_DISABLED:
      // Nothing to do
      system_pmod_mode_disabled();
      break;
    case PMOD_MODE_UI_BOARD:
      system_pmod_mode_ui_board();
      break;
    case PMOD_MODE_LED:
      system_pmod_mode_led();
      break;
    //case PMOD_MODE_GPIO: // Unimplemented
    default:
      system_pmod_mode_disabled();
      break;
  }
  return;
}

void system_pmod_led_isr(void) {
  static int isr_cnt = 0;
  static bool gpio_state = 0;
  if (isr_cnt == (FREQUENCY_PMOD_TIMER/8)-1) {
    isr_cnt = 0;
    marble_pmod_set_gpio(0, gpio_state);
    marble_pmod_set_gpio(4, gpio_state);
    gpio_state = !gpio_state;
  } else {
    ++isr_cnt;
  }
  return;
}

/* ============================ Static Functions ============================ */
static void fpga_done_handler(void)
{
   if (!marble_pwr_good()) {
      return;
   }
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

static void pmod_subsystem_service(void) {
  switch (pmod_mode) {
    case PMOD_MODE_DISABLED:
      // Nothing to do
      break;
    case PMOD_MODE_UI_BOARD:
#ifdef MARBLE_V2
      display_update();
#endif
      break;
    case PMOD_MODE_LED:
      // Unimplemented
      break;
    case PMOD_MODE_GPIO:
      // Unimplemented
      break;
    default:
      break;
  }
  return;
}
