import time
from utils import read_reg, write_reg, log_result

def test_led_register():
    """Toggle LED control and verify status"""
    try:
        write_reg("LED_CTRL", 0x01)
        time.sleep(0.1)
        val = read_reg("LED_STATUS")
        passed = (val & 0x1) == 1
    except Exception as e:
        log_result("LED Test", False)
        return False

    log_result("LED Test", passed)
    return passed
