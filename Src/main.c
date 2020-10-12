/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/

#include "main.h"
#include "i2c_pm.h"
#include "i2c_fpga.h"
#include "marble_api.h"
#include <stm32f2xx_hal_flash_ex.h>

#ifdef __GNUC__
/* With GCC Compiler, small printf (option LD Linker->Libraries->Small printf
   set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

// Setup UART strings
const char demo_str[] = "Marble Mini v1 UART DEMO\r\n";
const char lb_str[] = "Loopback... ESC to exit\r\n";
const char menu_str[] = "\r\n"
	"Menu:\r\n"
	"0) Loopback\r\n"
	"1) MDIO/PHY\r\n"
	"2) I2C monitor\r\n"
	"3) Live counter\r\n"
	"4) GPIO control\r\n"
	"5) Reset FPGA\r\n"
	"6) Push IP&MAC\r\n"
	"7) MAX6639\r\n"
	"8) LM75_0\r\n"
	"9) LM75_1\r\n"
	"a) I2C scan all ports\r\n"
	"b) Config ADN4600\r\n"
	"c) INA219 Main power supply\r\n"
	"d) MGT MUX - switch to QSFP 2\r\n";

const char unk_str[] = "> Unknown option\r\n";
const char gpio_str[] = "GPIO pins, caps for on, lower case for off\r\n"
	"a) FMC power\r\n"
	"b) EN_PSU_CH\r\n";


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

/* USER CODE BEGIN PFP */
static void gpio_cmd(void)
{
   char rx_ch;
   marble_UART_send(gpio_str, strlen(gpio_str));
   while(marble_UART_recv(&rx_ch, 1) == 0);
   bool on = ((rx_ch >> 5) & 1) == 0;
   if (on) marble_UART_send("ON\r\n", 4);
   else marble_UART_send("OFF\r\n", 5);
   switch (rx_ch) {
      case 'a':
      case 'A':
         marble_FMC_pwr(on);
         break;
      case 'b':
      case 'B':
         marble_PSU_pwr(on);
         break;
      default:
         marble_UART_send(unk_str, strlen(unk_str));
         break;
      }
}

static void print_uint(unsigned int x)
{
   char obuf[16];
   char *p = obuf+15;
   *p-- = '\0';
   do {
      *p-- = '0' + (x % 10);
      x = x/10;
   } while (x>0);
   marble_UART_send(p+1, obuf+14-p);
}


static void print_mac_ip(unsigned char mac_ip_data[10])
{
   for (unsigned ix=0; ix<10; ix++) {
      marble_UART_send(" ", 1);
      print_uint(mac_ip_data[ix]);
   }
   marble_UART_send("\r\n", 2);
}

int main(void)
{
	marble_init(0);

  while (1)
  {

	  unsigned char mac_ip_data[10] = {
	  	        18, 85, 85, 0, 1, 46,  // MAC (locally managed)
	  	        192, 168, 19, 31   // IP
	  	     };
	  marble_UART_send(demo_str, strlen(demo_str));

	  uint8_t rx_ch;

	  	     while (true) {

	  	        //marble_UART_send(menu_str, strlen(menu_str));
	  	    	 printf(menu_str);
	  	        // Wait for user selection

	  	    	while(marble_UART_recv(&rx_ch, 1) == 0);

	  	        switch (rx_ch) {
	  	           case '0':
	  	              printf(lb_str);
	  	              do {
	  	                 if (marble_UART_recv(&rx_ch, 1) != 0){
	  	                	 marble_UART_send(&rx_ch, 1);

	  	                 }
	  	              } while (rx_ch != 27);
	  	              printf("\r\n");
	  	              break;
	  	           case '1':
	  	              phy_print();
	  	              break;
	  	           case '2':
	  	              I2C_PM_probe();
	  	              break;
	  	           case '3':
	  	              printf("Live counter: %d\r\n",HAL_GetTick());
	  	              break;
	  	           case '4':
	  	              gpio_cmd();
	  	              break;
	  	           case '5':
	  	              reset_fpga();
	  	              break;
	  	           case '6':
	  	              print_mac_ip(mac_ip_data);
	  	              //push_fpga_mac_ip(mac_ip_data);
	  	              printf("DONE\r\n");
	  	              break;
	  	           case '7':
	  	              printf("Start\r\n");
	  	              print_max6639();
	  	              break;
	  	           case '8':
	  	              // Demonstrate setting over-temperature register and Interrupt mode
	  	              LM75_write(LM75_0, LM75_OS, 100*2);
	  	              LM75_write(LM75_0, LM75_CFG, LM75_CFG_COMP_INT);
	  	              LM75_print(LM75_0);
	  	              break;
	  	           case '9':
	  	              // Demonstrate setting over-temperature register
	  	              LM75_write(LM75_1, LM75_OS, 100*2);
	  	              LM75_print(LM75_1);
	  	              break;
	  	           case 'a':
	  	        	   printf("I2C scanner\r\n");
	  	        	   i2c_scan();
	  	        	   break;
	  	           case 'b':
					   printf("ADN4600\r\n");
					   switch_i2c_bus(2);
					   adn4600_init();
					   adn4600_printStatus();
					   //i2c_scan();
					   break;
	  	           case 'c':
					   printf("INA test\r\n");

					   switch_i2c_bus(6);
					   ina219_init();
					   //printf("Main bus: %dV, %dmA", getBusVoltage_V(INA219_0), getCurrentAmps(INA219_0));
					   getBusVoltage_V(INA219_0);
					   getCurrentAmps(INA219_0);
					   break;
	  	           case 'd':
	  	        	   printf("Switch MGT to QSFP 2\r\n");
	  	        	   HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, true);
	  	        	   break;
	  	           default:
	  	              printf(unk_str);
	  	              break;
	  	        }
	  	     }

  }
  /* USER CODE END 3 */
}

PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART3 and Loop until the end of transmission */
  marble_UART_send((uint8_t *)&ch, 1);

  return ch;
}

void phy_print()
{
	uint32_t value;
	//char p_buf[40];

	//marble_MDIO_write(4, 10);

	for (uint16_t i=0; i < 4; i++) {

	   value = marble_MDIO_read(i);
	   //snprintf(p_buf, 40, "  reg[%2.2x] = %4.4x\r\n", i, value);
	   //marble_UART_send(p_buf, strlen(p_buf));
	   printf( "  reg[%2.2x] = %4.4x\r\n", i, value);
	}

}

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
