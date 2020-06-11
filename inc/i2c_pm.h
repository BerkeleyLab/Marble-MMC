#ifndef _I2C_PM_H_
#define _I2C_PM_H_

typedef enum {
   LM75_0 = 0x92,
   LM75_1 = 0x96,
   MAX6639 = 0x58,
   XRP7724 = 0x50
} I2C_SLAVE;

#define I2C_NUM 4

void I2C_PM_probe(void);
void print_max6639(void);

typedef enum {
   LM75_TEMP = 0,
   LM75_CFG,
   LM75_HYST,
   LM75_OS,
   LM75_MAX
} LM75_REG;

typedef enum {
   LM75_CFG_SHUTD    = (1<<0),
   LM75_CFG_COMP_INT = (1<<1),
   LM75_CFG_OS_POL   = (1<<2),
   LM75_CFG_FQUEUE   = (1<<4 | 1<<3)
} LM75_CFG_FIELDS;

void LM75_print(uint8_t dev);

int LM75_readwrite(uint8_t dev, LM75_REG reg, int *data, bool rnw);

#endif /* _I2C_PM_H_ */

