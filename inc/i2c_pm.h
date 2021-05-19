#ifndef I2C_PM_H_
#define I2C_PM_H_

typedef enum {
   LM75_0 = 0x92,
   LM75_1 = 0x96,
   MAX6639 = 0x58,
   XRP7724 = 0x50
} I2C_SLAVE;

#define I2C_NUM 4

void I2C_PM_scan(void);
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

int LM75_read(uint8_t dev, LM75_REG reg, int *data);
int LM75_write(uint8_t dev, LM75_REG reg, int data);

int xrp_ch_status(uint8_t dev, uint8_t chn);
void xrp_dump(uint8_t dev);
void xrp_flash(uint8_t dev);
void xrp_go(uint8_t dev);
void xrp_hex_in(uint8_t dev);

/* communication between i2c_pm and hexrec */
int xrp_push_low(uint8_t dev, uint16_t addr, const uint8_t data[], unsigned len);
int xrp_set2(uint8_t dev, uint16_t addr, uint8_t data);
int xrp_read2(uint8_t dev, uint16_t addr);
int xrp_srecord(uint8_t dev, const uint8_t data[]);
int xrp_program_static(uint8_t dev);
int xrp_file(uint8_t dev);

#endif /* I2C_PM_H_ */
