import os
import time
import logging
from pathlib import Path

# Simulated hardware interface
from prog_support.hw_registers import read_reg, write_reg
from prog_support.bitfile_loader import load_bitfile

log = logging.getLogger("marble_test")
logging.basicConfig(level=logging.INFO)

BITFILE_MAP = {
    "v1.4": "bitfiles/marble_v1_4.bit",
    "v1.5": "bitfiles/marble_v1_5.bit"
}

RESULTS_LOG = Path("test_results.log")


def log_result(name, passed):
    status = "PASS" if passed else "FAIL"
    log.info(f"{name}: {status}")
    with open(RESULTS_LOG, "a") as f:
        f.write(f"{name}: {status}\n")


def select_bitfile(version):
    return BITFILE_MAP.get(version, None)


def setup_marble(version="v1.4"):
    bitfile = select_bitfile(version)
    if not bitfile or not Path(bitfile).exists():
        raise FileNotFoundError(f"Bitfile not found for version {version}")
    load_bitfile(bitfile)
    time.sleep(1)


def is_hardware_connected():
    try:
        _ = read_reg("ID")  # Assuming 'ID' is a safe register to read
        return True
    except Exception as e:
        log.error(f"Hardware not detected: {e}")
        return False
