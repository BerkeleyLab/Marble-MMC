SOURCES += $(SOURCE_DIR)/i2c_pm.c \
$(SOURCE_DIR)/i2c_fpga.c \
$(SOURCE_DIR)/mailbox.c \
$(SOURCE_DIR)/phy_mdio.c \
$(SOURCE_DIR)/hexrec.c \
$(SOURCE_DIR)/syscalls.c \
$(SOURCE_DIR)/main.c \
$(SOURCE_DIR)/uart_fifo.c \
$(SOURCE_DIR)/console.c \
$(SOURCE_DIR)/st-eeprom.c \
$(SOURCE_DIR)/pmbus.c \
$(SOURCE_DIR)/ltm4673.c \
$(SOURCE_DIR)/watchdog.c \
$(SOURCE_DIR)/refsip.c \
$(SOURCE_DIR)/system.c \

OLED_FW_DIR=$(OLED_DIR)/firmware
OLED_LIB_DIR=$(OLED_FW_DIR)/lib

OLED_LIB_SOURCES=aa_line.c \
								 frame_buffer.c \
								 gui.c \
								 lv_font.c \
								 ssd1322.c \
								 ui_board.c

OLED_FONTS=lv_font_fa.c lv_font_roboto_12.c lv_font_roboto_mono_17.c

OLED_SOURCES=$(addprefix $(OLED_LIB_DIR)/, $(OLED_LIB_SOURCES)) $(addprefix $(OLED_FW_DIR)/, $(OLED_FONTS)) $(SOURCE_DIR)/display.c
