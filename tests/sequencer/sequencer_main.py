import sys
import os
from utils import setup_marble, is_hardware_connected, RESULTS_LOG
from test_leds import test_led_register
from test_fmc import test_fmc_loopback

def run_all_tests():
    RESULTS_LOG.unlink(missing_ok=True)

    marble_version = os.environ.get("MARBLE_VERSION", "v1.4.2")

    if not is_hardware_connected():
        print("Marble not connected.")
        sys.exit(1)

    try:
        setup_marble(marble_version)
    except Exception as e:
        print(f"Setup failed: {e}")
        sys.exit(1)

    all_passed = all([
        test_led_register(),
        test_fmc_loopback(),
        # Add more as needed
    ])
    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    run_all_tests()
