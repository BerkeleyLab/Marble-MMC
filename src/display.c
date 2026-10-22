// Encapsulation of the routines pushing content to the display board

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "display.h"
#include "font.h"
#include "ui_board.h"
#include "ui_board_spi.h"
#include "frame_buffer.h"
#include "ssd1322.h"
#include "gui.h"
#include "eeprom.h"
#include "marble_api.h"
#include "marble_errors.h"
#include "console.h"
#include "i2c_fpga.h"
#include "i2c_pm.h"
#include "max6639.h"
#include "watchdog.h"
#include "rev.h"

// ======================= Policy ===========================
#define DISPLAY_TIMEOUT_MS      (10*60*1000)
#define UPDATE_INTERVAL_MS            (1)
#define UPDATE_BUFFER_INTERVAL_MS     (DISPLAY_HEIGHT)
#define USE_FONT_17
// TODO - Warning! The lv_font_roboto_12 font is not monospace, so ensuring any
//        future value fits inside the initial bounding box is somewhat tedious.
// Set to 1 to enable current monitoring of FMC cards.
// This violates the "don't spam the I2C bus" policy of the MMC, so it's disabled
// by default and may be removed completely.
#define ENABLE_FMC_CURRENT_CHECK         (0)
#define PAGE_INFO_ITEMS           (8) // Number of items on the INFO page
// ==========================================================

extern const font_header_t f_roboto, f_roboto_mono, f_font_awesome_5_free;
// Legacy compatibility
#define lv_font_roboto_12 f_roboto
#define lv_font_roboto_mono_17 f_roboto_mono
#define lv_font_fa f_font_awesome_5_free;

typedef enum {
  MENU=0,
  STATUS,
  INFO,
  POWER,
  TEMPERATURE,
  ERRORS,
  CLEAR_ERRORS,
  CONFIG_WARNING,
  SET_IP,
  NUMBER_OF_PAGES, // Keep me at the end
} display_page_t;

static const char *MenuItems[NUMBER_OF_PAGES] = { // triggers compile warning if not all enum values are covered
  "Main Menu",
	"Status",
  "Info",
	"Power",
	"Temperature",
	"Error logs",
  "Clear Error Light",
	"Set IP Address",
};

static display_page_t menu(unsigned events);
static display_page_t page_status(unsigned events);
static display_page_t page_info(unsigned events);
static display_page_t page_power(unsigned events);
static display_page_t page_temperature(unsigned events);
static display_page_t page_errors(unsigned events);
static display_page_t page_clear_errors(unsigned events);
static display_page_t config_warning(unsigned events);
static display_page_t set_IP(unsigned events);
// static int update_page_power(int refresh);

// static void scrollbar(int16_t percentage);
static void window_scrollbar(int16_t pos, int16_t total);
static void errorLight(unsigned frm);
static int16_t smooth_slide(int16_t title_y, int16_t title_y_goal);  // smooth title movement

// These will remain correct even if the page order changes in the enum above
#define PAGE_FIRST        (0)
#define PAGE_LAST         (NUMBER_OF_PAGES-1)

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
uint32_t error_counter = 0;
static uint32_t error_reset_time = 0;

static t_label menu_item;
static t_label label_warning_1;
static t_label label_warning_2;
static t_label label_warning_cancel;
static t_label label_warning_proceed;
static t_label IP_item;
static t_label error_light;
static t_label label_title;

// =================== ERROR Overlay ========================
// NOTE! The error overlay only works if the UI board is supplied with an alternate
// source of 3.3V, as the +3V3 net it connects to via the Pmod is disabled when the
// board state is one of these shutdown conditions
static t_label label_error;
// static const char label_error_init[] = "ERROR";
#define LABEL_ERROR_SIZE     (sizeof(label_error_init)/sizeof(char))

static t_label label_error_state;
static const char label_error_state_ot[] = "Over-Temperature Shutdown";
#define LABEL_ERROR_STATE_OT_SIZE     (sizeof(label_error_state_ot)/sizeof(char))
static const char label_error_state_pd[] = "Power Lost";
#define LABEL_ERROR_STATE_PD_SIZE     (sizeof(label_error_state_pd)/sizeof(char))


// ===================== Page STATE =========================
// Marble v1.4
static t_label label_image;
static t_label label_uptime;
static t_label label_marble_rev;
static t_label label_ip;
static t_label label_mac;
static t_label label_firmware;
static t_label label_SN;
static t_label label_MMC_chip_id;
static t_label label_PHY_id;
static t_label label_boot_id;

static const char label_marble_init[] = "Marble v1.X.X";
#define LABEL_MARBLE_REV_SIZE     (sizeof(label_marble_init)/sizeof(char))
// static const char label_image_init[] = "xxxxxx image";
#define LABEL_IMAGE_SIZE     (sizeof(label_marble_init)/sizeof(char))

static const char label_ip_init[] = "IP: xxx.xxx.xxx.xxx";
#define LABEL_IP_SIZE     (sizeof(label_ip_init)/sizeof(char))

static const char label_mac_init[] = "MAC: xx:xx:xx:xx:xx:xx";
#define LABEL_MAC_SIZE     (sizeof(label_mac_init)/sizeof(char))


// ==========================================================

// ===================== Page POWER =========================

static t_label label_3v3;
static t_label label_1v8;
static t_label label_1v0;


#define LABEL_POWER_12V_FMT                "Input (12V): %4.2fV @ %3.2fA"
static const char label_power_12V_init[] = "Input (12V): 12.00V @ 3.00A";
#define LABEL_POWER_12V_SIZE     (sizeof(label_power_12V_init)/sizeof(char))

#define LABEL_POWER_3V3_FMT                "3V3: %3.2fV @ %3.2fA"
#define LABEL_POWER_3V3_SIZE     (sizeof(label_power_3V3_init)/sizeof(char))

#define LABEL_POWER_2V5_FMT                "2V5: %3.2fV @ %3.2fA"
#define LABEL_POWER_2V5_SIZE     (sizeof(label_power_2V5_init)/sizeof(char))

#define LABEL_POWER_1V8_FMT                "1V8: %3.2fV @ %3.2fA"
#define LABEL_POWER_1V8_SIZE     (sizeof(label_power_1V8_init)/sizeof(char))

#define LABEL_POWER_1V0_FMT                "1V0: %3.2fV @ %3.2fA"
#define LABEL_POWER_1V0_SIZE     (sizeof(label_power_1V0_init)/sizeof(char))

// ==========================================================
//Note: The 'degree' symbol in lv_font_roboto_12 uses encoding (U+B0)
//      which is the result after UTF decoding.  The expected UTF-8 multi-byte
//      encoding for this value is \xC2\xB0.
//      I use octal \302\260 instead of hex \xC2\xB0 because of the char 'C' that
//      immediately follows.
//      And the "init" label contains a '@@' in its place to add extra space
//      and ensure that the non-monospace font gives us enough space for any
//      value in the numeric portion.

static t_label label_temperature_lm75_0_label;
static t_label label_temperature_lm75_0;
#define LABEL_TEMPERATURE_LM75_0_FMT                "%4.1f \302\260C"
static const char label_temperature_lm75_0_init[] = "25.3 @@C";
#define LABEL_TEMPERATURE_LM75_0_SIZE (sizeof(label_temperature_lm75_0_init)/sizeof(char))

static t_label label_temperature_lm75_1_label;
static t_label label_temperature_lm75_1;
#define LABEL_TEMPERATURE_LM75_1_FMT                "%4.1f \302\260C"
static const char label_temperature_lm75_1_init[] = "25.3 @@C";
#define LABEL_TEMPERATURE_LM75_1_SIZE (sizeof(label_temperature_lm75_1_init)/sizeof(char))

static t_label label_fan_max6639_1;
static t_label label_fan_max6639_2;

static t_label label_temperature_max6639_1_label;
static t_label label_temperature_max6639_1;
#define LABEL_TEMPERATURE_MAX6639_1_FMT                "%4.1f \302\260C"
static const char label_temperature_max6639_1_init[] = "25.3 @@C";
#define LABEL_TEMPERATURE_MAX6639_1_SIZE (sizeof(label_temperature_max6639_1_init)/sizeof(char))

static t_label label_temperature_max6639_2_label;
static t_label label_temperature_max6639_2;
#define LABEL_TEMPERATURE_MAX6639_2_FMT                "%4.1f \302\260C"
static const char label_temperature_max6639_2_init[] = "25.3 @@C";
#define LABEL_TEMPERATURE_MAX6639_2_SIZE (sizeof(label_temperature_max6639_2_init)/sizeof(char))

// ==========================================================

static void init_display_error(void);
static void update_display_error(int refresh);
#if ENABLE_FMC_CURRENT_CHECK
static void init_page_fmc(void);
static int update_page_fmc(int refresh);
#endif
static void display_enable(void);
static void display_disable(void);
static void display_timeout(void);
static int compare_systick(uint32_t old, uint32_t new, uint32_t threshold);
static int do_update(uint32_t last_update);
static int do_update_buffer(uint32_t last_buffer_update);
static void format_ip_addr(uint8_t *ip, char *ps, int maxlen);
static void format_mac_addr(uint8_t *mac, char *ps, int maxlen);
static void update_led(void);

void display_update(void) {
	static display_page_t current_page = MENU;
  display_enable();
  static uint32_t last_update = 0;
  static uint32_t last_buffer_update = 0;
  static Board_Status_t status = BOARD_STATUS_GOOD;
  Board_Status_t new_status = marble_get_status();
  // int refresh = 0;

  // Check encoder knob
  if (do_update_buffer(last_buffer_update)) {// events ||
      unsigned events = get_event_flags();
    if (new_status == BOARD_STATUS_GOOD) {
      if (display_enabled) {
        // update_page(refresh);
        switch(current_page) {
          case MENU:
            current_page = menu(events);
            break;
          case STATUS:
            current_page = page_status(events); // FIXME
            break;
          case INFO:
            current_page = page_info(events); // FIXME
            break;
          case POWER:
            current_page = page_power(events);
            break;
          case TEMPERATURE:
            current_page = page_temperature(events);
            break;
          case ERRORS:
            current_page = page_errors(events);
            break;
          case CLEAR_ERRORS:
            current_page = page_clear_errors(events);
            break;
          case CONFIG_WARNING:
            current_page = config_warning(events);
            break;
          // // case CONFIG:
          // //   current_page = config(btns);
          // //   break;
          case SET_IP:
            current_page = set_IP(events);
            break;
          default:
            current_page = MENU;
            break;
        }
        // send_fb();
      }
    } else {
      display_enable();
      if (status != new_status) {
        update_display_error(1);
      }
    }
    status = new_status;
    last_buffer_update = BSP_GET_SYSTICK();
  }
  if (do_update(last_update)) {
    last_update = BSP_GET_SYSTICK();
    ui_board_poll(true);  // start a new framebuffer transmission. One row per call.
  }
  if (display_enabled) {
    display_timeout();
    update_led();
  }
  return;
}

static void update_display_error(int refresh) {
  if (refresh) {
    fill(0);
  }
  static Board_Status_t last_status = BOARD_STATUS_GOOD;
  Board_Status_t status = marble_get_status();
  if (refresh) {
    lv_update_label(&label_error, "ERROR");
  }
  if (refresh || (status != last_status)) {
    if (status == BOARD_STATUS_GOOD) {
      lv_update_label(&label_error_state, "OK");
    } else if (status == BOARD_STATUS_OVERTEMP) {
      lv_update_label(&label_error_state, label_error_state_ot);
    } else if (status == BOARD_STATUS_POWERDOWN) {
      lv_update_label(&label_error_state, label_error_state_pd);
    }
    refresh = 1;
  }
  last_status = status;
  return;
}

static display_page_t menu(unsigned events){//int refresh
	static unsigned frm = 0;
	static int16_t cursor_y_goal = 23;
	static int16_t text_cursor = DISPLAY_HEIGHT;
	static int16_t selection_id = 0;
	// static uint8_t blink = 0;

	fill(0);
	if (events & EV_ROT_CCW) {  // left
		selection_id--;
	} else if (events & EV_ROT_CW) {  // right
		selection_id++;
	}
	
	cursor_y_goal = 43-(20*selection_id);
	if(text_cursor == (text_cursor+cursor_y_goal)/2) {
		text_cursor = cursor_y_goal;
		if(selection_id >= NUMBER_OF_PAGES+1) // bounce back at the end
			selection_id = NUMBER_OF_PAGES;
		if(selection_id <= 0) // bounce back at the end
			selection_id = STATUS;
		cursor_y_goal = 43-(20*selection_id);
	}
	else {
		text_cursor=(text_cursor+cursor_y_goal)/2;
	}

	for(display_page_t p=STATUS; p<NUMBER_OF_PAGES; p++) {
     lv_init_label(&menu_item, DISPLAY_WIDTH/2, text_cursor + 20*(p-1), &lv_font_roboto_mono_17, MenuItems[p], LV_CENTER, true);
	}

	if(text_cursor == cursor_y_goal && selection_id >=STATUS && selection_id <= NUMBER_OF_PAGES) {
    invertRoundedRect(0, LINE_SPACING_17+2, DISPLAY_WIDTH, 2*LINE_SPACING_17+1, (LINE_SPACING_17-2)/2); // white background for title
		if (events & EV_ENC_S)  // push
     return selection_id;
	}
	else {
      emptyRoundedRect(0, LINE_SPACING_17+2, DISPLAY_WIDTH, 2*LINE_SPACING_17+1, (LINE_SPACING_17-2)/2,1); // white background for title
	}
  // if (!refresh) {
  //   rval = 1;
  // }
	errorLight(frm);
	frm++;
	return MENU; // stay in menu
}



static display_page_t page_status(unsigned events) {
  static unsigned frm = 0;
  static int16_t title_y_goal = 0;
  static int16_t title_y = 23;
  static int16_t text_cursor = -DISPLAY_HEIGHT;
  static int16_t cursor_y_goal = LINE_SPACING_17;
  static int lm75_0_temp=0;
  static int max6639_ch1[2] = {0, 0};
  fill(0);

  if(events & EV_ENC_S) { // push button to go back to menu
    title_y_goal = 23;
    text_cursor = -DISPLAY_HEIGHT;
  }

  title_y = smooth_slide(title_y, title_y_goal);

  // update only when title in position
  if(title_y == 0) {

    if (frm%64 == 0) { // update every 64 frames
      LM75_get_temperature(LM75_0);
      LM75_get_temperature(LM75_1);
      return_max6639_reg(MAX6639_TEMP_CH1);
      return_max6639_reg(MAX6639_TEMP_EXT_CH1);
      return_max6639_reg(MAX6639_TEMP_CH2);
      return_max6639_reg(MAX6639_TEMP_EXT_CH2);
    }
    if (frm%16 == 0) { // update every 16 frames
      PM_UpdateTelem();
    }
    // roll out text
    text_cursor = smooth_slide(text_cursor, cursor_y_goal);

    // Marble label
    FPGAWD_State_t current_state = FPGAWD_GetState();
    char label[LABEL_IMAGE_SIZE];
    snprintf(label, LABEL_IMAGE_SIZE, "Bitfile");
    lv_init_label(&label_image, (0*DISPLAY_WIDTH)/5 + (DISPLAY_WIDTH)/10, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    snprintf(label, LABEL_IMAGE_SIZE, "%s", current_state == STATE_GOLDEN ? "Golden" : "User");
    lv_init_label(&label_image, (0*DISPLAY_WIDTH)/5 + (DISPLAY_WIDTH)/10, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    invertRoundedRect((0*DISPLAY_WIDTH)/5, text_cursor, (1*DISPLAY_WIDTH)/5-1, text_cursor + 12, 4); // white background for title
    emptyRoundedRect((0*DISPLAY_WIDTH)/5, text_cursor, (1*DISPLAY_WIDTH)/5-1, text_cursor + LINE_SPACING_12 + 13, 4,1); // erase background for title
    // uptime
    char label1[40];
    snprintf(label1, 40, "%s", print_uptime());
    lv_init_label(&label_uptime, (3*DISPLAY_WIDTH)/4+10, text_cursor + 2*LINE_SPACING_12+1, &lv_font_roboto_12, label1, LV_CENTER, true);
    // LM75_0
    lm75_0_temp = LM75_get_cached_temperature(LM75_0); // does not trigger readout so OK to call frequently
    snprintf(label, LABEL_TEMPERATURE_LM75_0_SIZE, LABEL_TEMPERATURE_LM75_0_FMT, ((float)lm75_0_temp)/2);
    lv_init_label(&label_temperature_lm75_0, (1*DISPLAY_WIDTH)/5+DISPLAY_WIDTH/10, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    // MAX6639 Ch 1
    max6639_ch1[0] = max6639_get_cached_temp(MAX6639_TEMP_CH1);
    max6639_ch1[1] = max6639_get_cached_temp(MAX6639_TEMP_EXT_CH1);
    snprintf(label, LABEL_TEMPERATURE_MAX6639_1_SIZE, LABEL_TEMPERATURE_MAX6639_1_FMT, MAX6639_GET_TEMP_DOUBLE(max6639_ch1[0], max6639_ch1[1]));
    lv_init_label(&label_temperature_max6639_1, (2*DISPLAY_WIDTH)/5+DISPLAY_WIDTH/10, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    // IP address
    uint8_t *pip = get_last_ip();
    char ip_string[LABEL_IP_SIZE + 1];
    format_ip_addr(pip, ip_string, LABEL_IP_SIZE);
    // ip_string[LABEL_IP_SIZE] = '\0'; // null-terminate
    lv_init_label(&label_ip, (1*DISPLAY_WIDTH)/4+8, text_cursor + 2*LINE_SPACING_12+1, &lv_font_roboto_12, ip_string, LV_CENTER, true);

    // titles
    lv_init_label(&label_temperature_max6639_2_label, (2*DISPLAY_WIDTH)/5+DISPLAY_WIDTH/10, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "FPGA", LV_CENTER, true);
    invertRoundedRect((2*DISPLAY_WIDTH)/5+1, text_cursor, (3*DISPLAY_WIDTH)/5-1, text_cursor + 12, 4); // white background for title
    emptyRoundedRect((2*DISPLAY_WIDTH)/5+1, text_cursor, (3*DISPLAY_WIDTH)/5-1, text_cursor + LINE_SPACING_12 + 13, 4,1); // erase background for title
    lv_init_label(&label_temperature_max6639_1_label, (1*DISPLAY_WIDTH)/5+DISPLAY_WIDTH/10, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "PSU", LV_CENTER, true);
    invertRoundedRect((1*DISPLAY_WIDTH)/5+1, text_cursor, (2*DISPLAY_WIDTH)/5-1, text_cursor + 12, 4); // white background for title
    emptyRoundedRect((1*DISPLAY_WIDTH)/5+1, text_cursor, (2*DISPLAY_WIDTH)/5-1, text_cursor + LINE_SPACING_12 + 13, 4,1); // erase background for title
    
    lv_init_label(&label_temperature_max6639_1_label, 4, text_cursor + 2*LINE_SPACING_12+1, &lv_font_roboto_12, "IP", LV_LEFT, true);
    invertRoundedRect(0, text_cursor + 2*LINE_SPACING_12, 17, text_cursor + DISPLAY_HEIGHT - LINE_SPACING_17, 4); // white background for title
    emptyRoundedRect(0, text_cursor + 2*LINE_SPACING_12, (DISPLAY_WIDTH)/2-1, text_cursor + DISPLAY_HEIGHT - LINE_SPACING_17, 4,1); // erase background for title

    lv_init_label(&label_temperature_max6639_1_label, (DISPLAY_WIDTH)/2+4, text_cursor + 2*LINE_SPACING_12+1, &lv_font_roboto_12, "Up", LV_LEFT, true);
    invertRoundedRect((DISPLAY_WIDTH)/2+1, text_cursor + 2*LINE_SPACING_12, (DISPLAY_WIDTH)/2+1+20, text_cursor + DISPLAY_HEIGHT - LINE_SPACING_17, 4); // white background for title
    emptyRoundedRect((DISPLAY_WIDTH)/2+1, text_cursor + 2*LINE_SPACING_12, (5*DISPLAY_WIDTH)/5, text_cursor + DISPLAY_HEIGHT - LINE_SPACING_17, 4,1); // erase background for title
    // VIN
    int newv = PM_GetTelem(VIN);
    int newi = PM_GetTelem(IIN);
    // snprintf(label, 40, LABEL_POWER_12V_FMT, (float)(newv/1000.0), (float)(newi/1000.0));
    snprintf(label, 6, "%3.1fV", (float)(newv/1000.0));
    lv_init_label(&label_3v3, (3*DISPLAY_WIDTH)/5+DISPLAY_WIDTH/10, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    snprintf(label, 6, "%3.2fA", (float)(newi/1000.0));
    lv_init_label(&label_3v3, (4*DISPLAY_WIDTH)/5+DISPLAY_WIDTH/10, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);

    lv_init_label(&label_1v8, (4*DISPLAY_WIDTH)/5, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "VIN", LV_CENTER, true);
    invertRoundedRect((3*DISPLAY_WIDTH)/5+1, text_cursor, (5*DISPLAY_WIDTH)/5, text_cursor + 12, 4); // white background for title
    emptyRoundedRect((3*DISPLAY_WIDTH)/5+1, text_cursor, (5*DISPLAY_WIDTH)/5, text_cursor + LINE_SPACING_12 + 13, 4,1); // erase background for title
    }
  // window title, error light
  fillRect(0, DISPLAY_WIDTH, title_y, title_y+LINE_SPACING_17-1, 0x00); // erase background for title
  lv_init_label(&label_title, DISPLAY_WIDTH/2, title_y, &lv_font_roboto_mono_17, "Status", LV_CENTER, true);
  errorLight(frm);
  invertRoundedRect(0, title_y, DISPLAY_WIDTH, title_y + LINE_SPACING_17-2, (LINE_SPACING_17-3)/2); // white background for title
  frm++;
  if(title_y == 23) {
    title_y_goal = 0;
    return MENU; // go back to menu
  }
  return STATUS;
}


static display_page_t page_info(unsigned events) {
  static unsigned frm = 0;
	static int16_t cursor_y_goal = 0;
  static int16_t title_y_goal = 0;
  static int16_t title_y = 23;
	static int16_t text_cursor = -2*DISPLAY_HEIGHT;
	static int16_t selection_id = 0;

	fill(0);

  // button handling
	if (events & EV_ROT_CCW) {  // scroll left
		selection_id--;
	} else if (events & EV_ROT_CW) {  // scroll right
		selection_id++;
	} else if(events & EV_ENC_S) { // push button to go back to menu - init values
    cursor_y_goal = 0;
    text_cursor = -2*DISPLAY_HEIGHT;
    selection_id = 0;
    title_y_goal = 23; // move title down
  }

  title_y = smooth_slide(title_y, title_y_goal);

  if(title_y == 0) { // when title in position

    // smooth scrolling
    cursor_y_goal = LINE_SPACING_17-(LINE_SPACING_12*selection_id);
    if(text_cursor == (text_cursor+cursor_y_goal)/2) {
      text_cursor = cursor_y_goal;
      if(selection_id >= PAGE_INFO_ITEMS-3+1) // bounce back at the end
        selection_id = PAGE_INFO_ITEMS-3;
      if(selection_id <= 0) // bounce back at the end
        selection_id = 0;
      cursor_y_goal = LINE_SPACING_17-(LINE_SPACING_12*selection_id);
    }
    else {
      text_cursor=(text_cursor+cursor_y_goal)/2;
    }
    
    // Marble Revision
    int rev = 0;
    if (marble_get_pcb_rev() == Marble_v1_4) {
      rev = 4;
    } else {
      rev = 3;
    }
    char marble_rev[LABEL_MARBLE_REV_SIZE];
    snprintf(marble_rev, sizeof(marble_rev), "Marble v1.%d.4", rev); // TODO: IMPLEMENT SUB-REVISIONS
    lv_init_label(&label_marble_rev, (LINE_SPACING_17-2)/2, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, marble_rev, LV_LEFT, true);

    // Firmware Revision
    char firmware_rev[34];
    snprintf(firmware_rev, sizeof(firmware_rev), "Firmware revision: %s [Git]", GIT_REV);
    lv_init_label(&label_firmware, (LINE_SPACING_17-2)/2, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, firmware_rev, LV_LEFT, true);
    
    // IP addr
    uint8_t *pip = get_last_ip();
    char label[16];
    char ip_string[LABEL_IP_SIZE + 1];
    format_ip_addr(pip, label, 16);
    snprintf(ip_string, LABEL_IP_SIZE+1, "IP: %s", label);
    lv_init_label(&label_ip, (LINE_SPACING_17-2)/2, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, ip_string, LV_LEFT, true);

    // MAC addr
    uint8_t *pmac = get_last_mac();
    char mac_string[LABEL_MAC_SIZE + 1];
    format_mac_addr(pmac, mac_string, LABEL_MAC_SIZE);
    mac_string[LABEL_MAC_SIZE] = '\0'; // null-terminate
    lv_init_label(&label_mac, (LINE_SPACING_17-2)/2, text_cursor + 3*LINE_SPACING_12, &lv_font_roboto_12, mac_string, LV_LEFT, true);

    // MMC CHIP ID
    char chip_id[35];
    snprintf(chip_id, sizeof(chip_id), "MMC %s", print_mmc_ID());
    lv_init_label(&label_MMC_chip_id, (LINE_SPACING_17-2)/2, text_cursor + 4*LINE_SPACING_12, &lv_font_roboto_12, chip_id, LV_LEFT, true);

    // Marble SN
    char marble_sn[37];
    snprintf(marble_sn, sizeof(marble_sn), "MMC SN: %s", print_marble_SN());
    lv_init_label(&label_SN, (LINE_SPACING_17-2)/2, text_cursor + 5*LINE_SPACING_12, &lv_font_roboto_12, marble_sn, LV_LEFT, true);

    // PHY ID
    char phy_id[23];
    snprintf(phy_id, sizeof(phy_id), "PHY ID: %s", print_PHY_ID());
    lv_init_label(&label_PHY_id, (LINE_SPACING_17-2)/2, text_cursor + 6*LINE_SPACING_12, &lv_font_roboto_12, phy_id, LV_LEFT, true);

    // MMC Boot ID
    char boot_id[23];
    snprintf(boot_id, sizeof(boot_id), "MMC Boot ID: %s", print_boot_ID());
    lv_init_label(&label_boot_id, (LINE_SPACING_17-2)/2, text_cursor + 7*LINE_SPACING_12, &lv_font_roboto_12, boot_id, LV_LEFT, true);

    window_scrollbar(text_cursor-LINE_SPACING_17, (PAGE_INFO_ITEMS)*LINE_SPACING_12);
    // do nothing, we are going back to menu
  }
  fillRect(0, DISPLAY_WIDTH, title_y, title_y+LINE_SPACING_17-1, 0x00); // erase background for title
  lv_init_label(&label_title, DISPLAY_WIDTH/2, title_y, &lv_font_roboto_mono_17, "Info", LV_CENTER, true);
  errorLight(frm);
  invertRoundedRect(0, title_y, DISPLAY_WIDTH, title_y + LINE_SPACING_17-2, (LINE_SPACING_17-3)/2); // white background for title
  frm++;
  if(title_y == 23) {
      title_y_goal = 0;
      return MENU; // go back to menu
  }
  return INFO; // TODO - return whether we need to update the screen (if a value has changed)
}




static display_page_t page_power(unsigned events) {
  static unsigned frm = 0;
  static int16_t title_y_goal = 0;
  static int16_t title_y = 23;
  static int16_t text_cursor = -DISPLAY_HEIGHT;
  static int16_t cursor_y_goal = LINE_SPACING_17;


  if (frm%16 == 0) { // update every 16 frames
    PM_UpdateTelem();
  }

  fill(0);

  if(events & EV_ENC_S) { // push button to go back to menu
    title_y_goal = 23;
  }

  title_y = smooth_slide(title_y, title_y_goal);

  // Display contents only when title in position
  if(title_y == 0) {
    text_cursor = smooth_slide(text_cursor, cursor_y_goal);
    // VIN
    int newv = PM_GetTelem(VIN);
    int newi = PM_GetTelem(IIN);
    char label[LABEL_POWER_12V_SIZE];
    snprintf(label, LABEL_POWER_12V_SIZE, LABEL_POWER_12V_FMT, (float)(newv/1000.0), (float)(newi/1000.0));
    snprintf(label, 6, "%3.1fV", (float)(newv/1000.0));
    lv_init_label(&label_3v3, (0*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    snprintf(label, 6, "%3.2fA", (float)(newi/1000.0));
    lv_init_label(&label_3v3, (0*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    // 3V3
    newv = PM_GetTelem(VOUT_3V3);
    newi = PM_GetTelem(IOUT_3V3);
    snprintf(label, 6, "%3.2fV", (float)(newv/1000.0));
    lv_init_label(&label_3v3, (1*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    snprintf(label, 6, "%3.2fA", (float)(newi/1000.0));
    lv_init_label(&label_3v3, (1*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    // 2V5
    newv = PM_GetTelem(VOUT_2V5);
    newi = PM_GetTelem(IOUT_2V5);
    snprintf(label, 6, "%3.2fV", (float)(newv/1000.0));
    lv_init_label(&label_3v3, (2*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    snprintf(label, 6, "%3.2fA", (float)(newi/1000.0));
    lv_init_label(&label_3v3, (2*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    // 1V8
    newv = PM_GetTelem(VOUT_1V8);
    newi = PM_GetTelem(IOUT_1V8);
    snprintf(label, 6, "%3.2fV", (float)(newv/1000.0));
    lv_init_label(&label_3v3, (3*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    snprintf(label, 6, "%3.2fA", (float)(newi/1000.0));
    lv_init_label(&label_3v3, (3*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    // 1V0
    newv = PM_GetTelem(VOUT_1V0);
    newi = PM_GetTelem(IOUT_1V0);
    snprintf(label, 6, "%3.2fV", (float)(newv/1000.0));
    lv_init_label(&label_3v3, (4*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    snprintf(label, 6, "%3.2fA", (float)(newi/1000.0));
    lv_init_label(&label_3v3, (4*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    // FMC1
    uint16_t new_fmc_current=0;
    static float fmc1_current_amps=0.0;
    static float fmc2_current_amps=0.0;
    // static uint8_t fmc_status = 0;
    uint8_t new_fmc_status = marble_FMC_status();
    uint8_t mask = (1 << M_FMC_STATUS_FMC1_PWR);
    if (new_fmc_status & mask) {
      lv_init_label(&label_3v3, (5*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, "ON", LV_CENTER, true);
      if ((frm+5)%64 == 0) { // update every 64 frames
        switch_i2c_bus(6);
        new_fmc_current = ina219_getShuntVoltage(INA219_FMC1);
        fmc1_current_amps = INA219_SHUNT_VOLTAGE_TO_CURRENT(new_fmc_current);
        switch_i2c_bus(2);
      }
      snprintf(label, 6, "%3.2fA", fmc1_current_amps);
      lv_init_label(&label_3v3, (5*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    }
    else {
      lv_init_label(&label_3v3, (5*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, "OFF", LV_CENTER, true);
      lv_init_label(&label_3v3, (5*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, "-", LV_CENTER, true);
    }
    // FMC2
    mask = (1 << M_FMC_STATUS_FMC2_PWR);
    if (new_fmc_status & mask) {
      lv_init_label(&label_3v3, (6*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, "ON", LV_CENTER, true);
      if ((frm+10)%64 == 0) { // update every 64 frames
        switch_i2c_bus(6);
        new_fmc_current = ina219_getShuntVoltage(INA219_FMC2);
        fmc2_current_amps = INA219_SHUNT_VOLTAGE_TO_CURRENT(new_fmc_current);
        switch_i2c_bus(2);
      }
      snprintf(label, 6, "%3.2fA", fmc2_current_amps);
      lv_init_label(&label_3v3, (6*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    }
    else {
      lv_init_label(&label_3v3, (6*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, "OFF", LV_CENTER, true);
      lv_init_label(&label_3v3, (6*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, "-", LV_CENTER, true);
    }

    lv_init_label(&label_1v0, (6*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "FMC2", LV_CENTER, true);
    invertRoundedRect((6*DISPLAY_WIDTH)/7+1, text_cursor, (7*DISPLAY_WIDTH)/7, text_cursor+12, 4); // white background for title
    lv_init_label(&label_1v8, (5*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "FMC1", LV_CENTER, true);
    invertRoundedRect((5*DISPLAY_WIDTH)/7+1, text_cursor, (6*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
    lv_init_label(&label_1v8, (4*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "1V0", LV_CENTER, true);
    invertRoundedRect((4*DISPLAY_WIDTH)/7+1, text_cursor, (5*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
    lv_init_label(&label_1v8, (3*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "1V8", LV_CENTER, true);
    invertRoundedRect((3*DISPLAY_WIDTH)/7+1, text_cursor, (4*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
    lv_init_label(&label_1v8, (2*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "2V5", LV_CENTER, true);
    invertRoundedRect((2*DISPLAY_WIDTH)/7+1, text_cursor, (3*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
    lv_init_label(&label_1v8, (1*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "3V3", LV_CENTER, true);
    invertRoundedRect((1*DISPLAY_WIDTH)/7+1, text_cursor, (2*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
    lv_init_label(&label_1v8, (0*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "VIN", LV_CENTER, true);
    invertRoundedRect((0*DISPLAY_WIDTH)/7, text_cursor, (1*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
  }
  // window title, error light
  fillRect(0, DISPLAY_WIDTH, title_y, title_y+LINE_SPACING_17-1, 0x00); // erase background for title
  lv_init_label(&label_title, DISPLAY_WIDTH/2, title_y, &lv_font_roboto_mono_17, "Power", LV_CENTER, true);
  errorLight(frm);
  invertRoundedRect(0, title_y, DISPLAY_WIDTH, title_y + LINE_SPACING_17-2, (LINE_SPACING_17-3)/2); // white background for title
  frm++;
  if(title_y == 23) {
    title_y_goal = 0;
    text_cursor = -DISPLAY_HEIGHT;
    return MENU; // go back to menu
  }
  return POWER;
}


static display_page_t page_temperature(unsigned events) {
  static unsigned frm = 0;
  static int16_t title_y_goal = 0;
  static int16_t title_y = 23;
  static uint8_t fan_speed;
  static int16_t text_cursor = -DISPLAY_HEIGHT;
  static int16_t cursor_y_goal = LINE_SPACING_17;

  static int lm75_0_temp=0, lm75_1_temp=0;
  static int max6639_ch1[2] = {0, 0};
  static int max6639_ch2[2] = {0, 0};
  char label[9];

  if (frm%64 == 0) { // update every 100 frames
    LM75_get_temperature(LM75_0);
    LM75_get_temperature(LM75_1);
    return_max6639_reg(MAX6639_TEMP_CH1);
    return_max6639_reg(MAX6639_TEMP_EXT_CH1);
    return_max6639_reg(MAX6639_TEMP_CH2);
    return_max6639_reg(MAX6639_TEMP_EXT_CH2);
  }

  fill(0);

  if(events & EV_ENC_S) { // push button to go back to menu
    title_y_goal = 23;
  }

  title_y = smooth_slide(title_y, title_y_goal);

  // Display contents only when title in position
  if(title_y == 0) {
    text_cursor = smooth_slide(text_cursor, cursor_y_goal);

    // LM75_0
    lm75_0_temp = LM75_get_temperature(LM75_0); // does not trigger readout so OK to call frequently
    snprintf(label, LABEL_TEMPERATURE_LM75_0_SIZE, LABEL_TEMPERATURE_LM75_0_FMT, ((float)lm75_0_temp)/2);
    lv_init_label(&label_temperature_lm75_0, (0*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    // LM75_1
    lm75_1_temp = LM75_get_cached_temperature(LM75_1);
    snprintf(label, LABEL_TEMPERATURE_LM75_1_SIZE, LABEL_TEMPERATURE_LM75_1_FMT, ((float)lm75_1_temp)/2);
    lv_init_label(&label_temperature_lm75_1, (1*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    // MAX6639 Ch 1
    max6639_ch1[0] = max6639_get_cached_temp(MAX6639_TEMP_CH1);
    max6639_ch1[1] = max6639_get_cached_temp(MAX6639_TEMP_EXT_CH1);
    snprintf(label, LABEL_TEMPERATURE_MAX6639_1_SIZE, LABEL_TEMPERATURE_MAX6639_1_FMT, MAX6639_GET_TEMP_DOUBLE(max6639_ch1[0], max6639_ch1[1]));
    lv_init_label(&label_temperature_max6639_1, (2*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    eeprom_read_fan_speed(&fan_speed, 1);
    snprintf(label, 9, "%d %%", (uint16_t)fan_speed * 5 / 6);
    lv_init_label(&label_fan_max6639_1, 85, text_cursor + 2*LINE_SPACING_12+1, &lv_font_roboto_12, label, LV_LEFT, true);
    // MAX6639 Ch 2
    max6639_ch2[0] = max6639_get_cached_temp(MAX6639_TEMP_CH2);
    max6639_ch2[1] = max6639_get_cached_temp(MAX6639_TEMP_EXT_CH2);
    snprintf(label, LABEL_TEMPERATURE_MAX6639_2_SIZE, LABEL_TEMPERATURE_MAX6639_2_FMT, MAX6639_GET_TEMP_DOUBLE(max6639_ch2[0], max6639_ch2[1]));
    lv_init_label(&label_temperature_max6639_2, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 1*LINE_SPACING_12+1, &lv_font_roboto_12, label, LV_CENTER, true);
    snprintf(label, 9, "%d %%", (uint16_t)fan_speed * 5 / 6);
    lv_init_label(&label_fan_max6639_2, (DISPLAY_WIDTH)/2+85, text_cursor + 2*LINE_SPACING_12+1, &lv_font_roboto_12, label, LV_LEFT, true);
    lv_init_label(&label_temperature_max6639_1_label, 4, text_cursor + 2*LINE_SPACING_12+1, &lv_font_roboto_12, "Left Fan: ", LV_LEFT, true);
    invertRoundedRect(0, text_cursor + 2*LINE_SPACING_12, 60, text_cursor + DISPLAY_HEIGHT - LINE_SPACING_17, 4); // white background for title
    emptyRoundedRect(0, text_cursor + 2*LINE_SPACING_12, (DISPLAY_WIDTH)/2-1, text_cursor + DISPLAY_HEIGHT - LINE_SPACING_17, 4,1); // erase background for title

    lv_init_label(&label_temperature_max6639_1_label, (DISPLAY_WIDTH)/2+4, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, "Right Fan: ", LV_LEFT, true);
    invertRoundedRect((DISPLAY_WIDTH)/2+1, text_cursor + 2*LINE_SPACING_12, (DISPLAY_WIDTH)/2+1+60, text_cursor + DISPLAY_HEIGHT - LINE_SPACING_17, 4); // white background for title
    emptyRoundedRect((DISPLAY_WIDTH)/2+1, text_cursor + 2*LINE_SPACING_12, (5*DISPLAY_WIDTH)/5, text_cursor + DISPLAY_HEIGHT - LINE_SPACING_17, 4,1); // erase background for title
    // VIN
    // titles
    lv_init_label(&label_temperature_max6639_2_label, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "PSU2", LV_CENTER, true);
    invertRoundedRect((3*DISPLAY_WIDTH)/4+1, text_cursor, (4*DISPLAY_WIDTH)/4-1, text_cursor+12, 4); // white background for title
    emptyRoundedRect((3*DISPLAY_WIDTH)/4+1, text_cursor, (4*DISPLAY_WIDTH)/4-1, text_cursor + LINE_SPACING_12 + 13, 4,1); // erase background for title
    lv_init_label(&label_temperature_max6639_1_label, (2*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "FPGA", LV_CENTER, true);
    invertRoundedRect((2*DISPLAY_WIDTH)/4+1, text_cursor, (3*DISPLAY_WIDTH)/4-1, text_cursor+12, 4); // white background for title
    emptyRoundedRect((2*DISPLAY_WIDTH)/4+1, text_cursor, (3*DISPLAY_WIDTH)/4-1, text_cursor + LINE_SPACING_12 + 13, 4,1); // erase background for title
    lv_init_label(&label_temperature_lm75_1_label, (1*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "MMC", LV_CENTER, true);
    invertRoundedRect((1*DISPLAY_WIDTH)/4+1, text_cursor, (2*DISPLAY_WIDTH)/4-1, text_cursor+12, 4); // white background for title
    emptyRoundedRect((1*DISPLAY_WIDTH)/4+1, text_cursor, (2*DISPLAY_WIDTH)/4-1, text_cursor + LINE_SPACING_12 + 13, 4,1); // erase background for title
    lv_init_label(&label_temperature_lm75_0_label, (0*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "PSU", LV_CENTER, true);
    invertRoundedRect((0*DISPLAY_WIDTH)/4, text_cursor, (1*DISPLAY_WIDTH)/4-1, text_cursor+12, 4); // white background for title
    emptyRoundedRect((0*DISPLAY_WIDTH)/4, text_cursor, (1*DISPLAY_WIDTH)/4-1, text_cursor + LINE_SPACING_12 + 13, 4,1); // erase background for title
    }
  // window title, error light
  fillRect(0, DISPLAY_WIDTH, title_y, title_y+LINE_SPACING_17-1, 0x00); // erase background for title
  lv_init_label(&label_title, DISPLAY_WIDTH/2, title_y, &lv_font_roboto_mono_17, "Temperature", LV_CENTER, true);
  errorLight(frm);
  invertRoundedRect(0, title_y, DISPLAY_WIDTH, title_y + LINE_SPACING_17-2, (LINE_SPACING_17-3)/2); // white background for title
  frm++;
  if(title_y == 23) {
    title_y_goal = 0;
    text_cursor = -DISPLAY_HEIGHT;
    return MENU; // go back to menu
  }
  return TEMPERATURE;
}

static display_page_t page_errors(unsigned events) {
  static unsigned frm = 0;
  static int16_t title_y_goal = 0;
  static int16_t title_y = 23;
	static int16_t cursor_y_goal = LINE_SPACING_17;
	static int16_t text_cursor = -2*DISPLAY_HEIGHT;
	static int16_t selection_id = 0;

  fill(0);

  // button handling
	if (events & EV_ROT_CCW) {  // scroll left
		selection_id--;
	} else if (events & EV_ROT_CW) {  // scroll right
		selection_id++;
	} else if(events & EV_ENC_S) { // push button to go back to menu - init values
    cursor_y_goal = LINE_SPACING_17;
    text_cursor = -DISPLAY_HEIGHT;
    selection_id = 0;
    title_y_goal = 23; // move title down
  }

  title_y = smooth_slide(title_y, title_y_goal);

  // Display contents only when title in position
  if(title_y == 0) {
    uint8_t error_lines = marble_get_error_order_max();

    if(error_lines == 0) {
      text_cursor = smooth_slide(text_cursor, cursor_y_goal);
      lv_init_label(&label_error, DISPLAY_WIDTH/2, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, "No errors have occurred", LV_CENTER, true);
    }
    else { // display errors

      if (error_lines <= 2) { // no scrolling needed
        selection_id = 0; 
        text_cursor = LINE_SPACING_17;
      } 
      else { // smooth scrolling
        cursor_y_goal = LINE_SPACING_17-(LINE_SPACING_12*selection_id);
        if(text_cursor == (text_cursor+cursor_y_goal)/2) {
          text_cursor = cursor_y_goal;
          if(selection_id >= error_lines-3+1) // bounce back at the end
            selection_id = error_lines-3;
          if(selection_id <= 0) // bounce back at the end
            selection_id = 0;
          cursor_y_goal = LINE_SPACING_17-(LINE_SPACING_12*selection_id);
        }
        else {
          text_cursor=(text_cursor+cursor_y_goal)/2;
        }
        window_scrollbar(text_cursor-LINE_SPACING_17, (error_lines+1)*LINE_SPACING_12);
      }
      // draw error lines
      marble_error_info_t error_info;
      uint32_t uptime = marble_uptime_seconds();
      uint32_t error_time_ago;
      char error[33];
      // printf("Displaying %d error lines\n", error_lines);
      for(uint8_t i=0; i<error_lines; i++) {
          error_info = marble_get_error_info(error_lines-i);
          // printf("Error %d: index %d, count %d, last time %d\n", i, error_info.error_index, error_info.error_count, error_info.last_occurrence_time_s);
          // print error
          snprintf(error, 33, "%s", ErrorCodeShortStrings[error_info.error_index]);
          lv_init_label(&label_error, (0*DISPLAY_WIDTH)/4+4, text_cursor + (i+1)*LINE_SPACING_12, &lv_font_roboto_12, error, LV_LEFT, true);
          // print count
          snprintf(error, 33, "%ld", error_info.error_count);
          lv_init_label(&label_error, (2*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8+7, text_cursor + (i+1)*LINE_SPACING_12, &lv_font_roboto_12, error, LV_CENTER, true);
          // print last time
          error_time_ago = uptime - error_info.last_occurrence_time_s;
          if(error_time_ago/86400) { // days
              snprintf(error, 33, "%ld days", error_time_ago/86400);
              lv_init_label(&label_error, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8-4, text_cursor + (i+1)*LINE_SPACING_12, &lv_font_roboto_12, error, LV_CENTER, true);
          } else if (error_time_ago/3600) { // hours
              snprintf(error, 33, "%ld h", error_time_ago/3600);
              lv_init_label(&label_error, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8-4, text_cursor + (i+1)*LINE_SPACING_12, &lv_font_roboto_12, error, LV_CENTER, true);
          } else if (error_time_ago/60) { // minutes
              snprintf(error, 33, "%ld min", error_time_ago/60);
              lv_init_label(&label_error, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8-4, text_cursor + (i+1)*LINE_SPACING_12, &lv_font_roboto_12, error, LV_CENTER, true);
          } else { // seconds
              snprintf(error, 33, "%ld s", error_time_ago);
              lv_init_label(&label_error, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8-4, text_cursor + (i+1)*LINE_SPACING_12, &lv_font_roboto_12, error, LV_CENTER, true);
          }
          // highlight if needed
          if(error_info.last_occurrence_time_s > error_reset_time) { // if error occurred after last error clear, highlight it
            if((frm>>2)%2)
              invertRoundedRect(1, text_cursor + (i+1)*LINE_SPACING_12, (2*DISPLAY_WIDTH)/4-1+14, text_cursor + (i+1)*LINE_SPACING_12 + 12, 4); // white background for title
          }
      }
      // titles
      lv_init_label(&label_error, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8-5, text_cursor, &lv_font_roboto_12, "Since", LV_CENTER, true);
      invertRoundedRect((3*DISPLAY_WIDTH)/4+1, text_cursor, (4*DISPLAY_WIDTH)/4-10, text_cursor+12, 4); // white background for title
      lv_init_label(&label_error, (2*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8+7, text_cursor, &lv_font_roboto_12, "Count", LV_CENTER, true);
      invertRoundedRect((2*DISPLAY_WIDTH)/4+1+14, text_cursor, (3*DISPLAY_WIDTH)/4-1, text_cursor+12, 4); // white background for title
      lv_init_label(&label_error, DISPLAY_WIDTH/4+7, text_cursor, &lv_font_roboto_12, "Error", LV_CENTER, true);
      invertRoundedRect((0*DISPLAY_WIDTH)/4, text_cursor, (2*DISPLAY_WIDTH)/4-1+14, text_cursor+12, 4); // white background for title
    }  
  }
  // window title, error light
  fillRect(0, DISPLAY_WIDTH, title_y, title_y+LINE_SPACING_17-1, 0x00); // erase background for title
  lv_init_label(&label_title, DISPLAY_WIDTH/2, title_y, &lv_font_roboto_mono_17, "Error Log", LV_CENTER, true);
  errorLight(frm);
  invertRoundedRect(0, title_y, DISPLAY_WIDTH, title_y + LINE_SPACING_17-2, (LINE_SPACING_17-3)/2); // white background for title
  frm++;
  if(title_y == 23) {
    title_y_goal = 0;
    return MENU; // go back to menu
  }
  return ERRORS;
}

static display_page_t page_clear_errors(unsigned events)
{
	static unsigned frm = 0;
	static int16_t selection_id = 0;

	const int rectangle_coordinates[][4] = {
		{DISPLAY_WIDTH/4 - 38, 43, DISPLAY_WIDTH/4 + 38, 63},  // CANCEL
		{(3*DISPLAY_WIDTH)/4 - 38, 43, (3*DISPLAY_WIDTH)/4 + 38, 63}  // Proceed
	};

	fill(0);

  lv_init_label(&label_warning_1, DISPLAY_WIDTH/2, 2, &lv_font_roboto_mono_17, "Clear error light", LV_CENTER, true);
  lv_init_label(&label_warning_2, DISPLAY_WIDTH/2, 21, &lv_font_roboto_12, "This action will not clear error logs", LV_CENTER, true);
  lv_init_label(&label_warning_cancel, DISPLAY_WIDTH/4, 44, &lv_font_roboto_mono_17, "Cancel", LV_CENTER, true);
  lv_init_label(&label_warning_proceed, (3*DISPLAY_WIDTH)/4, 44, &lv_font_roboto_mono_17, "Reset", LV_CENTER, true);
		// navigate menu
		if (events & EV_ROT_CCW) {  // left
			selection_id--;
		} else if (events & EV_ROT_CW) {  // right
			selection_id++;
		}
		if(selection_id >= 2)
			selection_id = 0;
		if(selection_id <= -1)
			selection_id = 1;
	

	if(selection_id == 0) {
		invertRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10);
		emptyRoundedRect(rectangle_coordinates[1][0]-1, rectangle_coordinates[1][1]-1, rectangle_coordinates[1][2]+1, rectangle_coordinates[1][3]+1, 10, 1);
		if (events & EV_ENC_S) { // push
			selection_id = 0; // default cancel button
			return MENU; // back to config menu
		}
	} else if (selection_id == 1) {
		emptyRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10, 1);
		invertRoundedRect(rectangle_coordinates[1][0]-1, rectangle_coordinates[1][1]-1, rectangle_coordinates[1][2]+1, rectangle_coordinates[1][3]+1, 10);
		if (events & EV_ENC_S) { // push
			selection_id = 0; // default cancel button
      error_reset_time = marble_uptime_seconds();
			return MENU; 
    }
	}

	frm++;
	return CLEAR_ERRORS; // stay in menu
}

static display_page_t config_warning(unsigned events)
{
	static unsigned frm = 0;
	static int16_t selection_id = 0;

	const int rectangle_coordinates[][4] = {
		{DISPLAY_WIDTH/4 - 38, 43, DISPLAY_WIDTH/4 + 38, 63},  // CANCEL
		{(3*DISPLAY_WIDTH)/4 - 43, 43, (3*DISPLAY_WIDTH)/4 + 43, 63}  // Proceed
	};

	fill(0);

  lv_init_label(&label_warning_1, DISPLAY_WIDTH/2, 2, &lv_font_roboto_mono_17, "WARNING", LV_CENTER, true);
  lv_init_label(&label_warning_2, DISPLAY_WIDTH/2, 21, &lv_font_roboto_12, "You are about to change critical device settings", LV_CENTER, true);
  lv_init_label(&label_warning_cancel, (DISPLAY_WIDTH)/4, 44, &lv_font_roboto_mono_17, "Cancel", LV_CENTER, true);
  lv_init_label(&label_warning_proceed, (3*DISPLAY_WIDTH)/4, 44, &lv_font_roboto_mono_17, "Proceed", LV_CENTER, true);
		// navigate menu
		if (events & EV_ROT_CCW) {  // left
			selection_id--;
		} else if (events & EV_ROT_CW) {  // right
			selection_id++;
		}
		if(selection_id >= 2) // bounce back at the end
			selection_id = 0;
		if(selection_id <= -1) // bounce back at the end
			selection_id = 1;
	

	if(selection_id == 0) {
		invertRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10);
		emptyRoundedRect(rectangle_coordinates[1][0]-1, rectangle_coordinates[1][1]-1, rectangle_coordinates[1][2]+1, rectangle_coordinates[1][3]+1, 10, 1);
		if (events & EV_ENC_S) { // push
			selection_id = 0; // default cancel button
			return MENU; // back to config menu
		}
	} else if (selection_id == 1) {
		emptyRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10, 1);
		invertRoundedRect(rectangle_coordinates[1][0]-1, rectangle_coordinates[1][1]-1, rectangle_coordinates[1][2]+1, rectangle_coordinates[1][3]+1, 10);
		if (events & EV_ENC_S) { // push
			selection_id = 0; // default cancel button
			return SET_IP; // back to config menu
		}
	}

	frm++;
	return CONFIG_WARNING; // stay in menu
}

static display_page_t set_IP(unsigned events)
{
	static unsigned frm = 0;
  static int16_t title_y_goal = 0;
  static int16_t title_y = 0;
	static int16_t cursor_x1 = 0;
	static int16_t cursor_x2 = 0;
	static int16_t cursor_y1 = 0;
	static int16_t cursor_y2 = 0;
	static int16_t selection_id = 0;
	static uint8_t ip_bytes[4] = {0, 0, 0, 0};
	static bool ip_selected = 0;
	static bool ip_success = 0;
	const int rectangle_coordinates[][4] = {
		{DISPLAY_WIDTH/4 - 38, 43, DISPLAY_WIDTH/4 + 38, DISPLAY_HEIGHT-1},  // CANCEL
		{51, 20, 84, 39}, // IP 1
		{91, 20, 124, 39}, // IP 2
		{131, 20, 164, 39}, // IP 3
		{171, 20, 204, 39},  // IP 4
		{(DISPLAY_WIDTH*3)/4 - 38, 43, (DISPLAY_WIDTH*3)/4 + 38, DISPLAY_HEIGHT-1}  // SET IP
	};

  // set bytes to current IP if first frame
  if (frm == 0) {
    title_y = 0;
    uint8_t *temp_pip = get_last_ip();
    if (temp_pip != NULL) {
      memcpy(ip_bytes, temp_pip, 4);
    }
  }

	fill(0);

  title_y = smooth_slide(title_y, title_y_goal);

  if(title_y == 0) {
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%03d.%03d.%03d.%03d", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    lv_init_label(&IP_item, DISPLAY_WIDTH/2, 21, &lv_font_roboto_mono_17, buffer, LV_CENTER, true);
    lv_init_label(&IP_item, DISPLAY_WIDTH/4, 44, &lv_font_roboto_mono_17, "Exit", LV_CENTER, true);
    lv_init_label(&IP_item, (DISPLAY_WIDTH*3)/4, 44, &lv_font_roboto_mono_17, "Set IP", LV_CENTER, true);

    if(ip_selected) {
      // modify selected byte
      if (events & EV_ROT_CCW) {  // left
        if(ip_bytes[selection_id - 1] == 0)
          ip_bytes[selection_id - 1] = 255;
        else
          ip_bytes[selection_id - 1]--;
      } else if (events & EV_ROT_CW) {  // right
        if(ip_bytes[selection_id - 1] == 255)
          ip_bytes[selection_id - 1] = 0;
        else
          ip_bytes[selection_id - 1]++;
      }
    } else {
      // navigate menu
      if (events & EV_ROT_CCW) {  // left
        selection_id--;
      } else if (events & EV_ROT_CW) {  // right
        selection_id++;
      }
      if(selection_id >= 6) // bounce back at the end
        selection_id = 0;
      if(selection_id <= -1) // bounce back at the end
        selection_id = 5;
    }
    if(selection_id == 0) {
      cursor_x1 = rectangle_coordinates[1][0];
      cursor_y1 = rectangle_coordinates[1][1];
      cursor_x2 = rectangle_coordinates[1][2];
      cursor_y2 = rectangle_coordinates[1][3];
      invertRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10);
      emptyRoundedRect(rectangle_coordinates[5][0]-1, rectangle_coordinates[5][1]-1, rectangle_coordinates[5][2]+1, rectangle_coordinates[5][3]+1, 10, 1);
      if (events & EV_ENC_S) { // push
        title_y_goal = 23;
      }
    } else if (selection_id == 5) {
      cursor_x1 = rectangle_coordinates[4][0];
      cursor_y1 = rectangle_coordinates[4][1];
      cursor_x2 = rectangle_coordinates[4][2];
      cursor_y2 = rectangle_coordinates[4][3];
      emptyRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10, 1);
      invertRoundedRect(rectangle_coordinates[5][0]-1, rectangle_coordinates[5][1]-1, rectangle_coordinates[5][2]+1, rectangle_coordinates[5][3]+1, 10);
      if (events & EV_ENC_S) { // push
        eeprom_store_ip_addr(ip_bytes, 4);
        set_last_ip(ip_bytes);
        ip_success = 1;
      }
    } else { // IP byte selected
      cursor_x1 = ((rectangle_coordinates[selection_id][0]+cursor_x1)>>1 == cursor_x1) ? rectangle_coordinates[selection_id][0] : (rectangle_coordinates[selection_id][0]+cursor_x1)>>1;
      cursor_y1 = ((rectangle_coordinates[selection_id][1]+cursor_y1)>>1 == cursor_y1) ? rectangle_coordinates[selection_id][1] : (rectangle_coordinates[selection_id][1]+cursor_y1)>>1;
      cursor_x2 = ((rectangle_coordinates[selection_id][2]+cursor_x2)>>1 == cursor_x2) ? rectangle_coordinates[selection_id][2] : (rectangle_coordinates[selection_id][2]+cursor_x2)>>1;
      cursor_y2 = ((rectangle_coordinates[selection_id][3]+cursor_y2)>>1 == cursor_y2) ? rectangle_coordinates[selection_id][3] : (rectangle_coordinates[selection_id][3]+cursor_y2)>>1;
      if(ip_selected) {
        invertRoundedRect(cursor_x1, cursor_y1, cursor_x2, cursor_y2, 5);
      } else {
        emptyRoundedRect(cursor_x1, cursor_y1, cursor_x2, cursor_y2, 5, 1);
      }
      // print cancel and set ip buttons
      emptyRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10, 1);
      emptyRoundedRect(rectangle_coordinates[5][0]-1, rectangle_coordinates[5][1]-1, rectangle_coordinates[5][2]+1, rectangle_coordinates[5][3]+1, 10, 1);
      if (events & EV_ENC_S) { // push
        ip_selected = !ip_selected; // select ip byte
      }
    }
    if(ip_success) {
      selection_id = 0; // select exit button
      char success_message[31];
      eeprom_read_ip_addr(ip_bytes, 4);
      snprintf(success_message, sizeof(success_message), "Success!  IP = %03d.%03d.%03d.%03d", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
      fillRect(0, DISPLAY_WIDTH, LINE_SPACING_17, 2*LINE_SPACING_17, 0x00); // erase background for title
      lv_init_label(&IP_item, DISPLAY_WIDTH/2, LINE_SPACING_17+4, &lv_font_roboto_12, success_message, LV_CENTER, true);
      if (events & (EV_ROT_CCW || EV_ROT_CW)) {  // left or right
        ip_success = 0; // reset success message
      }
    }
  }
  // window title, error light
  fillRect(0, DISPLAY_WIDTH, title_y, title_y+LINE_SPACING_17-1, 0x00); // erase background for title
  lv_init_label(&label_title, DISPLAY_WIDTH/2, title_y, &lv_font_roboto_mono_17, "Set IP Address", LV_CENTER, true);
  errorLight(frm);
  invertRoundedRect(0, title_y, DISPLAY_WIDTH, title_y + LINE_SPACING_17-2, (LINE_SPACING_17-3)/2); // white background for title

	frm++;
  if(title_y == 23) {
    title_y_goal = 0;
    ip_success = 0;
    frm=0;
    return MENU; // go back to menu
  }
	return SET_IP; // stay in menu
}

void display_init(bool is_ui_board_1u) {
  // For lack of a better place, let's put this here for now
  printf("Initializing UI_BOARD%s\r\n", is_ui_board_1u ? "_1U" : "");
  ui_board_spi_init();
  ui_init(is_ui_board_1u ? UI_BOARD_1U : UI_BOARD);
  set_inverted(0);

  fill(0);

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

static int do_update_buffer(uint32_t last_buffer_update) {
  uint32_t now = BSP_GET_SYSTICK();
  return compare_systick(last_buffer_update, now, UPDATE_BUFFER_INTERVAL_MS);
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
  snprintf(ps, (size_t)maxlen, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  return;
}

static void format_mac_addr(uint8_t *mac, char *ps, int maxlen) {
  snprintf(ps, (size_t)maxlen, "MAC: %x:%x:%x:%x:%x:%x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return;
}

static void window_scrollbar(int16_t pos, int16_t total){
  uint16_t windowCapacity = 3*LINE_SPACING_12; // number of pixels that can be displayed in window
  uint16_t barSize = DISPLAY_HEIGHT - 4 - LINE_SPACING_17; // total size of scrollbar area
  uint16_t blockLength = (windowCapacity*barSize)/total; // size of block in scrollbar
  if (blockLength > barSize) blockLength = barSize;
  int16_t blockTop = 2 + LINE_SPACING_17 - (pos * (barSize-blockLength)) / (total - 3*LINE_SPACING_12);
  int16_t blockBottom = blockTop + blockLength;
  if(blockTop < 2 + LINE_SPACING_17) {
    blockTop = 2 + LINE_SPACING_17;
    if(blockBottom < blockTop) blockBottom = blockTop;
  }
  if(blockBottom > DISPLAY_HEIGHT - 2) {
    blockBottom = DISPLAY_HEIGHT - 2;
    if(blockTop > blockBottom) blockTop = blockBottom;
  }
  emptyRoundedRect(DISPLAY_WIDTH - 7, 1 + LINE_SPACING_17, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 2, 3, 1);
  invertRoundedRect(DISPLAY_WIDTH - 6, blockTop, DISPLAY_WIDTH - 2, blockBottom, 2); 
}

static void errorLight(unsigned frm) {
  if(marble_last_error_tick() > error_reset_time){  // if error occurred after last error clear, highlight it
    lv_init_label(&error_light, 227, 3, &lv_font_roboto_12, "ERROR", LV_CENTER, true);
    if((frm>>2)%2){
      invertRoundedRect(200, 1, 254, 17, 7); // white background for title
      set_ledb(1); // red
    } else 
      set_ledb(0); // none
  }
  else {
    if(frm%32 == 0) {
      set_ledb(2); // green
    } else {
      set_ledb(0); // none
    }
  }
  return;
}

static int16_t smooth_slide(int16_t title_y, int16_t title_y_goal){  // smooth title movement
	if(title_y == (title_y+title_y_goal)/2) {
		title_y = title_y_goal;
	}
	else {
		title_y=(title_y+title_y_goal)/2;
	}
  return title_y;
}

#define ERROR_LED_TIME_ON_MS     (500)
#define ERROR_LED_TIME_OFF_MS   (1000)
static void update_led(void) {
  // static uint32_t blink_time = 0;
  // uint32_t now = BSP_GET_SYSTICK();
  // Board_Status_t status = marble_get_status();
  // if (status == BOARD_STATUS_GOOD) {
  //   set_ledb(2); // off
  //   return;
  // }
  // if ((now-blink_time) < ERROR_LED_TIME_ON_MS) {
  //   set_ledb(1); // red
  // } else if ((now-blink_time) < ERROR_LED_TIME_OFF_MS) {
  //   set_ledb(0); // off
  // } else {
  //   blink_time = now;
  // }
  // return;
  set_leda(7);
}
