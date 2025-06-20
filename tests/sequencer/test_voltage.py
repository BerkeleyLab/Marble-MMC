# test_voltage.py
from prog_support.hw_registers import read_reg
import logging

log = logging.getLogger("marble_test")

# Expected voltage in mV and acceptable error range
EXPECTED_VOLTAGES = {
    "VCCINT": (1000, 100),  # 1.0V ± 100mV
    "VCCAUX": (1800, 100),  # 1.8V ± 100mV
    "VCCBRAM": (1000, 100), # etc.
}

ADC_CHANNEL_MAP = {
    "VCCINT": "ADC_CH0",
    "VCCAUX": "ADC_CH1",
    "VCCBRAM": "ADC_CH2"
}

def test_voltage():
    all_ok = True
    for rail, (expected_mv, tolerance) in EXPECTED_VOLTAGES.items():
        try:
            adc_reg = ADC_CHANNEL_MAP[rail]
            raw_val = read_reg(adc_reg)
            measured_mv = int((raw_val / 1023.0) * 2500)  # assuming 10-bit ADC and 2.5V ref

            if abs(measured_mv - expected_mv) > tolerance:
                log.warning(f"{rail} voltage out of range: {measured_mv} mV")
                all_ok = False
            else:
                log.info(f"{rail}: {measured_mv} mV OK")
        except Exception as e:
            log.error(f"Voltage read failed for {rail}: {e}")
            all_ok = False
    return all_ok
