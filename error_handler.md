# Marble Error Handler

The Marble error handler is located in board\_support/marble\_v2/marble\_board.c. The error handler reports errors through the UART console and logs the total error count and last event for each error code. The error handler is muted until board bringup has been completed (i.e. as long as SN=0000). The following error codes have been implemented: 

| Error Code | Explanation |
| :--- | :--- |
| `ERROR_NONE` | No error detected.|
| `ERROR_MARBLE_POWERDOWN` | Power failure detected. |
| `ERROR_MARBLE_OVERTEMP` | Over-temperature detected. |
| `ERROR_MARBLE_PMOD` | PMOD configuration error encountered. |
| `ERROR_EEPROM_FAN` | EEPROM issue related to the fan speed. |
| `ERROR_EEPROM_OVERTEMP` | EEPROM issue related to over-temperature threshold. |
| `ERROR_EEPROM_UPDATE` | EEPROM update failure. |
| `ERROR_EEPROM_STORE` | EEPROM write operation error. |
| `ERROR_EEPROM_READ` | EEPROM read operation error. |
| `ERROR_RCC_OSC_CONFIG` | Configuration error related to the internal oscillator. |
| `ERROR_RCC_CLOCK_CONFIG` | Configuration error related to the system clock. |
| `ERROR_ETH_MDIO_INIT` | Error during the initialization of the Ethernet MDIO interface. |
| `ERROR_ETH_MDIO_ID` | PHY ID mismatch detected on the Ethernet interface. |
| `ERROR_I2C1_INIT` | I2C_FPGA bus initialization error. |
| `ERROR_I2C3_INIT` | I2C_PM bus initialization error. |
| `ERROR_I2C1_DEINIT` | I2C_FPGA bus de-initialization error. |
| `ERROR_I2C3_DEINIT` | I2C_PM bus de-initialization error. |
| `ERROR_SPI1_INIT` | Initialization failed for the SPI Port 1. |
| `ERROR_UART_CONSOLE_INIT` | Initialization failed for the UART console. |
| `ERROR_I2C_FPGA_NONE` | Unspecified error reported on the I2C FPGA bus. |
| `ERROR_I2C_FPGA_BERR` | General bus error detected on the I2C FPGA interface. |
| `ERROR_I2C_FPGA_ARLO` | Arbitration lost on the I2C FPGA bus. |
| `ERROR_I2C_FPGA_AF` | No Acknowledge (ACK) received from a device on the I2C FPGA bus. |
| `ERROR_I2C_FPGA_OVR` | Overrun error detected on the I2C FPGA bus. |
| `ERROR_I2C_FPGA_DMA` | DMA error on the I2C FPGA bus. |
| `ERROR_I2C_FPGA_TIMEOUT` | Timeout error occurred while communicating with the FPGA over I2C. |
| `ERROR_I2C_FPGA_BUSY` | The I2C FPGA bus is currently busy. |
| `ERROR_I2C_FPGA_HW_BUSY` | The I2C FPGA hardware is busy. |
| `ERROR_I2C_FPGA_LOCKUP` | The I2C FPGA bus has locked up. |
| `ERROR_I2C_FPGA_ADN4600` | I2C FPGA bus error related to the ADN4600. |
| `ERROR_I2C_FPGA_UNDEFINED` | Undefined error occurred related to the I2C FPGA bus. |
| `ERROR_I2C_PM_NONE` | Unspecified error reported on the I2C PM bus. |
| `ERROR_I2C_PM_BERR` | General bus error detected on the I2C PM interface. |
| `ERROR_I2C_PM_ARLO` | Arbitration lost on the I2C PM bus. |
| `ERROR_I2C_PM_AF` | No Acknowledge (ACK) received on the I2C PM bus. |
| `ERROR_I2C_PM_OVR` | Overrun error detected on the I2C PM bus. |
| `ERROR_I2C_PM_DMA` | DMA error on the I2C PM bus. |
| `ERROR_I2C_PM_TIMEOUT` | Timeout error occurred while communicating with the I2C PM bus. |
| `ERROR_I2C_PM_BUSY` | The I2C PM bus is currently busy. |
| `ERROR_I2C_PM_HW_BUSY` | The I2C PM hardware is busy. |
| `ERROR_I2C_PM_LOCKUP` | The I2C PM bus has locked up. |
| `ERROR_I2C_PM_UNDEFINED` | Undefined error occurred related to the I2C PM bus. |
| `ERROR_LTM_VOUT` | An output voltage fault or warning has occurred on the LTM. |
| `ERROR_LTM_IOUT` | An output current fault or warning has occurred on the LTM. |
| `ERROR_LTM_VIN` | An input voltage fault or warning has occurred on the LTM. |
| `ERROR_LTM_MFR` | A manufacturer-specific fault has been reported on the LTM. |
| `ERROR_LTM_POWERNGD` | The PWRGD pin, if enabled, is negated, indicating poor power status. |
| `ERROR_LTM_BUSY` | The LTM device is busy when a PMBus command is received. |
| `ERROR_LTM_NOPOWER` | The LTM unit is not providing power to the output. |
| `ERROR_LTM_VOUTOVER` | An output overvoltage fault has occurred on the LTM. |
| `ERROR_LTM_IOUTOVER` | An output overcurrent fault has occurred on the LTM. |
| `ERROR_LTM_VINUNDER` | A VIN undervoltage fault has occurred on the LTM. |
| `ERROR_LTM_OVERTEMP` | A temperature fault or warning has occurred on the LTM. |
| `ERROR_LTM_COMM` | A general communication, memory, or logic fault has occurred on the LTM. |
| `ERROR_UNDEFINED` | An unknown or unclassified error occurred. |
