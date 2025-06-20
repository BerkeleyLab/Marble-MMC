# test_uart_loopback.py
from prog_support.hw_registers import read_reg, write_reg
import time
import logging

log = logging.getLogger("marble_test")

def test_uart_loopback():
    try:
        test_byte = 0xAB
        write_reg("UART_TX", test_byte)     # Send byte
        time.sleep(0.05)
        result = read_reg("UART_RX") & 0xFF # Mask to 8 bits
        if result == test_byte:
            return True
        log.warning(f"UART loopback mismatch: sent {hex(test_byte)}, received {hex(result)}")
        return False
    except Exception as e:
        log.error(f"UART loopback test failed: {e}")
        return False
