// Encapsulation of the routines pushing content to the display board

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "display.h"
#include "lv_font.h"
#include "ui_board.h"
#include "frame_buffer.h"
#include "ssd1322.h"
#include "gui.h"
#include "st-eeprom.h"
#include "marble_api.h"
#include "console.h"
#include "i2c_fpga.h"
#include "i2c_pm.h"
#include "max6639.h"
#include "watchdog.h"

// ======================= Policy ===========================
#define DISPLAY_TIMEOUT_MS      (10*60*1000)
#define UPDATE_INTERVAL_MS            (1000)
#define USE_FONT_17
// TODO - Warning! The lv_font_roboto_12 font is not monospace, so ensuring any
//        future value fits inside the initial bounding box is somewhat tedious.
// Set to 1 to enable current monitoring of FMC cards.
// This violates the "don't spam the I2C bus" policy of the MMC, so it's disabled
// by default and may be removed completely.
#define ENABLE_FMC_CURRENT_CHECK         (0)
// ==========================================================

extern lv_font_t lv_font_roboto_12, lv_font_roboto_mono_17, lv_font_fa;

typedef enum {
  PAGE_STATE=0,
  PAGE_POWER,
#if ENABLE_FMC_CURRENT_CHECK
  PAGE_FMC,
#endif
  PAGE_TEMPERATURE,
  NUMBER_OF_PAGES, // Keep me at the end
} display_page_t;

// These will remain correct even if the page order changes in the enum above
#define PAGE_FIRST        (0)
#define PAGE_LAST         (NUMBER_OF_PAGES-1)

static display_page_t current_page = PAGE_STATE;

// Vertical line spacing depending on whether we're using lv_font_roboto_12 or lv_font_roboto_mono_17
// Hack to align labels. The 12pt font is not monospace, so assume the widest character for offsets.
// Max width of non-monospace font (character '@')
#define CHAR_WIDTH_12       (11)
#define CHAR_HEIGHT_12      (14)
#define LINE_SPACING_12     (CHAR_HEIGHT_12+1)
// lv_font_roboto_mono_17 is monospace
#define CHAR_WIDTH_17       (10)
#define CHAR_HEIGHT_17      (19)
#define LINE_SPACING_17     (CHAR_HEIGHT_17+1)

#ifdef USE_FONT_17
  #define DEFAULT_FONT        lv_font_roboto_mono_17
  #define LINE_SPACING        LINE_SPACING_17
  #define CHAR_WIDTH          CHAR_WIDTH_17
#else
  #define DEFAULT_FONT        lv_font_roboto_12
  #define LINE_SPACING        LINE_SPACING_12
  #define CHAR_WIDTH          CHAR_WIDTH_12
#endif

static int display_enabled = 1;
uint32_t last_touch = 0;

/*
static t_label label_test_12;
static t_label label_test_17;
*/

// =================== ERROR Overlay ========================
// NOTE! The error overlay only works if the UI board is supplied with an alternate
// source of 3.3V, as the +3V3 net it connects to via the Pmod is disabled when the
// board state is one of these shutdown conditions
static t_label label_error;
static const char label_error_init[] = "ERROR";
#define LABEL_ERROR_SIZE     (sizeof(label_error_init)/sizeof(char))

static t_label label_error_state;
static const char label_error_state_ot[] = "Over-Temperature Shutdown";
#define LABEL_ERROR_STATE_OT_SIZE     (sizeof(label_error_state_ot)/sizeof(char))
static const char label_error_state_pd[] = "Power Lost";
#define LABEL_ERROR_STATE_PD_SIZE     (sizeof(label_error_state_pd)/sizeof(char))

// ===================== Page STATE =========================
// Marble v1.4
static t_label label_marble;

// IP: 192.168.100.101
static t_label label_ip;

// MAC: aa:bb:cc:dd:ee:ff
static t_label label_mac;

//Over-Temperature
//Powerdown
//                                      "Marble v1.4 Golden Image"
static const char label_marble_init[] = "Marble v1.              ";
#define LABEL_MARBLE_SIZE     (sizeof(label_marble_init)/sizeof(char))

static const char label_ip_init[] = "IP: xxx.xxx.xxx.xxx";
#define LABEL_IP_SIZE     (sizeof(label_ip_init)/sizeof(char))

static const char label_mac_init[] = "MAC: xx:xx:xx:xx:xx:xx";
#define LABEL_MAC_SIZE     (sizeof(label_mac_init)/sizeof(char))

// ==========================================================

// ===================== Page POWER =========================

static t_label label_power_12V;
#define LABEL_POWER_12V_FMT                "Input (12V): %4.2fV @ %3.2fA"
static const char label_power_12V_init[] = "Input (12V): 12.00V @ 3.00A";
#define LABEL_POWER_12V_SIZE     (sizeof(label_power_12V_init)/sizeof(char))

static t_label label_power_3V3;
#define LABEL_POWER_3V3_FMT                "3V3: %3.2fV @ %3.2fA"
static const char label_power_3V3_init[] = "3V3: 3.30V @ 1.00A";
#define LABEL_POWER_3V3_SIZE     (sizeof(label_power_3V3_init)/sizeof(char))

static t_label label_power_2V5;
#define LABEL_POWER_2V5_FMT                "2V5: %3.2fV @ %3.2fA"
static const char label_power_2V5_init[] = "2V5: 2.50V @ 1.00A";
#define LABEL_POWER_2V5_SIZE     (sizeof(label_power_2V5_init)/sizeof(char))

static t_label label_power_1V8;
#define LABEL_POWER_1V8_FMT                "1V8: %3.2fV @ %3.2fA"
static const char label_power_1V8_init[] = "1V8: 1.80V @ 1.00A";
#define LABEL_POWER_1V8_SIZE     (sizeof(label_power_1V8_init)/sizeof(char))

static t_label label_power_1V0;
#define LABEL_POWER_1V0_FMT                "1V0: %3.2fV @ %3.2fA"
static const char label_power_1V0_init[] = "1V0: 1.00V @ 1.00A";
#define LABEL_POWER_1V0_SIZE     (sizeof(label_power_1V0_init)/sizeof(char))

// ==========================================================

// ======================= Page FMC =========================
// If !ENABLE_FMC_CURRENT_CHECK, FMC state is displayed on the "POWER" page
static t_label label_fmc_1;
static t_label label_fmc_2;

#if ENABLE_FMC_CURRENT_CHECK
static const char label_fmc_1_init[] = "FMC1: Disabled (0.00A)";
static const char label_fmc_2_init[] = "FMC2: Disabled (0.00A)";
#else
static const char label_fmc_1_disabled[] = "FMC1: Disabled";
static const char label_fmc_2_disabled[] = "FMC2: Disabled";
static const char label_fmc_1_enabled[] = "FMC1: Enabled";
static const char label_fmc_2_enabled[] = "FMC2: Enabled";
static const char *label_fmc_1_init = label_fmc_1_disabled;
static const char *label_fmc_2_init = label_fmc_2_disabled;
#endif

#define LABEL_FMC_2_SIZE     (sizeof(label_fmc_2_init)/sizeof(char))
#define LABEL_FMC_1_SIZE     (sizeof(label_fmc_1_init)/sizeof(char))

// ==========================================================

// =================== Page TEMPERATURE =====================
/*
   |LM75_0 Temp: 25.3 C        LM75_1 Temp: 25.3 C            |
   |MAX6639 Ch1 Temp: 25.3 C   MAX6639 Ch2 Temp: 25.3 C       |
 */
//Note: The 'degree' symbol in lv_font_roboto_12 uses encoding (U+B0)
//      which is the result after UTF decoding.  The expected UTF-8 multi-byte
//      encoding for this value is \xC2\xB0.
//      I use octal \302\260 instead of hex \xC2\xB0 because of the char 'C' that
//      immediately follows.
//      And the "init" label contains a '@@' in its place to add extra space
//      and ensure that the non-monospace font gives us enough space for any
//      value in the numeric portion.

static t_label label_temperature_lm75_0_label;
static const char label_temperature_lm75_0_label_init[] = "LM75 (U29):";
static t_label label_temperature_lm75_0;
#define LABEL_TEMPERATURE_LM75_0_FMT                "%4.1f \302\260C"
static const char label_temperature_lm75_0_init[] = "25.3 @@C";
#define LABEL_TEMPERATURE_LM75_0_SIZE (sizeof(label_temperature_lm75_0_init)/sizeof(char))

static t_label label_temperature_lm75_1_label;
static const char label_temperature_lm75_1_label_init[] = "LM75 (U28):";
static t_label label_temperature_lm75_1;
#define LABEL_TEMPERATURE_LM75_1_FMT                "%4.1f \302\260C"
static const char label_temperature_lm75_1_init[] = "25.3 @@C";
#define LABEL_TEMPERATURE_LM75_1_SIZE (sizeof(label_temperature_lm75_1_init)/sizeof(char))

static t_label label_temperature_max6639_1_label;
static const char label_temperature_max6639_1_label_init[] = "FPGA (U1):";
static t_label label_temperature_max6639_1;
#define LABEL_TEMPERATURE_MAX6639_1_FMT                "%4.1f \302\260C"
static const char label_temperature_max6639_1_init[] = "25.3 @@C";
#define LABEL_TEMPERATURE_MAX6639_1_SIZE (sizeof(label_temperature_max6639_1_init)/sizeof(char))

static t_label label_temperature_max6639_2_label;
static const char label_temperature_max6639_2_label_init[] = "Fan Ctrlr (U27):";
static t_label label_temperature_max6639_2;
#define LABEL_TEMPERATURE_MAX6639_2_FMT                "%4.1f \302\260C"
static const char label_temperature_max6639_2_init[] = "25.3 @@C";
#define LABEL_TEMPERATURE_MAX6639_2_SIZE (sizeof(label_temperature_max6639_2_init)/sizeof(char))

// ==========================================================

//static void xformat_ip_addr(uint32_t packed_ip, char *ps, int maxlen);
static void update_page(int refresh);
static void init_display_error(void);
static void init_page_state(void);
static void init_page_power(void);
static void init_page_temperature(void);
static void update_display_error(int refresh);
static int update_page_state(int refresh);
static int update_page_power(int refresh);
#if ENABLE_FMC_CURRENT_CHECK
static void init_page_fmc(void);
static int update_page_fmc(int refresh);
#endif
static int update_page_temperature(int refresh);
static void display_enable(void);
static void display_disable(void);
static void display_toggle(void);
static void display_timeout(void);
static int compare_systick(uint32_t old, uint32_t new, uint32_t threshold);
static int do_update(uint32_t last_update);
static void format_ip_addr(uint8_t *ip, char *ps, int maxlen);
static void format_mac_addr(uint8_t *mac, char *ps, int maxlen);
static int array_updated_uint8_t(volatile uint8_t *old, const uint8_t *new, int len);
static int array_updated_int(volatile int *old, const int *new, int len);
static void update_led(void);

/*
static void print_str(const char *ss, int len);
static char xitoa(uint8_t i);

static void print_str(const char *ss, int len) {
  for (int n=0; n<len; n++) {
    draw_char(ss[n]);
  }
  return;
}

static char xitoa(uint8_t i) {
  if (i <= 9) {
    //printf("  xitoa(%d) = %c\r\n", i, (char)'0'+i);
    return (char)('0' + i);
  }
  return ' ';
}
*/

/*
static void xformat_ip_addr(uint32_t packed_ip, char *ps, int maxlen) {
  uint8_t ip_byte;
  uint8_t higher_digit = 0;
  char *orig_ps = ps;
  for (int ndigit=0; ndigit<4; ndigit=ndigit+1) {
    ip_byte = (packed_ip >> 8*(3-ndigit)) & 0xff;
    //printf("ndigit = %d, ip_byte = %d\r\n", ndigit, ip_byte);
    if (ip_byte/100 > 0) {
      higher_digit = 1;
      *(ps++) = xitoa(ip_byte/100);
      if ((ps - orig_ps) > maxlen) {
        return;
      }
      //++ps;
      ip_byte = ip_byte - 100*(ip_byte/100);
    }
    if (higher_digit || (ip_byte/10 > 0)) {
      *(ps++) = xitoa(ip_byte/10);
      if ((ps - orig_ps) > maxlen) {
        return;
      }
      //++ps;
      ip_byte = ip_byte - 10*(ip_byte/10);
    }
    *(ps++) = xitoa(ip_byte);
    if ((ps - orig_ps) > maxlen) {
      return;
    }
    //++ps;
    if (ndigit < 3) {
      *(ps++) = '.';
      if ((ps - orig_ps) > maxlen) {
        return;
      }
    }
  }
  while ((ps - orig_ps) < maxlen) {
    *(ps++) = ' ';
  }
  return;
}
*/

void display_update(void) {
  static uint32_t last_update = 0;
  static Board_Status_t status = BOARD_STATUS_GOOD;
  // Check encoder knob
  uint8_t buttons = uiBoardPoll();
  int refresh = 0;
  if (buttons & 1) {
    // Turn left; decrease page number
    //setLed(1); // red
    if (current_page == PAGE_FIRST) {
      current_page = PAGE_LAST;
    } else {
      --current_page;
    }
    display_enable();
    refresh = 1;
  } else if (buttons & 2) {
    // Turn right; increase page number
    //setLed(2); // green
    if (current_page == PAGE_LAST) {
      current_page = PAGE_FIRST;
    } else {
      ++current_page;
    }
    display_enable();
    refresh = 1;
  } else if (buttons & 4) {
    //setLed(0); // off
    display_toggle();
  }
  if (!display_enabled) {
    return;
  }
  if (do_update(last_update) || refresh) {
    Board_Status_t new_status = marble_get_status();
    if (new_status == BOARD_STATUS_GOOD) {
      update_page(refresh);
    } else {
      if (status != new_status) {
        update_display_error(1);
      }
    }
    last_update = BSP_GET_SYSTICK();
    status = new_status;
  }
  display_timeout();
  update_led();
  return;
}

static void update_page(int refresh) {
  if (refresh) {
    fill(0);
  }
  switch (current_page) {
    case PAGE_STATE:
      refresh |= update_page_state(refresh);
      break;
    case PAGE_POWER:
      refresh |= update_page_power(refresh);
      break;
#if ENABLE_FMC_CURRENT_CHECK
    case PAGE_FMC:
      refresh |= update_page_fmc(refresh);
      break;
#endif
    case PAGE_TEMPERATURE:
      refresh |= update_page_temperature(refresh);
      break;
    default:
      break;
  }
  if (refresh) {
    send_fb();
  }
  return;
}

static void update_display_error(int refresh) {
  if (refresh) {
    display_enable();
    fill(0);
  }
  static Board_Status_t last_status = BOARD_STATUS_GOOD;
  Board_Status_t status = marble_get_status();
  if (refresh) {
    lv_update_label(&label_error, "ERROR");
  }
  if (status != last_status) {
    if (status == BOARD_STATUS_GOOD) {
      lv_update_label(&label_error_state, "OK");
    } else if (status == BOARD_STATUS_OVERTEMP) {
      lv_update_label(&label_error_state, label_error_state_ot);
    } else if (status == BOARD_STATUS_POWERDOWN) {
      lv_update_label(&label_error_state, label_error_state_pd);
    }
    refresh = 1;
  }
  if (refresh) {
    send_fb();
  }
  last_status = status;
  return;
}

static int update_page_state(int refresh) {
  static uint8_t pip[4] = {0, 0, 0, 0};
  static uint8_t pmac[6] = {0, 0, 0, 0, 0, 0};
  static FPGAWD_State_t fpga_state = STATE_GOLDEN;
  int rval = 0;
  // block scope to avoid unnecessarily filling the stack
  { // Marble label
    FPGAWD_State_t current_state = FPGAWD_GetState();
    int rev = 0;
    if (refresh || (current_state != fpga_state)) {
      if (marble_get_pcb_rev() == Marble_v1_4) {
        rev = 4;
      } else {
        rev = 3;
      }
      char label[LABEL_MARBLE_SIZE];
      snprintf(label, LABEL_MARBLE_SIZE, "Marble v1.%d %s Image", rev, current_state == STATE_GOLDEN ? "Golden" : "User");
      lv_update_label(&label_marble, label);
      fpga_state = current_state;
      rval = 1;
    }
  }
  { // IP addr
    if (array_updated_uint8_t(pip, get_last_ip(), 4) || refresh) {
      char ip_string[LABEL_IP_SIZE + 1];
      format_ip_addr(pip, ip_string, LABEL_IP_SIZE);
      ip_string[LABEL_IP_SIZE] = '\0'; // null-terminate
      lv_update_label(&label_ip, ip_string);
      if (!refresh) {
        rval = 1;
      }
    }
  }
  { // MAC addr
    if (array_updated_uint8_t(pmac, get_last_mac(), 6) || refresh) {
      char mac_string[LABEL_MAC_SIZE + 1];
      format_mac_addr(pmac, mac_string, LABEL_MAC_SIZE);
      mac_string[LABEL_MAC_SIZE] = '\0'; // null-terminate
      lv_update_label(&label_mac, mac_string);
      if (!refresh) {
        rval = 1;
      }
    }
  }
  //send_window_4((unsigned)label_ip.x0, (unsigned)label_ip.y0, (unsigned)label_ip.x1, (unsigned)label_ip.y1, uint8_t *data);
  return rval; // TODO - return whether we need to update the screen (if a value has changed)
}

static int update_page_power(int refresh) {
  int rval = 0;
  static int vin=0, iin=0;
  static int v3V3=0, i3V3=0;
  static int v2V5=0, i2V5=0;
  static int v1V8=0, i1V8=0;
  static int v1V0=0, i1V0=0;
  // 12V
  int newv = PM_GetTelem(VIN);
  int newi = PM_GetTelem(IIN);
  if (refresh || (newv != vin) || (newi != iin)) {
    vin = newv;
    iin = newi;
    char label[LABEL_POWER_12V_SIZE];
    snprintf(label, LABEL_POWER_12V_SIZE, LABEL_POWER_12V_FMT, (float)(newv/1000.0), (float)(newi/1000.0));
    lv_update_label(&label_power_12V, label);
    rval = 1;
  }
  // 3V3
  newv = PM_GetTelem(VOUT_3V3);
  newi = PM_GetTelem(IOUT_3V3);
  if (refresh || (newv != v3V3) || (newi != i3V3)) {
    v3V3 = newv;
    i3V3 = newi;
    char label[LABEL_POWER_3V3_SIZE];
    snprintf(label, LABEL_POWER_3V3_SIZE, LABEL_POWER_3V3_FMT, (float)(newv/1000.0), (float)(newi/1000.0));
    lv_update_label(&label_power_3V3, label);
    rval = 1;
  }
  // 2V5
  newv = PM_GetTelem(VOUT_2V5);
  newi = PM_GetTelem(IOUT_2V5);
  if (refresh || (newv != v2V5) || (newi != i2V5)) {
    v2V5 = newv;
    i2V5 = newi;
    char label[LABEL_POWER_2V5_SIZE];
    snprintf(label, LABEL_POWER_2V5_SIZE, LABEL_POWER_2V5_FMT, (float)(newv/1000.0), (float)(newi/1000.0));
    lv_update_label(&label_power_2V5, label);
    rval = 1;
  }
  // 1V8
  newv = PM_GetTelem(VOUT_1V8);
  newi = PM_GetTelem(IOUT_1V8);
  if (refresh || (newv != v1V8) || (newi != i1V8)) {
    v1V8 = newv;
    i1V8 = newi;
    char label[LABEL_POWER_1V8_SIZE];
    snprintf(label, LABEL_POWER_1V8_SIZE, LABEL_POWER_1V8_FMT, (float)(newv/1000.0), (float)(newi/1000.0));
    lv_update_label(&label_power_1V8, label);
    rval = 1;
  }
  // 1V0
  newv = PM_GetTelem(VOUT_1V0);
  newi = PM_GetTelem(IOUT_1V0);
  if (refresh || (newv != v1V0) || (newi != i1V0)) {
    v1V0 = newv;
    i1V0 = newi;
    char label[LABEL_POWER_1V0_SIZE];
    snprintf(label, LABEL_POWER_1V0_SIZE, LABEL_POWER_1V0_FMT, (float)(newv/1000.0), (float)(newi/1000.0));
    lv_update_label(&label_power_1V0, label);
    rval = 1;
  }
#if ENABLE_FMC_CURRENT_CHECK == 0
  // FMC
  static uint8_t fmc_status = 0;
  uint8_t new_fmc_status = marble_FMC_status();
  // FMC1
  uint8_t mask = (1 << M_FMC_STATUS_FMC1_PWR);
  uint8_t new_fmc = (new_fmc_status & mask) ^ (fmc_status & mask);
  if (new_fmc_status & mask) {
    if (refresh || new_fmc) {
      lv_update_label(&label_fmc_1, label_fmc_1_enabled);
      rval = 1;
    }
  } else {
    if (new_fmc_status != fmc_status) {
      lv_update_label(&label_fmc_1, label_fmc_1_disabled);
      rval = 1;
    }
  }
  // FMC2
  mask = (1 << M_FMC_STATUS_FMC2_PWR);
  new_fmc = (new_fmc_status & mask) ^ (fmc_status & mask);
  if (new_fmc_status & mask) {
    if (refresh || new_fmc) {
      lv_update_label(&label_fmc_2, label_fmc_2_enabled);
      rval = 1;
    }
  } else {
    if (new_fmc_status != fmc_status) {
      lv_update_label(&label_fmc_2, label_fmc_2_disabled);
      rval = 1;
    }
  }
  fmc_status = new_fmc_status;
#endif
  return rval;
}

#if ENABLE_FMC_CURRENT_CHECK
static int update_page_fmc(int refresh) {
  int rval = 0;
  // TODO - Use external load on FMC1/2 to check calibration of INA219_SHUNT_VOLTAGE_TO_CURRENT()
  static uint8_t fmc_status = 0;
  static uint16_t fmc1_current = 0;
  static uint16_t fmc2_current = 0;
  uint8_t new_fmc_status = marble_FMC_status();
  // FMC1
  uint8_t mask = (1 << M_FMC_STATUS_FMC1_PWR);
  uint8_t new_fmc = (new_fmc_status & mask) ^ (fmc_status & mask);
  uint16_t new_fmc_current=0;
  new_fmc_current = ina219_getShuntVoltage(INA219_FMC1);
  if (new_fmc_status & mask) {
    if (refresh || new_fmc || (new_fmc_current != fmc1_current)) {
      float fmc1_current_amps = INA219_SHUNT_VOLTAGE_TO_CURRENT(new_fmc_current);
      char label[LABEL_FMC_1_SIZE];
      snprintf(label, LABEL_FMC_1_SIZE, "FMC1: Enabled @ %4.2fA", fmc1_current_amps);
      lv_update_label(&label_fmc_1, label);
      fmc1_current = new_fmc_current;
      rval = 1;
    }
  } else {
    if (new_fmc_status != fmc_status) {
      lv_update_label(&label_fmc_1, "FMC1: Disabled");
      rval = 1;
    }
  }
  // FMC2
  mask = (1 << M_FMC_STATUS_FMC2_PWR);
  new_fmc = (new_fmc_status & mask) ^ (fmc_status & mask);
  new_fmc_current = ina219_getShuntVoltage(INA219_FMC2);
  if (new_fmc_status & mask) {
    if (refresh || new_fmc || (new_fmc_current != fmc2_current)) {
      float fmc2_current_amps = INA219_SHUNT_VOLTAGE_TO_CURRENT(new_fmc_current);
      char label[LABEL_FMC_2_SIZE];
      snprintf(label, LABEL_FMC_2_SIZE, "FMC2: Enabled @ %4.2fA", fmc2_current_amps);
      lv_update_label(&label_fmc_2, label);
      fmc2_current = new_fmc_current;
      rval = 1;
    }
  } else {
    if (new_fmc_status != fmc_status) {
      lv_update_label(&label_fmc_2, "FMC2: Disabled");
      rval = 1;
    }
  }
  fmc_status = new_fmc_status;
  return rval;
}
#endif

static int update_page_temperature(int refresh) {
  int rval = 0;
  static int lm75_0_temp=0, lm75_1_temp=0;
  static int max6639_ch1[2] = {0, 0};
  static int max6639_ch2[2] = {0, 0};
  if (refresh) {
    lv_update_label(&label_temperature_lm75_0_label, label_temperature_lm75_0_label_init);
    lv_update_label(&label_temperature_lm75_1_label, label_temperature_lm75_1_label_init);
    lv_update_label(&label_temperature_max6639_1_label, label_temperature_max6639_1_label_init);
    lv_update_label(&label_temperature_max6639_2_label, label_temperature_max6639_2_label_init);
    rval = 1;
  }
  // LM75_0
  int new_lm75_temp = LM75_get_cached_temperature(LM75_0);
  if (refresh || (new_lm75_temp != lm75_0_temp)) {
    char label[LABEL_TEMPERATURE_LM75_0_SIZE];
    snprintf(label, LABEL_TEMPERATURE_LM75_0_SIZE, LABEL_TEMPERATURE_LM75_0_FMT, ((float)new_lm75_temp)/2);
    lv_update_label(&label_temperature_lm75_0, label);
    lm75_0_temp = new_lm75_temp;
    rval = 1;
  }
  // LM75_1
  new_lm75_temp = LM75_get_cached_temperature(LM75_1);
  if (refresh || (new_lm75_temp != lm75_1_temp)) {
    char label[LABEL_TEMPERATURE_LM75_1_SIZE];
    snprintf(label, LABEL_TEMPERATURE_LM75_1_SIZE, LABEL_TEMPERATURE_LM75_1_FMT, ((float)new_lm75_temp)/2);
    lv_update_label(&label_temperature_lm75_1, label);
    lm75_1_temp = new_lm75_temp;
    rval = 1;
  }
  // MAX6639 Ch 1
  int new_max6639[2];
  new_max6639[0] = max6639_get_cached_temp(MAX6639_TEMP_CH1);
  new_max6639[1] = max6639_get_cached_temp(MAX6639_TEMP_EXT_CH1);
  if (array_updated_int(max6639_ch1, new_max6639, 2) || refresh) {
    char label[LABEL_TEMPERATURE_MAX6639_1_SIZE];
    snprintf(label, LABEL_TEMPERATURE_MAX6639_1_SIZE, LABEL_TEMPERATURE_MAX6639_1_FMT, MAX6639_GET_TEMP_DOUBLE(new_max6639[0], new_max6639[1]));
    lv_update_label(&label_temperature_max6639_1, label);
    rval = 1;
  }
  // MAX6639 Ch 2
  new_max6639[0] = max6639_get_cached_temp(MAX6639_TEMP_CH2);
  new_max6639[1] = max6639_get_cached_temp(MAX6639_TEMP_EXT_CH2);
  if (array_updated_int(max6639_ch2, new_max6639, 2) || refresh) {
    char label[LABEL_TEMPERATURE_MAX6639_2_SIZE];
    snprintf(label, LABEL_TEMPERATURE_MAX6639_2_SIZE, LABEL_TEMPERATURE_MAX6639_2_FMT, MAX6639_GET_TEMP_DOUBLE(new_max6639[0], new_max6639[1]));
    lv_update_label(&label_temperature_max6639_2, label);
    rval = 1;
  }
  return rval;
}

void display_init(void) {
  // For lack of a better place, let's put this here for now
  printf("Initializing UI board\r\n");
  uiBoardInit();
  set_inverted(0);

  fill(0);
  /*
  // TEST!
  lv_init_label(&label_test_12, 0, 0, &lv_font_roboto_12, "%", LV_LEFT, false);
  lv_init_label(&label_test_17, 0, 0, &lv_font_roboto_mono_17, "@", LV_LEFT, false);
  printf("12pt: dx = %d; dy = %d\r\n", label_test_12.x1-label_test_12.x0, label_test_12.y1-label_test_12.y0);
  printf("17pt: dx = %d; dy = %d\r\n", label_test_17.x1-label_test_17.x0, label_test_17.y1-label_test_17.y0);
  */

  init_display_error();
  init_page_state();
  init_page_power();
#if ENABLE_FMC_CURRENT_CHECK
  init_page_fmc();
#endif
  init_page_temperature();
  update_page(1); // force refresh
  send_fb();
  last_touch = BSP_GET_SYSTICK();
  return;
}

static void init_display_error(void) {
  lv_init_label(&label_error,       DISPLAY_WIDTH/2, LINE_SPACING_17/2, &lv_font_roboto_mono_17, label_error_init, LV_CENTER, false);
  if (LABEL_ERROR_STATE_OT_SIZE > LABEL_ERROR_STATE_PD_SIZE) {
    lv_init_label(&label_error_state, DISPLAY_WIDTH/2, 1*LINE_SPACING_17 + LINE_SPACING_17/2, &lv_font_roboto_mono_17, label_error_state_ot, LV_CENTER, false);
  } else {
    lv_init_label(&label_error_state, DISPLAY_WIDTH/2, 1*LINE_SPACING_17 + LINE_SPACING_17/2, &lv_font_roboto_mono_17, label_error_state_pd, LV_CENTER, false);
  }
  return;
}

static void init_page_state(void) {
  // PAGE_STATE
  lv_init_label(&label_marble, 0, 0*LINE_SPACING, &DEFAULT_FONT, label_marble_init, LV_LEFT, false);
  lv_init_label(&label_ip,     0, 1*LINE_SPACING, &DEFAULT_FONT, label_ip_init,     LV_LEFT, false);
  lv_init_label(&label_mac,    0, 2*LINE_SPACING, &DEFAULT_FONT, label_mac_init,    LV_LEFT, false);
  return;
}

static void init_page_power(void) {
  // PAGE_POWER
  lv_init_label(&label_power_12V,            0, 0*LINE_SPACING_12, &lv_font_roboto_12, label_power_12V_init, LV_LEFT, false);
  lv_init_label(&label_power_3V3,            0, 1*LINE_SPACING_12, &lv_font_roboto_12, label_power_3V3_init, LV_LEFT, false);
  lv_init_label(&label_power_1V8,            0, 2*LINE_SPACING_12, &lv_font_roboto_12, label_power_1V8_init, LV_LEFT, false);
#if ENABLE_FMC_CURRENT_CHECK == 0
  lv_init_label(&label_fmc_1,                0, 3*LINE_SPACING_12, &lv_font_roboto_12, label_fmc_1_init, LV_LEFT, false);
  int max_width = MAX(MAX(label_power_3V3.x1, label_power_1V8.x1), label_fmc_1.x1);
#else
  int max_width = MAX(label_power_3V3.x1, label_power_1V8.x1);
#endif
  lv_init_label(&label_power_2V5, max_width+12, 1*LINE_SPACING_12, &lv_font_roboto_12, label_power_2V5_init, LV_LEFT, false);
  lv_init_label(&label_power_1V0, max_width+12, 2*LINE_SPACING_12, &lv_font_roboto_12, label_power_1V0_init, LV_LEFT, false);
#if ENABLE_FMC_CURRENT_CHECK == 0
  lv_init_label(&label_fmc_2,     max_width+12, 3*LINE_SPACING_12, &lv_font_roboto_12, label_fmc_2_init, LV_LEFT, false);
#endif
  return;
}

#if ENABLE_FMC_CURRENT_CHECK
static void init_page_fmc(void) {
  // PAGE_FMC
  lv_init_label(&label_fmc_1, 0, 0*LINE_SPACING, &DEFAULT_FONT, label_fmc_1_init, LV_LEFT, false);
  lv_init_label(&label_fmc_2, 0, 1*LINE_SPACING, &DEFAULT_FONT, label_fmc_2_init, LV_LEFT, false);
  return;
}
#endif

static void init_page_temperature(void) {
  // PAGE_TEMPERATURE
  lv_init_label(&label_temperature_lm75_0_label,    0, 0*LINE_SPACING_12, &lv_font_roboto_12, label_temperature_lm75_0_label_init,    LV_LEFT, false);
  lv_init_label(&label_temperature_lm75_1_label,    0, 1*LINE_SPACING_12, &lv_font_roboto_12, label_temperature_lm75_1_label_init,    LV_LEFT, false);
  lv_init_label(&label_temperature_max6639_1_label, 0, 2*LINE_SPACING_12, &lv_font_roboto_12, label_temperature_max6639_1_label_init, LV_LEFT, false);
  lv_init_label(&label_temperature_max6639_2_label, 0, 3*LINE_SPACING_12, &lv_font_roboto_12, label_temperature_max6639_2_label_init, LV_LEFT, false);
  int xmax = MAX(MAX(label_temperature_lm75_0_label.x1, label_temperature_lm75_1_label.x1),
                 MAX(label_temperature_max6639_1_label.x1, label_temperature_max6639_2_label.x1));
  lv_init_label(&label_temperature_lm75_0,     xmax+4, 0*LINE_SPACING_12, &lv_font_roboto_12, label_temperature_lm75_0_init,    LV_LEFT, false);
  lv_init_label(&label_temperature_lm75_1,     xmax+4, 1*LINE_SPACING_12, &lv_font_roboto_12, label_temperature_lm75_1_init,    LV_LEFT, false);
  lv_init_label(&label_temperature_max6639_1,  xmax+4, 2*LINE_SPACING_12, &lv_font_roboto_12, label_temperature_max6639_1_init, LV_LEFT, false);
  lv_init_label(&label_temperature_max6639_2,  xmax+4, 3*LINE_SPACING_12, &lv_font_roboto_12, label_temperature_max6639_2_init, LV_LEFT, false);
  return;
}

static void display_enable(void) {
  // Set the display to the same brightness value used in the ui_board.c driver
  if (display_enabled) {
    return;
  }
  display_enabled = 1;
  last_touch = BSP_GET_SYSTICK();
  set_brightness(9);
  return;
}

static void display_disable(void) {
  if (!display_enabled) {
    return;
  }
  display_enabled = 0;
  set_brightness(0);
  return;
}

static void display_toggle(void) {
  if (display_enabled) {
    display_disable();
  } else {
    display_enable();
  }
  return;
}

static void display_timeout(void) {
  uint32_t now = BSP_GET_SYSTICK();
  if (compare_systick(last_touch, now, DISPLAY_TIMEOUT_MS)) {
    display_disable();
  }
  return;
}

static int do_update(uint32_t last_update) {
  uint32_t now = BSP_GET_SYSTICK();
  return compare_systick(last_update, now, UPDATE_INTERVAL_MS);
}

static int compare_systick(uint32_t old, uint32_t new, uint32_t threshold) {
  if (new < old) {
    // Handle oddball rollover case
    if ((new + (0xffffffff-old)) >= threshold) {
      return 1;
    }
  } else if ((new - old) >= threshold) {
    return 1;
  }
  return 0;
}

static void format_ip_addr(uint8_t *ip, char *ps, int maxlen) {
  snprintf(ps, (size_t)maxlen, "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  return;
}

static void format_mac_addr(uint8_t *mac, char *ps, int maxlen) {
  snprintf(ps, (size_t)maxlen, "MAC: %x:%x:%x:%x:%x:%x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return;
}

static int array_updated_uint8_t(volatile uint8_t *old, const uint8_t *new, int len) {
  //printf("array_updated?\r\n");
  int diff = 0;
  for (int n=0; n<len; n++) {
    if (*(old+n) != *(new+n)) {
      //printf("  Diff! %x != %x\r\n", *(old+n), *(new+n));
      diff = 1;
      break;
    }
  }
  if (diff) {
    memcpy((void *)old, (void *)new, (size_t)(len/sizeof(uint8_t)));
    //printf("  after memcpy: ");
    //PRINT_MULTIBYTE_DEC(old, 4, '.');
  } else {
    //printf("  Identical\r\n");
  }
  return diff;
}

static int array_updated_int(volatile int *old, const int *new, int len) {
  int diff = 0;
  for (int n=0; n<len; n++) {
    if (*(old+n) != *(new+n)) {
      diff = 1;
      break;
    }
  }
  if (diff) {
    memcpy((void *)old, (void *)new, (size_t)(len/sizeof(uint8_t)));
  } else {
    //printf("  Identical\r\n");
  }
  return diff;
}

#define ERROR_LED_TIME_ON_MS     (500)
#define ERROR_LED_TIME_OFF_MS   (1000)
static void update_led(void) {
  static uint32_t blink_time = 0;
  uint32_t now = BSP_GET_SYSTICK();
  Board_Status_t status = marble_get_status();
  if (status == BOARD_STATUS_GOOD) {
    setLed(0); // off
    return;
  }
  if ((now-blink_time) < ERROR_LED_TIME_ON_MS) {
    setLed(1); // red
  } else if ((now-blink_time) < ERROR_LED_TIME_OFF_MS) {
    setLed(0); // off
  } else {
    blink_time = now;
  }
  return;
}
