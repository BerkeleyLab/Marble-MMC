#include <string.h>
#include <stdio.h>
#include "marble_api.h"
#include "i2c_pm.h"
#include "i2c_fpga.h"
#include "ssp.h"
#include "phy_mdio.h"
#include "rev.h"

// Setup UART strings
#ifdef MARBLEM_V1
const char demo_str[] = "Marble Mini v1 UART DEMO\r\n";
#elif MARBLE_V2
const char demo_str[] = "Marble v2 UART DEMO\r\n";
#endif

#define PRINT_NA printf("Function not available on this board.\r\n")

const char lb_str[] = "Loopback... ESC to exit\r\n";
const char menu_str[] = "\r\n"
	"Build based on git commit " GIT_REV "\r\n"
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
	"i) timer check/cal\r\n";

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

static void print_mac_ip(unsigned char mac_ip_data[10])
{
   int ix;
   printf("MAC: ");
   for (ix=0; ix<5; ix++) {
      printf("%x:", mac_ip_data[ix]);
   }
   printf("%x", mac_ip_data[ix]);

   printf("\n\rIP: ");
   for (ix++; ix<9; ix++) {
      printf("%d.", mac_ip_data[ix]);
   }
   printf("%d", mac_ip_data[ix]);
   printf("\n\r");
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

#ifdef MARBLE_V2
static void mgtclk_xpoint_en(void)
{
   if (xrp_ch_status(XRP7724, 1)) { // CH1: 3.3V
      adn4600_init();
   }
}
#endif

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

static void fpga_done_handler(void) {
   fpga_prog_cnt++;
}

static void timer_int_handler(void)
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
   // Static for now; eventually needs to be read from EEPROM
   unsigned char mac_ip_data[10] = {
      18, 85, 85, 0, 1, 46,  // MAC (locally managed)
      192, 168, 19, 31   // IP
   };

#ifdef MARBLEM_V1
   // Initialize Marble(mini) board with IRC, so it works even when
   // the XRP7724 isn't running, keeping the final 25 MHz source away.
   const bool use_xtal = false;
   uint32_t sysclk_freq = marble_init(use_xtal);
   printf("marble_init with use_xtal = %d\n", use_xtal);
   printf("system clock = %lu Hz\n", sysclk_freq);
#elif MARBLE_V2
   marble_init(0);
#endif

   /* Turn on LEDs */
   marble_LED_set(0, true);
   marble_LED_set(1, true);
   marble_LED_set(2, true);

#ifdef XRP_AUTOBOOT
   xrp_boot();
#endif

#ifdef MARBLE_V2
   // Enable MGT clock cross-point switch if 3.3V rail is ON
   mgtclk_xpoint_en();
#endif

   // Power FMCs
   marble_FMC_pwr(true);

   // Register GPIO interrupt handlers
   marble_GPIOint_handlers(fpga_done_handler);

   // Register System Timer interrupt handler
   marble_SYSTIMER_handler(timer_int_handler);

   /* Configure the System Timer for 20 Hz interrupts */
   marble_SYSTIMER_ms(50);

   // Send demo string over UART at 115200 BAUD
   marble_UART_send(demo_str, strlen(demo_str));

   while (1) {
      char rx_ch;
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
            printf("Live counter: %u\r\n", live_cnt);
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
#ifdef MARBLEM_V1
            push_fpga_mac_ip(mac_ip_data);
#elif MARBLE_V2
            printf("Functionality not implemented!\r\n");
#endif
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
#ifdef MARBLEM_V1
            PRINT_NA;
#elif MARBLE_V2
            adn4600_init();
            adn4600_printStatus();
#endif
            break;
         case 'c':
            printf("INA test\r\n");
            ina219_test();
            break;
         case 'd':
            printf("Switch MGT to QSFP 2\r\n");
#ifdef MARBLEM_V1
            PRINT_NA;
#elif MARBLE_V2
            marble_MGTMUX_set(3, true);
#endif
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
         default:
            printf(unk_str);
            break;
      }
   }
}

// This probably belongs in some other file, but which one?
int __io_putchar(int ch);
int __io_putchar(int ch)
{
  marble_UART_send((const char *)&ch, 1);
  return ch;
}