#include <string.h>
#include "main.h"
#include "marble_api.h"
#include "i2c_pm.h"
#include "i2c_fpga.h"
#include <stm32f2xx_hal_flash_ex.h>
#include "rev.h"

void phy_print(void);

#ifdef __GNUC__
/* With GCC Compiler, small printf (option LD Linker->Libraries->Small printf
   set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

// Setup UART strings
const char demo_str[] = "Marble v2 UART DEMO\r\n";
const char lb_str[] = "Loopback... ESC to exit\r\n";
const char menu_str[] = "\r\n"
   "Build based on git commit " GIT_REV " \r\n"
   "Menu:\r\n"
   "0) Loopback\r\n"
   "1) MDIO/PHY\r\n"
   "2) I2C monitor\r\n"
   "3) Status counters\r\n"
   "4) GPIO control\r\n"
   "5) Reset FPGA\r\n"
   "6) Push IP&MAC\r\n"
   "7) MAX6639\r\n"
   "8) LM75_0\r\n"
   "9) LM75_1\r\n"
   "a) I2C scan all ports\r\n"
   "b) Config ADN4600\r\n"
   "c) INA219 Main power supply\r\n"
   "d) MGT MUX - switch to QSFP 2\r\n"
   "e) PM bus display\r\n"
   "f) XRP7724 flash\r\n"
   "g) XRP7724 go\r\n"
   "h) XRP7724 hex input\r\n"
   "i) timer check/cal\r\n"
   "j) save IP to EEPROM\r\n"
   "k) read IP from EEPROM\r\n";

const char unk_str[] = "> Unknown option\r\n";
const char gpio_str[] = "GPIO pins, caps for on, lower case for off\r\n"
   "a) FMC power\r\n"
   "b) EN_PSU_CH\r\n";

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

// We actually do have a libc installed,
// so this is useless.
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

static void ina219_test(void)
{
   switch_i2c_bus(6);
   if (1) {
      ina219_debug(INA219_0);
      ina219_debug(INA219_FMC1);
      ina219_debug(INA219_FMC2);
   } else {
      ina219_init();
      //printf("Main bus: %dV, %dmA", getBusVoltage_V(INA219_0), getCurrentAmps(INA219_0));
      getBusVoltage_V(INA219_0);
      getCurrentAmps(INA219_0);
   }
}

static void pm_bus_display(void)
{
   LM75_print(LM75_0);
   LM75_print(LM75_1);
   xrp_dump(XRP7724);
}

static void mgtclk_xpoint_en(void)
{
   if (xrp_ch_status(XRP7724, 1)) { // CH1: 3.3V
      adn4600_init();
   }
}

static void xrp_boot(void)
{
   uint8_t pwr_on=0;
   for (int i=1; i<5; i++) {
      pwr_on |= xrp_ch_status(XRP7724, i);
   }
   if (pwr_on) {
      printf("XRP already ON. Skipping autoboot...\r\n");
   } else {
      xrp_go(XRP7724);
      marble_SLEEP_ms(1000);
   }
}

unsigned int live_cnt=0;
unsigned int fpga_prog_cnt=0;

void fpga_done_handler(void) {
   fpga_prog_cnt++;
}

void timer_int_handler(void)
{
   live_cnt++;
   static uint16_t i = 0;
   // Snake-pattern LEDs
   if(i == 0)
      marble_LED_toggle(0);
   else if(i == 330)
      marble_LED_toggle(1);
   else if(i == 660)
      marble_LED_toggle(2);

   i = (i + 1) % 1000;
}

int main(void)
{
	uint8_t mac_ip_read_from_flash[10] = {
			0, 0, 0, 0, 0, 0,  // MAC (locally managed)
			0, 0, 0, 0   // IP
	};
   // Static for now; eventually needs to be read from EEPROM
   unsigned char mac_ip_data[10] = {
		   18, 85, 85, 0, 1, 46,  // MAC (locally managed)
		   192, 168, 19, 31   // IP
   };

   marble_init(0);

   /* Turn on LEDs */
   marble_LED_set(0, true);
   marble_LED_set(1, true);
   marble_LED_set(2, true);

#ifdef XRP_AUTOBOOT
   xrp_boot();
#endif

   // Enable MGT clock cross-point switch if 3.3V rail is ON
   mgtclk_xpoint_en();

   // Register GPIO interrupt handlers
   marble_GPIOint_handlers(fpga_done_handler);

   // Register System Timer interrupt handler
   marble_SYSTIMER_handler(timer_int_handler);

   /* Configure the System Timer for 20 Hz interrupts */
   marble_SYSTIMER_ms(50);

   // Send demo string over UART at 115200 baud
   marble_UART_send(demo_str, strlen(demo_str));

   while (1) {
      uint8_t rx_ch;
      printf("Single-character actions, ? for menu\r\n");
      // Wait for user selection
      while(marble_UART_recv(&rx_ch, 1) == 0);
      switch (rx_ch) {
         case '?':
            printf(menu_str);
            break;
         case '0':
            printf(lb_str);
            do {
               if (marble_UART_recv(&rx_ch, 1) != 0) {
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
            printf("Live counter: %ld\r\n", HAL_GetTick());
            printf("FPGA prog counter: %d\r\n", fpga_prog_cnt);
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
            I2C_PM_scan();
            I2C_FPGA_scan();
            break;
         case 'b':
            printf("ADN4600\r\n");
            adn4600_init();
            adn4600_printStatus();
            break;
         case 'c':
            printf("INA test\r\n");
            ina219_test();
            break;
         case 'd':
            printf("Switch MGT to QSFP 2\r\n");
            HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, true);
            break;
         case 'e':
            printf("PM bus display\r\n");
            pm_bus_display();
            break;
         case 'f':
            printf("XRP flash\r\n");
            xrp_flash(XRP7724);
            break;
         case 'g':
            printf("XRP go\r\n");
            xrp_boot();
            break;
         case 'h':
            printf("XRP hex input\r\n");
            xrp_hex_in(XRP7724);
            break;
         case 'i':
            for (unsigned ix=0; ix<10; ix++) {
               printf("%d\n", ix);
               marble_SLEEP_ms(1000);
            }
            break;
         case 'j':
        	init_eeprom();
			for (unsigned ix=0; ix<10; ix++) {
			  save_ip_address(mac_ip_data[ix], ix);
			}
			eeprom_lock_flash();
        	break;
         case 'k':
        	read_ip_address(mac_ip_read_from_flash, 10);
			for (unsigned jx=0; jx<10; jx++) {
				marble_UART_send(" ", 1);
				print_uint(mac_ip_read_from_flash[jx]);
			}
			marble_UART_send("\r\n", 2);
            break;
         default:
            printf(unk_str);
            break;
      }
   }
}

PUTCHAR_PROTOTYPE
{
  marble_UART_send((uint8_t *)&ch, 1);

  return ch;
}

void phy_print(void)
{
   uint32_t value;
   //char p_buf[40];

   //marble_MDIO_write(4, 10);

   for (uint16_t i=0; i < 4; i++) {
      value = marble_MDIO_read(i);
      printf( "  reg[%2.2x] = %4.4lx\r\n", i, value);
   }

}

void Error_Handler(void) {}
#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif /* USE_FULL_ASSERT */