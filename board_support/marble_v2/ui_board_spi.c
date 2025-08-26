// A patch between the STM32 HAL codebase and the picorv32-based UI board driver

#include "spi.h"
#include "stm32f2xx_hal.h"

static uint16_t _gpio_pins[UI_BOARD_NUM_GPIOS] = {
  GPIO_PIN_6,  // IO_CSN PD6
  GPIO_PIN_15, // !RST_IO: PB15
  GPIO_PIN_14, // INT: PB14
  GPIO_PIN_5,  // D_C: PD5
  GPIO_PIN_9   // OLED_CSN: PB9
};
/*
_gpio_pins[IO_CSN]       = GPIO_PIN_6;  // IO_CSN PD6
_gpio_pins[IO_RSTN]      = GPIO_PIN_15; // !RST_IO: PB15
_gpio_pins[IO_INT]       = GPIO_PIN_14; // INT: PB14
_gpio_pins[OLED_BIT_D_C] = GPIO_PIN_5;  // D_C: PD5
_gpio_pins[OLED_BIT_CSN] = GPIO_PIN_9;  // OLED_CSN: PB9
*/

static GPIO_TypeDef * _gpio_ports[UI_BOARD_NUM_GPIOS] = {
  GPIOD,      // IO_CSN PD6
  GPIOB,      // !RST_IO: PB15
  GPIOB,      // INT: PB14
  GPIOD,      // D_C: PD5
  GPIOB       // OLED_CSN: PB9
};

/*
_gpio_ports[IO_CSN]       = GPIOD;      // IO_CSN PD6
_gpio_ports[IO_RSTN]      = GPIOB;      // !RST_IO: PB15
_gpio_ports[IO_INT]       = GPIOB;      // INT: PB14
_gpio_ports[OLED_BIT_D_C] = GPIOD;      // D_C: PD5
_gpio_ports[OLED_BIT_CSN] = GPIOB;      // OLED_CSN: PB9
*/

static int wait_for_txe(SPI_TypeDef *spi, uint32_t timeout);
static int wait_for_txne(SPI_TypeDef *spi, uint32_t timeout);
static int wait_for_nbusy(SPI_TypeDef *spi, uint32_t timeout);
static int wait_for_rxne(SPI_TypeDef *spi, uint32_t timeout);

static int _spi_initialized = 0;
static uint8_t last_data;

void ui_board_set_gpio(_reg_patch_t reg, _gpio_patch_t index, int val) {
  if (index >= UI_BOARD_NUM_GPIOS) {
    return;
  }
  if (reg == GPIO_OUT_REG) {
    if (val) {
      //printf("Setting pin %d high\r\n", index);
      HAL_GPIO_WritePin(_gpio_ports[index], _gpio_pins[index], GPIO_PIN_SET);
    } else {
      //printf("Setting pin %d low\r\n", index);
      HAL_GPIO_WritePin(_gpio_ports[index], _gpio_pins[index], GPIO_PIN_RESET);
    }
  }
  // NOTE! I'm deliberately not implementing support for reg=GPIO_OE_REG
  // because this will be handled in the custom board init and is not needed.
  // Implementing support would require another patch enum and that's just annoying
  return;
}

void ui_board_spi_init(SPI_TypeDef *spi, int init, int nbits, uint32_t clk_div) {
  if (_spi_initialized) {
    // The UI board driver calls "init" on both "OLED_SPI" and "IO_SPI" which in this
    // case are the same SPI peripheral, so let's ensure this init only happens once.
    return;
  }
  _spi_initialized = 1;
  printf("ui_board_spi_init\r\n");
  SPI_HandleTypeDef hspi = {0};
  hspi.Instance = spi;
  hspi.Init.Mode = SPI_MODE_MASTER;
  hspi.Init.Direction = SPI_DIRECTION_2LINES;
  if (nbits == 8) {
    hspi.Init.DataSize = SPI_DATASIZE_8BIT;
  } else {
    hspi.Init.DataSize = SPI_DATASIZE_16BIT;
  }
  if (init & INIT_CPOL_MASK) {
    hspi.Init.CLKPolarity = SPI_POLARITY_HIGH;
  } else {
    hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
  }
  if (init & INIT_CPHA_MASK) {
    hspi.Init.CLKPhase = SPI_PHASE_2EDGE;
  } else {
    hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
  }
  if (init & INIT_SS_MAN_MASK) {
    hspi.Init.NSS = SPI_NSS_SOFT;
  } else {
    hspi.Init.NSS = SPI_NSS_HARD_OUTPUT;
  }
  // Here I'm using SPI_BAUDRATEPRESCALER_256 as a mask to avoid
  // fiddling with any other register fields regardless of value of clk_div
  hspi.Init.BaudRatePrescaler = (clk_div & SPI_BAUDRATEPRESCALER_256);
  if (init & INIT_LSB_MASK) {
    hspi.Init.FirstBit = SPI_FIRSTBIT_LSB;
  } else {
    hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
  }
  hspi.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi.Init.CRCPolynomial = 10;
  // Initialize peripheral
  hspi.State = HAL_SPI_STATE_RESET;
  HAL_SPI_Init(&hspi); // Calls HAL_SPI_MspInit() to init SPI pins
  /**Need to initialize non-SPI GPIOs
  PB9 -> GPIO Output (push-pull)
  PB14-> GPIO Input
  PB15-> GPIO Output (push-pull)
  PD6 -> GPIO Output (push-pull)
  PD5 -> GPIO Output (push-pull)
  */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  // PB9, PB15
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  // PB14
  GPIO_InitStruct.Pin = GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  // PD5, PD6
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  return;
}

void ui_board_send_data_blocking(SPI_TypeDef *spi, uint8_t data, uint32_t timeout) {
  spi->CR1 |=  SPI_CR1_SPE;
  int rval = wait_for_txe(spi, timeout);
  if (rval < 0) {
    // Silently ignoring errors since UI board driver can't handle them anyhow
    printf("wait_for_txe returned %d\r\n", rval);
    return;
  }
  spi->DR = (uint32_t)(data & 0x000000ff);
  wait_for_txne(spi, timeout);
  //wait_for_txe(spi, timeout);
  wait_for_rxne(spi, timeout);
  last_data = (spi->DR & 0xff);
  wait_for_nbusy(spi, timeout);
  return;
}

uint8_t ui_board_get_last_data(void) {
  return last_data;
}

static int wait_for_txe(SPI_TypeDef *spi, uint32_t timeout) {
  while (((spi->SR) & SPI_SR_TXE) == 0) {
    if (timeout-- == 0) {
      break;
    }
  }
  if (timeout == 0) {
    return -1;
  }
  return 0;
}

static int wait_for_txne(SPI_TypeDef *spi, uint32_t timeout) {
  while ((spi->SR) & SPI_SR_TXE) {
    if (timeout-- == 0) {
      break;
    }
  }
  if (timeout == 0) {
    return -1;
  }
  return 0;
}

static int wait_for_rxne(SPI_TypeDef *spi, uint32_t timeout) {
  while (((spi->SR) & SPI_SR_RXNE) == 0) {
    if (timeout-- == 0) {
      break;
    }
  }
  if (timeout == 0) {
    return -1;
  }
  return 0;
}

static int wait_for_nbusy(SPI_TypeDef *spi, uint32_t timeout) {
  while ((spi->SR) & SPI_SR_BSY) {
    if (timeout-- == 0) {
      break;
    }
  }
  if (timeout == 0) {
    return -1;
  }
  return 0;
}

/*
uint32_t ui_board_get_data(void) {
  return hspi.Instance->DR;
}

uint16_t ui_board_get_data_16(void) {
  return (uint16_t)(ui_board_get_data() & 0xffff);
}

uint8_t ui_board_get_data_8(void) {
  return (uint8_t)(ui_board_get_data() & 0xff);
}
*/
