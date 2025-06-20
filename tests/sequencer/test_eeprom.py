# test_eeprom.py
from prog_support.hw_registers import read_reg, write_reg
import time
import logging

log = logging.getLogger("marble_test")

def test_eeprom():
    try:
        test_addr = 0x10      # EEPROM offset address
        test_byte = 0x5A      # Arbitrary test value

        write_reg("EEPROM_ADDR", test_addr)
        write_reg("EEPROM_DATA", test_byte)
        write_reg("EEPROM_CTRL", 0x01)  # Trigger write
        time.sleep(0.1)  # Wait for write

        write_reg("EEPROM_ADDR", test_addr)
        write_reg("EEPROM_CTRL", 0x02)  # Trigger read
        time.sleep(0.05)
        result = read_reg("EEPROM_DATA") & 0xFF

        if result == test_byte:
            return True
        log.warning(f"EEPROM mismatch: wrote {hex(test_byte)}, read {hex(result)}")
        return False
    except Exception as e:
        log.error(f"EEPROM test failed: {e}")
        return False
