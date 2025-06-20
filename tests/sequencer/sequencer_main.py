import cocotb
from cocotb.triggers import Timer
import time
import os
import logging
from pathlib import Path

# Assume hardware access layer
from prog_support.hw_registers import read_reg, write_reg
from prog_support.bitfile_loader import load_bitfile

log = logging.getLogger("marble_test")
logging.basicConfig(level=logging.INFO)

# Sequencer configuration
BITFILE_MAP = {
    "v1.4": "bitfiles/marble_v1_4.bit",
    "v1.5": "bitfiles/marble_v1_5.bit"
}

RESULTS_LOG = Path("test_results.log")


def log_result(name, passed):
    with open(RESULTS_LOG, "a") as f:
        status = "PASS" if passed else "FAIL"
        log.info(f"{name}: {status}")
        f.write(f"{name}: {status}\n")


def select_bitfile(marble_version):
    return BITFILE_MAP.get(marble_version, None)


def test_led_register():
    """Check LED register toggles correctly"""
    try:
        write_reg("LED_CTRL", 0x01)
        time.sleep(0.1)
        result = read_reg("LED_STATUS")
        return result & 0x1 == 1
    except Exception as e:
        log.error(f"LED test failed: {e}")
        return False


def test_fmc_loopback():
    try:
        write_reg("FMC_CTRL", 0xA5)
        time.sleep(0.1)
        val = read_reg("FMC_STATUS")
        return val == 0xA5
    except Exception as e:
        log.error(f"FMC test failed: {e}")
        return False


def run_all_tests():
    log.info("Starting Marble Sequencer")
    RESULTS_LOG.unlink(missing_ok=True)

    marble_version = os.environ.get("MARBLE_VERSION", "v1.4")
    bitfile = select_bitfile(marble_version)
    if not bitfile or not Path(bitfile).exists():
        log.error("Bitfile not found.")
        return

    load_bitfile(bitfile)
    time.sleep(1)

    log_result("LED Test", test_led_register())
    log_result("FMC Loopback Test", test_fmc_loopback())
    # Add more test cases as needed


if __name__ == "__main__":
    run_all_tests()

