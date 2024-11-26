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

// ======================= Policy ===========================
#define DISPLAY_TIMEOUT_MS      (10*60*1000)
#define UPDATE_INTERVAL_MS            (1000)
#define USE_FONT_17
// TODO - Warning! The lv_font_roboto_12 font is not monospace, so ensuring any
//        future value fits inside the initial bounding box is somewhat tedious.
// ==========================================================

extern lv_font_t lv_font_roboto_12, lv_font_roboto_mono_17, lv_font_fa;

typedef enum {
  PAGE_STATE=0,
  PAGE_POWER,
  PAGE_FMC,
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

// ===================== Page STATE =========================
// Marble v1.4
static t_label label_marble;

// IP: 192.168.100.101
static t_label label_ip;

// MAC: aa:bb:cc:dd:ee:ff
static t_label label_mac;

//User Image
//Golden Image
//Over-Temperature
//Powerdown
//                                      "Marble v1.4 (Over-Temperature)"
static const char label_marble_init[] = "Marble v1.                    ";
#define LABEL_MARBLE_INDEX_REVISION           (10)
#define LABEL_MARBLE_INDEX_STATE              (LABEL_MARBLE_INDEX_REVISION+2)

//                                  "IP: 192.168.100.101"
static const char label_ip_init[] = "IP:                ";
#define LABEL_IP_SIZE     (sizeof(label_ip_init)/sizeof(char))

//                                   "MAC: aa:bb:cc:dd:ee:ff"
static const char label_mac_init[] = "MAC:                  ";
#define LABEL_MAC_SIZE     (sizeof(label_mac_init)/sizeof(char))

// ==========================================================

// ===================== Page POWER =========================
static t_label label_temp;
static const char label_temp_init[] = "TEMP:              ";
#define LABEL_TEMP_SIZE     (sizeof(label_temp_init)/sizeof(char))

static t_label label_power_12V;
//                                         "Input (12V): 12.0V @ 3.00A"
static const char label_power_12V_init[] = "Input (12V): 12.0V @ 3.00A";
#define LABEL_POWER_12V_SIZE     (sizeof(label_power_12V_init)/sizeof(char))

static t_label label_power_3V3;
//                                         "3V3: 3.30V @ 1.00A"
static const char label_power_3V3_init[] = "3V3: 3.30V @ 1.00A";
#define LABEL_POWER_3V3_SIZE     (sizeof(label_power_3V3_init)/sizeof(char))

static t_label label_power_2V5;
//                                         "2V5: 2.50V @ 1.00A"
static const char label_power_2V5_init[] = "2V5: 2.50V @ 1.00A";
#define LABEL_POWER_2V5_SIZE     (sizeof(label_power_2V5_init)/sizeof(char))

static t_label label_power_1V8;
//                                         "1V8: 1.80V @ 1.00A"
static const char label_power_1V8_init[] = "1V8: 1.80V @ 1.00A";
#define LABEL_POWER_1V8_SIZE     (sizeof(label_power_1V8_init)/sizeof(char))

static t_label label_power_1V0;
//                                         "1V0: 1.00V @ 1.00A"
static const char label_power_1V0_init[] = "1V0: 1.00V @ 1.00A";
#define LABEL_POWER_1V0_SIZE     (sizeof(label_power_1V0_init)/sizeof(char))

// ==========================================================

// ======================= Page FMC =========================
// ==========================================================

// =================== Page TEMPERATURE =====================
// ==========================================================

//static void xformat_ip_addr(uint32_t packed_ip, char *ps, int maxlen);
static void update_page(int refresh);
static void init_page_state(void);
static void init_page_power(void);
static void init_page_fmc(void);
static void init_page_temperature(void);
static int update_page_state(int refresh);
static int update_page_power(int refresh);
static int update_page_fmc(int refresh);
static int update_page_temperature(int refresh);
static void display_enable(void);
static void display_disable(void);
static void display_toggle(void);
static void display_timeout(void);
static int compare_systick(uint32_t old, uint32_t new, uint32_t threshold);
static int do_update(uint32_t last_update);
static void format_ip_addr(uint8_t *ip, char *ps, int maxlen);
static void format_mac_addr(uint8_t *mac, char *ps, int maxlen);
static int array_updated(volatile uint8_t *old, const uint8_t *new, int len);

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
    update_page(refresh);
    last_update = BSP_GET_SYSTICK();
  }
  display_timeout();
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
    case PAGE_FMC:
      refresh |= update_page_fmc(refresh);
      break;
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

static int update_page_state(int refresh) {
  static uint8_t pip[4] = {0, 0, 0, 0};
  static uint8_t pmac[6] = {0, 0, 0, 0, 0, 0};
  int rval = 0;
  // block scope to avoid unnecessarily filling the stack
  { // Marble label
    // TODO - Get marble PCB revision
    // TODO - Get state
    if (refresh) {
      lv_update_label(&label_marble, "Marble v1.4");
    }
  }
  { // IP addr
    // TODO - replace these eeprom reads with simple RAM reads
    //eeprom_read_ip_addr((volatile uint8_t *)pip, 4);
    if (array_updated(pip, get_last_ip(), 4) || refresh) {
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
    // TODO - replace these eeprom reads with simple RAM reads
    //eeprom_read_mac_addr((volatile uint8_t *)pmac, 6);
    if (array_updated(pmac, get_last_mac(), 6) || refresh) {
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
  //lv_update_label(&label_temp, "PAGE_POWER");
  int rval = 0;
  lv_update_label(&label_power_12V, "Input (12V): 12.0x @ 3.00y"); // TODO
  lv_update_label(&label_power_3V3, "3V3: 3.00x @ 1.00y"); // TODO
  lv_update_label(&label_power_2V5, "2V5: 2.50x @ 1.00y"); // TODO
  lv_update_label(&label_power_1V8, "1V8: 1.80x @ 1.00y"); // TODO
  lv_update_label(&label_power_1V0, "1V0: 1.00x @ 1.00y"); // TODO
  return rval;
}

static int update_page_fmc(int refresh) {
  int rval = 0;
  lv_update_label(&label_temp, "PAGE_FMC");
  return rval;
}

static int update_page_temperature(int refresh) {
  int rval = 0;
  lv_update_label(&label_temp, "PAGE_TEMPERATURE");
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

  init_page_state();
  init_page_power();
  init_page_fmc();
  init_page_temperature();
  update_page(1); // force refresh
  send_fb();
  last_touch = BSP_GET_SYSTICK();
  return;
}

static void init_page_state(void) {
  // PAGE_STATE (draw)
  lv_init_label(&label_marble, 0, 0*LINE_SPACING, &DEFAULT_FONT, label_marble_init, LV_LEFT, false);
  lv_init_label(&label_ip,     0, 1*LINE_SPACING, &DEFAULT_FONT, label_ip_init,     LV_LEFT, false);
  lv_init_label(&label_mac,    0, 2*LINE_SPACING, &DEFAULT_FONT, label_mac_init,    LV_LEFT, false);
  return;
}

static void init_page_power(void) {
  // PAGE_POWER
  lv_init_label(&label_temp,  40,  0*LINE_SPACING, &DEFAULT_FONT, label_temp_init,   LV_LEFT, false);
  return;
}

static void init_page_fmc(void) {
  // PAGE_FMC
  lv_init_label(&label_power_12V,           0,  0*LINE_SPACING, &lv_font_roboto_12, label_power_12V_init, LV_LEFT, false);
  lv_init_label(&label_power_3V3,           0,  1*LINE_SPACING, &lv_font_roboto_12, label_power_3V3_init, LV_LEFT, false);
  lv_init_label(&label_power_1V8,           0,  2*LINE_SPACING, &lv_font_roboto_12, label_power_1V8_init, LV_LEFT, false);
  int max_width = MAX(label_power_3V3.x1, label_power_1V8.x1);
  lv_init_label(&label_power_2V5, max_width+12,  1*LINE_SPACING, &lv_font_roboto_12, label_power_2V5_init, LV_LEFT, false);
  lv_init_label(&label_power_1V0, max_width+12,  2*LINE_SPACING, &lv_font_roboto_12, label_power_1V0_init, LV_LEFT, false);
  return;
}

static void init_page_temperature(void) {
  // PAGE_TEMPERATURE
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

static int array_updated(volatile uint8_t *old, const uint8_t *new, int len) {
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
