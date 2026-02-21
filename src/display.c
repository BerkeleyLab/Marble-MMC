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
#define UPDATE_INTERVAL_MS            (20)
#define USE_FONT_17
// TODO - Warning! The lv_font_roboto_12 font is not monospace, so ensuring any
//        future value fits inside the initial bounding box is somewhat tedious.
// Set to 1 to enable current monitoring of FMC cards.
// This violates the "don't spam the I2C bus" policy of the MMC, so it's disabled
// by default and may be removed completely.
#define ENABLE_FMC_CURRENT_CHECK         (0)
#define PAGE_INFO_ITEMS           (8) // Number of items on the INFO page
// ==========================================================

extern lv_font_t lv_font_roboto_12, lv_font_roboto_mono_17, lv_font_fa;

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
/*
  CONFIG,
  SET_MAC,
// CONFIG pages:
*/

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
/*
	"Configuration", // new menu page
	"Set MAC Address"
*/

static display_page_t menu(unsigned btns);
static display_page_t config_warning(unsigned btns);
static display_page_t set_IP(unsigned btns);

static void scrollbar(int16_t percentage);
static void window_scrollbar(int16_t pos, int16_t total);
static void errorLight(unsigned frm);
static int16_t smooth_slide(int16_t title_y, int16_t title_y_goal);  // smooth title movement


// These will remain correct even if the page order changes in the enum above
#define PAGE_FIRST        (0)
#define PAGE_LAST         (NUMBER_OF_PAGES-1)

// static display_page_t current_page = PAGE_STATE; REMOVE

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
static const char label_error_init[] = "ERROR";
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
// IP: 192.168.100.101
static t_label label_ip;
// MAC: aa:bb:cc:dd:ee:ff
static t_label label_mac;
static t_label label_firmware;
static t_label label_SN;
static t_label label_MMC_chip_id;
static t_label label_PHY_id;
static t_label label_boot_id;

//Over-Temperature
//Powerdown
//                                      "Marble v1.4 Golden Image"
static const char label_marble_init[] = "Marble v1.X.X";
#define LABEL_MARBLE_REV_SIZE     (sizeof(label_marble_init)/sizeof(char))
static const char label_image_init[] = "xxxxxx image";
#define LABEL_IMAGE_SIZE     (sizeof(label_marble_init)/sizeof(char))

static const char label_ip_init[] = "IP: xxx.xxx.xxx.xxx";
#define LABEL_IP_SIZE     (sizeof(label_ip_init)/sizeof(char))

static const char label_mac_init[] = "MAC: xx:xx:xx:xx:xx:xx";
#define LABEL_MAC_SIZE     (sizeof(label_mac_init)/sizeof(char))

// ==========================================================

// ===================== Page POWER =========================

static t_label label_12v;
static t_label label_3v3;
static t_label label_2v5;
static t_label label_1v8;
static t_label label_1v0;


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

static t_label label_fan_max6639_1;
static t_label label_fan_max6639_2;
// ==========================================================

static void update_page(int refresh);
static void init_display_error(void);
static void init_page_state(void);
static void init_page_power(void);
static void init_page_temperature(void);
static void update_display_error(int refresh);
static display_page_t page_status(unsigned btns);
static display_page_t page_info(unsigned btns);
static display_page_t page_power(unsigned btns);
static display_page_t page_temperature(unsigned btns);
static display_page_t page_errors(unsigned btns);
static display_page_t page_clear_errors(unsigned btns);
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



void display_update(void) {
	static display_page_t current_page = MENU;
  uint8_t btns = uiBoardPoll();
  display_enable();
  static uint32_t last_update = 0;
  static Board_Status_t status = BOARD_STATUS_GOOD;
  Board_Status_t new_status = marble_get_status();

  // Check encoder knob
  int refresh = 0;
  if (do_update(last_update) || btns) {
    if (new_status == BOARD_STATUS_GOOD) {
      if (display_enabled) {
        // update_page(refresh);
        switch(current_page) {
          case MENU:
            current_page = menu(btns);
            break;
          case STATUS:
            current_page = page_status(btns); // FIXME
            break;
          case INFO:
            current_page = page_info(btns); // FIXME
            break;
          case POWER:
            current_page = page_power(btns);
            break;
          case TEMPERATURE:
            current_page = page_temperature(btns);
            break;
          case ERRORS:
            current_page = page_errors(btns);
            break;
          case CLEAR_ERRORS:
            current_page = page_clear_errors(btns);
            break;
          case CONFIG_WARNING:
            current_page = config_warning(btns);
            break;
          // case CONFIG:
          //   current_page = config(btns);
          //   break;
          case SET_IP:
            current_page = set_IP(btns);
            break;
          default:
            current_page = MENU;
            break;
        }
        send_fb();
      }
    } else {
      display_enable();
      if (status != new_status) {
        update_display_error(1);
      }
    }
    last_update = BSP_GET_SYSTICK();
    status = new_status;
  }
  if (display_enabled) {
    display_timeout();
    update_led();
  }
  return;
}


static display_page_t menu(unsigned btns)
{
	static unsigned frm = 0;
	static int16_t cursor_y_goal = 23;
	static int16_t text_cursor = DISPLAY_HEIGHT;
	static int16_t selection_id = 0;
	static uint8_t blink = 0;

	fill(0);

	if (btns & (1 << 0)) {  // left
		selection_id--;
	} else if (btns & (1 << 1)) {  // right
		selection_id++;
	}
	
	cursor_y_goal = 43-(20*selection_id);
	if(text_cursor == (text_cursor+cursor_y_goal)/2) {
		text_cursor = cursor_y_goal;
		if(selection_id >= CONFIG_WARNING+1) // bounce back at the end
			selection_id = CONFIG_WARNING;
		if(selection_id <= 0) // bounce back at the end
			selection_id = STATUS;
		cursor_y_goal = 43-(20*selection_id);
	}
	else {
		text_cursor=(text_cursor+cursor_y_goal)/2;
	}

	for(display_page_t p=STATUS; p<=CONFIG_WARNING; p++) {
    lv_init_label(&menu_item, DISPLAY_WIDTH/2, text_cursor + 20*(p-1), &lv_font_roboto_mono_17, MenuItems[p], LV_CENTER, true);
	}


	if(text_cursor == cursor_y_goal && selection_id >=STATUS && selection_id <= CONFIG_WARNING) {
    invertRoundedRect(0, LINE_SPACING_17+2, DISPLAY_WIDTH, 2*LINE_SPACING_17+1, (LINE_SPACING_17-2)/2); // white background for title
		if (btns & (1 << 2))  // push
     return selection_id;
	}
	else {
      emptyRoundedRect(0, LINE_SPACING_17+2, DISPLAY_WIDTH, 2*LINE_SPACING_17+1, (LINE_SPACING_17-2)/2,1); // white background for title
	}

	// if(text_cursor == cursor_y_goal && selection_id >=STATUS && selection_id <= CONFIG_WARNING) {
	// 	if (btns & (1 << 2))  // push
	// 		blink++;
	// 	if (blink > 0) {
	// 		if((blink>>1)%2)
	// 			// invertRoundedRect(6, 22, 250, 42, 10);
  //       invertRoundedRect(0, LINE_SPACING_17+2, DISPLAY_WIDTH, 2*LINE_SPACING_17+1, (LINE_SPACING_17-2)/2); // white background for title
	// 		else {
	// 			// emptyRoundedRect(6, 22, 250, 42, 10, 1);
  //       emptyRoundedRect(0, LINE_SPACING_17+2, DISPLAY_WIDTH, 2*LINE_SPACING_17+1, (LINE_SPACING_17-2)/2,1); // white background for title
	// 		}
	// 		blink++;
	// 		if (blink > 6) {// selection made
	// 			blink = 0;
	// 			return selection_id;
	// 		}
	// 	} else {
	// 		// invertRoundedRect(6, 22, 250, 42, 10);
  //     invertRoundedRect(0, LINE_SPACING_17+2, DISPLAY_WIDTH, 2*LINE_SPACING_17+1, (LINE_SPACING_17-2)/2); // white background for title
	// 	}
	// }
	// else {
	// 		// emptyRoundedRect(6, 22, 250, 42, 10, 1);
  //     emptyRoundedRect(0, LINE_SPACING_17+2, DISPLAY_WIDTH, 2*LINE_SPACING_17+1, (LINE_SPACING_17-2)/2,1); // white background for title
	// }
	
	errorLight(frm);
	frm++;
	return MENU; // stay in menu
}


static display_page_t config_warning(unsigned btns)
{
	static unsigned frm = 0;
	static int16_t selection_id = 0;

	const int rectangle_coordinates[][4] = {
		{DISPLAY_WIDTH/4 - 38, 44, DISPLAY_WIDTH/4 + 38, 64},  // CANCEL
		{(3*DISPLAY_WIDTH)/4 - 43, 44, (3*DISPLAY_WIDTH)/4 + 43, 64}  // Proceed
	};

	fill(0);

  lv_init_label(&label_warning_1, DISPLAY_WIDTH/2, 2, &lv_font_roboto_mono_17, "WARNING", LV_CENTER, true);
  lv_init_label(&label_warning_2, DISPLAY_WIDTH/2, 21, &lv_font_roboto_12, "You are about to change critical device settings", LV_CENTER, true);
  lv_init_label(&label_warning_cancel, (DISPLAY_WIDTH)/4, 44, &lv_font_roboto_mono_17, "Cancel", LV_CENTER, true);
  lv_init_label(&label_warning_proceed, (3*DISPLAY_WIDTH)/4, 44, &lv_font_roboto_mono_17, "Proceed", LV_CENTER, true);
		// navigate menu
		if (btns & (1 << 0)) {  // left
			selection_id--;
		} else if (btns & (1 << 1)) {  // right
			selection_id++;
		}
		if(selection_id >= 2) // bounce back at the end
			selection_id = 0;
		if(selection_id <= -1) // bounce back at the end
			selection_id = 1;
	

	if(selection_id == 0) {
		invertRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10);
		emptyRoundedRect(rectangle_coordinates[1][0]-1, rectangle_coordinates[1][1]-1, rectangle_coordinates[1][2]+1, rectangle_coordinates[1][3]+1, 10, 1);
		if (btns & (1 << 2)) { // push
			selection_id = 0; // default cancel button
			return MENU; // back to config menu
		}
	} else if (selection_id == 1) {
		emptyRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10, 1);
		invertRoundedRect(rectangle_coordinates[1][0]-1, rectangle_coordinates[1][1]-1, rectangle_coordinates[1][2]+1, rectangle_coordinates[1][3]+1, 10);
		if (btns & (1 << 2)) { // push
			selection_id = 0; // default cancel button
			return SET_IP; // back to config menu
		}
	}

	frm++;
	return CONFIG_WARNING; // stay in menu
}

/* configuration menu page - disabled for now
static display_page_t config(unsigned btns)
{
	static unsigned frm = 0;
	static unsigned n_lines=4, x=128, y=32;
	static uint8_t led = 0;
	static unsigned isBtn;
	int dx = 0, dy = 0;
	static int16_t cursor_y_goal = 23;
	static int16_t text_cursor = 20;
	static int16_t selection_id = 0;
	static uint8_t blink = 0;

	fill(0);

	if (btns & (1 << 0)) {  // left
		selection_id--;
	} else if (btns & (1 << 1)) {  // right
		selection_id++;
	}
	
	cursor_y_goal = 23-(20*selection_id);

	if(text_cursor == (text_cursor+cursor_y_goal)/2) {
		text_cursor = cursor_y_goal;
		if(selection_id >= 6) // bounce back at the end
			selection_id = 5;
		if(selection_id <= -1) // bounce back at the end
			selection_id = 0;
		cursor_y_goal = 23-(20*selection_id);
	}
	else {
		text_cursor=(text_cursor+cursor_y_goal)/2;
	}
	// if(text_cursor < cursor_y_goal) {
	// 	text_cursor=(text_cursor+cursor_y_goal)/2;
	// } else if (text_cursor > cursor_y_goal) {
	// 	text_cursor--;
	// }

  lv_init_label(&menu_item, 21, text_cursor, &lv_font_roboto_mono_17, "Main Menu", LV_LEFT, true);  // TODO: CLEAN THIS UP
  lv_init_label(&menu_item, 21, text_cursor+20, &lv_font_roboto_mono_17, "Set IP Address", LV_LEFT, true);
  lv_init_label(&menu_item, 21, text_cursor+40, &lv_font_roboto_mono_17, "Set MAC Address", LV_LEFT, true);
  lv_init_label(&menu_item, 21, text_cursor+60, &lv_font_roboto_mono_17, "Other stuff", LV_LEFT, true);
  lv_init_label(&menu_item, 21, text_cursor+80, &lv_font_roboto_mono_17, "More other stuff", LV_LEFT, true);
  lv_init_label(&menu_item, 21, text_cursor+100, &lv_font_roboto_mono_17, "...", LV_LEFT, true);


	// set_cursor(220, 4);
	// print_dec(cursor_y_goal);
	// set_cursor(220, 49);
	// print_dec(text_cursor);

	// if(text_cursor == cursor_y_goal && selection_id >=0 && selection_id <= 5){
		// set_font(&lv_font_roboto_mono_17);
		// set_cursor(224, 25);
		// print_str(CHEVRON_DOWN);
	if(text_cursor == cursor_y_goal && selection_id >=0 && selection_id <= 5) {
		if (btns & (1 << 2))  // push
			blink++;
		if (blink > 0) {
			if((blink>>1)%2)
				invertRoundedRect(6, 22, 250, 42, 10);
			else {
				emptyRoundedRect(6, 22, 250, 42, 10, 1);
			}
			blink++;
			if (blink > 6) {// selection made
				blink = 0;
				if(selection_id == 0)
					return MENU;
				else
					return selection_id + CONFIG; // page id start at 1
			}
		} else {
			invertRoundedRect(11, 22, 254, 42, 10);
		}
	}
	else {
			emptyRoundedRect(11, 22, 254, 42, 10, 1);
	}

	scrollbar((-(text_cursor - 23)*10)/10); // percentage
	errorLight(frm);
	frm++;
	return CONFIG; // stay in menu
}
*/

static display_page_t set_IP(unsigned btns)
{
	static unsigned frm = 0;
  static int16_t title_y_goal = 0;
  static int16_t title_y = 0;
	static unsigned isBtn;
	int dx = 0, dy = 0;
	static int16_t cursor_x1 = 0;
	static int16_t cursor_x2 = 0;
	static int16_t cursor_y1 = 0;
	static int16_t cursor_y2 = 0;
	static int16_t selection_id = 0;
	static uint8_t ip_bytes[4] = {0, 0, 0, 0};
	static bool ip_selected = 0;
	static bool ip_success = 0;
	const int rectangle_coordinates[][4] = {
		{DISPLAY_WIDTH/4 - 38, 44, DISPLAY_WIDTH/4 + 38, DISPLAY_HEIGHT},  // CANCEL
		{51, 20, 84, 39}, // IP 1
		{91, 20, 124, 39}, // IP 2
		{131, 20, 164, 39}, // IP 3
		{171, 20, 204, 39},  // IP 4
		{(DISPLAY_WIDTH*3)/4 - 38, 44, (DISPLAY_WIDTH*3)/4 + 38, DISPLAY_HEIGHT}  // SET IP
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
      if (btns & (1 << 0)) {  // left
        if(ip_bytes[selection_id - 1] == 0)
          ip_bytes[selection_id - 1] = 255;
        else
          ip_bytes[selection_id - 1]--;
      } else if (btns & (1 << 1)) {  // right
        if(ip_bytes[selection_id - 1] == 255)
          ip_bytes[selection_id - 1] = 0;
        else
          ip_bytes[selection_id - 1]++;
      }
    } else {
      // navigate menu
      if (btns & (1 << 0)) {  // left
        selection_id--;
      } else if (btns & (1 << 1)) {  // right
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
      if (btns & (1 << 2)) { // push
        title_y_goal = 23;
      }
    } else if (selection_id == 5) {
      cursor_x1 = rectangle_coordinates[4][0];
      cursor_y1 = rectangle_coordinates[4][1];
      cursor_x2 = rectangle_coordinates[4][2];
      cursor_y2 = rectangle_coordinates[4][3];
      emptyRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10, 1);
      invertRoundedRect(rectangle_coordinates[5][0]-1, rectangle_coordinates[5][1]-1, rectangle_coordinates[5][2]+1, rectangle_coordinates[5][3]+1, 10);
      if (btns & (1 << 2)) { // push
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
      if (btns & (1 << 2)) { // push
        ip_selected = !ip_selected; // select ip byte
      }
    }
    if(ip_success) {
      selection_id = 0; // select exit button
      char success_message[31];
      int rval = eeprom_read_ip_addr(ip_bytes, 4);
      snprintf(success_message, sizeof(success_message), "Success!  IP = %03d.%03d.%03d.%03d", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
      fillRect(0, DISPLAY_WIDTH, LINE_SPACING_17, 2*LINE_SPACING_17, 0x00); // erase background for title
      lv_init_label(&IP_item, DISPLAY_WIDTH/2, LINE_SPACING_17+4, &lv_font_roboto_12, success_message, LV_CENTER, true);
      if (btns & 0x03) {  // left or right
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
  if (refresh) {
    send_fb();
  }
  last_status = status;
  return;
}


static display_page_t page_status(unsigned btns) {
  static unsigned frm = 0;
  static int16_t title_y_goal = 0;
  static int16_t title_y = 23;
  static FPGAWD_State_t fpga_state = STATE_GOLDEN;
  static int16_t text_cursor = -DISPLAY_HEIGHT;
  static int16_t cursor_y_goal = LINE_SPACING_17;
  static int lm75_0_temp=0;
  static int max6639_ch1[2] = {0, 0};
  fill(0);

  if(btns & 4) { // push button to go back to menu
    title_y_goal = 23;
    text_cursor = -DISPLAY_HEIGHT;
  }

  title_y = smooth_slide(title_y, title_y_goal);

  // update only when title in position
  if(title_y == 0) {
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
    fpga_state = current_state;
    // uptime
    char label1[40];
    snprintf(label1, 40, "%s", print_uptime());
    lv_init_label(&label_uptime, (3*DISPLAY_WIDTH)/4+10, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, label1, LV_CENTER, true);
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
    lv_init_label(&label_ip, (1*DISPLAY_WIDTH)/4+8, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, ip_string, LV_CENTER, true);

    // titles
    lv_init_label(&label_temperature_max6639_2_label, (2*DISPLAY_WIDTH)/5+DISPLAY_WIDTH/10, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "FPGA", LV_CENTER, true);
    invertRoundedRect((2*DISPLAY_WIDTH)/5+1, text_cursor, (3*DISPLAY_WIDTH)/5-1, text_cursor + 12, 4); // white background for title
    emptyRoundedRect((2*DISPLAY_WIDTH)/5+1, text_cursor, (3*DISPLAY_WIDTH)/5-1, text_cursor + LINE_SPACING_12 + 13, 4,1); // erase background for title
    lv_init_label(&label_temperature_max6639_1_label, (1*DISPLAY_WIDTH)/5+DISPLAY_WIDTH/10, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "PSU", LV_CENTER, true);
    invertRoundedRect((1*DISPLAY_WIDTH)/5+1, text_cursor, (2*DISPLAY_WIDTH)/5-1, text_cursor + 12, 4); // white background for title
    emptyRoundedRect((1*DISPLAY_WIDTH)/5+1, text_cursor, (2*DISPLAY_WIDTH)/5-1, text_cursor + LINE_SPACING_12 + 13, 4,1); // erase background for title
    
    lv_init_label(&label_temperature_max6639_1_label, 4, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, "IP", LV_LEFT, true);
    invertRoundedRect(0, text_cursor + 2*LINE_SPACING_12, 17, text_cursor + DISPLAY_HEIGHT - LINE_SPACING_17, 4); // white background for title
    emptyRoundedRect(0, text_cursor + 2*LINE_SPACING_12, (DISPLAY_WIDTH)/2-1, text_cursor + DISPLAY_HEIGHT - LINE_SPACING_17, 4,1); // erase background for title

    lv_init_label(&label_temperature_max6639_1_label, (DISPLAY_WIDTH)/2+4, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, "Up", LV_LEFT, true);
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


static display_page_t page_info(unsigned btns) {
  static unsigned frm = 0;
	static int16_t cursor_y_goal = 0;
  static int16_t title_y_goal = 0;
  static int16_t title_y = 23;
	static int16_t text_cursor = -2*DISPLAY_HEIGHT;
	static int16_t selection_id = 0;

	fill(0);

  // button handling
	if (btns & (1 << 0)) {  // scroll left
		selection_id--;
	} else if (btns & (1 << 1)) {  // scroll right
		selection_id++;
	} else if(btns & 4) { // push button to go back to menu - init values
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
  //send_window_4((unsigned)label_ip.x0, (unsigned)label_ip.y0, (unsigned)label_ip.x1, (unsigned)label_ip.y1, uint8_t *data);
  if(title_y == 23) {
      title_y_goal = 0;
      return MENU; // go back to menu
  }
  return INFO; // TODO - return whether we need to update the screen (if a value has changed)
}


static display_page_t page_power(unsigned btns) {
  static unsigned frm = 0;
  static int16_t title_y_goal = 0;
  static int16_t title_y = 23;
  static int16_t text_cursor = -DISPLAY_HEIGHT;
  static int16_t cursor_y_goal = LINE_SPACING_17;

  static int vin=0, iin=0;
  static int v3V3=0, i3V3=0;
  static int v2V5=0, i2V5=0;
  static int v1V8=0, i1V8=0;
  static int v1V0=0, i1V0=0;

  fill(0);

  if(btns & 4) { // push button to go back to menu
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
    static uint8_t fmc_status = 0;
    uint8_t new_fmc_status = marble_FMC_status();
    uint8_t mask = (1 << M_FMC_STATUS_FMC1_PWR);
    if (new_fmc_status & mask) {
      lv_init_label(&label_3v3, (5*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14+1, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, "ON", LV_CENTER, true);
      if (frm%100 == 0) { // update every 100 frames
        new_fmc_current = ina219_getShuntVoltage(INA219_FMC1);
        fmc1_current_amps = INA219_SHUNT_VOLTAGE_TO_CURRENT(new_fmc_current);
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
      if ((frm+500)%100 == 0) { // update every 100 frames
        new_fmc_current = ina219_getShuntVoltage(INA219_FMC2);
        fmc2_current_amps = INA219_SHUNT_VOLTAGE_TO_CURRENT(new_fmc_current);
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
    // emptyRoundedRect((6*DISPLAY_WIDTH)/7+1, LINE_SPACING_17, (7*DISPLAY_WIDTH)/7, DISPLAY_HEIGHT, 4,1); // erase background for title
    lv_init_label(&label_1v8, (5*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "FMC1", LV_CENTER, true);
    invertRoundedRect((5*DISPLAY_WIDTH)/7+1, text_cursor, (6*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
    // emptyRoundedRect((5*DISPLAY_WIDTH)/7+1, LINE_SPACING_17, (6*DISPLAY_WIDTH)/7-1, DISPLAY_HEIGHT, 4,1); // erase background for title
    lv_init_label(&label_1v8, (4*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "1V0", LV_CENTER, true);
    invertRoundedRect((4*DISPLAY_WIDTH)/7+1, text_cursor, (5*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
    // emptyRoundedRect((4*DISPLAY_WIDTH)/7+1, LINE_SPACING_17, (5*DISPLAY_WIDTH)/7-1, DISPLAY_HEIGHT, 4,1); // erase background for title
    lv_init_label(&label_1v8, (3*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "1V8", LV_CENTER, true);
    invertRoundedRect((3*DISPLAY_WIDTH)/7+1, text_cursor, (4*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
    // emptyRoundedRect((3*DISPLAY_WIDTH)/7+1, LINE_SPACING_17, (4*DISPLAY_WIDTH)/7-1, DISPLAY_HEIGHT, 4,1); // erase background for title
    lv_init_label(&label_1v8, (2*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "2V5", LV_CENTER, true);
    invertRoundedRect((2*DISPLAY_WIDTH)/7+1, text_cursor, (3*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
    // invertRoundedRect((2*DISPLAY_WIDTH)/7+1, LINE_SPACING_17, (3*DISPLAY_WIDTH)/7-1, DISPLAY_HEIGHT, 4); // white background for title
    // emptyRoundedRect((2*DISPLAY_WIDTH)/7+1, LINE_SPACING_17, (3*DISPLAY_WIDTH)/7-1, LINE_SPACING_17+12, 4,1); // erase background for title
    // emptyRoundedRect((2*DISPLAY_WIDTH)/7+1, LINE_SPACING_17, (3*DISPLAY_WIDTH)/7-1, DISPLAY_HEIGHT, 4,1); // erase background for title
    lv_init_label(&label_1v8, (1*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "3V3", LV_CENTER, true);
    invertRoundedRect((1*DISPLAY_WIDTH)/7+1, text_cursor, (2*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
    // emptyRoundedRect((1*DISPLAY_WIDTH)/7+1, LINE_SPACING_17, (2*DISPLAY_WIDTH)/7-1, LINE_SPACING_17+12, 4,1); // erase background for title
    // emptyRoundedRect((1*DISPLAY_WIDTH)/7+1, LINE_SPACING_17, (2*DISPLAY_WIDTH)/7-1, DISPLAY_HEIGHT, 4,1); // erase background for title
    lv_init_label(&label_1v8, (0*DISPLAY_WIDTH)/7+DISPLAY_WIDTH/14, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "VIN", LV_CENTER, true);
    invertRoundedRect((0*DISPLAY_WIDTH)/7, text_cursor, (1*DISPLAY_WIDTH)/7-1, text_cursor+12, 4); // white background for title
    // invertRoundedRect((0*DISPLAY_WIDTH)/7, LINE_SPACING_17, (1*DISPLAY_WIDTH)/7-1, DISPLAY_HEIGHT, 4); // white background for title
    // emptyRoundedRect((0*DISPLAY_WIDTH)/7, LINE_SPACING_17, (1*DISPLAY_WIDTH)/7-1, LINE_SPACING_17+12, 4,1); // erase background for title
    // emptyRoundedRect((0*DISPLAY_WIDTH)/7, LINE_SPACING_17, (1*DISPLAY_WIDTH)/7-1, DISPLAY_HEIGHT, 4,1); // erase background for title
    // fillRect((0*DISPLAY_WIDTH)/7, (1*DISPLAY_WIDTH)/7-1, LINE_SPACING_17+12, DISPLAY_HEIGHT, 0x01); // erase background for title

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


static display_page_t page_temperature(unsigned btns) {
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

  fill(0);

  if(btns & 4) { // push button to go back to menu
    title_y_goal = 23;
  }

  title_y = smooth_slide(title_y, title_y_goal);

  // Display contents only when title in position
  if(title_y == 0) {
    text_cursor = smooth_slide(text_cursor, cursor_y_goal);

    // LM75_0
    lm75_0_temp = LM75_get_cached_temperature(LM75_0); // does not trigger readout so OK to call frequently
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
    snprintf(label, 9, "Fan: %d", fan_speed);
    lv_init_label(&label_fan_max6639_1, (2*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    // MAX6639 Ch 2
    max6639_ch2[0] = max6639_get_cached_temp(MAX6639_TEMP_CH2);
    max6639_ch2[1] = max6639_get_cached_temp(MAX6639_TEMP_EXT_CH2);
    snprintf(label, LABEL_TEMPERATURE_MAX6639_2_SIZE, LABEL_TEMPERATURE_MAX6639_2_FMT, MAX6639_GET_TEMP_DOUBLE(max6639_ch2[0], max6639_ch2[1]));
    lv_init_label(&label_temperature_max6639_2, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 1*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    snprintf(label, 9, "Fan: %d", fan_speed);
    lv_init_label(&label_fan_max6639_2, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 2*LINE_SPACING_12, &lv_font_roboto_12, label, LV_CENTER, true);
    // titles
    lv_init_label(&label_temperature_max6639_2_label, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "PSU2", LV_CENTER, true);
    invertRoundedRect((3*DISPLAY_WIDTH)/4+1, text_cursor, (4*DISPLAY_WIDTH)/4-1, text_cursor+12, 4); // white background for title
    lv_init_label(&label_temperature_max6639_1_label, (2*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "FPGA", LV_CENTER, true);
    invertRoundedRect((2*DISPLAY_WIDTH)/4+1, text_cursor, (3*DISPLAY_WIDTH)/4-1, text_cursor+12, 4); // white background for title
    lv_init_label(&label_temperature_lm75_1_label, (1*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "MMC", LV_CENTER, true);
    invertRoundedRect((1*DISPLAY_WIDTH)/4+1, text_cursor, (2*DISPLAY_WIDTH)/4-1, text_cursor+12, 4); // white background for title
    lv_init_label(&label_temperature_lm75_0_label, (0*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8, text_cursor + 0*LINE_SPACING_12, &lv_font_roboto_12, "PSU", LV_CENTER, true);
    invertRoundedRect((0*DISPLAY_WIDTH)/4, text_cursor, (1*DISPLAY_WIDTH)/4-1, text_cursor+12, 4); // white background for title
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


static display_page_t page_errors(unsigned btns) {
  static unsigned frm = 0;
  static int16_t title_y_goal = 0;
  static int16_t title_y = 23;
	static int16_t cursor_y_goal = LINE_SPACING_17;
	static int16_t text_cursor = -2*DISPLAY_HEIGHT;
	static int16_t selection_id = 0;

  fill(0);

  // button handling
	if (btns & (1 << 0)) {  // scroll left
		selection_id--;
	} else if (btns & (1 << 1)) {  // scroll right
		selection_id++;
	} else if(btns & 4) { // push button to go back to menu - init values
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
          snprintf(error, 33, "%d", error_info.error_count);
          lv_init_label(&label_error, (2*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8+7, text_cursor + (i+1)*LINE_SPACING_12, &lv_font_roboto_12, error, LV_CENTER, true);
          // print last time
          error_time_ago = uptime - error_info.last_occurrence_time_s;
          if(error_time_ago/86400) { // days
              snprintf(error, 33, "%d days", error_time_ago/86400);
              lv_init_label(&label_error, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8-4, text_cursor + (i+1)*LINE_SPACING_12, &lv_font_roboto_12, error, LV_CENTER, true);
          } else if (error_time_ago/3600) { // hours
              snprintf(error, 33, "%d h", error_time_ago/3600);
              lv_init_label(&label_error, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8-4, text_cursor + (i+1)*LINE_SPACING_12, &lv_font_roboto_12, error, LV_CENTER, true);
          } else if (error_time_ago/60) { // minutes
              snprintf(error, 33, "%d min", error_time_ago/60);
              lv_init_label(&label_error, (3*DISPLAY_WIDTH)/4+DISPLAY_WIDTH/8-4, text_cursor + (i+1)*LINE_SPACING_12, &lv_font_roboto_12, error, LV_CENTER, true);
          } else { // seconds
              snprintf(error, 33, "%d s", error_time_ago);
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


static display_page_t page_clear_errors(unsigned btns)
{
	static unsigned frm = 0;
	static int16_t selection_id = 0;

	const int rectangle_coordinates[][4] = {
		{DISPLAY_WIDTH/4 - 38, 44, DISPLAY_WIDTH/4 + 38, 64},  // CANCEL
		{(3*DISPLAY_WIDTH)/4 - 38, 44, (3*DISPLAY_WIDTH)/4 + 38, 64}  // Proceed
	};

	fill(0);

  lv_init_label(&label_warning_1, DISPLAY_WIDTH/2, 2, &lv_font_roboto_mono_17, "Clear error light", LV_CENTER, true);
  lv_init_label(&label_warning_2, DISPLAY_WIDTH/2, 21, &lv_font_roboto_12, "This action will not clear error logs", LV_CENTER, true);
  lv_init_label(&label_warning_cancel, DISPLAY_WIDTH/4, 44, &lv_font_roboto_mono_17, "Cancel", LV_CENTER, true);
  lv_init_label(&label_warning_proceed, (3*DISPLAY_WIDTH)/4, 44, &lv_font_roboto_mono_17, "Reset", LV_CENTER, true);
		// navigate menu
		if (btns & (1 << 0)) {  // left
			selection_id--;
		} else if (btns & (1 << 1)) {  // right
			selection_id++;
		}
		if(selection_id >= 2)
			selection_id = 0;
		if(selection_id <= -1)
			selection_id = 1;
	

	if(selection_id == 0) {
		invertRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10);
		emptyRoundedRect(rectangle_coordinates[1][0]-1, rectangle_coordinates[1][1]-1, rectangle_coordinates[1][2]+1, rectangle_coordinates[1][3]+1, 10, 1);
		if (btns & (1 << 2)) { // push
			selection_id = 0; // default cancel button
			return MENU; // back to config menu
		}
	} else if (selection_id == 1) {
		emptyRoundedRect(rectangle_coordinates[0][0]-1, rectangle_coordinates[0][1]-1, rectangle_coordinates[0][2]+1, rectangle_coordinates[0][3]+1, 10, 1);
		invertRoundedRect(rectangle_coordinates[1][0]-1, rectangle_coordinates[1][1]-1, rectangle_coordinates[1][2]+1, rectangle_coordinates[1][3]+1, 10);
		if (btns & (1 << 2)) { // push
			selection_id = 0; // default cancel button
      error_reset_time = marble_uptime_seconds();
			return MENU; 
    }
	}

	frm++;
	return CLEAR_ERRORS; // stay in menu
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
  printf("        Initializing UI board\r\n");
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
  // update_page(1); // force refresh
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
  lv_init_label(&label_marble_rev, 0, 0*LINE_SPACING, &DEFAULT_FONT, label_marble_init, LV_LEFT, false);
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
  snprintf(ps, (size_t)maxlen, "%03d.%03d.%03d.%03d", ip[0], ip[1], ip[2], ip[3]);
  return;
}

static void format_mac_addr(uint8_t *mac, char *ps, int maxlen) {
  snprintf(ps, (size_t)maxlen, "MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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


static void scrollbar(int16_t percentage) {
  invertRoundedRect(2, 2 + (percentage*47)/100, 6, 14 + (percentage*47)/100, 2); // FIXME: take percentage
  emptyRoundedRect(1, 1, 7, DISPLAY_HEIGHT - 2, 3, 1);
}

static void window_scrollbar(int16_t pos, int16_t total){
  // total += LINE_SPACING_17;
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
  invertRoundedRect(DISPLAY_WIDTH - 6, blockTop, DISPLAY_WIDTH - 2, blockBottom, 2); 
  emptyRoundedRect(DISPLAY_WIDTH - 7, 1 + LINE_SPACING_17, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 2, 3, 1);
}


static void errorLight(unsigned frm) {
  if(marble_last_error_tick() > error_reset_time){  // if error occurred after last error clear, highlight it
    lv_init_label(&error_light, 227, 3, &lv_font_roboto_12, "ERROR", LV_CENTER, true);
    if((frm>>2)%2)
      invertRoundedRect(200, 1, 254, 17, 8);
  }
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
