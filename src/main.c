#include <string.h>
#include "marble_api.h"
#include "i2c_pm.h"
#include "ssp.h"

// Setup UART strings
const char demo_str[] = "Marble Mini v1 UART DEMO\r\n";
const char lb_str[] = "Loopback... ESC to exit\r\n";
const char menu_str[] = "\r\n"
	"Menu:\r\n"
	"0) Loopback\r\n"
	"1) FPGA IP/MAC update\r\n"
	"2) I2C monitor\r\n"
	"3) MDIO status\r\n"
	"4) GPIO control\r\n"
	"5) Reset FPGA\r\n"
	"6) Push IP&MAC\r\n"
	"7) MAX6639\r\n"
	"8) LM75_0\r\n"
	"9) LM75_1\r\n";

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

// At some point we will give up and install a libc.
// Consider picolibc.
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

int main (void) {
   // Static for now; eventually needs to be read from EEPROM
   unsigned char mac_ip_data[10] = {
      18, 85, 85, 0, 1, 46,  // MAC (locally managed)
      192, 168, 19, 31   // IP
   };

   // Initialize Marble(mini) board with IRC
   marble_init(false);

   /* Turn on LEDs */
   marble_LED_set(0, true);
   marble_LED_set(1, true);
   marble_LED_set(2, true);

   // Power FMCs
   marble_FMC_pwr(true);

   // Send demo string over UART at 115200 BAUD
   marble_UART_send(demo_str, strlen(demo_str));
   char rx_ch;

   while (true) {
      marble_UART_send(menu_str, strlen(menu_str));
      // Wait for user selection
      while(marble_UART_recv(&rx_ch, 1) == 0);
      switch (rx_ch) {
         case '0':
            marble_UART_send(lb_str, strlen(lb_str));
            do {
               if (marble_UART_recv(&rx_ch, 1) != 0) {
                  marble_UART_send(&rx_ch, 1);
               }
            } while (rx_ch != 27);
            marble_UART_send("\r\n", 2);
            break;
         case '1':
            break;
         case '2':
            I2C_PM_probe();
            break;
         case '3':
            break;
         case '4':
            gpio_cmd();
            break;
         case '5':
            reset_fpga();
            break;
         case '6':
            print_mac_ip(mac_ip_data);
            push_fpga_mac_ip(mac_ip_data);
            marble_UART_send("DONE\r\n", 6);
            break;
         case '7':
            marble_UART_send("Start\r\n", 7);
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
         default:
            marble_UART_send(unk_str, strlen(unk_str));
            break;
      }
   }
}

extern void SysTick_Handler(void) {
   marble_LED_toggle(0);
   marble_LED_toggle(1);
   marble_LED_toggle(2);
}
