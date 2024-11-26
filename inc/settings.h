#ifndef SETTINGS_H
#define SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "marble_api.h"
#include "stm32f2xx_hal.h"

//-----------------------------
// Settings related to UI board (oled)
//-----------------------------

typedef enum {
  IO_CSN = 0,
  IO_RSTN,
  IO_INT,
  OLED_BIT_D_C,
  OLED_BIT_CSN,
  UI_BOARD_NUM_GPIOS // keep at bottom of enum
} _gpio_patch_t;

#define OLED_BIT_RSTN           (-1)  // disabled for ui_board

typedef enum {
  GPIO_OUT_REG = 0,
  GPIO_OE_REG,
  UI_BOARD_NUM_REGS // keep at bottom of enum
} _reg_patch_t;

#define INIT_SS_MAN_BIT   (0)
#define INIT_SS_CTRL_BIT  (1)
#define INIT_CPOL_BIT     (2)
#define INIT_CPHA_BIT     (3)
#define INIT_LSB_BIT      (4)
#define INIT_SS_MAN_MASK  (1 << INIT_SS_MAN_BIT)
#define INIT_SS_CTRL_MASK (1 << INIT_SS_CTRL_BIT)
#define INIT_CPOL_MASK    (1 << INIT_CPOL_BIT)
#define INIT_CPHA_MASK    (1 << INIT_CPHA_BIT)
#define INIT_LSB_MASK     (1 << INIT_LSB_BIT)

#define F_CLK                   FREQUENCY_SYSCLK
// Using the same SPI controller for OLED and I/O
#define OLED_SPI                            SPI2
#define IO_SPI                          OLED_SPI
// SPI baud rate = f_APBclk * 2^(-N) for N = 1-8 (f_APBclk/2 to f_APBclk/256)
// For f_APBclk = 30 MHz, SPI baud rate = 15 MHz - 118 kHz
// Let's set it to 7.5 MHz (f_APBclk/4) for now
//#define OLED_SPI_CLKDIV  SPI_BAUDRATEPRESCALER_4
// Let's set it to 1.875 MHz (f_APBclk/16) for now
#define OLED_SPI_CLKDIV  SPI_BAUDRATEPRESCALER_16
#define IO_SPI_CLKDIV            OLED_SPI_CLKDIV

// These are not needed
#define IO_GPIO
#define OLED_GPIO

#ifdef __cplusplus
}
#endif

#endif // ifndef SETTINGS_H
