/*
 * marble_board.c
 *
 *  Created on: 11.09.2020
 *      Author: micha
 */
#include "stm32f2xx_hal.h"
#include "main.h"
#include "marble_api.h"
#include "string.h"
#include <stdio.h>

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
static void MX_USART3_UART_Init(void);


// Override default (weak) IRQHandler
void UART0_IRQHandler(void)
{
   //Chip_UART_IRQRBHandler(LPC_UART0, &rxring, &txring);
}

/* Initialize UART pins */
void marble_UART_init(void)
{

}

/* Send \0 terminated string over UART. Returns number of bytes sent */
int marble_UART_send(const char *str, int size)
{
   HAL_UART_Transmit(&huart1, str, size, 1000);
   //int sent;
   //int wait_cnt;
   //sent = Chip_UART_SendRB(LPC_UART0, &txring, str, size);
   //if (sent < size) {
    //  for (wait_cnt = UART_WAIT; wait_cnt > 0; wait_cnt--) {} // Busy-wait
      //sent += Chip_UART_SendRB(LPC_UART0, &txring, str+sent, size-sent);
   //}
   return 1;
}

/* Read at most size-1 bytes (due to \0) from UART. Returns bytes read */
int marble_UART_recv(char *str, int size)
{
	if(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) == SET){
		HAL_UART_Receive(&huart1, str, size, 100);
		return 1;
	}else{
		return 0;
	}
	//Chip_UART_ReadRB(LPC_UART0, &rxring, str, size);

}

/************
* LEDs
************/

#define MAXLEDS 3
static const uint8_t ledports[MAXLEDS] = {2, 2, 2};
static const uint8_t ledpins[MAXLEDS] = {21, 25, 24};

/* Initializes board LED(s) */
static void marble_LED_init(void)
{
   int i;

   /* Setup port direction and initial output state */
   for (i = 0; i < MAXLEDS; i++) {
      //Chip_GPIO_WriteDirBit(LPC_GPIO, ledports[i], ledpins[i], true);
      //Chip_GPIO_WritePortBit(LPC_GPIO, ledports[i], ledpins[i], true);
   }
}

/* Sets the state of a board LED to on or off */
void marble_LED_set(uint8_t led_num, bool on)
{
   if (led_num < MAXLEDS) {
      /* Set state, low is on, high is off */
      //Chip_GPIO_SetPinState(LPC_GPIO, ledports[led_num], ledpins[led_num], !on);
   }
}

/* Returns the current state of a board LED */
bool marble_LED_get(uint8_t led_num)
{
   bool state = false;

   if (led_num < MAXLEDS) {
      //state = Chip_GPIO_GetPinState(LPC_GPIO, ledports[led_num], ledpins[led_num]);
   }

   /* These LEDs are reverse logic. */
   return !state;
}

/* Toggles the current state of a board LED */
void marble_LED_toggle(uint8_t led_num)
{

   HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_0);
   HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_1);
   HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_2);
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

void marble_PSU_pwr(bool on)
{
	if(on == false){
		SystemClock_Config_HSI(); //switch to internal clock source, external clock is powered from 3V3!
		HAL_Delay(50);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, on);
	}else{
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, on);
		SystemClock_Config(); //switch back to external clock source
	}

   //Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 31, true);
   //Chip_GPIO_WritePortBit(LPC_GPIO, 1, 31, on);
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
   //if (Chip_GPIO_ReadPortBit(LPC_GPIO, 0, 19) == 0x00) {
   //   return false;
   //}
   return true;
}

void reset_fpga(void)
{
   /* Pulse the PROGRAM_B pin low; it's spelled PROG_B on schematic */
   const uint8_t rst_port = 0;
   const uint8_t rst_pin = 4;
   //Chip_GPIO_WriteDirBit(LPC_GPIO, rst_port, rst_pin, true);
   for (int wait_cnt = 20; wait_cnt > 0; wait_cnt--) {
      /* Only the first call does anything, but the compiler
       * doesn't know that.  That keeps its optimizer from
       * eliminating the associated time delay.
       */
      //Chip_GPIO_WritePortBit(LPC_GPIO, rst_port, rst_pin, false);
   }
   //Chip_GPIO_WritePortBit(LPC_GPIO, rst_port, rst_pin, true);
   //Chip_GPIO_WriteDirBit(LPC_GPIO, rst_port, rst_pin, false);
}

/************
* I2C
************/
#define SPEED_100KHZ 100000
#define I2C_POLL 1



// Override default (weak) IRQHandlers
void I2C0_IRQHandler(void)
{
   //i2c_state_handling(I2C0);
}
void I2C1_IRQHandler(void)
{
   //i2c_state_handling(I2C1);
}
void I2C2_IRQHandler(void)
{
   //i2c_state_handling(I2C2);
}




/* Generic I2C send function with selectable I2C bus and 8-bit I2C addresses (R/W bit = 0) */
int marble_I2C_send(I2C_BUS I2C_bus, uint8_t addr, const uint8_t *data, int size) {
   //addr = addr >> 1;
   switch (I2C_bus) {
      case I2C_IPMB:
         return 0; //Chip_I2C_MasterSend(I2C0, addr, data, size);
      case I2C_PM:
    	  return HAL_I2C_Master_Transmit(&hi2c3,(uint16_t)addr, data, size, HAL_MAX_DELAY);
      case I2C_FPGA:
    	  return HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)addr, data, size, HAL_MAX_DELAY);
   }
}

int marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
   //addr = addr >> 1;
   switch (I2C_bus) {
      case I2C_IPMB:
         return 0; //Chip_I2C_MasterCmdRead(I2C0, addr, cmd, data, size);
      case I2C_PM:
         return HAL_I2C_Mem_Write(&hi2c3, (uint16_t)addr, cmd, 1, data, size, HAL_MAX_DELAY);
      case I2C_FPGA:
    	  return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)addr, cmd, 1, data, size, HAL_MAX_DELAY);
   }
}

int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size) {
   //addr = addr >> 1;
   switch (I2C_bus) {
      case I2C_IPMB:
         //return HAL_I2C_Master_Transmit(&hi2c1, addr, data, size, HAL_MAX_DELAY);
    	 return 0;
      case I2C_PM:
         return HAL_I2C_Master_Receive(&hi2c3, (uint16_t)addr, data, size, HAL_MAX_DELAY);
      case I2C_FPGA:
    	  return HAL_I2C_Master_Receive(&hi2c1, (uint16_t)addr, data, size, HAL_MAX_DELAY);
   }
}

int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
   //addr = addr >> 1;
   switch (I2C_bus) {
      case I2C_IPMB:
         return 0; //Chip_I2C_MasterCmdRead(I2C0, addr, cmd, data, size);
      case I2C_PM:
         return HAL_I2C_Mem_Read(&hi2c3, (uint16_t)addr, cmd, 1, data, size, HAL_MAX_DELAY);
      case I2C_FPGA:
    	  return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)addr, cmd, 1, data, size, HAL_MAX_DELAY);
   }
}

/************
* SSP/SPI
************/


int marble_SSP_write(SSP_PORT ssp, uint8_t *buffer, int size)
{
   //if (ssp == SSP_FPGA) {
   //   return Chip_SSP_WriteFrames_Blocking(LPC_SSP0, buffer, size);
   //} else if (ssp == SSP_PMOD) {
   //   return Chip_SSP_WriteFrames_Blocking(LPC_SSP1, buffer, size);
  // }
   return 0;
}

int marble_SSP_read(SSP_PORT ssp, uint8_t *buffer, int size)
{
   //if (ssp == SSP_FPGA) {
   //   return Chip_SSP_ReadFrames_Blocking(LPC_SSP0, buffer, size);
   //} else if (ssp == SSP_PMOD) {
   //   return Chip_SSP_ReadFrames_Blocking(LPC_SSP1, buffer, size);
   //}
   return 0;
}

/************
* MDIO to PHY
************/
void marble_MDIO_init()
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

uint32_t marble_MDIO_read(uint8_t reg)
{
	uint32_t value;
	printf("\r\nPHYread Stat: %d\r\n", HAL_ETH_ReadPHYRegister(&heth, reg, &value));
	//printf("\r\nPHYread Stat: %d\r\n", HAL_ETH_ReadPHYRegister(&heth, reg, &value));
	return value;
}

void i2c_scan(void)
{
	printf("Scanning I2C_PM bus:\r\n");
	HAL_StatusTypeDef result;
	uint8_t i;
	uint8_t j;
	for (i = 1; i < 128; i++)
	{
			/*
			 * the HAL wants a left aligned i2c address
			 * &hi2c1 is the handle
			 * (uint16_t)(i<<1) is the i2c address left aligned
			 * retries 2
			 * timeout 2
			 */
		result = HAL_I2C_IsDeviceReady(&hi2c3, (uint16_t) (i<<1), 2, 2);
		if (result != HAL_OK) // HAL_ERROR or HAL_BUSY or HAL_TIMEOUT
		{
			printf("."); // No ACK received at that address
		}

		if (result == HAL_OK)
		{
			printf("0x%02X", i << 1); // Received an ACK at that address
		}
	}

	printf("\r\n");

	printf("Scanning I2C_APP bus:\r\n");
	for (j = 0; j < 8; j++)
	{
		printf("\r\nI2C switch port: %d\r\n", j);
		switch_i2c_bus(j);
		HAL_Delay(100);
		for (i = 1; i < 128; i++)
			{
				result = HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t) (i<<1), 2, 2);
				if (result != HAL_OK) // HAL_ERROR or HAL_BUSY or HAL_TIMEOUT
				{
					printf("."); // No ACK received at that address
				}

				if (result == HAL_OK)
				{
					printf("0x%02X", i << 1); // Received an ACK at that address
				}
			}
	}
	printf("\r\n");


}


/************
* Board Init
************/

void marble_init(bool use_xtal)
{
   // Must happen before any other clock manipulations:
   HAL_Init();
   SystemClock_Config_HSI();

   MX_GPIO_Init();
   marble_PSU_pwr(true);
   MX_USART1_UART_Init();
   MX_USART2_UART_Init();
   MX_USART3_UART_Init();
   MX_ETH_Init();
   MX_I2C1_Init();
   MX_I2C3_Init();
   MX_SPI1_Init();
   MX_SPI2_Init();

   marble_LED_init();
   marble_SW_init();
   marble_UART_init();
   ina219_init();
   adn4600_init();

   printf("** Marble Test ** \n");
   printf("** Init done ** \n");

   // Init I2C busses in interrupt mode
   //marble_I2C_init(I2C0, !I2C_POLL);
   //marble_I2C_init(I2C1, !I2C_POLL);
   //marble_I2C_init(I2C2, !I2C_POLL);

   // Init SSP busses
   //marble_SSP_init(LPC_SSP0);
   //marble_SSP_init(LPC_SSP1);

   //marble_MDIO_init();

   /* Configure the SysTick for 1 s interrupts */
   SysTick_Config(SystemCoreClock * 1);
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the CPU, AHB and APB busses clocks
  */
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
  /** Initializes the CPU, AHB and APB busses clocks
  */
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

  /** Initializes the CPU, AHB and APB busses clocks
  */
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
  /** Initializes the CPU, AHB and APB busses clocks
  */
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

/**
  * @brief ETH Initialization Function
  * @param None
  * @retval None
  */
static void MX_ETH_Init(void)
{

  /* USER CODE BEGIN ETH_Init 0 */

  /* USER CODE END ETH_Init 0 */

   uint8_t MACAddr[6] ;

  /* USER CODE BEGIN ETH_Init 1 */

  /* USER CODE END ETH_Init 1 */
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

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ETH_Init 2 */

  /* USER CODE END ETH_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.ClockSpeed = 100000;
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
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_HARD_OUTPUT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
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
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
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
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
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

  /*Configure GPIO pins : PE2 PE5 PE8 PE9
                           PE10 PE11 PE12 PE13
                           PE14 PE15 PE0 PE1 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_5|GPIO_PIN_8|GPIO_PIN_9
                          |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PC0 PC7 PC8 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_7|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
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

  /*Configure GPIO pins : PD9 PD10 PD11 PD1
                           PD2 */

  //GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_1
  //                        |GPIO_PIN_2;
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_1
                          |GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : PD12 PD13 PD14 PD15
                           PD0 PD3 PD4 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15
                          |GPIO_PIN_0|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PC12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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

