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
void print_status_counters(void);

/* Reset FPGA and schedule callback function 'cb' to execute after reset */
void reset_fpga_with_callback(void (*cb)(void));

/* Set the Pmod usage mode */
int system_set_pmod_mode(pmod_mode_t mode);

/* Get the Pmod usage mode */
pmod_mode_t system_get_pmod_mode(void);

// Frequency of underlying Pmod LED timer in Hz
#define FREQUENCY_PMOD_TIMER      (128)
void system_pmod_led_isr(void);

/* (Re-)Initialize the Pmod subsystem selected by pmod_mode */
//void pmod_subsystem_init(void);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_H
