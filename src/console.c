/*
 * File: console.c
 * Desc: Encapsulate console (UART) interaction API
 *       Line-based comms (not char-based).
 *       Non-blocking if possible.
 */

#include <string.h>
#include <stdio.h>
#include <assert.h> // Remove if needed
#include "rev.h"
#include "console.h"
#include "phy_mdio.h"
#include "marble_api.h"
#include "sim_platform.h"
#include "mailbox.h"
#include "i2c_pm.h"
#include "i2c_fpga.h"
#include "uart_fifo.h"
#include "st-eeprom.h"

#define AUTOPUSH
// TODO - Put this in a better place
#define FAN_SPEED_MAX       (120)

const char lb_str[] = "Loopback... ESC to exit\r\n";
const char unk_str[] = "> Unknown option\r\n";

const char *menu_str[] = {"\r\n",
  "Build based on git commit " GIT_REV "\r\n",
  "Menu:\r\n",
  "0 - Loopback\r\n",
  "1 - MDIO/PHY\r\n",
  "2 - I2C monitor\r\n",
  "3 - Status & counters\r\n",
  "4 gpio - GPIO control\r\n",
  "  a/A: FMC power OFF/ON\r\n",
  "  b/B: EN_PSU_CH OFF/ON\r\n",
  "  c/C: PB15 J16[4]\r\n",
  "5 - Reset FPGA\r\n",
  "6 - Push IP&MAC\r\n",
  "7 - MAX6639\r\n",
  "8 - LM75_0\r\n",             // Do I want to elaborate this option?
  "9 - LM75_1\r\n",
  "a - I2C scan all ports\r\n",
  "b - Config ADN4600\r\n",
  "c - INA219 Main power supply\r\n",
  "d - MGT MUX - switch to QSFP 2\r\n",
  "e - PM bus display\r\n",
  "f - XRP7724 flash\r\n",
  "g - XRP7724 go\r\n",
  "h - XRP7724 hex input\r\n",
  "i - timer check/cal\r\n",
  "j - Read SPI mailbox\r\n",
  "k - PCA9555 status\r\n",
  "l - Config PCA9555\r\n",
  "m d.d.d.d - Set IP Address\r\n",
  "n d:d:d:d:d:d - Set MAC Address\r\n",
  "o - SI570 status\r\n",
  "p speed[%] - Set fan speed (0-120 or 0%-100%)\r\n"
};
#define MENU_LEN (sizeof(menu_str)/sizeof(*menu_str))

typedef enum {
   CONSOLE_TOP,
   CONSOLE_LOOP
} console_mode_e;

static console_mode_e console_mode;
static uint8_t _msgReady;

// TODO - find a better home for these
static void pm_bus_display(void);
static int console_handle_msg(char *rx_msg, int len);
static int console_shift_all(uint8_t *pData);
static void ina219_test(void);
static void handle_gpio(const char *msg, int len);
static int toggle_gpio(char c);
static int handle_msg_IP(char *rx_msg, int len);
static int handle_msg_MAC(char *rx_msg, int len);
static int handle_msg_fan_speed(char *rx_msg, int len);
//static void print_mac_ip(mac_ip_data_t *pmac_ip_data);
static void print_mac(uint8_t *pdata);
static void print_ip(uint8_t *pdata);
static void print_this_ip(void);
static void print_this_mac(void);
static int sscanfIP(const char *s, volatile uint8_t *data, int len);
static int sscanfMAC(const char *s, volatile uint8_t *data, int len);
static int sscanfFanSpeed(const char *s, int len);
static int xatoi(char c);
static int htoi(char c);
// TODO - fix encapsulation
// Read from EEPROM at startup
static mac_ip_data_t mac_ip_data;

const uint8_t mac_id_default[MAC_LENGTH] = {18, 85, 85, 0, 1, 46};
const uint8_t ip_addr_default[IP_LENGTH] = {192, 168, 19, 31};

int console_init(void) {
  _msgReady = 0;
  console_mode = CONSOLE_TOP;
  return 0;
}

static int console_handle_msg(char *rx_msg, int len)
{
  // If we're in console loop mode, just echo the string
  if (console_mode == CONSOLE_LOOP) {
    for (int n = 0; n < len; n++) {
      marble_UART_send((rx_msg + n), 1);
      if (*(rx_msg + n) == 27) {
        console_mode = CONSOLE_TOP;
      }
    }
    printf("\r\n");
    return 0;
  }
  // Switch behavior based on first char
  switch (*rx_msg) {
        case '?':
           for (unsigned kx=0; kx<MENU_LEN; kx++) {
               printf("%s", menu_str[kx]);
           }
           break;
        case '0':
           printf(lb_str);
           console_mode = CONSOLE_LOOP;
           break;
        case '1':
           phy_print();
           break;
        case '2':
           I2C_PM_probe();
           break;
        case '3':
           print_status_counters();
           break;
        case '4':
           handle_gpio(rx_msg, len);
           break;
        case '5':
           reset_fpga();
           break;
        case '6':
           //print_mac_ip(&mac_ip_data);
           print_this_ip();
           print_this_mac();
           console_push_fpga_mac_ip();
           printf("DONE\r\n");
           break;
        case '7':
           printf("Start\r\n");
           print_max6639_decoded();
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
           PRINT_NA();
#else
#ifdef MARBLE_V2
           adn4600_init();
           adn4600_printStatus();
#endif
#endif
           break;
        case 'c':
           printf("INA test\r\n");
           ina219_test();
           break;
        case 'd':
           printf("Switch MGT to QSFP 2\r\n");
#ifdef MARBLEM_V1
           PRINT_NA();
#else
#ifdef MARBLE_V2
           marble_MGTMUX_set(3, true);
#endif
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
              printf("%d\r\n", ix);
              marble_SLEEP_ms(1000);
           }
           break;
        case 'j':
           mbox_peek();
           break;
        case 'k':
           pca9555_status();
           break;
        case 'l':
           pca9555_config();
           break;
        case 'm':
           handle_msg_IP(rx_msg, len);
           break;
        case 'n':
           handle_msg_MAC(rx_msg, len);
           break;
        case 'o':
           si570_status();
           break;
        case 'p':
           handle_msg_fan_speed(rx_msg, len);
           break;
        default:
           printf(unk_str);
           break;
     }
  return 0;
}

static int handle_msg_IP(char *rx_msg, int len) {
  uint8_t ip[IP_LENGTH];
  // NOTE: It seems like sscanf doesn't work so well in newlib-nano
  int rval = sscanfIP(rx_msg, ip, len);
  if (rval) {
    printf("Malformed IP address. Fail.\r\n");
    return rval;
  }
  print_ip(ip);
  eeprom_store_ip_addr(ip, IP_LENGTH);
#ifdef AUTOPUSH
  console_push_fpga_mac_ip();
#endif
  return 0;
}

static int handle_msg_MAC(char *rx_msg, int len) {
  uint8_t mac[MAC_LENGTH];
  int rval = sscanfMAC(rx_msg, mac, len);
  if (rval) {
    printf("Malformed MAC address. Fail.\r\n");
    return rval;
  }
  print_mac(mac);
  eeprom_store_mac_addr(mac, MAC_LENGTH);
#ifdef AUTOPUSH
  console_push_fpga_mac_ip();
#endif
  return 0;
}

static int handle_msg_fan_speed(char *rx_msg, int len) {
  int speed, speedPercent;
  uint8_t readSpeed;
  if (len < 3) {
    // Print the current value
    if (eeprom_read_fan_speed(&readSpeed, 1)) {
      printf("Could not read current fan speed\r\n");
    } else {
      speedPercent = (100 * readSpeed)/FAN_SPEED_MAX;
      printf("Current fan speed: %d (%d%%)\r\n", readSpeed, speedPercent);
    }
    return 0;
  }
  speed = sscanfFanSpeed(rx_msg, len);
  if (speed < 0) {
    printf("Could not interpret input. Fail.\r\n");
    return -1;
  }
  speedPercent = (100 * speed)/FAN_SPEED_MAX;
  printf("Setting fan speed to %d (%d%%)\r\n", speed, speedPercent);
  max6639_set_fans(speed);
  eeprom_store_fan_speed((uint8_t *)&speed, 1);
  return 0;
}

static void handle_gpio(const char *msg, int len) {
  char c = 0;
  int found = 0;
  //printf("len = %d\r\n", len);
  // Look for alphabetic characters and respond accordingly
  // (skips the first char which is the command char)
  for (int n = 1; n < len; n++) {
    c = *(msg + n);
    if (c >= 'A') {
      found |= toggle_gpio(c);
    }
  }
  if (!found) {
    printf("GPIO pins, caps for on, lower case for off\r\n"
           "a) FMC power\r\n"
           "b) EN_PSU_CH\r\n"
           "c) PB15 J16[4]\r\n");
  }
  return;
}

static int toggle_gpio(char c) {
  // TODO - Add additional GPIO and test on scope
  int found = 1;
  switch (c) {
    case 'a':
      marble_FMC_pwr(0);
      printf("FMC Power Off\r\n");
      break;
    case 'A':
      marble_FMC_pwr(1);
      printf("FMC Power On\r\n");
      break;
    case 'b':
      marble_PSU_pwr(0);
      printf("PSU Power Off\r\n");
      break;
    case 'B':
      marble_PSU_pwr(1);
      printf("PSU Power On\r\n");
      break;
    case 'c':
      // PMOD3_5 J16[4]
      marble_Pmod3_5_write(0);
      break;
    case 'C':
      marble_Pmod3_5_write(1);
      break;
    default:
      found = 0;
      break;
  }
  return found;
}

int console_push_fpga_mac_ip(void) {
  mac_ip_data_t pdata;
  int rval = eeprom_read_ip_addr(pdata.ip, IP_LENGTH);
  rval |= eeprom_read_mac_addr(pdata.mac, MAC_LENGTH);
  if (rval) {
    printf("Could not find one of IP or MAC address\r\n");
    return rval;
  }
  return push_fpga_mac_ip(&pdata);
}

void console_print_mac_ip(void) {
  //print_mac_ip(&mac_ip_data); // XXX
  print_this_ip();
  print_this_mac();
  return;
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

void xrp_boot(void)
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

static void print_mac(uint8_t *pdata) {
  printf("MAC: ");
  PRINT_MULTIBYTE_HEX(pdata, 6, :); // Note the unquoted colon :
  return;
}

//static void print_ip(mac_ip_data_t *pmac_ip_data) {
static void print_ip(uint8_t *pdata) {
  printf("IP: ");
  PRINT_MULTIBYTE_DEC(pdata, 4, .);  // Note the unquoted period .
  return;
}

static void print_this_mac(void) {
  uint8_t mac[MAC_LENGTH];
  int rval = eeprom_read_mac_addr(mac, MAC_LENGTH);
  if (rval) {
    printf("Could not find MAC address\r\n");
    return;
  }
  print_mac(mac);
  return;
}

#define PRINT_MULTIBYTE(pdata, len, div) do { \
  for (int ix=0; ix<len-1; ix++) { printf("%d" ## div, pdata[ix]); } \
  printf("%d\r\n", pdata[len-1]); \
} while (0);

static void print_this_ip(void) {
  uint8_t ip[IP_LENGTH];
  int rval = eeprom_read_ip_addr(ip, IP_LENGTH);
  if (rval) {
    printf("Could not find IP address\r\n");
    return;
  }
  print_ip(ip);
  return;
}

/*
static void print_mac_ip(mac_ip_data_t *pmac_ip_data) {
  print_mac(pmac_ip_data->mac);
  print_ip(pmac_ip_data->ip);
  return;
}
*/

void console_pend_msg(void) {
  _msgReady = 1;
  return;
}

/*
 * int console_service(void);
 *  Service the console system.
 *  This should run periodically in thread mode (i.e. in the main loop).
 */
int console_service(void) {
  if (!_msgReady) {
    return 0;
  }
  uint8_t msg[CONSOLE_MAX_MESSAGE_LENGTH];
  int len = console_shift_all(msg);
  _msgReady = 0;
  if (len != 0) {
    return console_handle_msg((char *)msg, len);
  }
  return 0;
}

/*
 * static int console_shift_all(uint8_t *pData);
 *  Shift up to CONSOLE_MAX_MESSAGE_LENGTH bytes into 'pData'
 *  'pData' must be at least CONSOLE_MAX_MESSAGE_LENGTH in length
 *  Returns the number of bytes shifted out.
 */
static int console_shift_all(uint8_t *pData) {
  return UARTQUEUE_ShiftOut(pData, CONSOLE_MAX_MESSAGE_LENGTH);
}

static int xatoi(char c) {
  if ((c >= '0') && (c <= '9')) {
    return (int)(c - '0');
  }
  return -1;
}

static int htoi(char c) {
  int n = -1;
  if ((c >= '0') && (c <= '9')) {
    n = (int)(c - '0');
  } else if ((c >= 'a') && (c <= 'f')) {
    n = (int)(10 + (c - 'a'));
  } else if ((c >= 'A') && (c <= 'F')) {
    n = (int)(10 + (c - 'A'));
  }
  return n;
}

/*
 * static int sscanfIP(const char *s, volatile uint8_t *data, int len);
 *    This hackery is needed because it seems newlib-nano's version of sscanf is
 *    not fully functional.  This function skips any non-numeric characters (0-9
 *    only, no hex) and looks for periods '.' to define number boundaries.
 *    Returns -1 if not all IP ADDR digits were encountered.
 */
static int sscanfIP(const char *s, volatile uint8_t *data, int len) {
  int ndig = 0;
  char c;
  int r;
  int sum = 0;
  // Start scan on char 1
  for (int n = 1; n < len; n++) {
    c = s[n];
    if (c == '.') {
      data[ndig++] = (uint8_t)(sum & 0xff);
      sum = 0;
    } else {
      r = xatoi(c);
      if (r >= 0) {
        sum = (sum * 10) + r;
      }
    }
  }
  // The last digit won't be transferred in the loop (no '.' to follow)
  data[ndig] = sum;
  if (ndig == IP_LENGTH - 1) {
    return 0;
  }
  // Error - too many or not all digits decoded
  return -1;
}

/*
 * static int sscanfMAC(const char *s, volatile uint8_t *data, int len);
 *    This hackery is needed because it seems newlib-nano's version of sscanf is
 *    not fully functional.  This function ignores anything before the first space
 *    character) and ignores any non-hex characters.
 *    It looks for semicolons ':' to define number boundaries.
 *    Returns -1 if not all MAC ADDR digits were encountered.
 */
static int sscanfMAC(const char *s, volatile uint8_t *data, int len) {
  int ndig = 0;
  char c;
  int r;
  int sum = 0;
  // Start scan on char 1
  for (int n = 1; n < len; n++) {
    c = s[n];
    if (c == ':') {
      data[ndig++] = (uint8_t)(sum & 0xff);
      sum = 0;
    } else {
      r = htoi(c);
      if (r >= 0) {
        sum = (sum << 4) | r;
      }
    }
  }
  // The last digit won't be transferred in the loop (no ':' to follow)
  data[ndig] = sum;
  if (ndig == MAC_LENGTH - 1) {
    return 0;
  }
  // Error - too many or not all digits decoded
  return -1;
}

/*
 * static int sscanfFanSpeed(const char *s, int len);
 *    This hackery is needed because it seems newlib-nano's version of sscanf is
 *    not fully functional.  This function skips any non-numeric characters (0-9
 *    only, no hex) and looks for percent sign '%' and terminates.
 *    Returns -1 if no digit is found.
 *    Returns scanned value otherwise.
 */
static int sscanfFanSpeed(const char *s, int len) {
  char c;
  int r;
  unsigned int sum = 0;
  bool toScale = false;
  bool digitsFound = false;
  for (int n = 1; n < len; n++) {
    c = s[n];
    if (c == '%') {
      toScale = true;
      break;
    } else {
      r = xatoi(c);
      if (r >= 0) {
        sum = (sum * 10) + r;
        digitsFound = true;
      }
    }
  }
  if (!digitsFound) {
    return -1;
  }
  sum = sum > FAN_SPEED_MAX ? FAN_SPEED_MAX : sum;
  if (toScale) {
    // Peg at 100%
    sum = sum > 100? 100: sum;
    sum = (sum * FAN_SPEED_MAX)/100;
  } else {
    // Peg at FAN_SPEED_MAX
    sum = sum > FAN_SPEED_MAX ? FAN_SPEED_MAX : sum;
  }
  return (int)sum;
}

int restoreIPAddr(void) {
  uint8_t ipAddr[IP_LENGTH];
  int rval = eeprom_read_ip_addr(ipAddr, IP_LENGTH);
  if (!rval) {
    // Success; use read value
    memcpy(mac_ip_data.ip, ipAddr, IP_LENGTH);
  } else {
    // Fail; use default value
    memcpy(mac_ip_data.ip, ip_addr_default, IP_LENGTH);
  }
  return rval;
}

int restoreMACAddr(void) {
  uint8_t macAddr[MAC_LENGTH];
  int rval = eeprom_read_mac_addr(macAddr, MAC_LENGTH);
  if (!rval) {
    // Success; use read value
    memcpy(mac_ip_data.mac, macAddr, MAC_LENGTH);
  } else {
    // Fail; use default value
    memcpy(mac_ip_data.mac, mac_id_default, MAC_LENGTH);
  }
  return rval;
}

#ifdef DEBUG_ENABLE_ERRNO_DECODE
#define X(en)   const char s_ ## en[] = #en;
FOR_ALL_ERRNOS()
#undef X

char s_OK[] = "Success";
char s_DEFAULT[] = "Unknown";

const char *decode_errno(int err) {
  switch (err) {
    case 0:
      return s_OK;
#define X(en)   case en: return s_ ## en;
    FOR_ALL_ERRNOS();
#undef X
    default:
      return s_DEFAULT;
  }
}
#endif
