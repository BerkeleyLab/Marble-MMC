/*
 * Based in part on (and therefore legally derived from):
 * STM32 HAL Library
 *
 * which is COPYRIGHT(c) 2017 STMicroelectronics
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "stm32f2xx_hal.h"
#include "marble_api.h"
#include "string.h"
#include <stdio.h>

void Error_Handler(void) {}
#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif /* USE_FULL_ASSERT */

void SystemClock_Config_HSI(void);

ETH_HandleTypeDef heth;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c3;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;



/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ETH_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C3_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);


/* Initialize UART pins */
void marble_UART_init(void)
{
   /* PA9, PA10 - MMC_CONS_PROG */
   MX_USART1_UART_Init();
   /* PD5, PD6 - UART4 (Pmod3_7/3_6) */
   MX_USART2_UART_Init();
}

/* Send \0 terminated string over UART. Returns number of bytes sent */
int marble_UART_send(const char *str, int size)
{
   HAL_UART_Transmit(&huart1, (const uint8_t *) str, size, 1000);
   return 1;
}

/* Read at most size-1 bytes (due to \0) from UART. Returns bytes read */
// TODO: Should return bytes read
int marble_UART_recv(char *str, int size)
{
   if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) == SET) {
      HAL_UART_Receive(&huart1, (uint8_t *) str, size, 100);
      return 1;
   } else {
      return 0;
   }
}

/************
* LEDs
************/

#define MAXLEDS 3
static const uint8_t ledpins[MAXLEDS] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2};

/* Initializes board LED(s) */
static void marble_LED_init(void)
{
   // Handled in MX_GPIO_Init
   return;
}

/* Sets the state of a board LED to on or off */
void marble_LED_set(uint8_t led_num, bool on)
{
   if (led_num < MAXLEDS) {
      /* Set state, low is on, high is off */
      HAL_GPIO_WritePin(GPIOE, ledpins[led_num], !on);
   }
}

/* Returns the current state of a board LED */
bool marble_LED_get(uint8_t led_num)
{
   bool state = false;

   if (led_num < MAXLEDS) {
      state = HAL_GPIO_ReadPin(GPIOE, ledpins[led_num]);
   }

   /* These LEDs are reverse logic. */
   return !state;
}

/* Toggles the current state of a board LED */
void marble_LED_toggle(uint8_t led_num)
{
   if (led_num < MAXLEDS) {
      HAL_GPIO_TogglePin(GPIOE, ledpins[led_num]);
   }
}

/************
* FMC & PSU
************/

/* Set FMC power */
void marble_FMC_pwr(bool on)
{
   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9, on);
   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, on);
}

uint8_t marble_FMC_status(void)
{
   uint8_t status = 0;
   status = WRITE_BIT(status, M_FMC_STATUS_FMC1_PWR,  HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_10));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC1_FUSE, HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_10));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC2_PWR,  HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_9));
   status = WRITE_BIT(status, M_FMC_STATUS_FMC2_FUSE, HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11));
   return status;
}

void marble_PSU_pwr(bool on)
{
   if (on == false) {
      SystemClock_Config_HSI(); // switch to internal clock source, external clock is powered from 3V3!
      marble_SLEEP_ms(50);
      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, on);
   } else {
      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, on);
      SystemClock_Config(); // switch back to external clock source
   }
}

uint8_t marble_PWR_status(void)
{
   uint8_t status = 0;
   status = WRITE_BIT(status, M_PWR_STATUS_PSU_EN, HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_11));
   status = WRITE_BIT(status, M_PWR_STATUS_POE,    HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8));
   status = WRITE_BIT(status, M_PWR_STATUS_OTEMP,  HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7));
   return status;
}

/************
* Switches and FPGA interrupt
************/

static void marble_SW_init(void)
{
   // SW1 and SW2
   //Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 12, false);
}

bool marble_SW_get(void)
{
   // Pin pulled low on button press
   //if (Chip_GPIO_ReadPortBit(LPC_GPIO, 2, 12) == 0x01) {
   //   return false;
   //}
   return true;
}

bool marble_FPGAint_get(void)
{
   if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3)) {
      return false;
   }
   return true;
}

void reset_fpga(void)
{
   /* Pull the PROGRAM_B pin low; it's spelled PROG_B on schematic */
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, false);
   marble_SLEEP_ms(50);
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, true);
}

/************
* GPIO interrupt setup and user-defined handlers
************/
void FPGA_DONE_dummy(void) {}
void (*volatile marble_FPGA_DONE_handler)(void) = FPGA_DONE_dummy;


// Override default (weak) IRQHandler and redirect to HAL shim
void EXTI0_IRQHandler(void)
{
   HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

// Override default (weak) callback
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
   if (GPIO_Pin == GPIO_PIN_0) { // Handles any interrupt on line 0 (e.g. PA0, PB0, PC0, PD0)
      marble_FPGA_DONE_handler();
   }
}

void marble_GPIOint_init(void)
{
   /*Configure GPIO pin : PD0 - FPGA_DONE rising edge interrupt */
   GPIO_InitTypeDef GPIO_InitStruct = {0};
   GPIO_InitStruct.Pin = GPIO_PIN_0;
   GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

   /* Enable interrupt in the NVIC */
   HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
   HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/* Register user-defined interrupt handlers */
void marble_GPIOint_handlers(void (*FPGA_DONE_handler)(void)) {
   marble_FPGA_DONE_handler = FPGA_DONE_handler;
}

/************
* MGT Multiplexer
************/
#define MAXMGT 3
static const uint16_t mgtmux_pins[MAXMGT] = {GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_10};

void marble_MGTMUX_set(uint8_t mgt_num, bool on)
{
   mgt_num -= 1;
   if (mgt_num < MAXMGT) {
      HAL_GPIO_WritePin(GPIOE, mgtmux_pins[mgt_num], on);
   }
}

uint8_t marble_MGTMUX_status()
{
   uint8_t status = 0;
   for (unsigned i=0; i < MAXMGT; i++) {
      status |= HAL_GPIO_ReadPin(GPIOE, mgtmux_pins[i])<<i;
   }
   return status;
}

/************
* I2C
************/
#define SPEED_100KHZ 100000


/* Non-destructive I2C probe function based on empty data command, i.e. S+[A,RW]+P */
int marble_I2C_probe(I2C_BUS I2C_bus, uint8_t addr) {
   return HAL_I2C_IsDeviceReady(I2C_bus, addr, 2, 2);
}

/* Generic I2C send function with selectable I2C bus and 8-bit I2C addresses (R/W bit = 0) */
/* 1-byte register addresses */
int marble_I2C_send(I2C_BUS I2C_bus, uint8_t addr, const uint8_t *data, int size) {
   return HAL_I2C_Master_Transmit(I2C_bus, (uint16_t)addr, data, size, HAL_MAX_DELAY);
}

int marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
   return HAL_I2C_Mem_Write(I2C_bus, (uint16_t)addr, cmd, 1, data, size, HAL_MAX_DELAY);
}

int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size) {
   return HAL_I2C_Master_Receive(I2C_bus, (uint16_t)addr, data, size, HAL_MAX_DELAY);
}

int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
   return HAL_I2C_Mem_Read(I2C_bus, (uint16_t)addr, cmd, 1, data, size, HAL_MAX_DELAY);
}

/* Same but 2-byte register addresses */
int marble_I2C_cmdsend_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
   return HAL_I2C_Mem_Write(I2C_bus, (uint16_t)addr, cmd, 2, data, size, HAL_MAX_DELAY);
}
int marble_I2C_cmdrecv_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
   return HAL_I2C_Mem_Read(I2C_bus, (uint16_t)addr, cmd, 2, data, size, HAL_MAX_DELAY);
}

/************
* SSP/SPI
************/
static void SPI_CSB_SET(SSP_PORT ssp, bool set);

int marble_SSP_write16(SSP_PORT ssp, uint16_t *buffer, unsigned size)
{
   SPI_CSB_SET(ssp, false);
   int rc = HAL_SPI_Transmit(ssp, (uint8_t*) buffer, size, HAL_MAX_DELAY);
   SPI_CSB_SET(ssp, true);
   return rc;
}

int marble_SSP_read16(SSP_PORT ssp, uint16_t *buffer, unsigned size)
{
   SPI_CSB_SET(ssp, false);
   int rc = HAL_SPI_Receive(ssp, (uint8_t*) buffer, size, HAL_MAX_DELAY);
   SPI_CSB_SET(ssp, true);
   return rc;
}

int marble_SSP_exch16(SSP_PORT ssp, uint16_t *tx_buf, uint16_t *rx_buf, unsigned size)
{
   SPI_CSB_SET(ssp, false);
   int rc = HAL_SPI_TransmitReceive(ssp, (uint8_t*) tx_buf, (uint8_t*) rx_buf,size, HAL_MAX_DELAY);
   SPI_CSB_SET(ssp, true);
   return rc;
}

/************
* MDIO to PHY
************/
void marble_MDIO_init(void)
{
   //Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_ENET);

   /* Setup MII clock rate and PHY address */
   //Chip_ENET_SetupMII(LPC_ETHERNET, Chip_ENET_FindMIIDiv(LPC_ETHERNET, 2500000), 0);
}

void marble_MDIO_write(uint16_t reg, uint32_t data)
{
   //Chip_ENET_StartMIIWrite(LPC_ETHERNET, reg, data);
   HAL_ETH_WritePHYRegister(&heth, reg, data);
   //while (Chip_ENET_IsMIIBusy(LPC_ETHERNET));
}

uint32_t marble_MDIO_read(uint16_t reg)
{
   uint32_t value;
   printf("\r\nPHYread Stat: %d\r\n", HAL_ETH_ReadPHYRegister(&heth, reg, &value));
   return value;
}

/************
* System Timer and Stopwatch
************/
void SysTick_Handler_dummy(void) {}
void (*volatile marble_SysTick_Handler)(void) = SysTick_Handler_dummy;

// Override default (weak) SysTick_Handler
void SysTick_Handler(void)
{
   HAL_IncTick(); // Advances HAL timebase used in HAL_Delay
   if (marble_SysTick_Handler)
      marble_SysTick_Handler();
}

/* Register user-defined interrupt handlers */
void marble_SYSTIMER_handler(void (*handler)(void)) {
   marble_SysTick_Handler = handler;
}

/* Configures 24-bit count-down timer and enables systimer interrupt */
uint32_t marble_SYSTIMER_ms(uint32_t delay)
{
   // WARNING: Hardcoded to 1 ms since this is what increments HAL_IncTick() and
   // enables HAL_Delay
   delay = 1; // TODO: Consider decoupling stopwatch from system timer

   const uint32_t MAX_TICKS = (1<<24)-1;
   const uint32_t MAX_DELAY_MS = (SystemCoreClock * 1000U) / MAX_TICKS;
   uint32_t ticks = (SystemCoreClock / 1000U) * delay;
   if (delay > MAX_DELAY_MS) {
      ticks = MAX_TICKS;
      delay = MAX_DELAY_MS;
   }
   SysTick_Config(ticks);
   return delay;
}

void marble_SLEEP_ms(uint32_t delay)
{
   HAL_Delay(delay); // TODO: Consider replacing with stopwatch built from timer peripheral
}

void marble_SLEEP_us(uint32_t delay)
{
   (void) delay;
   return; // XXX Not available unless HAL weak definitions are overridden
   // Good thing nobody depends on this (yet)
}


/************
* Board Init
************/

uint32_t marble_init(bool use_xtal)
{
   (void) use_xtal;  // feature not yet supported with this chip

   // Must happen before any other clock manipulations:
   HAL_Init();
   SystemClock_Config_HSI();

   MX_GPIO_Init();

   // Configure GPIO interrupts
   marble_GPIOint_init();

   marble_PSU_pwr(true);
   MX_ETH_Init();
   MX_I2C1_Init();
   MX_I2C3_Init();
   MX_SPI1_Init();
   MX_SPI2_Init();

   marble_LED_init();
   marble_SW_init();
   marble_UART_init();

   printf("** Marble init done **\r\n");

   // Init SSP busses
   //marble_SSP_init(LPC_SSP0);
   //marble_SSP_init(LPC_SSP1);

   //marble_MDIO_init();
   return 0;
}

void SystemClock_Config(void)
{
   RCC_OscInitTypeDef RCC_OscInitStruct = {0};
   RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

   /** Initializes the CPU, AHB and APB busses clocks */
   RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
   RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
   RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
   RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
   RCC_OscInitStruct.PLL.PLLM = 20;
   RCC_OscInitStruct.PLL.PLLN = 192;
   RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
   RCC_OscInitStruct.PLL.PLLQ = 4;
   if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
   {
      Error_Handler();
   }
   /** Initializes the CPU, AHB and APB busses clocks */
   RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                               |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
   RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
   RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
   RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
   RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

   if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
   {
      Error_Handler();
   }
}

void SystemClock_Config_HSI(void)
{
   RCC_OscInitTypeDef RCC_OscInitStruct = {0};
   RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

   /** Initializes the CPU, AHB and APB busses clocks */
   RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
   RCC_OscInitStruct.HSIState = RCC_HSI_ON;
   RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
   RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
   RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
   RCC_OscInitStruct.PLL.PLLM = 13;
   RCC_OscInitStruct.PLL.PLLN = 195;
   RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
   RCC_OscInitStruct.PLL.PLLQ = 4;
   if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
   {
     Error_Handler();
   }
   /** Initializes the CPU, AHB and APB busses clocks */
   RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                               |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
   RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
   RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
   RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
   RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

   if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
   {
      Error_Handler();
   }
}

static void MX_ETH_Init(void)
{
   uint8_t MACAddr[6] ;

  heth.Instance = ETH;
  heth.Init.AutoNegotiation = ETH_AUTONEGOTIATION_DISABLE;
  heth.Init.PhyAddress = PHY_USER_NAME_PHY_ADDRESS;
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;
  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.RxMode = ETH_RXPOLLING_MODE;
  heth.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
  heth.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;

  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_I2C1_Init(void)
{
   hi2c1.Instance = I2C1;
   hi2c1.Init.ClockSpeed = SPEED_100KHZ;
   hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
   hi2c1.Init.OwnAddress1 = 0;
   hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
   hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
   hi2c1.Init.OwnAddress2 = 0;
   hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
   hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
   if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
      Error_Handler();
   }
   I2C_FPGA = &hi2c1;  // set global
}

static void MX_I2C3_Init(void)
{
   hi2c3.Instance = I2C3;
   hi2c3.Init.ClockSpeed = SPEED_100KHZ;
   hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
   hi2c3.Init.OwnAddress1 = 0;
   hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
   hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
   hi2c3.Init.OwnAddress2 = 0;
   hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
   hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
   if (HAL_I2C_Init(&hi2c3) != HAL_OK)
   {
      Error_Handler();
   }
   I2C_PM = &hi2c3;  // set global
}

static void MX_SPI1_Init(void)
{
   hspi1.Instance = SPI1;
   hspi1.Init.Mode = SPI_MODE_MASTER;
   hspi1.Init.Direction = SPI_DIRECTION_2LINES;
   hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
   hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
   hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
   hspi1.Init.NSS = SPI_NSS_SOFT;
   hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
   hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
   hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
   hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
   hspi1.Init.CRCPolynomial = 10;
   if (HAL_SPI_Init(&hspi1) != HAL_OK)
   {
      Error_Handler();
   }
   SSP_FPGA = &hspi1;
}

static void MX_SPI2_Init(void)
{
   hspi2.Instance = SPI2;
   hspi2.Init.Mode = SPI_MODE_MASTER;
   hspi2.Init.Direction = SPI_DIRECTION_2LINES;
   hspi2.Init.DataSize = SPI_DATASIZE_16BIT;
   hspi2.Init.CLKPolarity = SPI_POLARITY_HIGH;
   hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
   hspi2.Init.NSS = SPI_NSS_SOFT;
   hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
   hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
   hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
   hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
   hspi2.Init.CRCPolynomial = 10;
   if (HAL_SPI_Init(&hspi2) != HAL_OK)
   {
      Error_Handler();
   }
   SSP_PMOD = &hspi2;
}

static void SPI_CSB_SET(SSP_PORT ssp, bool set)
{
   if (ssp == SSP_FPGA) {
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, set ? GPIO_PIN_SET : GPIO_PIN_RESET);
   } else {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, set ? GPIO_PIN_SET : GPIO_PIN_RESET);
   }
}

static void MX_USART1_UART_Init(void)
{
   huart1.Instance = USART1;
   huart1.Init.BaudRate = 115200;
   huart1.Init.WordLength = UART_WORDLENGTH_8B;
   huart1.Init.StopBits = UART_STOPBITS_1;
   huart1.Init.Parity = UART_PARITY_NONE;
   huart1.Init.Mode = UART_MODE_TX_RX;
   huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
   huart1.Init.OverSampling = UART_OVERSAMPLING_16;
   if (HAL_UART_Init(&huart1) != HAL_OK)
   {
      Error_Handler();
   }
}

static void MX_USART2_UART_Init(void)
{
   huart2.Instance = USART2;
   huart2.Init.BaudRate = 115200;
   huart2.Init.WordLength = UART_WORDLENGTH_8B;
   huart2.Init.StopBits = UART_STOPBITS_1;
   huart2.Init.Parity = UART_PARITY_NONE;
   huart2.Init.Mode = UART_MODE_TX_RX;
   huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
   huart2.Init.OverSampling = UART_OVERSAMPLING_16;
   if (HAL_UART_Init(&huart2) != HAL_OK)
   {
      Error_Handler();
   }
}

static void MX_GPIO_Init(void)
{
   GPIO_InitTypeDef GPIO_InitStruct = {0};

   /* GPIO Ports Clock Enable */
   __HAL_RCC_GPIOE_CLK_ENABLE();
   __HAL_RCC_GPIOH_CLK_ENABLE();
   __HAL_RCC_GPIOC_CLK_ENABLE();
   __HAL_RCC_GPIOA_CLK_ENABLE();
   __HAL_RCC_GPIOB_CLK_ENABLE();
   __HAL_RCC_GPIOD_CLK_ENABLE();

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOE,GPIO_PIN_0|GPIO_PIN_1| GPIO_PIN_2|GPIO_PIN_5|GPIO_PIN_8|GPIO_PIN_9
                           |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                           |GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_RESET);

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_1
                           |GPIO_PIN_2, GPIO_PIN_RESET);

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);

   /*Configure GPIO pins : PE2 PE8 PE9
                            PE10 PE11 PE12 PE13
                            PE14 PE15 PE0 PE1 */
   GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_8|GPIO_PIN_9
                           |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                           |GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

   /*Configure GPIO pins : PE5 (FPGA_RESETn) */
   GPIO_InitStruct.Pin = GPIO_PIN_5;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, true);
   HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

   /*Configure GPIO pins : PC7 PC8 */
   GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_8;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

   /*Configure GPIO pins : PC0 - PROG_B */
   GPIO_InitStruct.Pin = GPIO_PIN_0;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

   // N.B.: Order matters here; GPIO state must be set before it's init'd
   // so that there's no temporary glitch that pulls low PROG_B and thus
   // resets the FPGA
   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, true);
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

   /*Configure GPIO pin : PA0 */
   GPIO_InitStruct.Pin = GPIO_PIN_0;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

   /*Configure GPIO pin : PA3 - FPGA_INT (just an input for now; not an interrupt) */
   GPIO_InitStruct.Pin = GPIO_PIN_3;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

   /*Configure GPIO pins : PB0 PB1 PB2 */
   GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

   /*Configure GPIO pin : PB14 */
   GPIO_InitStruct.Pin = GPIO_PIN_14;
   GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

   /*Configure GPIO pin : PB15 */
   GPIO_InitStruct.Pin = GPIO_PIN_15;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

   /* Configure GPIO pins : PD9 PD10 PD11 PD1 PD2 */
   // GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_1|GPIO_PIN_2;
   GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_1
                           |GPIO_PIN_2;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

   /* Configure GPIO pins : PD12 PD13 PD14 PD15 PD0 PD3 PD4 */
   GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15
                           |GPIO_PIN_3|GPIO_PIN_4;
   GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

   /* Configure GPIO pin : PC12 */
   GPIO_InitStruct.Pin = GPIO_PIN_12;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */

#ifdef  USE_FULL_ASSERT
/**
   * @brief  Reports the name of the source file and the source line number
   *         where the assert_param error has occurred.
   * @param  file: pointer to the source file name
   * @param  line: assert_param error line source number
   * @retval None
*/
void assert_failed(uint8_t *file, uint32_t line)
{
   /* USER CODE BEGIN 6 */
   /* User can add his own implementation to report the file name and line number,
      tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
   /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

