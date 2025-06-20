# test_i2c.py
from prog_support.hw_registers import read_reg, write_reg
import logging
import time

log = logging.getLogger("marble_test")

def test_i2c():
    try:
        device_addr = 0x50  # Common EEPROM or sensor address
        reg_addr = 0x00     # Register to probe
        test_value = 0xA5

        write_reg("I2C_DEV_ADDR", device_addr)
        write_reg("I2C_REG_ADDR", reg_addr)
        write_reg("I2C_WRITE_DATA", test_value)
        write_reg("I2C_CTRL", 0x01)  # Trigger write
        time.sleep(0.05)

        write_reg("I2C_DEV_ADDR", device_addr)
        write_reg("I2C_REG_ADDR", reg_addr)
        write_reg("I2C_CTRL", 0x02)  # Trigger read
        time.sleep(0.05)
        read_back = read_reg("I2C_READ_DATA") & 0xFF

        if read_back == test_value:
            return True
        log.warning(f"I2C mismatch: wrote {hex(test_value)}, read {hex(read_back)}")
        return False
    except Exception as e:
        log.error(f"I2C test failed: {e}")
        return False
