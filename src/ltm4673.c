/* All the LTM4673-related stuff in one place
 * LTM4673 is new power management chip used on Marble >= 1.4
 */

#include <stdio.h>
#include "ltm4673.h"
#include "pmbus.h"
#include "marble_api.h"

/* ============================ Static Variables ============================ */
extern I2C_BUS I2C_PM;
static uint8_t ltm4673_page = 0;

#define PM_LIMITS_COLS 4
// Linear11 Signed PMBus data format
#define LTM4673_L16_LIMIT_MV(cmd, min_mv, max_mv) \
  {cmd, 0xff, min_mv, max_mv}

// Linear16 Unsigned PMBus data format
#define LTM4673_MV_TO_L11_LO(val_mv)                ((uint8_t)(MV_TO_L11(val_mv) & 0xff))
#define LTM4673_MV_TO_L11_HI(val_mv)                ((uint8_t)((MV_TO_L11(val_mv) >> 8) & 0xff))
#define LTM4673_L11_LIMIT_MV(cmd, min_mv, max_mv) \
  {cmd, 0xff, LTM4673_MV_TO_L11_LO(min_mv), LTM4673_MV_TO_L11_LO(max_mv), 0xff, \
   LTM4673_MV_TO_L11_HI(min_mv), LTM4673_MV_TO_L11_HI(max_mv)}

// Prevent Writes to this Register
#define LTM4673_PROTECT(cmd) {cmd, 0, 0, 0}
// NOTE! I need to operate on the DECODED values (not on their encodings) to properly
// determine if min <= X <= max for a given value X.
const int _ltm4673_pageff_limits [][PM_LIMITS_COLS] = {
// If pass_mask is 0, prevents writes to the register for the selected page
//{command_code, pass_mask,  min, max}
  {LTM4673_PAGE, 0xff, 0x00, 0xff},
  {LTM4673_VIN_OV_FAULT_LIMIT, 0xffff, 12500, 16000}, // 12.5V to 16.0V
};
const int _ltm4673_page0_limits [][PM_LIMITS_COLS] = {
  {LTM4673_VOUT_COMMAND, 950, 1050},  // 0.95V to 1.05V
};
const int _ltm4673_page1_limits [][PM_LIMITS_COLS] = {
  {LTM4673_VOUT_COMMAND, 1750, 1850},  // 1.75V to 1.85V
};
const int _ltm4673_page2_limits [][PM_LIMITS_COLS] = {
  {LTM4673_VOUT_COMMAND, 2450, 2550},  // 2.45V to 2.55V
};
const int _ltm4673_page3_limits [][PM_LIMITS_COLS] = {
  {LTM4673_VOUT_COMMAND, 3250, 3350},  // 3.25V to 3.35V
};

typedef struct {
  unsigned int nrows;
  const int (*array)[PM_LIMITS_COLS];
} _ltm4673_limits_t;
static const _ltm4673_limits_t ltm4673_pageff_limits = {
  .nrows = sizeof(_ltm4673_pageff_limits)/(sizeof(int)*PM_LIMITS_COLS),
  .array = _ltm4673_pageff_limits
};
static const _ltm4673_limits_t ltm4673_page0_limits = {
  .nrows = sizeof(_ltm4673_page0_limits)/(sizeof(int)*PM_LIMITS_COLS),
  .array = _ltm4673_page0_limits
};
static const _ltm4673_limits_t ltm4673_page1_limits = {
  .nrows = sizeof(_ltm4673_page1_limits)/(sizeof(int)*PM_LIMITS_COLS),
  .array = _ltm4673_page1_limits
};
static const _ltm4673_limits_t ltm4673_page2_limits = {
  .nrows = sizeof(_ltm4673_page2_limits)/(sizeof(int)*PM_LIMITS_COLS),
  .array = _ltm4673_page2_limits
};
static const _ltm4673_limits_t ltm4673_page3_limits = {
  .nrows = sizeof(_ltm4673_page3_limits)/(sizeof(int)*PM_LIMITS_COLS),
  .array = _ltm4673_page3_limits
};
static const _ltm4673_limits_t ltm4673_limits[] = {
  ltm4673_page0_limits,
  ltm4673_page1_limits,
  ltm4673_page2_limits,
  ltm4673_page3_limits,
  ltm4673_pageff_limits,
};
#define LTM4673_PAGE_INDEX(page)                        (page > 3 ? 4 : page)
#define LTM4673_NPAGES_CHECK  sizeof(ltm4673_limits)/sizeof(_ltm4673_limits_t)
#define LTM4673_NPAGES  (5)
#define LTM4673_LIMIT_GET_COMMAND(page, row) \
  ltm4673_limits[LTM4673_PAGE_INDEX(page)].array[row][0]
#define LTM4673_LIMIT_GET_MASK(page, row) \
  ltm4673_limits[LTM4673_PAGE_INDEX(page)].array[row][1]
#define LTM4673_LIMIT_GET_MIN(page, row) \
  ltm4673_limits[LTM4673_PAGE_INDEX(page)].array[row][2]
#define LTM4673_LIMIT_GET_MAX(page, row) \
  ltm4673_limits[LTM4673_PAGE_INDEX(page)].array[row][3]

static uint8_t ltm4673_addrs[] = {0xb8, 0xba, 0xbc, 0xbe, 0xc0, 0xc2, 0xc4, 0xc6, 0xc8};
#define LTM4673_MATCH_ADDRS     (sizeof(ltm4673_addrs)/sizeof(uint8_t))

#define HAL_OK (0)

#define LTM4673_ENCODING_RAW  (0)
#define LTM4673_ENCODING_L11  (1)
#define LTM4673_ENCODING_L16  (2)
#define LTM4673_UNUSED     (0xff)

static const uint8_t ltm4673_encodings[256] = {
  LTM4673_ENCODING_RAW, // LTM4673_PAGE
  LTM4673_ENCODING_RAW, // LTM4673_OPERATION
  LTM4673_ENCODING_RAW, // LTM4673_ON_OFF_CONFIG
  LTM4673_ENCODING_RAW, // LTM4673_CLEAR_FAULTS
  LTM4673_UNUSED, // 0x04
  LTM4673_UNUSED, // 0x05
  LTM4673_UNUSED, // 0x06
  LTM4673_UNUSED, // 0x07
  LTM4673_UNUSED, // 0x08
  LTM4673_UNUSED, // 0x09
  LTM4673_UNUSED, // 0x0a
  LTM4673_UNUSED, // 0x0b
  LTM4673_UNUSED, // 0x0c
  LTM4673_UNUSED, // 0x0d
  LTM4673_UNUSED, // 0x0e
  LTM4673_UNUSED, // 0x0f
  LTM4673_ENCODING_RAW, // LTM4673_WRITE_PROTECT
  LTM4673_UNUSED, // 0x11
  LTM4673_UNUSED, // 0x12
  LTM4673_UNUSED, // 0x13
  LTM4673_UNUSED, // 0x14
  LTM4673_ENCODING_RAW, // LTM4673_STORE_USER_ALL
  LTM4673_ENCODING_RAW, // LTM4673_RESTORE_USER_ALL
  LTM4673_UNUSED, // 0x17
  LTM4673_UNUSED, // 0x18
  LTM4673_ENCODING_RAW, // LTM4673_CAPABILITY
  LTM4673_UNUSED, // 0x1a
  LTM4673_UNUSED, // 0x1b
  LTM4673_UNUSED, // 0x1c
  LTM4673_UNUSED, // 0x1d
  LTM4673_UNUSED, // 0x1e
  LTM4673_UNUSED, // 0x1f
  LTM4673_ENCODING_RAW, // LTM4673_VOUT_MODE
  LTM4673_ENCODING_L16, // LTM4673_VOUT_COMMAND
  LTM4673_UNUSED, // 0x22
  LTM4673_UNUSED, // 0x23
  LTM4673_ENCODING_L16, // LTM4673_VOUT_MAX
  LTM4673_ENCODING_L16, // LTM4673_VOUT_MARGIN_HIGH
  LTM4673_ENCODING_L16, // LTM4673_VOUT_MARGIN_LOW
  LTM4673_UNUSED, // 0x27
  LTM4673_UNUSED, // 0x28
  LTM4673_UNUSED, // 0x29
  LTM4673_UNUSED, // 0x2a
  LTM4673_UNUSED, // 0x2b
  LTM4673_UNUSED, // 0x2c
  LTM4673_UNUSED, // 0x2d
  LTM4673_UNUSED, // 0x2e
  LTM4673_UNUSED, // 0x2f
  LTM4673_UNUSED, // 0x30
  LTM4673_UNUSED, // 0x31
  LTM4673_UNUSED, // 0x32
  LTM4673_UNUSED, // 0x33
  LTM4673_UNUSED, // 0x34
  LTM4673_ENCODING_L11, // LTM4673_VIN_ON
  LTM4673_ENCODING_L11, // LTM4673_VIN_OFF
  LTM4673_UNUSED, // 0x37
  LTM4673_ENCODING_L11, // LTM4673_IOUT_CAL_GAIN
  LTM4673_UNUSED, // 0x39
  LTM4673_UNUSED, // 0x3a
  LTM4673_UNUSED, // 0x3b
  LTM4673_UNUSED, // 0x3c
  LTM4673_UNUSED, // 0x3d
  LTM4673_UNUSED, // 0x3e
  LTM4673_UNUSED, // 0x3f
  LTM4673_ENCODING_L16, // LTM4673_VOUT_OV_FAULT_LIMIT
  LTM4673_ENCODING_RAW, // LTM4673_VOUT_OV_FAULT_RESPONSE
  LTM4673_ENCODING_L16, // LTM4673_VOUT_OV_WARN_LIMIT
  LTM4673_ENCODING_L16, // LTM4673_VOUT_UV_WARN_LIMIT
  LTM4673_ENCODING_L16, // LTM4673_VOUT_UV_FAULT_LIMIT
  LTM4673_ENCODING_RAW, // LTM4673_VOUT_UV_FAULT_RESPONSE
  LTM4673_ENCODING_L11, // LTM4673_IOUT_OC_FAULT_LIMIT
  LTM4673_ENCODING_RAW, // LTM4673_IOUT_OC_FAULT_RESPONSE
  LTM4673_UNUSED, // 0x48
  LTM4673_UNUSED, // 0x49
  LTM4673_ENCODING_L11, // LTM4673_IOUT_OC_WARN_LIMIT
  LTM4673_ENCODING_L11, // LTM4673_IOUT_UC_FAULT_LIMIT
  LTM4673_ENCODING_RAW, // LTM4673_IOUT_UC_FAULT_RESPONSE
  LTM4673_UNUSED, // 0x4d
  LTM4673_UNUSED, // 0x4e
  LTM4673_ENCODING_L11, // LTM4673_OT_FAULT_LIMIT
  LTM4673_ENCODING_RAW, // LTM4673_OT_FAULT_RESPONSE
  LTM4673_ENCODING_L11, // LTM4673_OT_WARN_LIMIT
  LTM4673_ENCODING_L11, // LTM4673_UT_WARN_LIMIT
  LTM4673_ENCODING_L11, // LTM4673_UT_FAULT_LIMIT
  LTM4673_ENCODING_RAW, // LTM4673_UT_FAULT_RESPONSE
  LTM4673_ENCODING_L11, // LTM4673_VIN_OV_FAULT_LIMIT
  LTM4673_ENCODING_RAW, // LTM4673_VIN_OV_FAULT_RESPONSE
  LTM4673_ENCODING_L11, // LTM4673_VIN_OV_WARN_LIMIT
  LTM4673_ENCODING_L11, // LTM4673_VIN_UV_WARN_LIMIT
  LTM4673_ENCODING_L11, // LTM4673_VIN_UV_FAULT_LIMIT
  LTM4673_ENCODING_RAW, // LTM4673_VIN_UV_FAULT_RESPONSE
  LTM4673_UNUSED, // 0x5b
  LTM4673_UNUSED, // 0x5c
  LTM4673_UNUSED, // 0x5d
  LTM4673_ENCODING_L16, // LTM4673_POWER_GOOD_ON
  LTM4673_ENCODING_L16, // LTM4673_POWER_GOOD_OFF
  LTM4673_ENCODING_L11, // LTM4673_TON_DELAY
  LTM4673_ENCODING_L11, // LTM4673_TON_RISE
  LTM4673_ENCODING_L11, // LTM4673_TON_MAX_FAULT_LIMIT
  LTM4673_ENCODING_RAW, // LTM4673_TON_MAX_FAULT_RESPONSE
  LTM4673_ENCODING_L11, // LTM4673_TOFF_DELAY
  LTM4673_UNUSED, // 0x65
  LTM4673_UNUSED, // 0x66
  LTM4673_UNUSED, // 0x67
  LTM4673_UNUSED, // 0x68
  LTM4673_UNUSED, // 0x69
  LTM4673_UNUSED, // 0x6a
  LTM4673_UNUSED, // 0x6b
  LTM4673_UNUSED, // 0x6c
  LTM4673_UNUSED, // 0x6d
  LTM4673_UNUSED, // 0x6e
  LTM4673_UNUSED, // 0x6f
  LTM4673_UNUSED, // 0x70
  LTM4673_UNUSED, // 0x71
  LTM4673_UNUSED, // 0x72
  LTM4673_UNUSED, // 0x73
  LTM4673_UNUSED, // 0x74
  LTM4673_UNUSED, // 0x75
  LTM4673_UNUSED, // 0x76
  LTM4673_UNUSED, // 0x77
  LTM4673_ENCODING_RAW, // LTM4673_STATUS_BYTE
  LTM4673_ENCODING_RAW, // LTM4673_STATUS_WORD
  LTM4673_ENCODING_RAW, // LTM4673_STATUS_VOUT
  LTM4673_ENCODING_RAW, // LTM4673_STATUS_IOUT
  LTM4673_ENCODING_RAW, // LTM4673_STATUS_INPUT
  LTM4673_ENCODING_RAW, // LTM4673_STATUS_TEMPERATURE
  LTM4673_ENCODING_RAW, // LTM4673_STATUS_CML
  LTM4673_UNUSED, // 0x7f
  LTM4673_ENCODING_RAW, // LTM4673_STATUS_MFR_SPECIFIC
  LTM4673_UNUSED, // 0x81
  LTM4673_UNUSED, // 0x82
  LTM4673_UNUSED, // 0x83
  LTM4673_UNUSED, // 0x84
  LTM4673_UNUSED, // 0x85
  LTM4673_UNUSED, // 0x86
  LTM4673_UNUSED, // 0x87
  LTM4673_ENCODING_L11, // LTM4673_READ_VIN
  LTM4673_ENCODING_L11, // LTM4673_READ_IIN
  LTM4673_UNUSED, // 0x8a
  LTM4673_ENCODING_L16, // LTM4673_READ_VOUT
  LTM4673_ENCODING_L11, // LTM4673_READ_IOUT
  LTM4673_ENCODING_L11, // LTM4673_READ_TEMPERATURE_1
  LTM4673_ENCODING_L11, // LTM4673_READ_TEMPERATURE_2
  LTM4673_UNUSED, // 0x8f
  LTM4673_UNUSED, // 0x90
  LTM4673_UNUSED, // 0x91
  LTM4673_UNUSED, // 0x92
  LTM4673_UNUSED, // 0x93
  LTM4673_UNUSED, // 0x94
  LTM4673_UNUSED, // 0x95
  LTM4673_ENCODING_L11, // LTM4673_READ_POUT
  LTM4673_ENCODING_L11, // LTM4673_READ_PIN
  LTM4673_ENCODING_RAW, // LTM4673_PMBUS_REVISION
  LTM4673_UNUSED, // 0x99
  LTM4673_UNUSED, // 0x9a
  LTM4673_UNUSED, // 0x9b
  LTM4673_UNUSED, // 0x9c
  LTM4673_UNUSED, // 0x9d
  LTM4673_UNUSED, // 0x9e
  LTM4673_UNUSED, // 0x9f
  LTM4673_UNUSED, // 0xa0
  LTM4673_UNUSED, // 0xa1
  LTM4673_UNUSED, // 0xa2
  LTM4673_UNUSED, // 0xa3
  LTM4673_UNUSED, // 0xa4
  LTM4673_UNUSED, // 0xa5
  LTM4673_UNUSED, // 0xa6
  LTM4673_UNUSED, // 0xa7
  LTM4673_UNUSED, // 0xa8
  LTM4673_UNUSED, // 0xa9
  LTM4673_UNUSED, // 0xaa
  LTM4673_UNUSED, // 0xab
  LTM4673_UNUSED, // 0xac
  LTM4673_UNUSED, // 0xad
  LTM4673_UNUSED, // 0xae
  LTM4673_UNUSED, // 0xaf
  LTM4673_ENCODING_RAW, // LTM4673_USER_DATA_00
  LTM4673_ENCODING_RAW, // LTM4673_USER_DATA_01
  LTM4673_ENCODING_RAW, // LTM4673_USER_DATA_02
  LTM4673_ENCODING_RAW, // LTM4673_USER_DATA_03
  LTM4673_ENCODING_RAW, // LTM4673_USER_DATA_04
  LTM4673_ENCODING_RAW, // LTM4673_MFR_LTC_RESERVED_1
  LTM4673_UNUSED, // 0xb6
  LTM4673_UNUSED, // 0xb7
  LTM4673_ENCODING_L11, // LTM4673_MFR_T_SELF_HEAT
  LTM4673_ENCODING_L11, // LTM4673_MFR_IOUT_CAL_GAIN_TAU_INV
  LTM4673_ENCODING_L11, // LTM4673_MFR_IOUT_CAL_GAIN_THETA
  LTM4673_ENCODING_RAW, // LTM4673_MFR_READ_IOUT
  LTM4673_ENCODING_RAW, // LTM4673_MFR_LTC_RESERVED_2
  LTM4673_ENCODING_RAW, // LTM4673_MFR_EE_UNLOCK
  LTM4673_ENCODING_RAW, // LTM4673_MFR_EE_ERASE
  LTM4673_ENCODING_RAW, // LTM4673_MFR_EE_DATA
  LTM4673_ENCODING_RAW, // LTM4673_MFR_EIN
  LTM4673_ENCODING_RAW, // LTM4673_MFR_EIN_CONFIG
  LTM4673_ENCODING_RAW, // LTM4673_MFR_SPECIAL_LOT
  LTM4673_ENCODING_RAW, // LTM4673_MFR_IIN_CAL_GAIN_TC
  LTM4673_ENCODING_L11, // LTM4673_MFR_IIN_PEAK
  LTM4673_ENCODING_L11, // LTM4673_MFR_IIN_MIN
  LTM4673_ENCODING_L11, // LTM4673_MFR_PIN_PEAK
  LTM4673_ENCODING_L11, // LTM4673_MFR_PIN_MIN
  LTM4673_ENCODING_RAW, // LTM4673_MFR_COMMAND_PLUS
  LTM4673_ENCODING_RAW, // LTM4673_MFR_DATA_PLUS0
  LTM4673_ENCODING_RAW, // LTM4673_MFR_DATA_PLUS1
  LTM4673_UNUSED, // 0xcb
  LTM4673_UNUSED, // 0xcc
  LTM4673_UNUSED, // 0xcd
  LTM4673_UNUSED, // 0xce
  LTM4673_UNUSED, // 0xcf
  LTM4673_ENCODING_RAW, // LTM4673_MFR_CONFIG_LTM4673
  LTM4673_ENCODING_RAW, // LTM4673_MFR_CONFIG_ALL_LTM4673
  LTM4673_ENCODING_RAW, // LTM4673_MFR_FAULTB0_PROPAGATE
  LTM4673_ENCODING_RAW, // LTM4673_MFR_FAULTB1_PROPAGATE
  LTM4673_ENCODING_RAW, // LTM4673_MFR_PWRGD_EN
  LTM4673_ENCODING_RAW, // LTM4673_MFR_FAULTB0_RESPONSE
  LTM4673_ENCODING_RAW, // LTM4673_MFR_FAULTB1_RESPONSE
  LTM4673_ENCODING_L11, // LTM4673_MFR_IOUT_PEAK
  LTM4673_ENCODING_L11, // LTM4673_MFR_IOUT_MIN
  LTM4673_ENCODING_RAW, // LTM4673_MFR_CONFIG2_LTM4673
  LTM4673_ENCODING_RAW, // LTM4673_MFR_CONFIG3_LTM4673
  LTM4673_ENCODING_L11, // LTM4673_MFR_RETRY_DELAY
  LTM4673_ENCODING_L11, // LTM4673_MFR_RESTART_DELAY
  LTM4673_ENCODING_L16, // LTM4673_MFR_VOUT_PEAK
  LTM4673_ENCODING_L11, // LTM4673_MFR_VIN_PEAK
  LTM4673_ENCODING_L11, // LTM4673_MFR_TEMPERATURE_1_PEAK
  LTM4673_ENCODING_RAW, // LTM4673_MFR_DAC
  LTM4673_ENCODING_L11, // LTM4673_MFR_POWERGOOD_ASSERTION_DELAY
  LTM4673_ENCODING_L11, // LTM4673_MFR_WATCHDOG_T_FIRST
  LTM4673_ENCODING_L11, // LTM4673_MFR_WATCHDOG_T
  LTM4673_ENCODING_RAW, // LTM4673_MFR_PAGE_FF_MASK
  LTM4673_ENCODING_RAW, // LTM4673_MFR_PADS
  LTM4673_ENCODING_RAW, // LTM4673_MFR_I2C_BASE_ADDRESS
  LTM4673_ENCODING_RAW, // LTM4673_MFR_SPECIAL_ID
  LTM4673_ENCODING_L11, // LTM4673_MFR_IIN_CAL_GAIN
  LTM4673_ENCODING_L11, // LTM4673_MFR_VOUT_DISCHARGE_THRESHOLD
  LTM4673_ENCODING_RAW, // LTM4673_MFR_FAULT_LOG_STORE
  LTM4673_ENCODING_RAW, // LTM4673_MFR_FAULT_LOG_RESTORE
  LTM4673_ENCODING_RAW, // LTM4673_MFR_FAULT_LOG_CLEAR
  LTM4673_ENCODING_RAW, // LTM4673_MFR_FAULT_LOG_STATUS
  LTM4673_ENCODING_RAW, // LTM4673_MFR_FAULT_LOG
  LTM4673_ENCODING_RAW, // LTM4673_MFR_COMMON
  LTM4673_UNUSED, // 0xf0
  LTM4673_UNUSED, // 0xf1
  LTM4673_UNUSED, // 0xf2
  LTM4673_UNUSED, // 0xf3
  LTM4673_UNUSED, // 0xf4
  LTM4673_UNUSED, // 0xf5
  LTM4673_ENCODING_RAW, // LTM4673_MFR_IOUT_CAL_GAIN_TC
  LTM4673_ENCODING_RAW, // LTM4673_MFR_RETRY_COUNT
  LTM4673_ENCODING_RAW, // LTM4673_MFR_TEMP_1_GAIN
  LTM4673_ENCODING_L11, // LTM4673_MFR_TEMP_1_OFFSET
  LTM4673_ENCODING_RAW, // LTM4673_MFR_IOUT_SENSE_VOLTAGE
  LTM4673_ENCODING_L16, // LTM4673_MFR_VOUT_MIN
  LTM4673_ENCODING_L11, // LTM4673_MFR_VIN_MIN
  LTM4673_ENCODING_L11, // LTM4673_MFR_TEMPERATURE_1_MIN
  LTM4673_UNUSED, // 0xfe
  LTM4673_UNUSED, // 0xff
};

static int ltm4673_decode(uint8_t cmd, uint16_t data);
static uint16_t ltm4673_encode(uint8_t cmd, int val);

uint8_t ltm4673_get_page(void) {
  return ltm4673_page;
}

static int ltm4673_decode(uint8_t cmd, uint16_t data) {
  // TODO - Get encoding type from 'cmd'
  uint8_t encoding = ltm4673_encodings[cmd];
  int val = 0;
  if (encoding == LTM4673_ENCODING_RAW) {
    val = (int)data;
  } else if (encoding == LTM4673_ENCODING_L11) {
    val = l11_to_mv_int(data);
  } else if (encoding == LTM4673_ENCODING_L16) {
    val = L16_TO_MV(data);
  }
  return val;
}

static uint16_t ltm4673_encode(uint8_t cmd, int val) {
  uint16_t data = 0;
  uint8_t encoding = ltm4673_encodings[cmd];
  if (encoding == LTM4673_ENCODING_RAW) {
    data = (uint16_t)val;
  } else if (encoding == LTM4673_ENCODING_L11) {
    data = mv_to_l11_int(val);
  } else if (encoding == LTM4673_ENCODING_L16) {
    data = MV_TO_L16(val);
  }
  return data;
}

int ltm4673_ch_status(uint8_t dev)
{
  if (marble_get_board_id() < Marble_v1_3) {
    printf("LTM4673 not present; bypassed.\n");
    return 0;
  }
   const uint8_t STATUS_WORD = 0x79;
   uint8_t i2c_dat[4];
   for (unsigned jx = 0; jx < 4; jx++) {
      // start selecting channel/page 0 until you finish reading
      // data for all 4 channels
      uint8_t page = 0x00 + jx;
      marble_I2C_cmdsend(I2C_PM, dev, 0x00, &page, 1);
      // marble_I2C_cmd_recv should return 0, if everything is good, see page 100
      int rc = marble_I2C_cmdrecv(I2C_PM, dev, STATUS_WORD, i2c_dat, 2);
      if (rc == HAL_OK) {
          uint16_t word0 = ((unsigned int) i2c_dat[1] << 8) | i2c_dat[0];
          if (word0) {
              printf("BAD! LTM4673 Channel %x, Status_word r[%2.2x] = 0x%x\r\n", page, STATUS_WORD, word0);
              return 0;
          }
      }
   }
   return 1;
}

void ltm4673_read_telem(uint8_t dev) {
   struct {int b; const char *m;} r_table[] = {
      // see page 105
      {0x88, "V     READ_VIN"},
      {0x89, "A     READ_IIN"},
      {0x97, "W     READ_PIN"},
      {0x8B, "V     READ_VOUT"},
      {0x8C, "A     READ_IOUT"},
      {0x8D, "degC  READ_TEMPERATURE_1"},
      {0x8E, "degC  READ_TEMPERATURE_2"},
      {0x96, "W     READ_POUT"},
      {0xBB, "mA    MFR_READ_IOUT"},
      {0xC4, "A     MFR_IIN_PEAK"},
      {0xC5, "A     MFR_IIN_MIN"},
      {0xC6, "P     MFR_PIN_PEAK"},
      {0xC7, "P     MFR_PIN_MIN"},
      {0xFA, "V     MFR_IOUT_SENSE_VOLTAGE"},
      {0xDE, "V     MFR_VIN_PEAK"},
      {0xDD, "V     MFR_VOUT_PEAK"},
      {0xD7, "A     MFR_IOUT_PEAK"},
      {0xDF, "degC  MFR_TEMPERATURE_1_PEAK"},
      {0xFC, "V     MFR_VIN_MIN"},
      {0xFB, "V     MFR_VOUT_MIN"},
      {0xD8, "A     MFR_IOUT_MIN"},
      {0xFD, "degC  MFR_TEMPERATURE_1_MIN"}};
   printf("LTM4673 Telemetry register dump:\n");
   //float L16 = 0.0001220703125;  // 2**(-13)
   for (unsigned jx = 0; jx < 4; jx++) {
      // start selecting channel/page 0 until you finish reading
      // telemetry data for all 4 channels
      uint8_t page = 0x00 + jx;
      marble_I2C_cmdsend(I2C_PM, dev, 0x00, &page, 1);
      printf("> Read page/channel: %x\n", page);
      const unsigned tlen = sizeof(r_table)/sizeof(r_table[0]);
      for (unsigned ix=0; ix<tlen; ix++) {
          uint8_t i2c_dat[4];
          int regno = r_table[ix].b;
          int rc = marble_I2C_cmdrecv(I2C_PM, dev, regno, i2c_dat, 2);
          uint16_t word0 = ((unsigned int) i2c_dat[1] << 8) | i2c_dat[0];
          float phys_unit;
          //int mask, comp2;
          if (rc == HAL_OK) {
              if ((uint8_t)regno == LTM4673_MFR_READ_IOUT) {
                  phys_unit = word0*2.5;  // special for MFR_READ_IOUT
              } else if (ltm4673_encodings[(uint8_t)regno] == LTM4673_ENCODING_L11) {
                  phys_unit = l11_to_v_float(word0);
              } else if (ltm4673_encodings[(uint8_t)regno] == LTM4673_ENCODING_L16) {
                  phys_unit = l16_to_v_float(word0);
              } else {
                  phys_unit = (float)word0;
              }
              /*
              if (ix == 3 || ix == 15 || ix == 19)
                  phys_unit = word0*L16;  // L16 format
              else if (ix == 8)
                  phys_unit = word0*2.5;  // special for MFR_READ_IOUT
              else if (ix == 13)
                  phys_unit = word0*0.025*pow(2, -13);  // special for MFR_IOUT_SENSE_VOLTAGE
              else {
                  // L11 format, see page 35
                  mask = (word0 >> 11);
                  comp2 = pow(2, 5) - mask;
                  phys_unit = (word0 & 0x7FF)*(1.0/(1<<comp2));
              }
              */
              printf("r[%2.2x] = 0x%4.4x = %5d = %7.3f %s\r\n", regno, word0, word0, phys_unit, r_table[ix].m);
          } else {
              printf("r[%2.2x]    unread          (%s)\r\n", regno, r_table[ix].m);
          }
      }
   }
   return;
}

int ltm4673_hook_write(uint8_t addr, const uint8_t *data, int len) {
  int matched = 0;
  // Look for LTM4673 writes
  for (unsigned int n = 0; n < LTM4673_MATCH_ADDRS; n++) {
    if (addr == ltm4673_addrs[n]) {
      matched = 1;
      break;
    }
  }
  if (!matched) {
    return 0;
  }
  // Look for page writes
  if (data[0] == LTM4673_PAGE) {
    if (len < 2) {
      printf("Invalid PAGE write length %d\r\n", len);
    } else if (data[1] == 0xff) {
      printf("# LTM4673_PAGE 0xff\r\n");
      ltm4673_page = 0xff;
    } else if (data[1] < 4) {
      printf("# LTM4673_PAGE 0x%02x\r\n", data[1]);
      ltm4673_page = data[1];
    } else {
      printf("LTM4673 invalid PAGE write: 0x%02x\r\n", data[1]);
    }
  }
  return matched;
}

int ltm4673_hook_read(uint8_t addr, uint8_t cmd, const uint8_t *data, int len) {
  int matched = 0;
  // Look for LTM4673 reads
  for (unsigned int n = 0; n < LTM4673_MATCH_ADDRS; n++) {
    if (addr == ltm4673_addrs[n]) {
      matched = 1;
      break;
    }
  }
  if (!matched) {
    return 0;
  }
  // Look for page reads
  if (cmd == LTM4673_PAGE) {
    if (len == 0) {
      printf("Invalid read length 0\r\n");
    } else if (data[0] == 0xff) {
      ltm4673_page = 0xff;
    } else if (data[0] < 4) {
      ltm4673_page = data[0];
    } else {
      printf("LTM4673 invalid PAGE returned on read: 0x%02x\r\n", data[0]);
    }
  }
  return matched;
}

int ltm4673_apply_limits(uint16_t *xact, int len) {
  uint16_t data;
  int matched = 0;
  unsigned int row;
  int val_decoded;
  int mask;
  int lim;
  uint8_t command_code = xact[1] & 0xff;
  // Look for LTM4673 writes
  for (unsigned int n = 0; n < LTM4673_MATCH_ADDRS; n++) {
    if (xact[0] == ltm4673_addrs[n]) { // Recall we already know xact[0] & 1 is 0
      matched = 1;
      break;
    }
  }
  if (matched) {
    // Compare with limits on the current page
    for (row = 0; row < ltm4673_limits[LTM4673_PAGE_INDEX(ltm4673_page)].nrows; row++) {
      if (command_code == LTM4673_LIMIT_GET_COMMAND(ltm4673_page, row)) {
        mask = LTM4673_LIMIT_GET_MASK(ltm4673_page, row);
        if (mask == 0) {
          printf("Vetoing write to protected register 0x%02x\r\n", command_code);
          return 0;
        }
        data = (uint16_t)xact[2];
        if (len > 3) {
          data |= ((uint16_t)xact[3] << 8);
        }
        // TODO - Determine the encoding type (L11, L16, raw) before proceeding
        // Decode the bytes before comparing value
        val_decoded = ltm4673_decode(command_code, data);
        // Apply pass_mask
        val_decoded = val_decoded & mask;
        // Clip at lower limit
        lim = LTM4673_LIMIT_GET_MIN(ltm4673_page, row);
        val_decoded = val_decoded < lim ? lim : val_decoded;
        // Clip at upper limit
        lim = LTM4673_LIMIT_GET_MAX(ltm4673_page, row);
        val_decoded = val_decoded > lim ? lim : val_decoded;
        // Encode the bytes before storing
        data = ltm4673_encode(command_code, val_decoded);
        // Clobber old data
        xact[2] = (uint8_t)(data & 0xff);
        if (len > 3) {
          xact[3] = (uint8_t)((data >> 8) & 0xff);
        }
      }
    }
  }
  return 0;
}

void ltm4673_print_limits(void) {
  uint8_t cmd, mask, min, max;
  unsigned int page, row;
  for (page = 0; page < LTM4673_NPAGES; page++) {
    if (page > 3) {
      printf("Page 0xff\r\n");
    } else {
      printf("Page 0x%x\r\n", page);
    }
    printf("cmd   mask  min   max\r\n");
    for (row = 0; row < ltm4673_limits[LTM4673_PAGE_INDEX(page)].nrows; row++) {
      cmd  = LTM4673_LIMIT_GET_COMMAND(page, row);
      mask = LTM4673_LIMIT_GET_MASK(page, row);
      min  = LTM4673_LIMIT_GET_MIN(page, row);
      max  = LTM4673_LIMIT_GET_MAX(page, row);
      printf("0x%02x  0x%02x  0x%02x  0x%02x\r\n", cmd, mask, min, max);
    }
  }
  return;
}


