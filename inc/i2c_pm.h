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

#endif /* I2C_PM_H_ */
