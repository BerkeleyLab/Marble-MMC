def read_reg(name):
    print(f"[MOCK] Reading from register: {name}")
    return 0x01 if name == "LED_STATUS" else 0xA5

def write_reg(name, value):
    print(f"[MOCK] Writing {hex(value)} to register: {name}")
