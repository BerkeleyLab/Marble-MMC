#! /usr/bin/python3

# LTM4673 PMBus protocol definitions

# SMBus
# Legend:
#   ~> means controller to peripheral
#   <~ means peripheral to controller
#   |  is byte boundary with implied ACK (NACK and/or STOP at the end as needed)
#   !  is stop with repeated start
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~> ~~~~~~~~~~>
#   WRITE_BYTE:         | addr + wr  | command code | data byte |
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~> ~~~~~~~~~~~~~~> ~~~~~~~~~~~~~~~>
#   WRITE_WORD:         | addr + wr  | command code | data word low | data word high |
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~> ~~~~~~~~~~> ~~~~>
#   WRITE_BYTE w/ PEC:  | addr + wr  | command code | data byte | PEC |
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~> ~~~~~~~~~~~~~~> ~~~~~~~~~~~~~~~> ~~~~>
#   WRITE_WORD w/ PEC:  | addr + wr  | command code | data word low | data word high | PEC |
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~>
#   SEND_BYTE:          | addr + wr  | command code |
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~> ~~~~>
#   SEND_BYTE w/ PEC:   | addr + wr  | command code | PEC |
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~> ~~~~~~~~~~> <~~~~~~~~~~
#   READ_BYTE:          | addr + wr  | command code ! addr + rd | data byte |
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~> ~~~~~~~~~~> <~~~~~~~~~~~~~~~ <~~~~~~~~~~~~~~
#   READ_WORD:          | addr + wr  | command code ! addr + rd | data word high | data word low |
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~> ~~~~~~~~~~> <~~~~~~~~~~ <~~~~
#   READ_BYTE w/ PEC:   | addr + wr  | command code ! addr + rd | data byte | PEC |
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~> ~~~~~~~~~~> <~~~~~~~~~~~~~~~ <~~~~~~~~~~~~~~ <~~~~
#   READ_WORD w/ PEC:   | addr + wr  | command code ! addr + rd | data word high | data word low | PEC |
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~> ~~~~~~~~~~> <~~~~~~~~~~~~~ <~~~~~~~     <~~~~~~~~~
#   READ_BLOCK:         | addr + wr  | command code ! addr + rd | byte count N | byte 0 |...| byte N-1 |
#
#                        ~~~~~~~~~~~> ~~~~~~~~~~~~~> ~~~~~~~~~~> <~~~~~~~~~~~~~ <~~~~~~~     <~~~~~~~~~ <~~~~
#   READ_BLOCK w/ PEC:  | addr + wr  | command code ! addr + rd | byte count N | byte 0 |...| byte N-1 | PEC |

# Commands

# Supported SMBus commands
# Write Byte, Write Word, Send Byte
# Read Byte, Read Word, Block Read
# Alert Response Address

addr_alert  = 0x19 # 8-bit
addr_global = 0xb6 # 8-bit
addr_base   = 0xb8 # 8-bit (note this can be changed via MFR_I2C_BASE_ADDRESS register)
pin_offset  = 0
addr_dev    = addr_base + pin_offset

# nbytes = 0: send_byte command
# nbytes = 1: write_byte/read_byte command
# nbytes = 2: write_word/read_word command
# nbytes = 3: write_block/read_block command
MODE_SEND = 0
MODE_BYTE = 1
MODE_WORD = 2
MODE_BLOCK= 3

RD=1
WR=0

commands = {
    # name : (addr, nbytes)
    "PAGE":                          (0x00, MODE_BYTE),
    "OPERATION":                     (0x01, MODE_BYTE),
    "ON_OFF_CONFIG":                 (0x02, MODE_BYTE),
    "CLEAR_FAULTS":                  (0x03, MODE_SEND),
    "WRITE_PROTECT":                 (0x10, MODE_BYTE),
    "STORE_USER_ALL":                (0x15, MODE_SEND),
    "RESTORE_USER_ALL":              (0x16, MODE_SEND),
    "CAPABILITY":                    (0x19, MODE_BYTE),
    "VOUT_MODE":                     (0x20, MODE_BYTE),
    "VOUT_COMMAND":                  (0x21, MODE_WORD),
    "VOUT_MAX":                      (0x24, MODE_WORD),
    "VOUT_MARGIN_HIGH":              (0x25, MODE_WORD),
    "VOUT_MARGIN_LOW":               (0x26, MODE_WORD),
    "VIN_ON":                        (0x35, MODE_WORD),
    "VIN_OFF":                       (0x36, MODE_WORD),
    "IOUT_CAL_GAIN":                 (0x38, MODE_WORD),
    "VOUT_OV_FAULT_LIMIT":           (0x40, MODE_WORD),
    "VOUT_OV_FAULT_RESPONSE":        (0x41, MODE_BYTE),
    "VOUT_OV_WARN_LIMIT":            (0x42, MODE_WORD),
    "VOUT_UV_WARN_LIMIT":            (0x43, MODE_WORD),
    "VOUT_UV_FAULT_LIMIT":           (0x44, MODE_WORD),
    "VOUT_UV_FAULT_RESPONSE":        (0x45, MODE_BYTE),
    "IOUT_OC_FAULT_LIMIT":           (0x46, MODE_WORD),
    "IOUT_OC_FAULT_RESPONSE":        (0x47, MODE_BYTE),
    "IOUT_OC_WARN_LIMIT":            (0x4a, MODE_WORD),
    "IOUT_UC_FAULT_LIMIT":           (0x4b, MODE_WORD),
    "IOUT_UC_FAULT_RESPONSE":        (0x4c, MODE_BYTE),
    "OT_FAULT_LIMIT":                (0x4f, MODE_WORD),
    "OT_FAULT_RESPONSE":             (0x50, MODE_BYTE),
    "OT_WARN_LIMIT":                 (0x51, MODE_WORD),
    "UT_WARN_LIMIT":                 (0x52, MODE_WORD),
    "UT_FAULT_LIMIT":                (0x53, MODE_WORD),
    "UT_FAULT_RESPONSE":             (0x54, MODE_BYTE),
    "VIN_OV_FAULT_LIMIT":            (0x55, MODE_WORD),
    "VIN_OV_FAULT_RESPONSE":         (0x56, MODE_BYTE),
    "VIN_OV_WARN_LIMIT":             (0x57, MODE_WORD),
    "VIN_UV_WARN_LIMIT":             (0x58, MODE_WORD),
    "VIN_UV_FAULT_LIMIT":            (0x59, MODE_WORD),
    "VIN_UV_FAULT_RESPONSE":         (0x5a, MODE_BYTE),
    "POWER_GOOD_ON":                 (0x5e, MODE_WORD),
    "POWER_GOOD_OFF":                (0x5f, MODE_WORD),
    "TON_DELAY":                     (0x60, MODE_WORD),
    "TON_RISE":                      (0x61, MODE_WORD),
    "TON_MAX_FAULT_LIMIT":           (0x62, MODE_WORD),
    "TON_MAX_FAULT_RESPONSE":        (0x63, MODE_BYTE),
    "TOFF_DELAY":                    (0x64, MODE_WORD),
    "STATUS_BYTE":                   (0x78, MODE_BYTE),
    "STATUS_WORD":                   (0x79, MODE_WORD),
    "STATUS_VOUT":                   (0x7a, MODE_BYTE),
    "STATUS_IOUT":                   (0x7b, MODE_BYTE),
    "STATUS_INPUT":                  (0x7c, MODE_BYTE),
    "STATUS_TEMPERATURE":            (0x7d, MODE_BYTE),
    "STATUS_CML":                    (0x7e, MODE_BYTE),
    "STATUS_MFR_SPECIFIC":           (0x80, MODE_BYTE),
    "READ_VIN":                      (0x88, MODE_WORD),
    "READ_IIN":                      (0x89, MODE_WORD),
    "READ_VOUT":                     (0x8b, MODE_WORD),
    "READ_IOUT":                     (0x8c, MODE_WORD),
    "READ_TEMPERATURE_1":            (0x8d, MODE_WORD),
    "READ_TEMPERATURE_2":            (0x8e, MODE_WORD),
    "READ_POUT":                     (0x96, MODE_WORD),
    "READ_PIN":                      (0x97, MODE_WORD),
    "PMBUS_REVISION":                (0x98, MODE_BYTE),
    "USER_DATA_00":                  (0xb0, MODE_WORD),
    "USER_DATA_01":                  (0xb1, MODE_WORD),
    "USER_DATA_02":                  (0xb2, MODE_WORD),
    "USER_DATA_03":                  (0xb3, MODE_WORD),
    "USER_DATA_04":                  (0xb4, MODE_WORD),
    "MFR_LTC_RESERVED_1":            (0xb5, MODE_WORD),
    "MFR_T_SELF_HEAT":               (0xb8, MODE_WORD),
    "MFR_IOUT_CAL_GAIN_TAU_INV":     (0xb9, MODE_WORD),
    "MFR_IOUT_CAL_GAIN_THETA":       (0xba, MODE_WORD),
    "MFR_READ_IOUT":                 (0xbb, MODE_WORD),
    "MFR_LTC_RESERVED_2":            (0xbc, MODE_WORD),
    "MFR_EE_UNLOCK":                 (0xbd, MODE_BYTE),
    "MFR_EE_ERASE":                  (0xbe, MODE_BYTE),
    "MFR_EE_DATA":                   (0xbf, MODE_WORD),
    "MFR_EIN":                       (0xc0, MODE_BLOCK),
    "MFR_EIN_CONFIG":                (0xc1, MODE_BYTE),
    "MFR_SPECIAL_LOT":               (0xc2, MODE_BYTE),
    "MFR_IIN_CAL_GAIN_TC":           (0xc3, MODE_WORD),
    "MFR_IIN_PEAK":                  (0xc4, MODE_WORD),
    "MFR_IIN_MIN":                   (0xc5, MODE_WORD),
    "MFR_PIN_PEAK":                  (0xc6, MODE_WORD),
    "MFR_PIN_MIN":                   (0xc7, MODE_WORD),
    "MFR_COMMAND_PLUS":              (0xc8, MODE_WORD),
    "MFR_DATA_PLUS0":                (0xc9, MODE_WORD),
    "MFR_DATA_PLUS1":                (0xca, MODE_WORD),
    "MFR_CONFIG_LTM4673":            (0xd0, MODE_WORD),
    "MFR_CONFIG_ALL_LTM4673":        (0xd1, MODE_WORD),
    "MFR_FAULTB0_PROPAGATE":         (0xd2, MODE_BYTE),
    "MFR_FAULTB1_PROPAGATE":         (0xd3, MODE_BYTE),
    "MFR_PWRGD_EN":                  (0xd4, MODE_WORD),
    "MFR_FAULTB0_RESPONSE":          (0xd5, MODE_BYTE),
    "MFR_FAULTB1_RESPONSE":          (0xd6, MODE_BYTE),
    "MFR_IOUT_PEAK":                 (0xd7, MODE_WORD),
    "MFR_IOUT_MIN":                  (0xd8, MODE_WORD),
    "MFR_CONFIG2_LTM4673":           (0xd9, MODE_BYTE),
    "MFR_CONFIG3_LTM4673":           (0xda, MODE_BYTE),
    "MFR_RETRY_DELAY":               (0xdb, MODE_WORD),
    "MFR_RESTART_DELAY":             (0xdc, MODE_WORD),
    "MFR_VOUT_PEAK":                 (0xdd, MODE_WORD),
    "MFR_VIN_PEAK":                  (0xde, MODE_WORD),
    "MFR_TEMPERATURE_1_PEAK":        (0xdf, MODE_WORD),
    "MFR_DAC":                       (0xe0, MODE_WORD),
    "MFR_POWERGOOD_ASSERTION_DELAY": (0xe1, MODE_WORD),
    "MFR_WATCHDOG_T_FIRST":          (0xe2, MODE_WORD),
    "MFR_WATCHDOG_T":                (0xe3, MODE_WORD),
    "MFR_PAGE_FF_MASK":              (0xe4, MODE_BYTE),
    "MFR_PADS":                      (0xe5, MODE_WORD),
    "MFR_I2C_BASE_ADDRESS":          (0xe6, MODE_BYTE),
    "MFR_SPECIAL_ID":                (0xe7, MODE_WORD),
    "MFR_IIN_CAL_GAIN":              (0xe8, MODE_WORD),
    "MFR_VOUT_DISCHARGE_THRESHOLD":  (0xe9, MODE_WORD),
    "MFR_FAULT_LOG_STORE":           (0xea, MODE_SEND),
    "MFR_FAULT_LOG_RESTORE":         (0xeb, MODE_SEND),
    "MFR_FAULT_LOG_CLEAR":           (0xec, MODE_SEND),
    "MFR_FAULT_LOG_STATUS":          (0xed, MODE_BYTE),
    "MFR_FAULT_LOG":                 (0xee, MODE_BLOCK),
    "MFR_COMMON":                    (0xef, MODE_BYTE),
    "MFR_IOUT_CAL_GAIN_TC":          (0xf6, MODE_WORD),
    "MFR_RETRY_COUNT":               (0xf7, MODE_BYTE),
    "MFR_TEMP_1_GAIN":               (0xf8, MODE_WORD),
    "MFR_TEMP_1_OFFSET":             (0xf9, MODE_WORD),
    "MFR_IOUT_SENSE_VOLTAGE":        (0xfa, MODE_WORD),
    "MFR_VOUT_MIN":                  (0xfb, MODE_WORD),
    "MFR_VIN_MIN":                   (0xfc, MODE_WORD),
    "MFR_TEMPERATURE_1_MIN":         (0xfd, MODE_WORD),
}

def print_commands_c():
    for name, arg in commands.items():
        addr, mode = arg
        print(f"#define LTM4673_{name:32s} (0x{addr:02x})")
    return

def write_byte(cmd, val):
    #   WRITE_BYTE:         | cmd + wr  | command code | data byte |
    args = vet_args(cmd, val)
    if args is None:
        return False
    else:
        cmd, val = args
    if hasattr(val, '__len__'):
        # write byte can only have one byte
        val = val[0]
    val = val & 0xff
    msg = (addr_dev + WR, cmd, val)
    return (msg,)

def write_word(cmd, val):
    #   WRITE_WORD:         | addr + wr  | command code | data word low | data word high |
    args = vet_args(cmd, val)
    if args is None:
        return False
    else:
        cmd, val = args
    if not hasattr(val, '__len__'):
        # val should be list of bytes (lsb to msb)
        val = (val & 0xff, (val >> 8) & 0xff)
    msg = (addr_dev + WR, cmd, val)
    return (msg,)

def send_byte(cmd, val):
    # val ignored
    #   SEND_BYTE:          | addr + wr  | command code |
    args = vet_args(cmd, 0)
    if args is None:
        return False
    else:
        cmd, val = args
    msg = (addr_dev + WR, cmd)
    return (msg,)

def write_byte_pec(cmd, val):
    #   WRITE_BYTE w/ PEC:  | addr + wr  | command code | data byte | PEC |
    args = vet_args(cmd, val)
    if args is None:
        return False
    else:
        cmd, val = args
    if hasattr(val, '__len__'):
        # write byte can only have one byte
        val = val[0]
    val = val & 0xff
    msg = [addr_dev + WR, cmd, val]
    pec = calc_pec(*msg)
    return (msg + [pec],)

def write_word_pec(cmd, val):
    #   WRITE_WORD w/ PEC:  | addr + wr  | command code | data word low | data word high | PEC |
    args = vet_args(cmd, val)
    if args is None:
        return False
    else:
        cmd, val = args
    if not hasattr(val, '__len__'):
        # val should be list of bytes (lsb to msb)
        val = (val & 0xff, (val >> 8) & 0xff)
    msg = [addr_dev + WR, cmd, val]
    pec = calc_pec(*msg)
    return (msg + [pec],)

def send_byte_pec(cmd, val):
    # val ignored
    #   SEND_BYTE w/ PEC:   | addr + wr  | command code | PEC |
    args = vet_args(cmd, 0)
    if args is None:
        return False
    else:
        cmd, val = args
    msg = [addr_dev + WR, cmd]
    pec = calc_pec(*msg)
    return (msg + [pec],)

def read_byte(cmd, val):
    # val ignored
    #   READ_BYTE:          | addr + wr  | command code ! addr + rd | data byte |
    args = vet_args(cmd, 0)
    if args is None:
        return False
    else:
        cmd, val = args
    msg = (addr_dev + WR, cmd)
    rsp = (addr_dev + RD, None)
    return (msg, rsp)

def read_word(cmd, val):
    # val ignored
    #   READ_WORD:          | addr + wr  | command code ! addr + rd | data word high | data word low |
    args = vet_args(cmd, 0)
    if args is None:
        return False
    else:
        cmd, val = args
    msg = (addr_dev + WR, cmd)
    rsp = (addr_dev + RD, None, None)
    return (msg, rsp)

def read_byte_pec(cmd, val):
    #   READ_BYTE w/ PEC:   | addr + wr  | command code ! addr + rd | data byte | PEC |
    return read_word(cmd, val)

def read_word_pec(cmd, val):
    # val ignored
    #   READ_WORD w/ PEC:   | addr + wr  | command code ! addr + rd | data word high | data word low | PEC |
    args = vet_args(cmd, 0)
    if args is None:
        return False
    else:
        cmd, val = args
    msg = (addr_dev + WR, cmd)
    rsp = (addr_dev + RD, None, None, None)
    return (msg, rsp)

def read_block(cmd, val):
    # val ignored
    #   READ_BLOCK:         | addr + wr  | command code ! addr + rd | byte count N | byte 0 |...| byte N-1 |
    args = vet_args(cmd, 0)
    if args is None:
        return False
    else:
        cmd, val = args
    msg = (addr_dev + WR, cmd)
    rsp = (addr_dev + RD, None, [])
    return (msg, rsp)

def read_block_pec(cmd, val):
    #   READ_BLOCK w/ PEC:  | addr + wr  | command code ! addr + rd | byte count N | byte 0 |...| byte N-1 | PEC |
    return read_block(cmd, val)

def get_cmd_params(cmd):
    """Returns (name_string, command_code, mode) where mode is one of:
    MODE_BYTE, MODE_WORD, MODE_SEND, MODE_BLOCK"""
    if hasattr(cmd, 'lower'): # it's a string
        cmd_mode = commands.get(cmd, None)
        if cmd_mode is None:
            return None
        cmd_code, mode = cmd_mode
        return (cmd, cmd_code, mode)
    else:
        for name, val in commands.items():
            vcmd, mode = val
            if cmd == vcmd:
                return (name, vcmd, mode)
    return None

def get_xact(cmd, val, pec=False):
    """Set val = None for READ. Otherwise WRITE."""
    if val is None:
        read = True
    else:
        read = False
    params = get_cmd_params(cmd)
    if params is None:
        return None
    name, cmd, mode = params
    if mode == MODE_SEND:
        if pec:
            return send_byte_pec(cmd, 0)
        else:
            return send_byte(cmd, 0)
    elif mode == MODE_BYTE:
        if read:
            if pec:
                return read_byte_pec(cmd, 0)
            else:
                return read_byte(cmd, 0)
        else:
            if pec:
                return write_byte_pec(cmd, val)
            else:
                return write_byte(cmd, val)
    elif mode == MODE_WORD:
        if read:
            if pec:
                return read_word_pec(cmd, 0)
            else:
                return read_word(cmd, 0)
        else:
            if pec:
                return write_word_pec(cmd, val)
            else:
                return write_word(cmd, val)
    elif mode == MODE_BLOCK:
        if read:
            if pec:
                return read_block_pec(cmd, 0)
            else:
                return read_block(cmd, 0)
        else:
            return None
    # Unknown mode
    return None

def vet_args(cmd, val):
    try:
        cmd = int(cmd)
    except ValueError:
        cmd = commands.get(cmd, None)
    if cmd is None:
        return None
    cmd = cmd & 0xff
    # val can be int or list of ints (LSB-first)
    try:
        val = int(val) & 0xff
    except ValueError:
        pass
    if hasattr(val, '__len__'):
        ok = True
        for n in range(len(val)):
            if not isinstance(val[n], int):
                ok = False
                break
            val[n] = val[n] & 0xff
        if not ok:
            return None
    return cmd, val

# =============================== PROGRAM API ================================
# Each returns nested list of bytes
#   ((addr_byte + rnw, cmd_code, ...), (addr_byte + rnw, cmd_code, ...), ...)
# If multiple transactions are present, they should be joined by repeated start
# If a msg byte is None, it is a placeholder for a single byte read
# If a msg byte is a list (empty), it means an unknown number of bytes should be
# read (determined by the N value returned by the first read byte).

def write(cmd, val):
    return get_xact(cmd, val, pec=False)

def read(cmd):
    return get_xact(cmd, None, pec=False)

def translate_xact_mmc(xact):
    line = []
    first = True
    for msg in xact:
        if not first:
            line.append(MMC_REPEAT_START)
        for mbyte in msg:
            if mbyte is None:
                line.append(MMC_READ_ONE)
            elif hasattr(mbyte, '__len__'):
                line.append(MMC_READ_BLOCK)
            else:
                line.append("0x{mbyte:02x}")
        first = False
    return line

def translate_mmc(xacts):
    lines = []
    for xact in xacts:
        line = ' '.join(translate_xact_mmc(xact))
        lines.append(line)
    return lines

def translate_xact_i2cbridge(xact):
    # TODO
    return []

def translate_i2cbridge(xact):
    lines = []
    for xact in xacts:
        line = ' '.join(translate_xact_i2cbridge(xact))
        lines.append(line)
    return lines

MMC_REPEAT_START    = '!'
MMC_READ_ONE        = '?'
MMC_READ_BLOCK      = '*'

# TODO
#   Translate transactions to specific syntaces
#   1. MMC console syntax
#   2. i2cbridge syntax

# MMC console syntax
# Each line is a list of any of the following (whitespace-separated)
#   ! : Repeated start
#   ? : Read 1 byte from the target device
#   * : Read 1 byte, then use that as N and read the next N bytes
#   0xHH: Use hex value 0xHH as the next transaction byte
#   DDD : Use decimal value DDD as the next transaction byte

if __name__ == "__main__":
    print_commands_c()
