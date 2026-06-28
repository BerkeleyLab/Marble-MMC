/*
 * File: console.c
 * Desc: Encapsulate console (UART) interaction API
 *       Line-based comms (not char-based).
 *       Non-blocking if possible.
 *       marble_error_handler caller_id reserved: 64-79
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
#include "eeprom.h"
#include "ltm4673.h"
#include "watchdog.h"

#define AUTOPUSH
// TODO - Put this in a better place
#define FAN_SPEED_MAX           (120)
#define OVERTEMP_HARD_MAXIMUM   (125)
#define LTM_CONSOLE_ACTIVE_TIMEOUT_MS (5000)

const char unk_str[] = "Unknown option. Press '?' for help.\r\n";

const char *menu_str[] = {
  // "Build based on git commit " GIT_REV "\r\n",
  "Commands:\r\n",
  "    - ------------- ---------------------------------------------------------\r\n",
  "    0               Show board/chip identification and MMC status info\r\n"
  "    1 [-v]          Show MDIO/PHY Status (-v for verbose output)\r\n",
  "    2               I2C monitor\r\n",
  "    3               Status & counters\r\n",
  "    4 gpio          GPIO control\r\n",
  "    5               Reset FPGA\r\n",
  "    6               Push IP&MAC\r\n",
  "    7               Readout MAX6639 (Thermometer and fan controller).\r\n",
  "    8               Readout LM75_0 (Thermometer, U29)\r\n",
  "    9               Readout LM75_1 (Thermometer, U28)\r\n",
  "    a               I2C scan all ports\r\n",
#ifdef APP_MARBLE
  "    b               Config ADN4600 (Clock mux)\r\n",
#endif
  "    c               Readout INA219 (Current monitors)\r\n",
#ifdef APP_MARBLE
  "    d               MGT MUX - switch to QSFP 2\r\n",
#endif
  "    e               I2C_PM bus display\r\n",
#ifdef APP_MARBLE
//  "f - Flash XRP7724 (Power supply, Marble v1.1-1.3)\r\n",
#endif
#ifdef APP_MINI
  "    f               Flash XRP7724 (Power supply)\r\n",
#endif
  "    g               Enable XRP7724\r\n",
#ifdef APP_MARBLE
  "    h               FMC MGT MUX set\r\n",
#endif
  "    i               Timer check/cal\r\n",
  "    j               Read SPI mailbox\r\n",
  "    k               Readout PCA9555 (I2C GPIO expanders U34 and U39)\r\n",
  "    l               Config PCA9555\r\n",
  "    m [IP/MAC/SN]   Set/Query: IP d.d.d.d -- MAC xx:xx:xx:xx:xx:xx -- SN xxxx\r\n",  // TODO - SUPPORT m IP d.d.d.d, m MAC d:d:d:d:d:d, m SN xxxxxxxx for consistency with other set commands
  // "    n d:d:d:d:d:d   Set MAC Address\r\n",
#ifdef APP_MARBLE
  "    o               SI570 (Frequency synthesizer) status\r\n",
#endif
  "    p speed[%]      Set fan speed (0-120 or 0%-100%)\r\n",
  "    q otemp         Set overtemperature threshold (degC)\r\n",
  "    r bool          Set mailbox enable/disable (1/0, on/off)\r\n",
#ifdef APP_MARBLE
  "    s addr f cfg    Set Si570: addr[hex], f[Hz}, cfg[hex]\r\n",
#endif
#ifdef APP_MARBLE
  "    t pmbus_msg     Forward PMBus transaction to LTM4673\r\n",
#endif
  "    u period        Set/get watchdog timeout period (in seconds)\r\n",
  "    v key           Set a new 128-bit secret key (non-volatile, write only).\r\n",
  "    w bool          Set fan tachometer enable/disable (1/0, on/off)\r\n",
  "    x mode          Set MMC Pmod usage mode\r\n",
  "    - ------------- ---------------------------------------------------------\r\n",
  "    z lock/unlock.  Temporarily unlock MMC settings\r\n",
  "    ?               Help\r\n",
  "    - ------------- ---------------------------------------------------------\r\n",
};
#define MENU_LEN (sizeof(menu_str)/sizeof(*menu_str))

static uint8_t _msgCount;
static uint8_t _fpgaEnable;
static uint32_t _LTM_console_timestamp = 0;
static uint8_t _settings_lock = 1;
static uint32_t _settings_lock_tick = 0;

// TODO - find a better home for these
static int console_handle_msg(char *rx_msg, int len);
//static int console_shift_all(uint8_t *pData);
static void handle_menu_print(int len);
static int console_shift_msg(uint8_t *pData);
static void ina219_test(void);
static void handle_gpio(const char *msg, int len);
static int toggle_gpio(char c);
static uint8_t parse_boolean(const char *rx_msg, int len);
static int handle_mdio_phy_print(const char *rx_msg, int len);
static int handle_msg_IP_MAC_SN(const char *rx_msg, int len);
// static int handle_msg_MAC(const char *rx_msg, int len);
static int handle_msg_fan_speed(const char *rx_msg, int len);
static int handle_msg_overtemp(const char *rx_msg, int len);
static int handle_msg_watchdog(const char *rx_msg, int len);
static int handle_msg_key(const char *rx_msg, int len);
static int handle_mailbox_enable(const char *rx_msg, int len);
static int handle_tach_enable(const char *rx_msg, int len);
static int handle_pmod_mode(const char *rx_msg, int len);
static int handle_settings_lock(const char *rx_msg, int len);
static uint8_t check_settings_lock(void);
//static void print_mac_ip(mac_ip_data_t *pmac_ip_data);
static void print_mac(uint8_t *pdata);
static void print_ip(uint8_t *pdata);
static void print_sn(uint8_t *pdata);
static void print_this_ip(void);
static void print_this_mac(void);
static command_m_type_t sscanfm(const char *s, int len);
static int sscanfIP(const char *s, volatile uint8_t *data, int len);
static int sscanfSN(const char *s, volatile uint8_t *data, int len);
static int sscanfMAC(const char *s, volatile uint8_t *data, int len);
static int sscanfFanSpeed(const char *s, int len);
static int sscanfUnsignedDecimal(const char *s, int len);
static int sscanfUHexExact(const char *s, int len);
static int sscanfQuery(const char *rx_msg, int len);
static int sscanfSpace(const char *s, int len);
static int sscanfNonSpace(const char *s, int len);
static int sscanfNext(const char *s, int len);
#ifdef APP_MARBLE
static int sscanfUnsignedHex(const char *s, int len);
static int sscanfMGTMUX(const char *s, int len);
static int handle_msg_MGTMUX(char *rx_msg, int len);
static int handle_msg_fsynth(const char *s, int len);
static void console_print_fsynth(void);
static int handle_msg_pmbridge(const char *s, int len);
static int PMBridgeConsumeArg(const char *s, int len, volatile int *arg);
#endif
static int xatoi(char c);
static int htoi(char c);
static const char *pmod_mode_string(pmod_mode_t mode);

int console_init(void) {
  _msgCount = 0;
  _fpgaEnable = 0;
  return 0;
}

static int console_handle_msg(char *rx_msg, int len)
{
  #ifdef APP_MARBLE
  reset_error_repeat();
  #endif
  // TODO all these should return 0 on success, 1 on failure
  //      then we should print a simple global help string on failure
  // Switch behavior based on first char
  switch (*rx_msg) {
        case '?':
           handle_menu_print(len);
           break;
        case '0':
           marble_print_ID_status(len);
           break;
        case '1':
           handle_mdio_phy_print(rx_msg, len);
           break;
        case '2':
           I2C_PM_probe(len);
           break;
        case '3':
           print_status_counters(len);
           break;
        case '4':
           handle_gpio(rx_msg, len);
           break;
        case '5':
          if(len == 2){
            if(check_settings_lock()) {
              printf("Settings are locked. Unlock to allow FPGA Reset.\r\n");
            } else {
              printf("Resetting FPGA\r\n");
              FPGAWD_SelfReset();
              marble_SLEEP_ms(1000);
            }
          } else {
            printf(unk_str);
          }
           break;
        case '6':
           if(len == 2){
            console_print_mac_ip();
            console_push_fpga_mac_ip();
            printf("DONE\r\n");
           } else {
            printf(unk_str);
           }
           break;
        case '7':
           if(len == 2){
            printf("Start\r\n");
            print_max6639_decoded();
           } else {
            printf(unk_str);
           }
           break;
        case '8':
           if(len == 2){
            LM75_print_decoded(LM75_0);
           } else {
            printf(unk_str);
           }
           break;
        case '9':
           if(len == 2){
            LM75_print_decoded(LM75_1);
           } else {
            printf(unk_str);
           }
           break;
        case 'a':
          if(len == 2){
           printf("I2C scanner\r\n");
           I2C_PM_scan();
           I2C_FPGA_scan();
          } else {
            printf(unk_str);
           }
           break;
#ifdef APP_MARBLE
        case 'b':
          if(len == 2){
           printf("ADN4600\r\n");
           adn4600_init();
           adn4600_printStatus();
          } else {
            printf(unk_str);
           }
           break;
#endif
        case 'c':
          if(len == 2){
          //  printf("Readout INA219\r\n");
           ina219_test();
          } else {
            printf(unk_str);
           }
           break;
#ifdef APP_MARBLE
        case 'd':
          if(len == 2){
            if(check_settings_lock()) {
              printf("Settings are locked. Unlock to allow MGT MUX switch.\r\n");
            } else {
              printf("Switch MGT to QSFP 2\r\n");
              marble_MGTMUX_set(3, true);}
          } else {
            printf(unk_str);
          }
          break;
#endif
        case 'e':
          if(len == 2){
           printf("PM bus display\r\n");
           I2C_PM_bus_display();
          } else {
            printf(unk_str);
          }
          break;
#ifdef APP_MINI
        case 'f':
          if(len == 2){
           printf("XRP flash\r\n");
           xrp_flash(XRP7724);
          } else {
            printf(unk_str);
          }
           break;
#endif
        case 'g':
          if(len == 2){
            if(check_settings_lock()) {
              printf("Settings are locked. Unlock to enable XRP7724.\r\n");
            } else {
            printf("Enabling XRP7724\r\n");
            xrp_boot();}
          } else {
            printf(unk_str);
          }
           break;
#ifdef APP_MARBLE
        case 'h':
           handle_msg_MGTMUX(rx_msg, len);
           break;
#endif
        case 'i':
          if(len == 2){
           for (unsigned ix=0; ix<10; ix++) {
              printf("%u\r\n", ix);
              marble_SLEEP_ms(1000);
           }
          } else {
            printf(unk_str);
          }
          break;
        case 'j':
          if(len == 2){
           //mbox_peek();
           mailbox_read_print_all();
          } else {
            printf(unk_str);
          }
           break;
        case 'k':
          if(len == 2){
            pca9555_status();
          } else {
            printf(unk_str);
          }
           break;
        case 'l':
          if(len == 2){
            if(check_settings_lock()) {
              printf("Settings are locked. Unlock to configure PCA9555.\r\n");
            } else {
              pca9555_config();
            }
          } else {
            printf(unk_str);
          }
           break;
        case 'm':
           handle_msg_IP_MAC_SN(rx_msg, len);
           break;
        // case 'n':
        //    handle_msg_MAC(rx_msg, len);
        //    break;
#ifdef APP_MARBLE
        case 'o':
           si570_status();
           break;
#endif
        case 'p':
           handle_msg_fan_speed(rx_msg, len);
           break;
        case 'q':
           handle_msg_overtemp(rx_msg, len);
           break;
        case 'r':
            if(check_settings_lock()) {
              printf("Settings are locked. Unlock to configure mailbox.\r\n");
            } else {
              handle_mailbox_enable(rx_msg, len);
            }
           break;
#ifdef APP_MARBLE
        case 's':
           handle_msg_fsynth(rx_msg, len);
           break;
#endif
#ifdef APP_MARBLE
        case 't':
            if(check_settings_lock()) {
              printf("Settings are locked. Unlock to configure PMBus.\r\n");
            } else {
              handle_msg_pmbridge(rx_msg, len);
            }
           break;
#endif
        case 'u':
           handle_msg_watchdog(rx_msg, len);
           break;
        case 'v':
            if(check_settings_lock()) {
              printf("Settings are locked. Unlock to set a new secret key.\r\n");
            } else {
            handle_msg_key(rx_msg, len);
            }
           break;
        case 'w':
            if(check_settings_lock()) {
              printf("Settings are locked. Unlock to configure fan tachometer.\r\n");
            } else {
            handle_tach_enable(rx_msg, len);
            }
           break;
        case 'x':
           handle_pmod_mode(rx_msg, len);
           break;
        case 'z':
           handle_settings_lock(rx_msg, len);
           break;
        case 0x0A: // LF
           break;
        case 0x0D: // CR
           break;
        default:
           marble_UART_send(rx_msg, len); // Echo back unrecognized commands
           printf("%s [%c] 0x%02X \r\n", unk_str, *rx_msg, *rx_msg);
           break;
     }
  fflush(stdout);
  printf("> ");
  fflush(stdout);
  return 0;
}

static void handle_menu_print(int len) {
  if(len == 2) {
    for (unsigned kx=0; kx<MENU_LEN; kx++) {
        printf("%s", menu_str[kx]);
    }
  } else {
    printf(unk_str);
  }
}

static int handle_mdio_phy_print(const char *rx_msg, int len) {
  int query = sscanfQuery(rx_msg, len);
  int verbose = 0;
  if (query) {
    mdio_phy_print(verbose);
    return 0;
  }
  int offset = sscanfNext(rx_msg + 1, len-1) + 1;
  if (offset < len) {
    if (rx_msg[offset] == 'v') verbose = 1;
    else if (rx_msg[offset] == '-') {
      if (((offset+1) < len) && (rx_msg[offset+1] == 'v')) {
        verbose = 1;
      }
    }
  }
  mdio_phy_print(verbose);
  return 0;
}

static int handle_msg_IP_MAC_SN(const char *rx_msg, int len) {
  command_m_type_t command = sscanfm(rx_msg, len);
  if (command == IP) {
    if (sscanfQuery(rx_msg+3, len-3)) {
      print_this_ip();
      return 0;
    }
    else if(check_settings_lock()) {
      printf("Settings are locked. Unlock to configure IP.\r\n");
    } else {
      int rval;
      uint8_t ip[IP_LENGTH];
      rval = sscanfIP(rx_msg, ip, len);
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
  }
  else if (command == MAC) {
    if (sscanfQuery(rx_msg+4, len-4)) {
      print_this_mac();
      return 0;
    }
    else if(check_settings_lock()) {
      printf("Settings are locked. Unlock to configure MAC.\r\n");
    } else {
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
  }
  else if (command == SN) {
    if (sscanfQuery(rx_msg+3, len-3)) {
      console_print_SN();
      return 0;
    }
    else if(check_settings_lock()) {
      printf("Settings are locked. Unlock to configure SN.\r\n");
    } else {
      uint8_t sn[SN_LENGTH];
      uint8_t eeprom_sn[SN_LENGTH];
      uint32_t sn_sum = 0;
      int sn_read_val = eeprom_read_sn(eeprom_sn, SN_LENGTH);
      if (sn_read_val) {
        printf("Could not find Serial Number\r\n");
        return sn_read_val;
      }
      int rval = sscanfSN(rx_msg, sn, len);
      // printf("EEPROM SN read returned %d\r\n", eeprom_sn);
      if (rval) { // SN parsing failure
        printf("Malformed serial number. Fail.\r\n");
        return rval;
      }
      for (int i = 0; i < SN_LENGTH; i++) { // Check if SN is all zeros (indicating a reset)
        sn_sum += sn[i];
      }
      if (sn_sum == 0){ 
        eeprom_store_sn(sn, SN_LENGTH);
        printf("Serial number reset.\r\n"); // reset and exit
        return 0;
      }
      for (int i = 0; i < SN_LENGTH; i++) {  // Check if SN is already configured (non-zero)
        if (eeprom_sn[i] != 0) {
          printf("Serial number already configured. Write access denied.\r\n");
          return 1;
        }
      }
      print_sn(sn);
      eeprom_store_sn(sn, SN_LENGTH);
    #ifdef AUTOPUSH
      // console_push_fpga_sn(); // TODO - implement this
    #endif
      return 0;
    }
  }
  else {
    // explain command options
    printf(unk_str);
    return 1;
  }
  return 0;
}

static int handle_msg_fan_speed(const char *rx_msg, int len) {
  int query = sscanfQuery(rx_msg, len);
  int speed, speedPercent;
  uint8_t readSpeed;
  if (query) {
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
  if(check_settings_lock()){
    printf("Settings are locked. Unlock to configure fans.\r\n");
    return -1;
  }
  speedPercent = (100 * speed)/FAN_SPEED_MAX;
  printf("Setting fan speed to %d (%d%%)\r\n", speed, speedPercent);
  max6639_set_fans(speed);
  eeprom_store_fan_speed((uint8_t *)&speed, 1);
  return 0;
}

static int handle_msg_overtemp(const char *rx_msg, int len) {
  // Overtemp is stored in MAX6639 as degrees C
  int query = sscanfQuery(rx_msg, len);
  uint8_t otbyte;
  if (query) {
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
  if(check_settings_lock()){
    printf("Settings are locked. Unlock to configure overtemp.\r\n");
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

static int handle_tach_enable(const char *rx_msg, int len) {
  uint8_t rval = parse_boolean(rx_msg, len);
  uint8_t tach_en;
  if (rval == 0x01) {
    // Query
    tach_en = max6639_get_tach_en();
    if (tach_en) {
      printf("Fan tachometer (PWM pulse stretching) enabled\r\n");
    } else {
      printf("Fan tachometer (PWM pulse stretching) disabled\r\n");
    }
    return 0;
  } else if (rval == 0x02) {
    // Disable
    printf("Disabling fan tachometer (PWM pulse stretching)\r\n");
    tach_en = 0;
  } else if (rval == 0x03) {
    // Enable
    printf("Enabling fan tachometer (PWM pulse stretching)\r\n");
    tach_en = 1;
  } else {
    // Bad parsing
    printf("Failed to parse\r\n");
    return 1;
  }
  max6639_set_tach_en(tach_en);
  eeprom_store_tach_en((const uint8_t *)&tach_en, 1);
  return 0;
}

static int handle_mailbox_enable(const char *rx_msg, int len) {
  uint8_t rval = parse_boolean(rx_msg, len);
  int en;
  if (rval == 0x01) {
    // Query
    en = mbox_get_enable();
    if (en) {
      printf("Mailbox enabled\r\n");
    } else {
      printf("Mailbox disabled\r\n");
    }
  } else if (rval == 0x02) {
    // Disable
    printf("Disabling mailbox update\r\n");
    mbox_disable();
  } else if (rval == 0x03) {
    // Enable
    printf("Enabling mailbox update\r\n");
    mbox_enable();
  } else {
    // Bad parsing
    printf("Failed to parse\r\n");
    return 1;
  }
  return 0;
}

static uint8_t parse_boolean(const char *rx_msg, int len) {
  //  Msg   Action        retval
  //        Bad parsing   0x00
  //  r ?   Print status  0x01
  //  r     Print status  0x01
  //  r 0   Disable       0x02
  //  r 1   Enable        0x03
  //  r off Disable       0x02
  //  r on  Enable        0x03
  int en = 0;
  char c;
  int doParse = 0;
  // 'doParse' 0 means unparsed; 1 means parse fail; 2 means parse success
  char arg[3];
  int argp = 0;
  int query = sscanfQuery(rx_msg, len);
  if (query) {
    return 0x01;
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
        // Shouldn't hit this
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
    return 0;
  }
  if (query) {
    return 0x01;
  } else {
    if (en) {
      return 0x03;
    } else {
      return 0x02;
    }
  }
  return 0;
}

#ifdef APP_MARBLE
static int handle_msg_MGTMUX(char *rx_msg, int len) {
  // int query = sscanfQuery((const char *)rx_msg, len);
  int rval = sscanfMGTMUX(rx_msg, len);
  uint8_t rbyte = 0;
  if (rval == -1) {
    printf("E.g. Set all MUXn pin states: h 1=1 2=0 3=0\r\n");
    printf("E.g. Set just MUX2 pin high (ignore others): h 2=1\r\n");
    printf("E.g. Read MGTMUX state: h ?\r\n");
    return rval;
  } else if (rval == -2) {
    // Get and print current MGT MUX state
    rbyte = marble_MGTMUX_status();
    printf("  ");
    for (int n = 0; n < MGT_MAX_PINS; n++) {
      printf("MUX%d=%d ", n+1, ((rbyte >> n) & 1));
    }
    printf("\r\n");
  } else if(check_settings_lock()){
    printf("Settings are locked. Unlock to configure MGT MUX.\r\n");
  } else {
    printf("  "); // Indent the line printed by the following function
    marble_MGTMUX_config((uint8_t)rval, 1, 1); // Store nonvolatile, print
  }
  return rval;
}
#endif

static void handle_gpio(const char *msg, int len) {
  char c = 0;
  int found = 0;
  c = *(msg + 1);
  if(len == 3 || ((len == 4) && (c == ' '))) {
  //printf("len = %d\r\n", len);
  // Look for alphabetic characters and respond accordingly
  // (skips the first char which is the command char)
  // for (int n = 1; n < len; n++) {
    int n = len - 2;
    c = *(msg + n);
    if (c == '?') {
      found = -1;
    } else if(check_settings_lock() == 1) {
      printf("Settings are locked. Unlock to allow GPIO control.\r\n");
      found = -1;
    } else if(c >= 'A') {
      found |= toggle_gpio(c);
    }
  // }
  if (found == -1) {
    // Print state
    marble_print_GPIO_status();
  }
  else if (!found) {
    marble_list_GPIOs();
  }
  return;
  } else {
    printf(unk_str);
  }
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
      marble_SLEEP_ms(100);
      break;
    case 'B':
      marble_PSU_pwr(1);
      marble_SLEEP_ms(1500);
      printf("PSU Power On\r\n");
      break;
    case 'c':
      // PMOD3_5 J16[4]
      marble_Pmod3_5_write(0);
      break;
    case 'C':
      marble_Pmod3_5_write(1);
      break;
    case 'd':
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
  set_last_ip(pdata.ip);
  set_last_mac(pdata.mac);
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
  PRINT_MULTIBYTE_HEX(pdata, MAC_LENGTH, ':');
  return;
}

static void print_sn(uint8_t *pdata)
{
    /* ---- 1. Print the raw SN bytes as two‑digit hex values ---------- */
    printf("SN: ");
    for (int i = 0; i < SN_LENGTH; ++i)
        printf("%02X", pdata[i]);

#if SN_LENGTH == 2
    /* ---- 2. Determine manufacturer from the first byte ------------- */
    const char *mfg = "Unknown";
    uint8_t b0 = pdata[0];                     /* first byte */

    if ((b0 & 0xC0) == 0xC0)                    /* 0b11X.. */
        mfg = "LBNL";
    else if ((b0 & 0xC0) == 0x40)               /* 0b01X.. */
        mfg = "SLAC";
    else if ((b0 & 0xE0) == 0x80)                 /* 0b100.. */
        mfg = "Osprey";
    else if ((b0 & 0xE0) == 0xA0)               /* 0b101.. */
        mfg = "Betz Eng";
    else if ((b0 & 0xE0) == 0x20)               /* 0b001.. */
        mfg = "NOT ALLOWED - contact LBNL ATG";
    else if ((b0 & 0xE0) == 0x00)               /* 0b000.. */
        mfg = "Non-unique - unprotected SN range";

    /* ---- 3. Finish the line with the manufacturer ----------------  */
    printf(" [%s]\r\n", mfg);
#endif
}

static void print_ip(uint8_t *pdata) {
  printf("IP: ");
  PRINT_MULTIBYTE_DEC(pdata, IP_LENGTH, '.');
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

void console_print_SN(void) {
  uint8_t sn[SN_LENGTH];
  int rval = eeprom_read_sn(sn, SN_LENGTH);
  if (rval) {
    printf("Could not find Serial Number\r\n");
    return;
  }
  print_sn(sn);
  return;
}

static void print_this_ip(void) {
  uint8_t ip[IP_LENGTH];
  int rval = eeprom_read_ip_addr(ip, IP_LENGTH);
  if (rval) {
    printf("Could not find IP address\r\n");
    return;
  }
  set_last_ip(ip);
  print_ip(ip);
  return;
}

void CONSOLE_USART_ISR(void) {
  USART_RXNE_ISR(); // Handle RX interrupts first
  USART_TXE_ISR();  // Then handle TX interrupts
  return;
}

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
    //printf("_msgCount, len = %d\r\n", _msgCount);
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

/*********************************************************************
 *  xatoi:   convert a decimal digit to int, –1 otherwise
 *********************************************************************/
static int xatoi(char c) {
    if ((c >= '0') && (c <= '9')) {
        return (int)(c - '0');
    }
    return -1;
}

/*********************************************************************
 *  htoi:   convert a hex digit to int (0‑15), –1 otherwise
 *********************************************************************/
static int htoi(char c) {
    if ((c >= '0') && (c <= '9')) {
        return (int)(c - '0');
    } else if ((c >= 'a') && (c <= 'f')) {
        return (int)(10 + (c - 'a'));
    } else if ((c >= 'A') && (c <= 'F')) {
        return (int)(10 + (c - 'A'));
    }
    return -1;
}

/*
 * interprets the "m" command.
 * Expects the command to be in the form of "m <type>" where <type> is one of:
 *  IP - for IP address
 *  MAC - for MAC address
 *  SN - for serial number
 */
static command_m_type_t sscanfm(const char *s, int len)
{
  // marble_UART_send(s, len); // Echo back the command for clarity in debugging
  if (!s || len <= 0) return NONE;

  int i = 0;

  /* expect 'm' */
  if (s[i] != 'm') return NONE;
  i++;

  /* require at least one whitespace after 'm' */
  if (i >= len || s[i] == '\0' || !(s[i] == ' '))  return NONE;
  i++;

  /* read the type token (IP / MAC / SN) */
  char tok[4] = {0}; /* longest is "MAC" (3) */
  int n = 0;
  while (i < len && s[i] != '\0' && !(s[i] == ' ') && !(s[i] == '\n') && !(s[i] == '\r'))  {
    if (n >= 3) return NONE;              /* token too long */
    tok[n++] = ((unsigned char)s[i]);
    i++;
  }
  tok[n] = '\0';
  
  if (strcmp(tok, "IP") == 0)  return IP;
  if (strcmp(tok, "MAC") == 0) return MAC;
  if (strcmp(tok, "SN") == 0)  return SN;

  return NONE;
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
  for (int n = 4; n < len; n++) {
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



/*********************************************************************
 *  Parse a serial number that is encoded as a *contiguous* string
 *  of hex digits (no separators).  The function ignores any
 *  characters preceding the first legal hex digit.
 *
 *  Parameters
 *      s      – input buffer (len bytes long)
 *      data   – buffer to receive the binary SN (must be ≥ SN_LENGTH)
 *      len    – actual length of *s* (not counting a trailing NUL)
 *
 *  Returns
 *      0   – success (exact `SN_LENGTH*2` hex digits parsed)
 *     -1   – malformed packet (wrong number of digits)
 *********************************************************************/
static int sscanfSN(const char *s, volatile uint8_t *data, int len)
{
    uint32_t tmp  = 0;   /* accumulation for the whole SN   */
    int      nhex = 0;   /* number of hex digits already seen */

    /* ---------- 1. Skip any initial non‑hex characters -------------- */
    int i = 0;
    while (i < len && htoi(s[i]) < 0) {  /* htoi<0 ⇒ not a hex digit */
        ++i;
    }

    /* ---------- 2. Read exactly SN_LENGTH*2 hex digits --------------- */
    for (; i < len && nhex < SN_LENGTH * 2; ++i) {
        int nib = htoi(s[i]);          /* must be >=0 for a real digit */
        if (nib >= 0) {                 /* ignore any stray char */
            tmp = (tmp << 4) | (uint32_t)nib;
            ++nhex;
        }
    }

    /* ---------- 3. Ensure we have the required amount of digits ------- */
    if (nhex != SN_LENGTH * 2)
        return -1;                       /* malformed packet */

    /* ---------- 4. Split the accumulator into bytes (MSB first) ----- */
    for (i = SN_LENGTH - 1; i >= 0; --i) {
        data[i] = (uint8_t)(tmp & 0xFF);
        tmp >>= 8;
    }

    return 0;                            /* success */
}

// /*
//  * static int sscanfSN(const char *s, volatile uint8_t *data, int len);
//  *    This hackery is needed because it seems newlib-nano's version of sscanf is
//  *    not fully functional.  
//  */
// static int sscanfSN(const char *s, volatile uint8_t *data, int len) {
//   char c;
//   int r;
//   uint32_t sum = 0;
//   // Start scan on char 1
//   for (int n = 4; n < len; n++) {
//     c = s[n];
//     r = xatoi(c);
//     if (r >= 0) {
//       sum = (sum * 10) + r;
//     }
//   }
  
//   for (int n = SN_LENGTH - 1; n >= 0; n--) {
//     data[n] = (uint8_t)(sum & 0xFF);
//     sum = sum >> 8;
//   }
//   return 0;
// }

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

static int handle_msg_watchdog(const char *rx_msg, int len) {
  int index = sscanfQuery(rx_msg, len);
  int val;
  if (index) {
    val = FPGAWD_GetPeriod();
    if (val == 0) {
      printf("Watchdog disabled (period = 0)\r\n");
    } else {
      printf("Current watchdog timeout: %d seconds\r\n", val);
    }
    return -1;
  }
  index = sscanfNext(rx_msg, len);
  val = sscanfUnsignedDecimal((rx_msg + index), len-index);
  if (val < 0) {
    printf("Failed to parse\r\n");
    return -1;
  }
  if(check_settings_lock()) {
    printf("Settings are locked. Unlock to configure watchdog.\r\n");
    return -1;
  }
  // Set and peg to limits
  val = FPGAWD_SetPeriod((unsigned int)val);
  eeprom_store_wd_period((const uint8_t *)&val, 1);
  return 0;
}

#define KEY_LEN     (16)
static int handle_msg_key(const char *rx_msg, int len) {
  int index = sscanfNext(rx_msg, len);
  uint8_t key[KEY_LEN];
  int rval;
  int n;
  for (n = 0; n < KEY_LEN; n++) {
    // Parse hex string into bytes
    rval = sscanfUHexExact(rx_msg + index + 2*n, 2);
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

#ifdef APP_MARBLE
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
#endif

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

#ifdef APP_MARBLE
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
    if (c0 == '?' || c1 == '?') {
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
#endif

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

/* static int sscanfQuery(const char *rx_msg, int len);
 *    Return 1 if rx_msg represents a query, defined as:
 *      "x"       // Single control char
 *      "x?"      // Control char followed by '?'
 *      "x  \t\n" // Control char followed by any amount of whitespace
 *      "x ?"     // Control char, whitespace, then '?'
 *    Else, return 0
 *    Note that the first character is skipped (assumed to be some type
 *    of control character vetted external to this function).
 */
static int sscanfQuery(const char *rx_msg, int len) {
  int query = 0;
  // Check for query
  if (len <= 1) {
    query = 1;
  } else {
    // Re-using the same int. Being extra stingy with stack space
    query = sscanfNonSpace((const char *)(rx_msg+1), len-1);
    if ((query < 0) || (rx_msg[query+1] == '?')) {
      query = 1;
    } else {
      query = 0;
    }
  }
  return query;
}



#ifdef APP_MARBLE
static int handle_msg_fsynth(const char *s, int len) {
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
    if(check_settings_lock() == 1) {
      printf("Settings are locked. Unlock to allow fsynth configuration.\r\n");
      return -1;
    } 
    printf("I2C Addr = 0x%x, Freq = %d Hz, Config = 0x%x\r\n", (unsigned) i2c_addr, freq, (unsigned) config);
    FSYNTH_ASSEMBLE(data, i2c_addr, freq, config);
    eeprom_store_fsynth((const uint8_t *)data, 6);
  }
  return 0;
}
#endif

#ifdef APP_MARBLE
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
#endif

static const char *pmod_mode_string(pmod_mode_t mode) {
  switch (mode) {
    case PMOD_MODE_DISABLED:
      return "Disabled";
    case PMOD_MODE_UI_BOARD:
      return "ALS OLED UI Board";
    case PMOD_MODE_LED:
      return "Indicator LEDs";
    case PMOD_MODE_GPIO:
      return "Slow GPIO";
    default:
      break;
  }
  return "Unknown";
}

static int handle_pmod_mode(const char *rx_msg, int len) {
  // Parse messages:
  //   "x"      -> Query pmod_mode
  //   "x?"     -> Query pmod_mode
  //   "x ?"    -> Query pmod_mode
  //   "x 0"    -> Set pmod_mode = PMOD_MODE_DISABLED
  //   "x 1"    -> Set pmod_mode = PMOD_MODE_UI_BOARD
  //   "x 2"    -> Set pmod_mode = PMOD_MODE_LED
  //   "x 3"    -> Set pmod_mode = PMOD_MODE_GPIO
  //   "x 4"    -> Invalid; error
  int query = sscanfQuery(rx_msg, len);
  const char *modestr;
  pmod_mode_t pmod_mode;
  if (query) {
    pmod_mode = system_get_pmod_mode();
    modestr = pmod_mode_string(pmod_mode);
    printf("Current Pmod mode: %s\r\n", modestr);
    printf("  Options:\r\n");
    printf("  --------\r\n");
    for (int n=0; n<PMOD_MODE_SIZE; n++) {
      printf("    %d: %s\r\n", n, pmod_mode_string((pmod_mode_t)n));
    }
    return 0;
  }
  int mode = -1;
  int index = sscanfNext(rx_msg+1, len) + 1;
  mode = sscanfUnsignedDecimal(rx_msg+index, len-index);
  if ((mode < 0) || (mode >= PMOD_MODE_SIZE)) {
    printf("Invalid option. Valid choices are (%d-%d).\r\n", PMOD_MODE_DISABLED, PMOD_MODE_SIZE-1);
    return -1;
  } else if(check_settings_lock()) {
    printf("Settings are locked. Unlock to set pmod mode.\r\n");
  } else {
    printf("Setting Pmod mode to: %s... ", pmod_mode_string((pmod_mode_t)mode));
    // Re-using index as rval
    if ((index = system_set_pmod_mode(mode)) == 0) {
      printf("\r\n");
    } else {
      #ifdef MARBLE_V2 // Only V2 has error handler (todo - implement for Marble Mini)
        marble_error_handler(ERROR_MARBLE_PMOD, 64);
      #endif
      printf("Failed. Error code %d\r\n", index);
      return -1;
    }
  }
  return 0;
}


static uint8_t check_settings_lock(void) {
    if (_settings_lock) {
        return 1;   /* locked */
    }
    /* If we are here, settings are currently unlocked.  Check if the
     * timeout has expired. */
    if (marble_get_tick() - _settings_lock_tick > SETTINGS_UNLOCK_TIMEOUT) {
        _settings_lock = 1;   /* re-lock */
        printf("Settings lock re-enabled - timeout.\r\n");
        return 1;           /* locked */
    }
    return 0;               /* still unlocked */
}
/*********************************************************************
 *  Settings–lock commands
 *
 *  Commands understood (case‑sensitive, must start with `z`):
 *      "z LOCK"   → lock settings      (len 7)
 *      "z UNLOCK" → unlock settings    (len 9)
 *
 *  The function keeps the same signature and behaviour as before.
 *********************************************************************/

 /* -----------------------------------------------------------------
  *  The small helper callbacks that actually perform the two actions.
  *  They are *file‑scope* static functions so the compiler can
  *  embed them into the command table.
  * ----------------------------------------------------------------- */
static int exec_locked(void) {
    _settings_lock = 1;
    printf("Settings lock enabled\r\n");
    return 0;
}

static int exec_unlocked(void) {
    _settings_lock = 0;
    _settings_lock_tick = marble_get_tick();
    printf("Settings unlocked for 120 seconds\r\n");
    return 0;
}

/* -----------------------------------------------------------------
 *  Command description table – one entry per supported command.
 *  The `cmd` field contains the exact text to compare after the
 *  first byte (`'z'`) and the `len` field says the *full* expected
 *  length of the incoming message.
 * ----------------------------------------------------------------- */
typedef struct {
    const char *cmd;        /* text to match (includes the leading space) */
    int         cmd_len;    /* strlen(cmd)                            */
    int    (*exec)(void);   /* function to call on a match            */
} cmd_t;

static const cmd_t cmd_table[] = {
    { " lock",   5, exec_locked  },
    { " LOCK",   5, exec_locked  },   /* 'z LOCK' – expected length 7 */
    { " unlock", 7, exec_unlocked},    /* 'z UNLOCK' – expected length 9 */
    { " UNLOCK", 7, exec_unlocked}    /* 'z UNLOCK' – expected length 9 */
};

/* -----------------------------------------------------------------
 *  Main handler – this is the function you drop into your code.
 * ----------------------------------------------------------------- */
static int handle_settings_lock(const char *rx_msg, int len)
{
    /* Fast sanity: first char must be 'z' and there must be a space. */
    if (len < 4 || rx_msg[0] != 'z' || rx_msg[1] != ' ')
        goto error;

    /* Try to find a matching entry in the table. */
    for (size_t i = 0; i < sizeof(cmd_table)/sizeof(*cmd_table); ++i) {
        const cmd_t *c = &cmd_table[i];
        /* `len` must match the full expected length. */
        if (c->cmd_len + 2 != len)            /* 1 ('z') + 1 (space) + cmd_len */
            continue;
        /* Compare the rest of the message. */
        if (memcmp(rx_msg + 1, c->cmd, c->cmd_len) == 0)
            return c->exec();                 /* success: callback returns 0 */
    }

error: /* No match found – same error message as the original */
    printf("Invalid command. Use 'z unlock' or 'z UNLOCK' to unlock settings.\n");
    exec_locked();
    return -1;
}

#ifdef APP_MARBLE
/* static int handle_msg_pmbridge(const char *s, int len);
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
static int handle_msg_pmbridge(const char *s, int len) {
  // Skip the first character (command char)
  int ptr = sscanfNext(s, len);
  int ptrinc;
  int max_len = len > PMBRIDGE_MAX_LINE_LENGTH ? PMBRIDGE_MAX_LINE_LENGTH : len;
  int arg;
  int item_index = 0;
  int fail = 0;
  uint16_t xact[PMBRIDGE_XACT_MAX_ITEMS];
  _LTM_console_timestamp = marble_get_tick();
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
#endif

#ifdef APP_MARBLE
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
#endif

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

static uint8_t last_ip_addr[IP_LENGTH] = {0, 0, 0, 0};
static uint8_t last_mac_addr[MAC_LENGTH] = {0, 0, 0, 0, 0, 0};

void set_last_ip(const uint8_t *ip) {
  //printf("Setting ip: ");
  //PRINT_MULTIBYTE_DEC(ip, 4, '.');
  memcpy((void *)last_ip_addr, (void *)ip, (size_t)IP_LENGTH/sizeof(uint8_t));
  //printf("last_ip_addr = ");
  //PRINT_MULTIBYTE_DEC(last_ip_addr, 4, '.');
  return;
}

uint8_t *get_last_ip(void) {
  return last_ip_addr;
}

void set_last_mac(const uint8_t *mac) {
  memcpy((void *)last_mac_addr, (void *)mac, (size_t)MAC_LENGTH/sizeof(uint8_t));
  return;
}

uint8_t *get_last_mac(void) {
  return last_mac_addr;
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

uint8_t _LTM_console_active(void) {
    uint32_t now = marble_get_tick();
    uint32_t delta = now - _LTM_console_timestamp;
    return (delta < LTM_CONSOLE_ACTIVE_TIMEOUT_MS) ? 1 : 0;
  }
