/* File: src/mailbox_def.c
 * Desc: Auto-generated by scripts/mkmbox.py
 */

/* ============= DO NOT EDIT THIS AUTO-GENERATED FILE DIRECTLY ============== */
/* ============== Define mailbox structure in scripts/mbox.def ============== */

#include "mailbox_def.h"

uint32_t mailbox_get_hash(void) {
  return 0x16db2127;
}

void mailbox_update_output(void) {
  {
    // Page 3
    uint8_t page[MB3_SIZE];
    int val;
    val = mbox_get_update_count();
    page[MB3_COUNT_1] = (uint8_t)((val >> 8) & 0xff);
    page[MB3_COUNT_0] = (uint8_t)((val >> 0) & 0xff);
    LM75_read(LM75_0, LM75_TEMP, (int *)&val);
    page[MB3_LM75_0_1] = (uint8_t)((val >> 8) & 0xff);
    page[MB3_LM75_0_0] = (uint8_t)((val >> 0) & 0xff);
    LM75_read(LM75_1, LM75_TEMP, (int *)&val);
    page[MB3_LM75_1_1] = (uint8_t)((val >> 8) & 0xff);
    page[MB3_LM75_1_0] = (uint8_t)((val >> 0) & 0xff);
    page[MB3_FMC_ST] = marble_FMC_status();
    page[MB3_PWR_ST] = marble_PWR_status();
    page[MB3_MGTMUX_ST] = marble_MGTMUX_status();
    val = GIT_REV_32BIT;
    page[MB3_GIT32_3] = (uint8_t)((val >> 24) & 0xff);
    page[MB3_GIT32_2] = (uint8_t)((val >> 16) & 0xff);
    page[MB3_GIT32_1] = (uint8_t)((val >> 8) & 0xff);
    page[MB3_GIT32_0] = (uint8_t)((val >> 0) & 0xff);
    // Write page data
    mbox_write_page(3, MB3_SIZE, page);
  }
  {
    // Page 4
    uint8_t page[MB4_SIZE];
    page[MB4_MAX_T1_HI] = return_max6639_reg(MAX6639_TEMP_CH1);
    page[MB4_MAX_T1_LO] = return_max6639_reg(MAX6639_TEMP_EXT_CH1);
    page[MB4_MAX_T2_HI] = return_max6639_reg(MAX6639_TEMP_CH2);
    page[MB4_MAX_T2_LO] = return_max6639_reg(MAX6639_TEMP_EXT_CH2);
    page[MB4_MAX_F1_TACH] = return_max6639_reg(MAX6639_FAN1_TACH_CNT);
    page[MB4_MAX_F2_TACH] = return_max6639_reg(MAX6639_FAN2_TACH_CNT);
    page[MB4_MAX_F1_DUTY] = return_max6639_reg(MAX6639_FAN1_DUTY);
    page[MB4_MAX_F2_DUTY] = return_max6639_reg(MAX6639_FAN2_DUTY);
    page[MB4_PCB_REV] = marble_get_board_id();
    int val;
    val = mbox_get_update_count();
    page[MB4_COUNT_1] = (uint8_t)((val >> 8) & 0xff);
    page[MB4_COUNT_0] = (uint8_t)((val >> 0) & 0xff);
    val = mailbox_get_hash();
    page[MB4_HASH_3] = (uint8_t)((val >> 24) & 0xff);
    page[MB4_HASH_2] = (uint8_t)((val >> 16) & 0xff);
    page[MB4_HASH_1] = (uint8_t)((val >> 8) & 0xff);
    page[MB4_HASH_0] = (uint8_t)((val >> 0) & 0xff);
    // Write page data
    mbox_write_page(4, MB4_SIZE, page);
  }
  {
    // Page 5
    uint8_t page[MB5_SIZE];
    page[MB5_I2C_BUS_STATUS] = (uint8_t)getI2CBusStatus();
    // Write page data
    mbox_write_page(5, MB5_SIZE, page);
  }
  {
    // Page 6
    uint8_t page[MB6_SIZE];
    page[MB6_FSYNTH_I2C_ADDR] = (uint8_t)fsynthGetAddr();
    page[MB6_FSYNTH_CONFIG] = (uint8_t)fsynthGetConfig();
    int val;
    val = fsynthGetFreq();
    page[MB6_FSYNTH_FREQ_3] = (uint8_t)((val >> 24) & 0xff);
    page[MB6_FSYNTH_FREQ_2] = (uint8_t)((val >> 16) & 0xff);
    page[MB6_FSYNTH_FREQ_1] = (uint8_t)((val >> 8) & 0xff);
    page[MB6_FSYNTH_FREQ_0] = (uint8_t)((val >> 0) & 0xff);
    // Write page data
    mbox_write_page(6, MB6_SIZE, page);
  }
  return;
}

void mailbox_update_input(void) {
  {
    // Page 2
    uint8_t page[MB2_SIZE];
    mbox_read_page(2, MB2_SIZE, page);
    marble_MGTMUX_config(page[MB2_FMC_MGT_CTL], 0, 0);
    mbox_write_entry(MB2_FMC_MGT_CTL, page[MB2_FMC_MGT_CTL] & 0x55);
  }
  {
    // Page 5
    uint8_t page[MB5_SIZE];
    mbox_read_page(5, MB5_SIZE, page);
    mbox_handleI2CBusStatusMsg(page[MB5_I2C_BUS_STATUS]);
  }
  return;
}

void mailbox_read_print_all(void) {
  {
    // Page 2
    uint8_t page[MB2_SIZE];
    mbox_read_page(2, MB2_SIZE, page);
    MBOX_PRINT_PAGE(2);
  }
  {
    // Page 3
    uint8_t page[MB3_SIZE];
    mbox_read_page(3, MB3_SIZE, page);
    MBOX_PRINT_PAGE(3);
  }
  {
    // Page 4
    uint8_t page[MB4_SIZE];
    mbox_read_page(4, MB4_SIZE, page);
    MBOX_PRINT_PAGE(4);
  }
  {
    // Page 5
    uint8_t page[MB5_SIZE];
    mbox_read_page(5, MB5_SIZE, page);
    MBOX_PRINT_PAGE(5);
  }
  {
    // Page 6
    uint8_t page[MB6_SIZE];
    mbox_read_page(6, MB6_SIZE, page);
    MBOX_PRINT_PAGE(6);
  }
  return;
}
