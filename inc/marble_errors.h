/*
 * MARBLE_ERRORS.h
 * Error handler specific definitions.
 */

#ifndef _MARBLE_ERRORS_H_
#define _MARBLE_ERRORS_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SIMULATION

#else /* ndef SIMULATION */

#include "marble_api.h"

static const char *ErrorCodeStrings[ERROR_CODE_COUNT] = { // triggers compile warning if not all enum values are covered
    "ERROR_NONE",
    "ERROR_MARBLE_POWERDOWN: Power failure detected",
    "ERROR_MARBLE_OVERTEMP: Over-temperature detected",
    "ERROR_MARBLE_PMOD: PMOD configuration error",
    "ERROR_EEPROM_FAN: fan speed",
    "ERROR_EEPROM_OVERTEMP: over-temperature threshold",
    "ERROR_EEPROM_UPDATE: failed to store page",
    "ERROR_EEPROM_STORE",
    "ERROR_EEPROM_READ",
    "ERROR_RCC_OSC_CONFIG",
    "ERROR_RCC_CLOCK_CONFIG",
    "ERROR_ETH_MDIO_INIT",
    "ERROR_ETH_MDIO_ID - PHY ID mismatch",
    "ERROR_I2C1_INIT - I2C_FPGA bus init failed",
    "ERROR_I2C3_INIT - I2C_PM bus init failed",
    "ERROR_I2C1_DEINIT - I2C_FPGA bus de-init failed",
    "ERROR_I2C3_DEINIT - I2C_PM bus de-init failed",
    "ERROR_SPI_TRANSMIT",
    "ERROR_SPI_READ16",
    "ERROR_SPI_EXCH16",
    "ERROR_SPI1_INIT",
    "ERROR_SPI2_INIT",
    "ERROR_SPI2_SR_TXE - Timeout",
    "ERROR_SPI2_SR_TXNE - Timeout",
    "ERROR_SPI2_SR_RXNE - Timeout",
    "ERROR_SPI2_SR_BSY - Timeout",
    "ERROR_UART_CONSOLE_INIT",
    "ERROR_I2C_FPGA_NONE - No error",
    "ERROR_I2C_FPGA_BERR - Bus error",
    "ERROR_I2C_FPGA_ARLO - Arbitration lost",
    "ERROR_I2C_FPGA_AF - No ACK received",
    "ERROR_I2C_FPGA_OVR - Overrun error",
    "ERROR_I2C_FPGA_DMA - DMA transfer error",
    "ERROR_I2C_FPGA_TIMEOUT - Timeout error",
    "ERROR_I2C_FPGA_BUSY - Bus busy",
    "ERROR_I2C_FPGA_HW_BUSY",
    "ERROR_I2C_FPGA_LOCKUP",
    "ERROR_I2C_FPGA_ADN4600",
    "ERROR_I2C_FPGA_UNDEFINED - Undefined I2C FPGA error",
    "ERROR_I2C_PM_NONE - No error",
    "ERROR_I2C_PM_BERR - Bus error",
    "ERROR_I2C_PM_ARLO - Arbitration lost",
    "ERROR_I2C_PM_AF - No ACK received",
    "ERROR_I2C_PM_OVR - Overrun error",
    "ERROR_I2C_PM_DMA - DMA transfer error",
    "ERROR_I2C_PM_TIMEOUT - Timeout error",
    "ERROR_I2C_PM_BUSY - Warning: Bus busy",
    "ERROR_I2C_PM_HW_BUSY",
    "ERROR_I2C_PM_LOCKUP",
    "ERROR_I2C_PM_UNDEFINED - Undefined I2C PM error",
    "ERROR_LTM_VOUT - An output voltage fault or warning has occurred",
    "ERROR_LTM_IOUT - An output current fault or warning has occurred",
    "ERROR_LTM_VIN - An input voltage fault or warning has occurred",
    "ERROR_LTM_MFR - A manufacturer specific fault has occurred",
    "ERROR_LTM_POWERNGD - The PWRGD pin, if enabled, is negated. Power is not good",
    "ERROR_LTM_BUSY - Device busy when PMBus command received",
    "ERROR_LTM_NOPOWER - The unit is not providing power to the output",
    "ERROR_LTM_VOUTOVER - An output overvoltage fault has occurred",
    "ERROR_LTM_IOUTOVER - An output overcurrent fault has occurred",
    "ERROR_LTM_VINUNDER - A VIN undervoltage fault has occurred",
    "ERROR_LTM_OVERTEMP - A temperature fault or warning has occurred",
    "ERROR_LTM_COMM - A communication, memory or logic fault has occurred",
    "ERROR_UNDEFINED - Good luck figuring this one out!"
};

static const char *ErrorCodeShortStrings[ERROR_CODE_COUNT] = { // triggers compile warning if not all enum values are covered
    "NONE",
    "MARBLE_POWERDOWN",
    "MARBLE_OVERTEMP",
    "MARBLE_PMOD",
    "EEPROM_FAN",
    "EEPROM_OVERTEMP",
    "EEPROM_UPDATE",
    "EEPROM_STORE",
    "EEPROM_READ",
    "RCC_OSC_CONFIG",
    "RCC_CLOCK_CONFIG",
    "ETH_MDIO_INIT",
    "ETH_MDIO_ID",
    "I2C1_INIT",
    "I2C3_INIT",
    "I2C1_DEINIT",
    "I2C3_DEINIT",
    "SPI_TRANSMIT",
    "SPI_READ16",
    "SPI_EXCH16",
    "SPI1_INIT",
    "SPI2_INIT",
    "SPI2_SR_TXE",
    "SPI2_SR_TXNE",
    "SPI2_SR_RXNE",
    "SPI2_SR_BSY",
    "UART_CONSOLE_INIT",
    "I2C_FPGA_NONE",
    "I2C_FPGA_BERR",
    "I2C_FPGA_ARLO",
    "I2C_FPGA_AF",
    "I2C_FPGA_OVR",
    "I2C_FPGA_DMA",
    "I2C_FPGA_TIMEOUT",
    "I2C_FPGA_BUSY",
    "I2C_FPGA_HW_BUSY",
    "I2C_FPGA_LOCKUP",
    "I2C_FPGA_ADN4600",
    "I2C_FPGA_UNDEFINED",
    "I2C_PM_NONE",
    "I2C_PM_BERR",
    "I2C_PM_ARLO",
    "I2C_PM_AF",
    "I2C_PM_OVR",
    "I2C_PM_DMA",
    "I2C_PM_TIMEOUT",
    "I2C_PM_BUSY",
    "I2C_PM_HW_BUSY",
    "I2C_PM_LOCKUP",
    "I2C_PM_UNDEFINED",
    "LTM_VOUT",
    "LTM_IOUT",
    "LTM_VIN",
    "LTM_MFR",
    "LTM_POWERNGD",
    "LTM_BUSY",
    "LTM_NOPOWER",
    "LTM_VOUTOVER",
    "LTM_IOUTOVER",
    "LTM_VINUNDER",
    "LTM_OVERTEMP",
    "LTM_COMM",
    "UNDEFINED"
};

#endif

#ifdef __cplusplus
}
#endif

#endif /* _MARBLE_API_H_ */
