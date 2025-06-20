# test_gpio_toggle.py
from prog_support.hw_registers import read_reg, write_reg
import time
import logging

log = logging.getLogger("marble_test")

def test_gpio_toggle():
    try:
        # Example GPIO bitmask (bit 0)
        gpio_bit = 0x01
        
        # Set GPIO high
        write_reg("GPIO_CTRL", gpio_bit)
        time.sleep(0.05)
        result_high = read_reg("GPIO_STATUS") & gpio_bit

        # Set GPIO low
        write_reg("GPIO_CTRL", 0x00)
        time.sleep(0.05)
        result_low = read_reg("GPIO_STATUS") & gpio_bit

        passed = (result_high == gpio_bit) and (result_low == 0)
        if not passed:
            log.warning(f"GPIO toggle failed: high={result_high}, low={result_low}")
        return passed
    except Exception as e:
        log.error(f"GPIO toggle test failed: {e}")
        return False
