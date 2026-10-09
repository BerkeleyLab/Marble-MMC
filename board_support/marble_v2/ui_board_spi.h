#ifndef UI_BOARD_SPI_H
#define UI_BOARD_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "marble_api.h"

//-----------------------------
// Settings related to UI board (oled)
//-----------------------------
#define PIN_IO_CS_N GPIO_PIN_6
#define PIN_RST_IO_N GPIO_PIN_15
#define PIN_INT GPIO_PIN_14
#define PIN_D_C GPIO_PIN_5
#define PIN_OLED_CS_N GPIO_PIN_9

#define PORT_IO_CS_N GPIOD
#define PORT_RST_IO_N GPIOB
#define PORT_INT GPIOB
#define PORT_D_C GPIOD
#define PORT_OLED_CS_N GPIOB

#include "stm32f2xx_hal.h"

// SPI baud rate = f_APBclk * 2^(-N) for N = 1-8 (f_APBclk/2 to f_APBclk/256)
// For f_APBclk = 30 MHz, SPI baud rate = 15 MHz - 118 kHz
#define OLED_SPI_CLKDIV  SPI_BAUDRATEPRESCALER_4

void ui_board_spi_init(void);

#ifdef __cplusplus
}
#endif

#endif // ifndef UI_BOARD_SPI_H
