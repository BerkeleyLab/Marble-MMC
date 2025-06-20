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
    "v1.4.2": "bitfiles/marble2.1df8621f.bit"
}

RESULTS_LOG = Path("test_results.log")


def log_result(name, passed):
    status = "PASS" if passed else "FAIL"
    log.info(f"{name}: {status}")
    with open(RESULTS_LOG, "a") as f:
        f.write(f"{name}: {status}\n")


def select_bitfile(version):
    return BITFILE_MAP.get(version, None)


def setup_marble(version):
    from prog_support.bitfile_loader import load_bitfile
    from prog_support.hw_registers import read_reg

    bitfile = select_bitfile(version)
    if not bitfile or not Path(bitfile).exists():
        raise FileNotFoundError(f"Bitfile not found for version {version}")

    print(f"Loading bitfile: {bitfile}")
    load_bitfile(bitfile)

    id_reg = read_reg("ID")
    print(f"Read back ID register: {id_reg}")

def is_hardware_connected():
    print("[MOCK] Reading from register: ID")
    return True  # or implement real check
