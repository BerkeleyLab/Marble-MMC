// Board specific driver functions for the ui_board

#include "ui_board_spi.h"
#include "hardware_interface.h"
#include "stm32f2xx_hal.h"

extern SPI_HandleTypeDef hspi2;

void ui_board_spi_init(void) {
  // --------
  //  SPI
  // --------
  // De-init
  if (hspi2.Instance != 0)
    HAL_SPI_DeInit(&hspi2);
  // Init
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = OLED_SPI_CLKDIV;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.State = HAL_SPI_STATE_RESET;
  HAL_SPI_Init(&hspi2); // Calls HAL_SPI_MspInit() to init SPI pins

  // --------------
  //  GPIO outputs
  // --------------
  __HAL_RCC_GPIOD_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  GPIO_InitStruct.Pin = PIN_OLED_CS_N;
  HAL_GPIO_Init(PORT_OLED_CS_N, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = PIN_IO_CS_N;
  HAL_GPIO_Init(PORT_IO_CS_N, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = PIN_RST_IO_N;
  HAL_GPIO_Init(PORT_RST_IO_N, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = PIN_D_C;
  HAL_GPIO_Init(PORT_D_C, &GPIO_InitStruct);

  // ---------------
  //  Input pin
  // ---------------
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Pin = PIN_INT;
  HAL_GPIO_Init(PORT_INT, &GPIO_InitStruct);

  // --------
  //  RESET!
  // --------
  HAL_GPIO_WritePin(PORT_RST_IO_N, PIN_RST_IO_N, 0);
  HAL_Delay(1);
  HAL_GPIO_WritePin(PORT_RST_IO_N, PIN_RST_IO_N, 1);
  HAL_Delay(1);
}

// Return true as long as a background (DMA / interrupts based)
// SPI transaction is in progress.
bool ui_spi_is_busy(void) {
  return HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY;
}

// Initiate a blocking 8 bit SPI transaction. Transmit val on SDO, MSB first.
// return received data from SDI. Don't touch the CS_N pin.
uint8_t ui_spi_rx_tx(uint8_t val) {
  uint8_t rx_data = 0;
  HAL_SPI_TransmitReceive(&hspi2, &val, &rx_data, 1, 10);
  return rx_data;
}

void ui_spi_tx_chunk(uint8_t *buf, unsigned len) {
  HAL_SPI_Transmit_DMA(&hspi2, buf, len);
}

// Set the state of the 2 CS_N pins: bit1: CS_N_MCP, bit0: CS_N_OLED
void ui_set_cs_n(t_ui_cs_n val) {
  HAL_GPIO_WritePin(PORT_OLED_CS_N, PIN_OLED_CS_N, val & 1);
  HAL_GPIO_WritePin(PORT_IO_CS_N, PIN_IO_CS_N, (val >> 1) & 1);
}

// Set the state of the D_C pin (1 = command, 0 = data for the SSD1322)
void ui_set_dc(bool val) { HAL_GPIO_WritePin(PORT_D_C, PIN_D_C, val); }

// Get the state of the MCP23Sxx INT pin. Speeds up the polling loop.
bool ui_get_int(void) { return HAL_GPIO_ReadPin(PORT_INT, PIN_INT); }

// Return a running number of elapsed milliseconds
// used to distinguish between short and long push
unsigned ui_get_cycles(void) { return BSP_GET_SYSTICK(); }

// Min. duration for a long-press event. In [ms]
const unsigned ui_t_long_press = 500;
