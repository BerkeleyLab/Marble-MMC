/* file: system.h
 * desc: System-wide generic (not Marble-specific) utilities
 */
#ifndef SYSTEM_H
#define SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PMOD_MODE_DISABLED = 0,
  PMOD_MODE_UI_BOARD,     // Requires OLED ui_board (see submodules/oled)
  PMOD_MODE_LED,          // (UNIMPLEMENTED) Blinking/steady LED control via mailbox
  PMOD_MODE_GPIO,         // (UNIMPLEMENTED) Slow GPIO control via mailbox
  PMOD_MODE_UI_BOARD_1U,  // Requires OLED ui_board_1u (see submodules/oled)
  /* KEEP AS LAST ENTRY */ PMOD_MODE_SIZE
} pmod_mode_t;

/* Board-generic internal initialization */
void system_init(void);

/* Board-generic external initialization */
void system_off_chip_init(void);

/* Board-generic periodic service (should be in main loop) */
void system_service(void);

/* Retrieve and apply non-volatile parameters */
void system_apply_params(void);

/* Print various status fields */
void print_status_counters(int len);

/* Reset FPGA and schedule callback function 'cb' to execute after reset */
void reset_fpga_with_callback(void (*cb)(void));

/* Set the Pmod usage mode */
int system_set_pmod_mode(pmod_mode_t mode);

/* Get the Pmod usage mode */
pmod_mode_t system_get_pmod_mode(void);

// Frequency of underlying Pmod LED timer in Hz
#define FREQUENCY_PMOD_TIMER      (128)
void system_pmod_led_isr(void);

/* Helper function for the mailbox to avoid reading the Pmod LEDs page
   if the feature isn't even enabled. */
int system_pmod_leds_enabled(void);

/* The mailbox uses this function to update LED values */
void system_handle_pmod_led(int val, int pin);

/* (Re-)Initialize the Pmod subsystem selected by pmod_mode */
//void pmod_subsystem_init(void);

#ifdef MARBLE_V2
/// @brief  Possible STM32 system reset causes
typedef enum reset_cause_e
{
    RESET_CAUSE_UNKNOWN = 0,
    RESET_CAUSE_LOW_POWER_RESET,
    RESET_CAUSE_WINDOW_WATCHDOG_RESET,
    RESET_CAUSE_INDEPENDENT_WATCHDOG_RESET,
    RESET_CAUSE_SOFTWARE_RESET,
    RESET_CAUSE_POWER_ON_POWER_DOWN_RESET,
    RESET_CAUSE_EXTERNAL_RESET_PIN_RESET,
    RESET_CAUSE_BROWNOUT_RESET,
} reset_cause_t;

/// @brief      print the name of a reset cause
void print_reset_cause(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_H
