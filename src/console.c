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
#include "mailbox.h"
#include "i2c_pm.h"
#include "i2c_fpga.h"
#include "uart_fifo.h"
#include "st-eeprom.h"
#include "ltm4673.h"
#include "watchdog.h"

#define AUTOPUSH
// TODO - Put this in a better place
#define FAN_SPEED_MAX           (120)
#define OVERTEMP_HARD_MAXIMUM   (125)

const char unk_str[] = "> Unknown option\r\n";

const char *menu_str[] = {"\r\n",
  "Build based on git commit " GIT_REV "\r\n",
  "Menu:\r\n",
  "1 - MDIO/PHY\r\n",
  "2 - I2C monitor\r\n",
  "3 - Status & counters\r\n",
  "4 gpio - GPIO control\r\n",
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
  //"h - XRP7724 hex input\r\n",
  "h - FMC MGT MUX set\r\n",
  "i - timer check/cal\r\n",
  "j - Read SPI mailbox\r\n",
  "k - PCA9555 status\r\n",
  "l - Config PCA9555\r\n",
  "m d.d.d.d - Set IP Address\r\n",
  "n d:d:d:d:d:d - Set MAC Address\r\n",
  "o - SI570 status\r\n",
  "p speed[%] - Set fan speed (0-120 or 0%-100%)\r\n",
  "q otemp - Set overtemperature threshold (degC)\r\n",
  "r enable - Set mailbox enable/disable (1/0, on/off)\r\n",
  "s addr_hex freq_hz config_hex - Set Si570 configuration\r\n",
  "t pmbus_msg - Forward PMBus transaction to LTM4673\r\n",
  "u period - Set/get watchdog timeout period (in seconds)\r\n",
  "v key - Set a new 128-bit secret key (non-volatile, write only).\r\n"
};
#define MENU_LEN (sizeof(menu_str)/sizeof(*menu_str))

static uint8_t _msgCount;
static uint8_t _fpgaEnable;

// TODO - find a better home for these
static int console_handle_msg(char *rx_msg, int len);
//static int console_shift_all(uint8_t *pData);
static int console_shift_msg(uint8_t *pData);
static void ina219_test(void);
static void handle_gpio(const char *msg, int len);
static int toggle_gpio(char c);
static int handle_msg_IP(char *rx_msg, int len);
static int handle_msg_MAC(char *rx_msg, int len);
static int handle_msg_fan_speed(char *rx_msg, int len);
static int handle_msg_overtemp(char *rx_msg, int len);
static int handle_msg_watchdog(char *rx_msg, int len);
static int handle_msg_key(char *rx_msg, int len);
static int handle_mailbox_enable(char *rx_msg, int len);
static int handle_msg_MGTMUX(char *rx_msg, int len);
//static void print_mac_ip(mac_ip_data_t *pmac_ip_data);
static void print_mac(uint8_t *pdata);
static void print_ip(uint8_t *pdata);
static void print_this_ip(void);
static void print_this_mac(void);
static int sscanfIP(const char *s, volatile uint8_t *data, int len);
static int sscanfMAC(const char *s, volatile uint8_t *data, int len);
static int sscanfFanSpeed(const char *s, int len);
static int sscanfUnsignedDecimal(const char *s, int len);
static int sscanfUnsignedHex(const char *s, int len);
static int sscanfUHexExact(const char *s, int len);
static int sscanfMGTMUX(const char *s, int len);
static int sscanfSpace(const char *s, int len);
static int sscanfNonSpace(const char *s, int len);
static int sscanfNext(const char *s, int len);
static int sscanfFSynth(const char *s, int len);
static int sscanfPMBridge(const char *s, int len);
static int PMBridgeConsumeArg(const char *s, int len, volatile int *arg);
static int xatoi(char c);
static int htoi(char c);
static void console_print_fsynth(void);

int console_init(void) {
  _msgCount = 0;
  return 0;
}

#ifdef MARBLE_V2
void mgtclk_xpoint_en(void)
{
   if ((marble_get_pcb_rev() < Marble_v1_4) & xrp_ch_status(XRP7724, 1)) { // CH1: 3.3V
      adn4600_init();
   } else if ((marble_get_pcb_rev() > Marble_v1_3) & ltm4673_ch_status(LTM4673)) {
      printf("Using LTM4673 and adn4600_init\r\n");
      adn4600_init();
   } else {
      printf("Skipping adn4600_init\r\n");
   }
}
#endif

static int console_handle_msg(char *rx_msg, int len)
{
  // TODO all these should return 0 on success, 1 on failure
  //      then we should print a simple global help string on failure
  // Switch behavior based on first char
  switch (*rx_msg) {
        case '?':
           for (unsigned kx=0; kx<MENU_LEN; kx++) {
               printf("%s", menu_str[kx]);
           }
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
           //LM75_write(LM75_0, LM75_OS, 100*2);
           //LM75_write(LM75_0, LM75_CFG, LM75_CFG_COMP_INT);
           //LM75_print(LM75_0);
           LM75_print_decoded(LM75_0);
           break;
        case '9':
           // Demonstrate setting over-temperature register
           //LM75_write(LM75_1, LM75_OS, 100*2);
           LM75_print_decoded(LM75_1);
           //LM75_print(LM75_1);
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
           I2C_PM_bus_display();
           break;
        case 'f':
           printf("XRP flash\r\n");
           xrp_flash(XRP7724);
           break;
        case 'g':
           printf("XRP go\r\n");
           xrp_boot();
           break;
#if 0
        case 'h':
           printf("XRP hex input\r\n");
           xrp_hex_in(XRP7724);
           break;
#endif
        case 'h':
           handle_msg_MGTMUX(rx_msg, len);
           break;
        case 'i':
           for (unsigned ix=0; ix<10; ix++) {
              printf("%d\r\n", ix);
              marble_SLEEP_ms(1000);
           }
           break;
        case 'j':
           //mbox_peek();
           mailbox_read_print_all();
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
        case 'q':
           handle_msg_overtemp(rx_msg, len);
           break;
        case 'r':
           handle_mailbox_enable(rx_msg, len);
           break;
        case 's':
           sscanfFSynth(rx_msg, len);
           break;
        case 't':
           sscanfPMBridge(rx_msg, len);
           break;
        case 'u':
           handle_msg_watchdog(rx_msg, len);
           break;
        case 'v':
           handle_msg_key(rx_msg, len);
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
  if (len < 4) {
    // Print the current value
    if (eeprom_read_fan_speed(&readSpeed, 1)) {
      printf("Could not read current fan speed.\r\n");
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

static int handle_msg_overtemp(char *rx_msg, int len) {
  // Overtemp is stored in MAX6639 as degrees C
  uint8_t otbyte;
  if (len < 4) {
    if (eeprom_read_overtemp(&otbyte, 1)) {
      printf("Could not read current over-temperature threshold.\r\n");
    } else {
      printf("Current over-temperature threshold: %d degC\r\n", otbyte);
    }
    return 0;
  }
  int overtemp = sscanfUnsignedDecimal(rx_msg+1, len-1);
  if (overtemp < 0) {
    printf("Could not interpret input. Fail.\r\n");
    return -1;
  }
  // Peg at hard max
  overtemp = overtemp > OVERTEMP_HARD_MAXIMUM ? OVERTEMP_HARD_MAXIMUM : overtemp;
  printf("Setting over-temperature threshold to %d degC\r\n", overtemp);
  otbyte = (uint8_t)(overtemp & 0xFF);
  //int rval = max6639_set_overtemp(otbyte);
  max6639_set_overtemp(otbyte); // Discarding return value for now
  LM75_set_overtemp((int)otbyte);
  //int rval = eeprom_store_overtemp(&otbyte, 1);
  eeprom_store_overtemp(&otbyte, 1); // Discarding return value for now
  // int eeprom_read_overtemp(volatile uint8_t *pdata, int len);
  return 0;
}

static int handle_mailbox_enable(char *rx_msg, int len) {
  //  Msg   Action
  //  r 0   Disable
  //  r 1   Enable
  //  r ?   Print status
  //  r     Print status
  //  r on  Enable
  //  r off Disable
  int en = 0;
  int query = 0;
  char c;
  int doParse = 0;
  // 'doParse' 0 means unparsed; 1 means parse fail; 2 means parse success
  char arg[3];
  int argp = 0;
  if (len < 4) {
    query = 1;
  }
  for (int n = 1; n < len; n++) {
    c = rx_msg[n];
    if (doParse) {
      if (c == '0') {
        en = 0;
        doParse = 2;
        break;
      } else if (c == '1') {
        en = 1;
        doParse = 2;
        break;
      } else if ((c >= 'A') && (c <= 'z')) {
        if (argp < 2) {
          arg[argp++] = c;
        } else {
          break;
        }
      } else if (c == '?') {
        query = 1;
        break;
      }
    } else {  // Haven't started parsing
      if (c == ' ') {
        doParse = 1;
      }
    }
  }
  if (argp > 0) {
    if ((arg[0] == 'o') || (arg[0] == 'O')) {
      if ((arg[1] == 'n') || (arg[1] == 'N')) {
        en = 1;
        doParse = 2;
      } else if ((arg[1] == 'f') || (arg[1] == 'F')) {
        en = 0;
        doParse = 2;
      }
    }
  }
  if (!doParse) {
    query = 1;
  }
  if ((doParse < 2) && (!query)) {
    printf("Failed to parse\r\n");
    return 1;
  }
  if (query) {
    en = mbox_get_enable();
    if (en) {
      printf("Mailbox enabled\r\n");
    } else {
      printf("Mailbox disabled\r\n");
    }
  } else {
    if (en) {
      printf("Enabling mailbox update\r\n");
      mbox_enable();
    } else {
      printf("Disabling mailbox update\r\n");
      mbox_disable();
    }
  }
  return 0;
}

static int handle_msg_MGTMUX(char *rx_msg, int len) {
  if (len < 4) {
    printf("E.g. Set all MUXn pin states: h 1=1 2=0 3=0\r\n");
    printf("E.g. Set just MUX2 pin high (ignore others): h 2=1\r\n");
    printf("E.g. Read MGTMUX state: h ?\r\n");
    return 1;
  }
  int rval = sscanfMGTMUX(rx_msg, len);
  uint8_t rbyte = 0;
  if (rval == -1) {
    printf("Could not interpret assignments. Use 'h' for usage.\r\n");
  } else if (rval == -2) {
    // Get and print current MGT MUX state
    rbyte = marble_MGTMUX_status();
    printf("  ");
    for (int n = 0; n < MGT_MAX_PINS; n++) {
      printf("MUX%d=%d ", n+1, ((rbyte >> n) & 1));
    }
    printf("\r\n");
  } else {
    printf("  "); // Indent the line printed by the following function
    marble_MGTMUX_config((uint8_t)rval, 1, 1); // Store nonvolatile, print
  }
  return rval;
}

static void handle_gpio(const char *msg, int len) {
  char c = 0;
  int found = 0;
  //printf("len = %d\r\n", len);
  // Look for alphabetic characters and respond accordingly
  // (skips the first char which is the command char)
  for (int n = 1; n < len; n++) {
    c = *(msg + n);
    if (c == '?') {
      found = -1;
      break;
    }
    if (c >= 'A') {
      found |= toggle_gpio(c);
      if (found) {
        break;
      }
    }
  }
  if (found == -1) {
    // Print state
    marble_print_GPIO_status();
  }
  else if (!found) {
    printf("GPIO pins, caps for on, lower case for off\r\n"
           "?) Print state of GPIOs\r\n"
           "a) FMC power\r\n"
           "b) EN_PSU_CH\r\n"
           "c) PB15 J16[4]\r\n"
           "d) PSU reset\r\n"
           "e) PSU alert\r\n");
  }
  return;
}

static int toggle_gpio(char c) {
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
      printf("PSU Powered On\r\n");
      #ifdef MARBLE_V2
        marble_SLEEP_ms(800);
        mgtclk_xpoint_en();
        reset_fpga();
      #endif
      break;
    case 'c':
      // PMOD3_5 J16[4]
      marble_Pmod3_5_write(0);
      break;
    case 'C':
      marble_Pmod3_5_write(1);
      break;
    case 'd':
      // PMOD3_5 J16[4]
      marble_PSU_reset_write(0);
      break;
    case 'D':
      marble_PSU_reset_write(1);
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

static void print_mac(uint8_t *pdata) {
  printf("MAC: ");
  PRINT_MULTIBYTE_HEX(pdata, 6, ':');
  return;
}

//static void print_ip(mac_ip_data_t *pmac_ip_data) {
static void print_ip(uint8_t *pdata) {
  printf("IP: ");
  PRINT_MULTIBYTE_DEC(pdata, 4, '.');
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
  _msgCount++;
  return;
}

/*
 * int console_service(void);
 *  Service the console system.
 *  This should run periodically in thread mode (i.e. in the main loop).
 */
int console_service(void) {
  uint8_t msg[CONSOLE_MAX_MESSAGE_LENGTH];
  int len;
  if (_msgCount) {
    len = console_shift_msg(msg);
    _msgCount--;
    if (len) {
      return console_handle_msg((char *)msg, len);
    }
  }
  if (_fpgaEnable) {
    enable_fpga();
    _fpgaEnable = 0;
  }
  return 0;
}

/*
 * static int console_shift_all(uint8_t *pData);
 *  Shift up to CONSOLE_MAX_MESSAGE_LENGTH bytes into 'pData'
 *  'pData' must be at least CONSOLE_MAX_MESSAGE_LENGTH in length
 *  Returns the number of bytes shifted out.
static int console_shift_all(uint8_t *pData) {
  return UARTQUEUE_ShiftOut(pData, CONSOLE_MAX_MESSAGE_LENGTH);
}
*/

/*
 * static int console_shift_msg(uint8_t *pData);
 *  Shift up to CONSOLE_MAX_MESSAGE_LENGTH bytes into 'pData', breaking
 *  at any of:
 *    UART_MSG_TERMINATOR found
 *    FIFO empty
 *    CONSOLE_MAX_MESSAGE_LENGTH bytes shifted
 *  'pData' must be at least CONSOLE_MAX_MESSAGE_LENGTH in length
 *  Returns the number of bytes shifted out.
 */
static int console_shift_msg(uint8_t *pData) {
  //UARTQUEUE_ShiftOut(pData, CONSOLE_MAX_MESSAGE_LENGTH);
  return UARTQUEUE_ShiftUntil(pData, UART_MSG_TERMINATOR, CONSOLE_MAX_MESSAGE_LENGTH);
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

/*
 * static int sscanfUnsignedDecimal(const char *s, int len);
 *    This hackery is needed because it seems newlib-nano's version of sscanf is
 *    not fully functional.  This function skips any non-numeric characters (0-9
 *    only, no hex) until a numeric character is found, then shifts/sums any
 *    numeric characters until 'len' or until a non-numeric character is found.
 *    Returns -1 if no digit is found.
 *    Returns scanned value otherwise.
 */
static int sscanfUnsignedDecimal(const char *s, int len) {
  char c;
  int r;
  unsigned int sum = 0;
  bool digitsFound = false;
  for (int n = 0; n < len; n++) {
    c = s[n];
    r = xatoi(c);
    if (r >= 0) {
      sum = (sum * 10) + r;
      digitsFound = true;
    } else if (digitsFound) {
      // If digits have been found, break on first non-numeric character
      break;
    }
  }
  if (!digitsFound) {
    return -1;
  }
  return (int)sum;
}

static int handle_msg_watchdog(char *rx_msg, int len) {
  int index = sscanfNext(rx_msg, len);
  int val;
  if (index < 0) {
    val = FPGAWD_GetPeriod();
    if (val == 0) {
      printf("Watchdog disabled (period = 0)\r\n");
    } else {
      printf("Current watchdog timeout: %d seconds\r\n", val);
    }
    return -1;
  }
  val = sscanfUnsignedDecimal((rx_msg + index), len-index);
  if (val < 0) {
    printf("Failed to parse\r\n");
    return -1;
  }
  // Set and peg to limits
  val = FPGAWD_SetPeriod((unsigned int)val);
  eeprom_store_wd_period((const uint8_t *)&val, 1);
  return 0;
}

#define KEY_LEN     (16)
static int handle_msg_key(char *rx_msg, int len) {
  int index = sscanfNext(rx_msg, len);
  uint8_t key[KEY_LEN];
  int rval;
  int n;
  for (n = 0; n < KEY_LEN; n++) {
    // Parse hex string into bytes
    rval = sscanfUHexExact((const char *)(rx_msg + index + 2*n), 2);
    if (rval < 0) {
      key[n] = 0xcc;
      break;
    }
    key[n] = (uint8_t)rval;
  }
  if (n < KEY_LEN-1) {
    // Failed to parse all 2*KEY_LEN chars
    printf("Failed to parse %d consecutive hex characters. Key not stored.\r\n", 2*KEY_LEN);
    return -1;
  }
  if (1) {
    for (n = 0; n < 16; n++) {
      printf("%02x ", key[n]);
      if ((n == 7) || (n == 15)) printf("\r\n");
    }
  }
  // Store non-volatile
  eeprom_store_wd_key((const uint8_t *)key, KEY_LEN);
  // Clobber the stack memory before exiting.
  memset(key, 0xaa, KEY_LEN);
  return 0;
}

/*
 * static int sscanfUnsignedHex(const char *s, int len);
 *    This function skips any non-hex characters (0-9,A-F,a-f)
 *    until a numeric character is found, then shifts/sums any
 *    numeric characters until 'len' or until a non-hex character
 *    is found.
 *    Returns -1 if no digit is found.
 *    Returns scanned value otherwise.
 */
static int sscanfUnsignedHex(const char *s, int len) {
  char c;
  int r;
  unsigned int sum = 0;
  bool digitsFound = false;
  for (int n = 0; n < len; n++) {
    c = s[n];
    r = htoi(c);
    if (r >= 0) {
      sum = (sum * 16) + r;
      digitsFound = true;
    } else if (digitsFound) {
      // If digits have been found, break on first non-numeric character
      break;
    }
  }
  if (!digitsFound) {
    return -1;
  }
  return (int)sum;
}

/* static int sscanfUHexExact(const char *s, int len);
 *  This function is less permissive than sscanfUnsignedHex()
 *  and expects to find exactly 'len' hex characters starting
 *  from 's'.
 *
 *  Returns -1 if the above criterion is not met (any non-hex
 *  chars within the first 'len' chars)
 *
 *  Returns the scanned value otherwise.
 */
static int sscanfUHexExact(const char *s, int len) {
  int r;
  unsigned int sum = 0;
  int n;
  for (n = 0; n < len; n++) {
    r = htoi(s[n]);
    if (r >= 0) {
      sum = (sum * 16) + r;
    } else {
      break;
    }
  }
  if (n < (len-1)) {
    return -1;
  }
  return (int)sum;
}

/*
 * static int sscanfMGTMUX(const char *s, int len);
 *    Scans for "x=y" assignments separated by whitespace where 'x' can be 1, 2, or 3
 *    and 'y' can be 0, or 1.
 *    Returns -1 if no valid assignment is found.
 *    Returns -2 if a question mark ('?') is found, indicating we should return the current state.
 *    Returns bitmask suitable to pass to marble_MGTMUX_config otherwise.
 */
static int sscanfMGTMUX(const char *s, int len) {
  char c0,c1,c2;
  int r0,r2;
  bool assignmentFound = false;
  int bmask = 0;
  for (int n = 1; n < len-2; n++) {
    // Moving window of 3 characters, starting from char 1 (skip command char)
    c0 = s[n];
    c1 = s[n+1];
    c2 = s[n+2];
    if (c0 == '?') {
      return -2; // Requesting help
    }
    if (c1 == '=') {
      r0 = xatoi(c0);
      r2 = xatoi(c2);
      switch (r0) {
        case 1: // Intentional fall-through
        case 2: // Intentional fall-through
        case 3:
          if (r2 == 1) {
            bmask |= (3 << (2*r0));
            assignmentFound = true;
          } else if (r2 == 0) {
            bmask |= (2 << (2*r0));
            assignmentFound = true;
          }
          break;
        default:
          break;
      }
    }
  }
  if (!assignmentFound) {
    return -1;
  }
  return bmask;
}

/* static int sscanfSpace(const char *s, int len);
 *  Return the index of first whitespace found scanning string 's'
 *  Returns -1 if no whitespace found.
 */
static int sscanfSpace(const char *s, int len) {
  int rval = -1;
  for (int n = 0; n < len; n++) {
    if ((s[n] == ' ') || (s[n] == '\t') || (s[n] == '\n') || (s[n] == '\r')) {
      rval = n;
      break;
    }
  }
  return rval;
}

/* static int sscanfNonSpace(const char *s, int len);
 *  Return the index of first non-whitespace found scanning string 's'
 *  Returns -1 if only whitespace found.
 */
static int sscanfNonSpace(const char *s, int len) {
  int rval = -1;
  for (int n = 0; n < len; n++) {
    if ((s[n] != ' ') && (s[n] != '\t') && (s[n] != '\n') && (s[n] != '\r')) {
      rval = n;
      break;
    }
  }
  return rval;
}

/* static int sscanfNext(const char *s, int len);
 *  Scan for the next whitespace, then for the next non-whitespace.
 *  Returns that index or -1 if not found.
 */
static int sscanfNext(const char *s, int len) {
  int win;
  int cin;
  win = sscanfSpace(s, len);
  if (win < 0) {
    return -1;
  }
  cin = sscanfNonSpace(s+win, len-win);
  if (cin > 0) {
    return win+cin;
  }
  return -1;
}

static int sscanfFSynth(const char *s, int len) {
  // Input string format:
  //  s cc 40000 1
  int i2c_addr = -1;
  int freq = -1;
  int config = -1;
  int query = 0;
  int index = sscanfNext(s, len);
  uint8_t data[6];
  if (len < 4) {
    query = 2;  // print usage too
  } else if (s[index] == '?') {
    query = 1;
  } else {
    i2c_addr = sscanfUnsignedHex(s+index, len-index); // will break at first non-hex char
    index += sscanfNext(s+index, len-index);
    if ((index >= 0) && (index < len-1)) {
      freq = sscanfUnsignedDecimal(s+index, len-index);
    }
    index += sscanfNext(s+index, len-index);
    if ((index >= 0) && (index < len-1)) {
      config = sscanfUnsignedHex(s+index, len-index);
    }
  }
  if (query) {
    if (query > 1) {
      printf("USAGE: s ADDR(hex) FREQ_HZ(decimal) CONFIG(hex)\r\n");
      printf("  Set frequency synthesizer (Si570) configuration parameters.\r\n");
    }
    console_print_fsynth();
  } else {
    if ((i2c_addr < 0) || (freq < 0) || (config < 0)) {
      printf("Could not interpret input\r\n");
      return -1;
    }
    printf("I2C Addr = 0x%x, Freq = %d Hz, Config = 0x%x\r\n", i2c_addr, freq, config);
    FSYNTH_ASSEMBLE(data, i2c_addr, freq, config);
    eeprom_store_fsynth((const uint8_t *)data, 6);
  }
  return 0;
}

static void console_print_fsynth(void) {
  uint8_t data[6];
  uint8_t i2c_addr;
  uint8_t config;
  int freq;
  int rval = eeprom_read_fsynth(data, 6);
  if (rval >= 0) {
    i2c_addr = FSYNTH_GET_ADDR(data);
    config = FSYNTH_GET_CONFIG(data);
    freq = FSYNTH_GET_FREQ(data);
    printf("I2C Addr = 0x%x, Freq = %d Hz, Config = 0x%x\r\n", i2c_addr, freq, config);
  } else {
    printf("Could not recall frequency synthesizer parameters\r\n");
  }
  return;
}


/* static int sscanfPMBridge(const char *s, int len);
 *  Parse a line from the user representing a PMBus transaction
 *  Syntax: x command
 */
/*MMC console syntax
  Each line is a list of any of the following (whitespace-separated)
    ! : Repeated start
    ? : Read 1 byte from the target device
    * : Read 1 byte, then use that as N and read the next N bytes
    0xHH: Use hex value 0xHH as the next transaction byte
    DDD : Use decimal value DDD as the next transaction byte
*/
static int sscanfPMBridge(const char *s, int len) {
  // Skip the first character (command char)
  int ptr = sscanfNext(s, len);
  int ptrinc;
  int max_len = len > PMBRIDGE_MAX_LINE_LENGTH ? PMBRIDGE_MAX_LINE_LENGTH : len;
  int arg;
  int item_index = 0;
  int fail = 0;
  uint16_t xact[PMBRIDGE_XACT_MAX_ITEMS];
  while (ptr < max_len) {
    if (s[ptr] == '\n') {
      break;
    }
    ptrinc = PMBridgeConsumeArg(s+ptr, len-ptr, &arg);
    //printf("consume arg ptr %d -> %d\r\n", ptr, ptr+ptrinc);
    ptr += ptrinc;
    if (arg < 0) {
      printf("ERROR: Parse failed at character %d [%c]\r\n", ptr, s[ptr]);
      fail = 1;
      break;
    } else {
      if (item_index < PMBRIDGE_XACT_MAX_ITEMS) {
        xact[item_index++] = (uint16_t)(arg & 0xffff);
      } else {
        printf("ERROR: Exceeded maximum number of bytes per transaction\r\n");
        fail = 1;
        break;
      }
    }
    ptrinc = sscanfNonSpace(s+ptr, len-ptr);
    if (ptrinc == -1) {
      break;
    }
    //printf("finding whitespace ptr %d -> %d\r\n", ptr, ptr+ptrinc);
    ptr += ptrinc;
  }
  if (fail) {
    return fail;
  }
  /*
  printf("xact = [ ");
  for (int n = 0; n < item_index; n++) {
    printf("0x%x ", xact[n]);
  }
  printf("]\r\n");
  */
  PMBridge_xact(xact, item_index);
  return 0;
}

#define MMC_REPEAT_START      ('!')
#define MMC_READ_ONE          ('?')
#define MMC_READ_BLOCK        ('*')
/* static int PMBridgeConsumeArg(const char *s, int len, volatile int *arg);
 *  Consume one whitespace-separated arg from string 's'.
 *  Returns when:
 *    1. Whitespace is encountered
 *      1a. If no valid chars have been parsed, *arg is set to -1.
 *      1b. Otherwise, *arg is set to the parsed value.
 *    2. An invalid character is encountered
 *      2a. Always sets *arg to -1.
 *    3. 'len' characters have been consumed
 *      3a. Check value of *arg for validity.
 *  Always returns the index into 's' where parsing stopped.
 */
static int PMBridgeConsumeArg(const char *s, int len, volatile int *arg) {
  int max_len = len > PMBRIDGE_MAX_LINE_LENGTH ? PMBRIDGE_MAX_LINE_LENGTH : len;
  int state = 0;
  // state:
  //  0 : No chars processed
  //  1 : A '0' has been found. Awaiting 'x' or 'X'
  //  2 : A '0x' or '0X' prefix has been found. Waiting for first hex char.
  //  3 : Consuming additional hex chars.
  //  4 : Special char found.
  //  5 : Consuming decimal chars.
  int val = 0;
  int n;
  char c;
  int cn;
  for (n = 0; n < max_len; n++) {
    c = s[n];
    // Stop at whitespace
    if ((c == ' ') || (c == '\n') || (c == '\t') || (c == '\r')) {
      if ((state == 2) || (state == 0)) {
        // state=1 is valid for bare '0' argument
        val = -1;
      }
      break;
    // Look for special PMBridge characters
    } else if (c == MMC_REPEAT_START) {
      state = 4;
      val = PMBRIDGE_XACT_REPEAT_START;
    } else if (c == MMC_READ_ONE) {
      state = 4;
      val = PMBRIDGE_XACT_READ_ONE;
    } else if (c == MMC_READ_BLOCK) {
      state = 4;
      val = PMBRIDGE_XACT_READ_BLOCK;
    } else if (state == 4) {
      // If we get here, then a special char was not properly followed by whitespace. Fail.
      printf("Special not followed by whitespace\r\n");
      val = -1;
      break;
    } else if (state == 0) {
      // Haven't seen any chars yet
      if (c == '0') {
        // Seems like it's going to be a hex prefix
        state = 1;
      } else {
        // Assume decimal number
        cn = xatoi(c);
        if (cn < 0) {
          val = -1;
          break;
        }
        val = val*10 + cn;
        state = 5;
      }
    } else if (state == 1) {
      // First char was '0', looking for an 'x' or 'X'
      if ((c == 'x') || (c == 'X')) {
        // Hex prefix satisfied
        state = 2;
      } else {
        // Hex prefix not satisfied. Could be decimal with leading '0'
        state = 5;
      }
    } else if ((state == 2) || (state == 3)) {
      // Hex prefix satisfied
      cn = htoi(c);
      if (cn < 0) {
        val = -1;
        break;
      }
      val = (val<<4) + cn;
      state = 3;
    } else if (state == 5) {
      // Assume decimal number
      cn = xatoi(c);
      if (cn < 0) {
        val = -1;
        break;
      }
      val = val*10 + cn;
    } else {
      // Invalid character
      val = -1;
    }
  }
  if ((state != 4) & (val >= 0)) {
    // Only special chars are allowed to extend beyond 1 byte
    val = val & 0xff;
  }
  // If val is negative, parsing failed
  *arg = val;
  return n;
}


/*
 * void console_pend_FPGA_enable(void);
 *    NOTE! This is called from an ISR.
 *    FPGA will be re-enabled in the main loop in console_service().
 *    TODO - Should I count to ensure enough time has passed since reset?
 */
void console_pend_FPGA_enable(void) {
  _fpgaEnable = 1;
  return;
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
