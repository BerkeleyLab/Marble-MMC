#include "marble_api.h"
#define HAL_OK (0U)
#include <stdio.h>
#include <string.h>
#include "i2c_fpga.h"
#include <math.h>

extern I2C_BUS I2C_FPGA;

extern I2C_BUS I2C_FPGA;

void I2C_FPGA_scan(void)
{
   printf("Scanning I2C_FPGA bus:\r\n");
   for (unsigned j = 0; j < 8; j++)
   {
      printf("\r\nI2C switch port: %d\r\n", j);
      switch_i2c_bus(j);
      marble_SLEEP_ms(100);
      for (unsigned i = 1; i < 128; i++)
      {
         // Using 8-bit I2C addresses
         if (marble_I2C_probe(I2C_FPGA, (uint8_t) (i<<1)) != HAL_OK) {
            printf("."); // No ACK received at that address
         } else {
            printf("0x%02X", i << 1); // Received an ACK at that address
         }
      }
   }
   printf("\r\n");
}

/************
* INA219 interface
************/
void setCalibration_16V_2A(void);

//uint16_t calValue;
uint32_t currentDivider_mA;
float powerMultiplier_mW;

void switch_i2c_bus(uint8_t i)
{
   if (i > 7) return;
   uint8_t addr = TCA9548;
   uint8_t data;
   data = (1 << i);
   marble_I2C_send(I2C_FPGA, addr, &data, 1);
}

void ina219_init()
{
   setCalibration_16V_2A();
}

static int wireWriteRegister (uint8_t addr, uint8_t reg, uint16_t value)
{
   uint8_t data[3];
   data[0] = reg;
   data[2] = value & 0xff;
   data[1] = (value >> 8);
   return marble_I2C_send(I2C_FPGA, addr, data, 3);
}

static bool wireReadRegister(uint8_t addr, uint8_t reg, uint16_t *value)
{
   uint8_t buffer[2];
   bool success = 0;

   buffer[0] = reg;
   marble_I2C_send(I2C_FPGA, addr, buffer, 1);
   buffer[0] = 0xde;
   buffer[1] = 0xad;
   success = marble_I2C_recv(I2C_FPGA, addr, buffer, 2) == HAL_OK;
   if (success) {
      *value = (((uint16_t)buffer[0] << 8) | buffer[1]);
   } else {
      *value = 0xbeef;
   }
   return success;
}

void ina219_debug(uint8_t addr)
{
   uint16_t value = 0;
   bool rc;
   printf("> INA219 debug at address %2.2xh\r\n", (unsigned) addr);
   rc = wireReadRegister(addr, INA_REG_CONFIG, &value);
   printf("Register %d value 0x%4.4x (%d)\r\n", INA_REG_CONFIG, value, rc);
   rc = wireReadRegister(addr, INA_REG_SHUNTVOLTAGE, &value);
   printf("Register %d value 0x%4.4x (%d)\r\n", INA_REG_SHUNTVOLTAGE, value, rc);
   rc = wireReadRegister(addr, INA_REG_BUSVOLTAGE, &value);
   printf("Register %d value 0x%4.4x (%d)\r\n", INA_REG_BUSVOLTAGE, value, rc);
}

void setCalibration_16V_2A(void)
{
   // VBUS_MAX = 16V             (Assumes 16V, can also be set to 32V)
   // VSHUNT_MAX = 0.08          (Assumes Gain 8, 320mV, can also be 0.16, 0.08, 0.04)
   // RSHUNT = 0.0227               (Resistor value in ohms)

   // 1. Determine max possible current
   // MaxPossible_I = VSHUNT_MAX / RSHUNT
   // MaxPossible_I = 3.52A

   // 2. Determine max expected current
   // MaxExpected_I = 2.0A

   // 3. Calculate possible range of LSBs (Min = 15-bit, Max = 12-bit)
   // MinimumLSB = MaxExpected_I/32767
   // MinimumLSB = 0.000061              (61uA per bit)
   // MaximumLSB = MaxExpected_I/4096
   // MaximumLSB = 0.000488              (488uA per bit)

   // 4. Choose an LSB between the min and max values
   //    (Preferrably a roundish number close to MinLSB)
   // CurrentLSB = 0.0001 (100uA per bit)

   // 5. Compute the calibration register
   // Cal = trunc (0.04096 / (Current_LSB * RSHUNT))
   // Cal = 18044 (0x467C)

   uint16_t calValue = 18044;

   // 6. Calculate the power LSB
   // PowerLSB = 20 * CurrentLSB
   // PowerLSB = 0.002 (2mW per bit)

   // 7. Compute the maximum current and shunt voltage values before overflow
   //
   // Max_Current = Current_LSB * 32767
   // Max_Current = 3.2767A before overflow
   //
   // If Max_Current > Max_Possible_I then
   //    Max_Current_Before_Overflow = MaxPossible_I
   // Else
   //    Max_Current_Before_Overflow = Max_Current
   // End If
   //
   // Max_ShuntVoltage = Max_Current_Before_Overflow * RSHUNT
   // Max_ShuntVoltage = 2A * 0.0226 = 0.0452V
   //
   // If Max_ShuntVoltage >= VSHUNT_MAX
   //    Max_ShuntVoltage_Before_Overflow = VSHUNT_MAX
   // Else
   //    Max_ShuntVoltage_Before_Overflow = Max_ShuntVoltage
   // End If

   // 8. Compute the Maximum Power
   // MaximumPower = Max_Current_Before_Overflow * VBUS_MAX
   // MaximumPower = 2A * 16V
   // MaximumPower = 32W

   // Set multipliers to convert raw current/power values
   currentDivider_mA = 10;  // Current LSB = 100uA per bit (1000/100 = 10)
   powerMultiplier_mW = 2;     // Power LSB = 1mW per bit (2/1)

   // Set Calibration register to 'Cal' calculated above

   wireWriteRegister(INA219_0, INA_REG_CALIBRATION, calValue);
   // wireWriteRegister(INA_REG_CALIBRATION, calValue);

   // Set Config register to take into account the settings above
   uint16_t config = CONFIG_BVOLTAGERANGE_16V |
               CONFIG_GAIN_2_80MV |
               CONFIG_BADCRES_12BIT |
               CONFIG_SADCRES_12BIT_1S_532US |
               CONFIG_MODE_SANDBVOLT_CONTINUOUS;

   //printf("> INA219 config val  [%d]\r\n",  config);
   wireWriteRegister(INA219_0, INA_REG_CONFIG, config);

}

static int16_t getBusVoltage_raw(uint8_t ina)
{
   uint16_t value = 0;
   wireReadRegister(ina, INA_REG_BUSVOLTAGE, &value);

   // Shift to the right 3 to drop CNVR and OVF and multiply by LSB
   return (int16_t)((value >> 3) * 4);
}

static int16_t getCurrent_raw(uint8_t ina)
{
   uint16_t value = 0;

   // Sometimes a sharp load will reset the INA219, which will
   // reset the cal register, meaning CURRENT and POWER will
   // not be available ... avoid this by always setting a cal
   // value even if it's an unfortunate extra step
   //wireWriteRegister(ina, INA_REG_CALIBRATION, calValue);

   // Now we can safely read the CURRENT register!
   //wireReadRegister(ina, INA_REG_CURRENT, &value);
   for(uint8_t i = 0; i < 7; i++)
      {
         wireReadRegister(ina, i, &value);
         printf("> INA219 reg: %x:  [%d]\r\n", i, value);
      }

   return (int16_t)value;
}

#if 0
static int16_t getPower_raw(uint8_t ina)
{
   uint16_t value = 0;

   // Sometimes a sharp load will reset the INA219, which will
   // reset the cal register, meaning CURRENT and POWER will
   // not be available ... avoid this by always setting a cal
   // value even if it's an unfortunate extra step
   //wireWriteRegister(ina, INA_REG_CALIBRATION, calValue);

   // Now we can safely read the POWER register!
   wireReadRegister(ina, INA_REG_POWER, &value);

   return (int16_t)value;
}
#endif

#if 0
static int16_t getShuntVoltage_raw(uint8_t ina)
{
   uint16_t value = 0;
   wireReadRegister(ina, INA_REG_SHUNTVOLTAGE, &value);
   return (int16_t)value;
}

static float getShuntVoltage_mV(uint8_t ina)
{
   int16_t value = 0;
   value = getShuntVoltage_raw(ina);
   return value * 0.01f;
}
#endif

float getBusVoltage_V(uint8_t ina)
{
   int16_t value = getBusVoltage_raw(ina);
   printf("Voltage: %dV\r\n", value);
   return value * 0.001;
}

#if 0
static float getCurrent_mA(uint8_t ina)
{
   float valueDec = getCurrent_raw(ina);
   valueDec /= currentDivider_mA;
   return valueDec;
}
#endif

float getCurrentAmps(uint8_t ina)
{
   int16_t valueRaw;
   float valueDec;
   valueRaw = getCurrent_raw(ina);
   printf("Current: %dmA\r\n", valueRaw);
   valueDec = valueRaw;
   valueDec /= currentDivider_mA;
   valueDec /= 1000.0f;

   return valueDec;
}

/************
* ADN4600 interface
************/

void adn4600_init()
{
   uint8_t disables[] = {0xD0, 0xD8, 0xF0, 0xF};  // Channels 2, 3, 6, 7
   uint8_t configs[] = {
      (ADN4600_OUT_CFG_0 << 4) + ADN4600_OUT_0,
      (ADN4600_OUT_CFG_1 << 4) + ADN4600_OUT_1,
      (ADN4600_OUT_CFG_4 << 4) + ADN4600_OUT_4,
      (ADN4600_OUT_CFG_5 << 4) + ADN4600_OUT_5};
   uint8_t config;
   int rc;

   switch_i2c_bus(6);
   // Reset U39 P1_7, P1_3 and P0_0, and turn on LED LD13
   uint8_t data[3];
   data[0] = 0x6; // Config reg (6) and (7)
   data[1] = 0xFE; // Configure P0_0 (SI570_OE) as output (set those bits to 0)
   data[2] = 0x77; // Configure P1_7 (CLKMUX_RST) and P1_3 (LD13) as outputs (set those bits to 0)
   marble_I2C_send(I2C_FPGA, 0x42, data, 3);
   marble_SLEEP_ms(100);

   // from Part number(570_N_): N --> LVDS output with output enable polarity low
   // SI570_OE = 0, from a scope generally default freq = 125 MHz
   // but one should measure it through a frequency counter on FPGA
   data[0] = 0x2; // P0, by default it will jump to next address 3 for P1
   data[1] = 0x1; // Write one to P0_0
   // LEDs have reverse polarity
   data[2] = 0x04; // Write zero to P1_7 and one to P1_3
   marble_I2C_send(I2C_FPGA, 0x42, data, 3);

   marble_SLEEP_ms(1000);
   data[0] = 0x2; // P0
   data[1] = 0x00; // Write zero to P0_0, thereby enabling SI570
   data[2] = 0x80; // Write one to P1_7 and zero to P1_3 (LED 13 should be ON)
   marble_I2C_send(I2C_FPGA, 0x42, data, 3);

   switch_i2c_bus(2);
   marble_SLEEP_ms(100);

   // Disable Tx channels (ones that have N/C on PCB)
   const unsigned disable_len = sizeof disables / sizeof disables[0];
   for (unsigned ix=0; ix < disable_len; ix++) {
      uint8_t disable = disables[ix];
      config = 0;
      rc = marble_I2C_cmdsend(I2C_FPGA, ADN4600, disable, &config, 1);
      printf("> ADN4600 reg[0x%2.2x] <= 0x%2.2x (rc=%d)\r\n", disable, config, rc);
   }

   const unsigned config_len = sizeof configs / sizeof configs[0];
   for (unsigned ix=0; ix < config_len; ix++) {
      config = configs[ix];
      rc = marble_I2C_cmdsend(I2C_FPGA, ADN4600, ADN4600_XPT_Conf, &config, 1);
      printf("> ADN4600 XPT Conf <= 0x%2.2x (rc=%d)\r\n", config, rc);
   }

   // Table 9. Switch Core Temporary Registers
   uint8_t status;
   for (unsigned ix=0; ix<4; ix++) {
      uint8_t cmd = 0x58 + ix;
      rc = marble_I2C_cmdrecv(I2C_FPGA, ADN4600, cmd, &status, 1);
      printf("> ADN6400 XPT Temp %d r[0x%x] = 0x%2.2x (rc=%d)\r\n", ix, cmd, status, rc);
   }

   config = 1;
   rc = marble_I2C_cmdsend(I2C_FPGA, ADN4600, ADN4600_XPT_Update, &config, 1);
   printf("> ADN6400 Update (rc=%d)\r\n", rc);
}

void adn4600_printStatus()
{
   uint8_t status;

   for (unsigned ix = 0; ix < 8; ix++) {
      uint8_t cmd = ADN4600_XPT_Status0 + ix;
      marble_I2C_cmdrecv(I2C_FPGA, ADN4600, cmd, &status, 1);
      printf("> ADN4600 reg: %x: Output number: %d, Connected input: [%d]\r\n", cmd, ix, status);
   }
}


// Try read the values at all the registers 0x42 and 0x40
void pca9555_status()
{
   uint8_t val;
   switch_i2c_bus(6);
   for (unsigned jx = 0x42; jx <= 0x44; jx+=2) {
       printf("PCA9555 status at address 0x%x\r\n", jx);
       for (unsigned ix = 0; ix < 8; ix++) {
           uint8_t reg = 0x00 + ix;
           marble_I2C_cmdrecv(I2C_FPGA, jx, reg, &val, 1);
           printf("> Reg: %x: Value: %x\r\n", reg, val);
       }
   }
}


void pca9555_config()
{
   switch_i2c_bus(6);
   // Reset U39 P1_7, P1_3 and P0_0, and turn on LED LD13
   uint8_t data[3];
   data[0] = 0x6; // Config reg (6) and (7)
   data[1] = 0xFE; // Configure P0_0 (SI570_OE) as output (set those bits to 0)
   data[2] = 0x77; // Configure P1_7 (CLKMUX_RST) and P1_3 (LD13) as outputs (set those bits to 0)
   marble_I2C_send(I2C_FPGA, 0x42, data, 3);
   marble_SLEEP_ms(100);

   // from Part number(570_N_): N --> LVDS output with output enable polarity low
   // SI570_OE = 0, from a scope generally default freq = 125 MHz
   // but one should measure it through a frequency counter on FPGA
   data[0] = 0x2; // P0, by default it will jump to next address 3 for P1
   data[1] = 0x1; // Write one to P0_0
   // LEDs have reverse polarity
   data[2] = 0x04; // Write zero to P1_7 and one to P1_3
   marble_I2C_send(I2C_FPGA, 0x42, data, 3);

   // Reassert CLKMUX_RST
   marble_SLEEP_ms(1000);
   data[0] = 0x2; // P0
   data[1] = 0x00; // Write zero to P0_0, thereby enabling SI570
   data[2] = 0x80; // Write one to P1_7 and zero to P1_3 (LED 13 should be ON)
   marble_I2C_send(I2C_FPGA, 0x42, data, 3);
   printf("> reg: %x: value: %x\r\n", data[0], data[1]);
   printf("> reg: %x: value: %x\r\n", data[0]+1, data[2]);

   printf("Configuring PCA9555 at address 0x44\r\n");
   data[0] = 0x6; // Config regs 6(port 0) and 7(port 1)
   data[1] = 0x37; // Configure P0_7, P0_6 and P0_3 as outputs (set those bits to 0)
   data[2] = 0x37; // Configure P1_7, P1_6 and P0_3 as outputs (set those bits to 0)
   marble_I2C_send(I2C_FPGA, 0x44, data, 3);
   printf("> reg: %x: value: %x\r\n", data[0], data[1]);
   printf("> reg: %x: value: %x\r\n", data[0]+1, data[2]);
   marble_SLEEP_ms(100);

   data[0] = 0x2; // P1 and P2
   data[1] = 0x48; // Write ones to P0_7 and P0_3
   data[2] = 0x48; // Write ones to P1_7 and P1_3
   marble_I2C_send(I2C_FPGA, 0x44, data, 3);
   printf("> reg: %x: value: %x\r\n", data[0], data[1]);
   printf("> reg: %x: value: %x\r\n", data[0]+1, data[2]);
}

// Compute the current SI570 Frequency
void si570_status()
{
   switch_i2c_bus(6);
   uint8_t val[6];
   printf("SI570 status at address 0xee\r\n");
   // First do register reset
   // marble_I2C_cmdsend(I2C_FPGA, 0xee, 0x87, 128, 1);
   // marble_SLEEP_ms(100);
   for (unsigned ix = 0; ix < 6; ix++) {
      uint8_t reg = 0x0d + ix;
      marble_I2C_cmdrecv(I2C_FPGA, 0xee, reg, &val[ix], 1);
      printf("> Reg: %x: Value: %2.2x\r\n", reg, val[ix]);
   }

   uint8_t hs_div = (val[0] >> 5) + 4;
   uint8_t n1 = (((val[0] & 0x1f) << 2) | (val[1] >> 6)) + 1;
   double rfreq = (((val[1] & 0x3f) << 8) | (val[2])) * pow(2.0, -4.0);
   rfreq += ((val[3] << 16) | (val[4] << 8) | val[5]) * pow(2.0, -28.0);
   // Nominal internal crystal frequency
   // from datasheet, typically 114.285 MHz +/- 2000 ppm, see page 12
   // In the future, maybe this could this come from a non-volatile mmc parameter
   float fxtal = 114.286e6;
   float fout = (fxtal * rfreq)/(hs_div*n1);

   printf("> HS_DIV: %x\r\n", hs_div);
   printf("> N1: %x\r\n", n1);
   printf("> RFREQ: %lf\r\n", rfreq);
   printf("> assume fxtal: %f MHz\r\n", fxtal*0.000001);
   printf("> guess SI570 output: %f MHz\r\n", fout*0.000001);
}
