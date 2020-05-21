/*
 * MARBLE_API.h
 * API consisting of wrapper functions providing access to low-level features
 * of Marble Mini (v1 and v2) and Marble.
 * The functions defined herein are necessarily uController-agnostic.
 */

#ifndef _MARBLE_API_H_
#define _MARBLE_API_H_

/* Initialize uC and peripherals before main code can run. */
void marble_init(bool use_xtal);

/****
* UART
****/
void marble_UART_putch(char ch);

int marble_UART_getch(char ch);

void marble_UART_putstr(char *str);

/****
* LED
****/
void marble_LED_set(uint8_t led_num, bool on);

bool marble_LED_get(uint8_t led_num);

void marble_LED_toggle(uint8_t led_num);

/****
* Push-Buttons
****/
bool marble_SW_get(void);

/****
* FPGA int
****/
bool marble_FPGAint_get(void);

/****
* FMC
****/
void marble_FMC_pwr(bool on);


/****
* SPI/SSP
****/

/****
* I2C
****/


#endif /* _MARBLE_API_H_ */
