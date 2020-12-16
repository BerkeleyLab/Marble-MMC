#ifndef I2C_FPGA_H_
#define I2C_FPGA_H_

#define TCA9548 (0xe0)

typedef enum {
   I2C_FMC1  = 0,
   I2C_FMC2  = 1,
   I2C_CLK   = 2,
   I2C_QSFP1 = 4,
   I2C_QSFP2 = 5,
   I2C_APP   = 6
} I2C_FPGA_BUS;

typedef enum {
   INA219_0    = 0x84,
   INA219_FMC1 = 0x80,
   INA219_FMC2 = 0x82,
   PCA9555_0   = 0x40,
   PCA9555_1   = 0x42
} I2C_APP_BUS;

#define I2C_APP_NUM 5

void I2C_FPGA_scan(void);
void switch_i2c_bus(uint8_t);
void adn4600_init(void);
void adn4600_printStatus(void);
void ina219_init(void);
void ina219_debug(uint8_t addr);
float getBusVoltage_V(uint8_t);
float getCurrentAmps(uint8_t);

/************
* INA219
************/

#define INA_READ                      (0x01)
#define INA_REG_CONFIG                (0x00)
#define INA_CONFIG_RESET              (0x8000)
#define INA_CONFIG_BVOLTAGERANGE_MASK (0x2000)

typedef enum CONFIG_BVOLTAGERANGE {
   CONFIG_BVOLTAGERANGE_16V = (0x0000), // 0-16V Range
   CONFIG_BVOLTAGERANGE_32V = (0x2000)  // 0-32V Range
} CONFIG_BVOLTAGERANGE;

#define INA_CONFIG_GAIN_MASK (0x1800)

typedef enum CONFIG_GAIN {
   CONFIG_GAIN_1_40MV  = (0x0000), // Gain 1, 40mV Range
   CONFIG_GAIN_2_80MV  = (0x0800), // Gain 2, 80mV Range
   CONFIG_GAIN_4_160MV = (0x1000), // Gain 4, 160mV Range
   CONFIG_GAIN_8_320MV = (0x1800)  // Gain 8, 320mV Range
} CONFIG_GAIN;

#define INA_CONFIG_BADCRES_MASK (0x0780)

typedef enum CONFIG_BADCRES {
   CONFIG_BADCRES_9BIT  = (0x0000), // 9-bit bus res = 0..511
   CONFIG_BADCRES_10BIT = (0x0080), // 10-bit bus res = 0..1023
   CONFIG_BADCRES_11BIT = (0x0100), // 11-bit bus res = 0..2047
   CONFIG_BADCRES_12BIT = (0x0180)  // 12-bit bus res = 0..4097
} CONFIG_BADCRES;

typedef enum CONFIG_SADCRES {
   CONFIG_SADCRES_9BIT_1S_84US     = (0x0000), // 1 x 9-bit shunt sample
   CONFIG_SADCRES_10BIT_1S_148US   = (0x0008), // 1 x 10-bit shunt sample
   CONFIG_SADCRES_11BIT_1S_276US   = (0x0010), // 1 x 11-bit shunt sample
   CONFIG_SADCRES_12BIT_1S_532US   = (0x0018), // 1 x 12-bit shunt sample
   CONFIG_SADCRES_12BIT_2S_1060US  = (0x0048), // 2 x 12-bit shunt samples averaged together
   CONFIG_SADCRES_12BIT_4S_2130US  = (0x0050), // 4 x 12-bit shunt samples averaged together
   CONFIG_SADCRES_12BIT_8S_4260US  = (0x0058), // 8 x 12-bit shunt samples averaged together
   CONFIG_SADCRES_12BIT_16S_8510US = (0x0060), // 16 x 12-bit shunt samples averaged together
   CONFIG_SADCRES_12BIT_32S_17MS   = (0x0068), // 32 x 12-bit shunt samples averaged together
   CONFIG_SADCRES_12BIT_64S_34MS   = (0x0070), // 64 x 12-bit shunt samples averaged together
   CONFIG_SADCRES_12BIT_128S_69MS  = (0x0078)  // 128 x 12-bit shunt samples averaged together
} CONFIG_SADCRES;

#define INA_CONFIG_MODE_MASK (0x0007)

 typedef enum CONFIG_MODE {
   CONFIG_MODE_POWERDOWN            = (0x0000),
   CONFIG_MODE_SVOLT_TRIGGERED      = (0x0001),
   CONFIG_MODE_BVOLT_TRIGGERED      = (0x0002),
   CONFIG_MODE_SANDBVOLT_TRIGGERED  = (0x0003),
   CONFIG_MODE_ADCOFF               = (0x0004),
   CONFIG_MODE_SVOLT_CONTINUOUS     = (0x0005),
   CONFIG_MODE_BVOLT_CONTINUOUS     = (0x0006),
   CONFIG_MODE_SANDBVOLT_CONTINUOUS = (0x0007)
} CONFIG_MODE;

#define INA_REG_SHUNTVOLTAGE (0x01)
#define INA_REG_BUSVOLTAGE   (0x02)
#define INA_REG_POWER        (0x03)
#define INA_REG_CURRENT      (0x04)
#define INA_REG_CALIBRATION  (0x05)

typedef enum {
   ADN4600 = 0x90
} I2C_CLK_BUS;

#define I2C_CLK_NUM 1

/************
* ADN4600
************/

#define ADN4600_OUT_0 0
#define ADN4600_OUT_1 1
#define ADN4600_OUT_4 4
#define ADN4600_OUT_5 5

#define ADN4600_OUT_CFG_0 2   // input 2 connected to output 0
#define ADN4600_OUT_CFG_1 3   // input 3 connected to output 1
#define ADN4600_OUT_CFG_4 4   // input 4 connected to output 4
#define ADN4600_OUT_CFG_5 6   // input 6 connected to output 6

#define ADN4600_XPT_Conf    (0x40)
#define ADN4600_XPT_Update  (0x41)
#define ADN4600_XPT_Status0 (0x50)
#define ADN4600_RX0_Config  (0x80)

#endif /* I2C_FPGA_H_ */
