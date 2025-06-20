import time
from utils import read_reg, write_reg, log_result

def test_fmc_loopback():
    """Verify FMC loopback register"""
    try:
        write_reg("FMC_CTRL", 0xA5)
        time.sleep(0.1)
        val = read_reg("FMC_STATUS")
        passed = val == 0xA5
    except Exception as e:
        log_result("FMC Loopback", False)
        return False

    log_result("FMC Loopback", passed)
    return passed
