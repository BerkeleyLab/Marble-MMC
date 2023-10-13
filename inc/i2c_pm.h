#ifndef I2C_PM_H_
#define I2C_PM_H_

#include <stdint.h>

typedef enum {
   LM75_0 = 0x92,   // U29
   LM75_1 = 0x96,   // U28
   MAX6639 = 0x58,
   XRP7724 = 0x50,
   LTM4673 = 0xC0
} I2C_SLAVE;

#define I2C_NUM 5

// Set hysteresis value in degC
// Devices will enter shutdown at T = T_overtemp and exit
// shutdown at T = T_overtemp - TEMPERATURE_HYST_DEGC
// Note that the MAX6639 has a hard-coded 5degC hysteresis
#define TEMPERATURE_HYST_DEGC           (5)

void I2C_PM_scan(void);
void I2C_PM_probe(void);
void print_max6639(void);
void print_max6639_decoded(void);
int get_max6639_reg(int regno, int *value);
int return_max6639_reg(int regno);

#define LM75_FOR_EACH_REGISTER() \
  X(LM75_TEMP, 0) \
  X(LM75_CFG, 1) \
  X(LM75_HYST, 2) \
  X(LM75_OS, 3)

#define X(name, val)     name = val,
typedef enum {
  LM75_FOR_EACH_REGISTER()
  LM75_MAX
} LM75_REG;
#undef X

/*
typedef enum {
   LM75_TEMP = 0,
   LM75_CFG,
   LM75_HYST,
   LM75_OS,
   LM75_MAX
} LM75_REG;
*/

typedef enum {
   LM75_CFG_SHUTD    = (1<<0),
   LM75_CFG_COMP_INT = (1<<1),
   LM75_CFG_OS_POL   = (1<<2),
   LM75_CFG_FQUEUE   = (1<<4 | 1<<3)
} LM75_CFG_FIELDS;

#define LM75_CFG_DEFAULT        (0)

void LM75_print(uint8_t dev);
void LM75_print_decoded(uint8_t dev);

void LM75_Init(void);
int LM75_read(uint8_t dev, LM75_REG reg, int *data);
int LM75_write(uint8_t dev, LM75_REG reg, int data);
int LM75_set_overtemp(int ot);

int xrp_ch_status(uint8_t dev, uint8_t chn);
void xrp_dump(uint8_t dev);
void xrp_flash(uint8_t dev);
void xrp_go(uint8_t dev);
void xrp_hex_in(uint8_t dev);

// Helper functions
int max6639_set_fans(int speed);
int max6639_set_overtemp(uint8_t ot);

/* communication between i2c_pm and hexrec */
int xrp_push_low(uint8_t dev, uint16_t addr, const uint8_t data[], unsigned len);
int xrp_set2(uint8_t dev, uint16_t addr, uint8_t data);
int xrp_read2(uint8_t dev, uint16_t addr);
int xrp_srecord(uint8_t dev, const uint8_t data[]);
int xrp_program_static(uint8_t dev);
int xrp_file(uint8_t dev);

void ltm_read_telem(uint8_t dev);
int ltm_ch_status(uint8_t dev);

// PMBridge
#define PMBRIDGE_MAX_LINE_LENGTH        (256)
#define PMBRIDGE_XACT_MAX_ITEMS          (32)
// Trigger a repeat_start
#define PMBRIDGE_XACT_REPEAT_START   (0x100)
// Expect one byte from peripheral
#define PMBRIDGE_XACT_READ_ONE       (0x101)
// Read one byte; use it as N. Then read N more bytes.
#define PMBRIDGE_XACT_READ_BLOCK     (0x102)

int PMBridge_xact(uint16_t *xact, int len);

// LTM4673 Register (Command Code) Map
#define LTM4673_PAGE                             (0x00)
#define LTM4673_OPERATION                        (0x01)
#define LTM4673_ON_OFF_CONFIG                    (0x02)
#define LTM4673_CLEAR_FAULTS                     (0x03)
#define LTM4673_WRITE_PROTECT                    (0x10)
#define LTM4673_STORE_USER_ALL                   (0x15)
#define LTM4673_RESTORE_USER_ALL                 (0x16)
#define LTM4673_CAPABILITY                       (0x19)
#define LTM4673_VOUT_MODE                        (0x20)
#define LTM4673_VOUT_COMMAND                     (0x21)
#define LTM4673_VOUT_MAX                         (0x24)
#define LTM4673_VOUT_MARGIN_HIGH                 (0x25)
#define LTM4673_VOUT_MARGIN_LOW                  (0x26)
#define LTM4673_VIN_ON                           (0x35)
#define LTM4673_VIN_OFF                          (0x36)
#define LTM4673_IOUT_CAL_GAIN                    (0x38)
#define LTM4673_VOUT_OV_FAULT_LIMIT              (0x40)
#define LTM4673_VOUT_OV_FAULT_RESPONSE           (0x41)
#define LTM4673_VOUT_OV_WARN_LIMIT               (0x42)
#define LTM4673_VOUT_UV_WARN_LIMIT               (0x43)
#define LTM4673_VOUT_UV_FAULT_LIMIT              (0x44)
#define LTM4673_VOUT_UV_FAULT_RESPONSE           (0x45)
#define LTM4673_IOUT_OC_FAULT_LIMIT              (0x46)
#define LTM4673_IOUT_OC_FAULT_RESPONSE           (0x47)
#define LTM4673_IOUT_OC_WARN_LIMIT               (0x4a)
#define LTM4673_IOUT_UC_FAULT_LIMIT              (0x4b)
#define LTM4673_IOUT_UC_FAULT_RESPONSE           (0x4c)
#define LTM4673_OT_FAULT_LIMIT                   (0x4f)
#define LTM4673_OT_FAULT_RESPONSE                (0x50)
#define LTM4673_OT_WARN_LIMIT                    (0x51)
#define LTM4673_UT_WARN_LIMIT                    (0x52)
#define LTM4673_UT_FAULT_LIMIT                   (0x53)
#define LTM4673_UT_FAULT_RESPONSE                (0x54)
#define LTM4673_VIN_OV_FAULT_LIMIT               (0x55)
#define LTM4673_VIN_OV_FAULT_RESPONSE            (0x56)
#define LTM4673_VIN_OV_WARN_LIMIT                (0x57)
#define LTM4673_VIN_UV_WARN_LIMIT                (0x58)
#define LTM4673_VIN_UV_FAULT_LIMIT               (0x59)
#define LTM4673_VIN_UV_FAULT_RESPONSE            (0x5a)
#define LTM4673_POWER_GOOD_ON                    (0x5e)
#define LTM4673_POWER_GOOD_OFF                   (0x5f)
#define LTM4673_TON_DELAY                        (0x60)
#define LTM4673_TON_RISE                         (0x61)
#define LTM4673_TON_MAX_FAULT_LIMIT              (0x62)
#define LTM4673_TON_MAX_FAULT_RESPONSE           (0x63)
#define LTM4673_TOFF_DELAY                       (0x64)
#define LTM4673_STATUS_BYTE                      (0x78)
#define LTM4673_STATUS_WORD                      (0x79)
#define LTM4673_STATUS_VOUT                      (0x7a)
#define LTM4673_STATUS_IOUT                      (0x7b)
#define LTM4673_STATUS_INPUT                     (0x7c)
#define LTM4673_STATUS_TEMPERATURE               (0x7d)
#define LTM4673_STATUS_CML                       (0x7e)
#define LTM4673_STATUS_MFR_SPECIFIC              (0x80)
#define LTM4673_READ_VIN                         (0x88)
#define LTM4673_READ_IIN                         (0x89)
#define LTM4673_READ_VOUT                        (0x8b)
#define LTM4673_READ_IOUT                        (0x8c)
#define LTM4673_READ_TEMPERATURE_1               (0x8d)
#define LTM4673_READ_TEMPERATURE_2               (0x8e)
#define LTM4673_READ_POUT                        (0x96)
#define LTM4673_READ_PIN                         (0x97)
#define LTM4673_PMBUS_REVISION                   (0x98)
#define LTM4673_USER_DATA_00                     (0xb0)
#define LTM4673_USER_DATA_01                     (0xb1)
#define LTM4673_USER_DATA_02                     (0xb2)
#define LTM4673_USER_DATA_03                     (0xb3)
#define LTM4673_USER_DATA_04                     (0xb4)
#define LTM4673_MFR_LTC_RESERVED_1               (0xb5)
#define LTM4673_MFR_T_SELF_HEAT                  (0xb8)
#define LTM4673_MFR_IOUT_CAL_GAIN_TAU_INV        (0xb9)
#define LTM4673_MFR_IOUT_CAL_GAIN_THETA          (0xba)
#define LTM4673_MFR_READ_IOUT                    (0xbb)
#define LTM4673_MFR_LTC_RESERVED_2               (0xbc)
#define LTM4673_MFR_EE_UNLOCK                    (0xbd)
#define LTM4673_MFR_EE_ERASE                     (0xbe)
#define LTM4673_MFR_EE_DATA                      (0xbf)
#define LTM4673_MFR_EIN                          (0xc0)
#define LTM4673_MFR_EIN_CONFIG                   (0xc1)
#define LTM4673_MFR_SPECIAL_LOT                  (0xc2)
#define LTM4673_MFR_IIN_CAL_GAIN_TC              (0xc3)
#define LTM4673_MFR_IIN_PEAK                     (0xc4)
#define LTM4673_MFR_IIN_MIN                      (0xc5)
#define LTM4673_MFR_PIN_PEAK                     (0xc6)
#define LTM4673_MFR_PIN_MIN                      (0xc7)
#define LTM4673_MFR_COMMAND_PLUS                 (0xc8)
#define LTM4673_MFR_DATA_PLUS0                   (0xc9)
#define LTM4673_MFR_DATA_PLUS1                   (0xca)
#define LTM4673_MFR_CONFIG_LTM4673               (0xd0)
#define LTM4673_MFR_CONFIG_ALL_LTM4673           (0xd1)
#define LTM4673_MFR_FAULTB0_PROPAGATE            (0xd2)
#define LTM4673_MFR_FAULTB1_PROPAGATE            (0xd3)
#define LTM4673_MFR_PWRGD_EN                     (0xd4)
#define LTM4673_MFR_FAULTB0_RESPONSE             (0xd5)
#define LTM4673_MFR_FAULTB1_RESPONSE             (0xd6)
#define LTM4673_MFR_IOUT_PEAK                    (0xd7)
#define LTM4673_MFR_IOUT_MIN                     (0xd8)
#define LTM4673_MFR_CONFIG2_LTM4673              (0xd9)
#define LTM4673_MFR_CONFIG3_LTM4673              (0xda)
#define LTM4673_MFR_RETRY_DELAY                  (0xdb)
#define LTM4673_MFR_RESTART_DELAY                (0xdc)
#define LTM4673_MFR_VOUT_PEAK                    (0xdd)
#define LTM4673_MFR_VIN_PEAK                     (0xde)
#define LTM4673_MFR_TEMPERATURE_1_PEAK           (0xdf)
#define LTM4673_MFR_DAC                          (0xe0)
#define LTM4673_MFR_POWERGOOD_ASSERTION_DELAY    (0xe1)
#define LTM4673_MFR_WATCHDOG_T_FIRST             (0xe2)
#define LTM4673_MFR_WATCHDOG_T                   (0xe3)
#define LTM4673_MFR_PAGE_FF_MASK                 (0xe4)
#define LTM4673_MFR_PADS                         (0xe5)
#define LTM4673_MFR_I2C_BASE_ADDRESS             (0xe6)
#define LTM4673_MFR_SPECIAL_ID                   (0xe7)
#define LTM4673_MFR_IIN_CAL_GAIN                 (0xe8)
#define LTM4673_MFR_VOUT_DISCHARGE_THRESHOLD     (0xe9)
#define LTM4673_MFR_FAULT_LOG_STORE              (0xea)
#define LTM4673_MFR_FAULT_LOG_RESTORE            (0xeb)
#define LTM4673_MFR_FAULT_LOG_CLEAR              (0xec)
#define LTM4673_MFR_FAULT_LOG_STATUS             (0xed)
#define LTM4673_MFR_FAULT_LOG                    (0xee)
#define LTM4673_MFR_COMMON                       (0xef)
#define LTM4673_MFR_IOUT_CAL_GAIN_TC             (0xf6)
#define LTM4673_MFR_RETRY_COUNT                  (0xf7)
#define LTM4673_MFR_TEMP_1_GAIN                  (0xf8)
#define LTM4673_MFR_TEMP_1_OFFSET                (0xf9)
#define LTM4673_MFR_IOUT_SENSE_VOLTAGE           (0xfa)
#define LTM4673_MFR_VOUT_MIN                     (0xfb)
#define LTM4673_MFR_VIN_MIN                      (0xfc)
#define LTM4673_MFR_TEMPERATURE_1_MIN            (0xfd)

#endif /* I2C_PM_H_ */
