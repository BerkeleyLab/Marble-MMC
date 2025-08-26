/* File: system.c
 * Desc: Non-board-specific routines that are not part of a particular subsystem
 */

#include "marble_api.h"
#include "console.h"
#include "eeprom.h"
#include "i2c_pm.h"
#include "mailbox.h"
#include "rev.h"
#include "console.h"
#include "ltm4673.h"
#include "watchdog.h"

#undef UI_BOARD_SUPPORTED

#ifdef MARBLE_V2
// NOTE - Support for the oled UI board will be re-introduced once licensing has been ironed out
// NOTE - Currently only supported on Marble (not Marble-mini)
// #define UI_BOARD_SUPPORTED
#include "display.h"
#endif

#include <stdio.h>

//#define DEBUG_PRINT
#include "dbg.h"

#define LED_SNAKE
#define FPGA_PUSH_DELAY_MS              (2)
#define FPGA_RESET_DURATION_MS         (50)

#define PMOD_LED_NCHANS     (8)
#define PMOD_LED_CHANMASK   ((1<<PMOD_LED_NCHANS)-1)
#define PMOD_LED_CONST(v)     ((v == 0xff) || ((v & 7) == 0))
#define PMOD_LED_NONCONST(v)  ((v != 0xff) && ((v & 7) != 0))
#define PMOD_LED_UPDATE_NONCONST(pin, val) do { \
  pmod_led_nonconst = (pmod_led_nonconst & ~(1 << pin)) | (PMOD_LED_NONCONST(val) << pin);\
  } while (0)

/* Our timer system will be a counter from 0-255
 * Each pin will have a count mask which will select a smaller
 * portion of the count window for higher frequencies
 * Each pin will have an "on window" and an "off window"
 * These will be conveyed by a "assert" count and a "deassert" count
 * If assert_count == deassert_count == 0, then const OFF
 * If assert_count == deassert_count != 0, then const ON
 */
#define PMOD_LED_COUNTS       (256)
// {inv, duty[1:0], phase[1:0], freq[2:0]}
#define PMOD_LED_FREQ(val)    (val & 0x7)
#define PMOD_LED_PHASE(val)   ((val >> 3) & 0x3)
#define PMOD_LED_DUTY(val)    ((val >> 5) & 0x3)
#define PMOD_LED_INV(val)     ((val >> 7) & 0x1)

typedef struct {
  uint8_t count_on;
  uint8_t count_off;
  uint8_t modulo;
} pmod_led_t;

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
static bool pmod_leds_timer_en = 0;
static uint8_t pmod_leds_raw[PMOD_LED_NCHANS];
static pmod_led_t pmod_leds[PMOD_LED_NCHANS];
// Let's keep a bitmask of the "non-constant" LEDs.  If this ever
// reaches zero, we can disable the timer.
static unsigned int pmod_led_nonconst=0;
/*
Blink state machine is 256 time intervals.  Using modulo math, we
can approximate a linear frequency range with the non-power-of-two
divisions creating a small frequency/duty-cycle dither.
256/1  = modulo 256    ->    2s = 0.5Hz
256/2  = modulo 128    ->    1s = 1.0Hz
256/4  = modulo 64     -> 0.50s = 2.0Hz
256/6  = modulo 43     -> 0.33s = 3.0Hz
256/8  = modulo 32     -> 0.25s = 4.0Hz
256/12 = modulo 22     -> 0.17s = 6.0Hz
256/16 = modulo 16     -> 0.13s = 8.0Hz
 */
static const uint16_t pmod_led_modulo_map[] = {0, 256, 128, 64, 43, 32, 22, 16};

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
static void system_pmod_led_init(void);
static void system_pmod_timer_disable(void);
static void system_pmod_timer_enable(void);
static void pmod_led_counts(uint8_t val, volatile pmod_led_t *pled);

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

#ifdef APP_MINI
  marble_PSU_pwr(1);
#endif
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

  // Init static values for LED subsystem
  system_pmod_led_init();
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

  eeprom_update();
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
  system_pmod_timer_disable();
  return;
}

static void system_pmod_mode_ui_board(void) {
#ifdef UI_BOARD_SUPPORTED
  display_init();
#else
  printf("*** UI board support disabled for this build!  Please update MMC image to use this feature ***\r\n");
#endif
  system_pmod_timer_disable();
  return;
}

static void system_pmod_led_init(void) {
  for (int n=0; n<PMOD_LED_NCHANS; n++) {
    pmod_leds[n].count_on = 0;
    pmod_leds[n].count_off = 0;
    pmod_leds[n].modulo = 1;
  }
  return;
}

static void system_pmod_mode_led(void) {
  marble_pmod_config_outputs();
  marble_pmod_timer_config();
  system_pmod_timer_enable();
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
  if (isr_cnt == (PMOD_LED_COUNTS-1)) {
    isr_cnt = 0;
  } else {
    ++isr_cnt;
  }
  uint8_t led_cnt;
  for (int n=0; n<PMOD_LED_NCHANS; n++) {
    if (pmod_leds[n].modulo == 1) {
      // Skipping const LEDs
      continue;
    }
    if (pmod_leds[n].modulo == 0) {
      led_cnt = (uint8_t)isr_cnt;
    } else {
      led_cnt = (uint8_t)isr_cnt % pmod_leds[n].modulo;
    }
    if (led_cnt == pmod_leds[n].count_on) {
      marble_pmod_set_gpio((uint8_t)n, 1);
    } else if (led_cnt == pmod_leds[n].count_off) {
      marble_pmod_set_gpio((uint8_t)n, 0);
    }
  }
  return;
}

void system_handle_pmod_led(int val, int pin) {
  pin = pin & PMOD_LED_CHANMASK; // limit 0-7
  if ((val & 0xff) == pmod_leds_raw[pin]) {
    // Early exit.  No change; nothing to do.
    return;
  }
  // Value changed!
  printd("  Pmod pin %d 0x%x -> 0x%x\r\n", pin, pmod_leds_raw[pin], (val & 0xff));
  pmod_leds_raw[pin] = (uint8_t)(val & 0xff);
  PMOD_LED_UPDATE_NONCONST(pin, val);
  printd("  pmod_led_nonconst = 0x%x\r\n", pmod_led_nonconst);
  if (!pmod_leds_timer_en) {
    // If the timer is disabled, check to see if we need to enable it
    if (PMOD_LED_NONCONST(val)) {
      // We have a blinky. Enable the timer.
      printd("We have a blinky. Enable the timer\r\n");
      system_pmod_timer_enable();
    }
  } else {
    // If the timer is enabled, we can disable it if all leds are const
    if (pmod_led_nonconst == 0) {
      printd("All const. Disabling the timer.\r\n");
      system_pmod_timer_disable();
    }
  }
  if (PMOD_LED_CONST(val)) {
    bool state = (val == 0x00) || (val == 0xff) ? 0 : 1;
    printd("Constant pin. Setting GPIO manually to %d\r\n", state);
    // If the timer is disabled, we need to set the GPIO manually here
    // We also know that this value must be constant (or else the timer would be enabled)
    marble_pmod_set_gpio((uint8_t)pin, state);
    // Finally update the pmod_led_t struct
    pmod_leds[pin].count_on = state;
    pmod_leds[pin].count_off = state;
    pmod_leds[pin].modulo = 1; // Little code that says "skip me, I'm constant"
  } else {
    // Blinky. Update the struct accordingly
    pmod_led_counts((uint8_t)val, &pmod_leds[pin]);
  }
  return;
}

/* Parse the Pmod LED state encoded in byte val into "modulo", "count_on", and "count_off" values.*/
static void pmod_led_counts(uint8_t val, volatile pmod_led_t *pled) {
  int xx = PMOD_LED_FREQ(val);
  uint16_t modulo = pmod_led_modulo_map[xx];
  if (modulo == 0) {
    // This branch should not be reached, but let's make a safe exit if it is
    pled->count_on = 0;
    pled->count_off = 0;
    pled->modulo = 1;
    return;
  }
  pled->modulo = (uint8_t)(modulo & 0xff);
  uint8_t offset = 0;
  xx = PMOD_LED_PHASE(val);
  uint8_t duration = 0;
  switch (xx) {
    case 1:
      offset = (uint8_t)(modulo/4);
      break;
    case 2:
      offset = (uint8_t)(modulo/2);
      break;
    case 3:
      offset = (uint8_t)(3*modulo/4);
      break;
    default:
      break;
  }
  xx = PMOD_LED_DUTY(val);
  switch (xx) {
    case 0:
      duration = (uint8_t)(modulo/8);
      break;
    case 1:
      duration = (uint8_t)(modulo/4);
      break;
    case 2:
      duration = (uint8_t)(3*modulo/8);
      break;
    case 3:
      duration = (uint8_t)(modulo/2);
      break;
    default:
      break;
  }
  uint8_t count_on = offset;
  uint8_t count_off = (uint8_t)((offset + duration) % modulo);
  if (PMOD_LED_INV(val)) {
    // Swap 'em
    pled->count_on = count_off;
    pled->count_off = count_on;
  } else {
    pled->count_on = count_on;
    pled->count_off = count_off;
  }
  printd("Pin gets modulo %d, count_on = %d, count_off = %d\r\n", modulo, pled->count_on, pled->count_off);
  return;
}

static void system_pmod_timer_disable(void) {
  marble_pmod_timer_disable();
  pmod_leds_timer_en = 0;
  return;
}

static void system_pmod_timer_enable(void) {
  marble_pmod_timer_enable();
  pmod_leds_timer_en = 1;
  return;
}

int system_pmod_leds_enabled(void) {
  if (pmod_mode == PMOD_MODE_LED) {
    return 1;
  }
  return 0;
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
#ifdef UI_BOARD_SUPPORTED
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
