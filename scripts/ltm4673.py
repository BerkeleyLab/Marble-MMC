#! /usr/bin/python3

# LTM4673 PMBus protocol definitions

import re

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

MMC_COMMAND_CHAR_PMBRIDGE = 't'
addr_alert  = 0x19  # 8-bit
addr_global = 0xb6  # 8-bit
addr_base   = 0xb8  # 8-bit (0x5c 7-bit)(note this can be changed via MFR_I2C_BASE_ADDRESS register)
pin_offset  = 4
addr_dev    = addr_base + 2*pin_offset

# nbytes = 0: send_byte command
# nbytes = 1: write_byte/read_byte command
# nbytes = 2: write_word/read_word command
# nbytes = 3: write_block/read_block command
MODE_SEND   = 0
MODE_BYTE   = 1
MODE_WORD   = 2
MODE_BLOCK  = 3

RD = 1
WR = 0

ENCODING_RAW = 0
ENCODING_L11 = 1
ENCODING_L16 = 2

commands = {
    # name : (addr, nbytes)
    "PAGE":                          (0x00, MODE_BYTE, ENCODING_RAW),
    "OPERATION":                     (0x01, MODE_BYTE, ENCODING_RAW),
    "ON_OFF_CONFIG":                 (0x02, MODE_BYTE, ENCODING_RAW),
    "CLEAR_FAULTS":                  (0x03, MODE_SEND, ENCODING_RAW),
    "WRITE_PROTECT":                 (0x10, MODE_BYTE, ENCODING_RAW),
    "STORE_USER_ALL":                (0x15, MODE_SEND, ENCODING_RAW),
    "RESTORE_USER_ALL":              (0x16, MODE_SEND, ENCODING_RAW),
    "CAPABILITY":                    (0x19, MODE_BYTE, ENCODING_RAW),
    "VOUT_MODE":                     (0x20, MODE_BYTE, ENCODING_RAW),
    "VOUT_COMMAND":                  (0x21, MODE_WORD, ENCODING_L16),
    "VOUT_MAX":                      (0x24, MODE_WORD, ENCODING_L16),
    "VOUT_MARGIN_HIGH":              (0x25, MODE_WORD, ENCODING_L16),
    "VOUT_MARGIN_LOW":               (0x26, MODE_WORD, ENCODING_L16),
    "VIN_ON":                        (0x35, MODE_WORD, ENCODING_L11),
    "VIN_OFF":                       (0x36, MODE_WORD, ENCODING_L11),
    "IOUT_CAL_GAIN":                 (0x38, MODE_WORD, ENCODING_L11),
    "VOUT_OV_FAULT_LIMIT":           (0x40, MODE_WORD, ENCODING_L16),
    "VOUT_OV_FAULT_RESPONSE":        (0x41, MODE_BYTE, ENCODING_RAW),
    "VOUT_OV_WARN_LIMIT":            (0x42, MODE_WORD, ENCODING_L16),
    "VOUT_UV_WARN_LIMIT":            (0x43, MODE_WORD, ENCODING_L16),
    "VOUT_UV_FAULT_LIMIT":           (0x44, MODE_WORD, ENCODING_L16),
    "VOUT_UV_FAULT_RESPONSE":        (0x45, MODE_BYTE, ENCODING_RAW),
    "IOUT_OC_FAULT_LIMIT":           (0x46, MODE_WORD, ENCODING_L11),
    "IOUT_OC_FAULT_RESPONSE":        (0x47, MODE_BYTE, ENCODING_RAW),
    "IOUT_OC_WARN_LIMIT":            (0x4a, MODE_WORD, ENCODING_L11),
    "IOUT_UC_FAULT_LIMIT":           (0x4b, MODE_WORD, ENCODING_L11),
    "IOUT_UC_FAULT_RESPONSE":        (0x4c, MODE_BYTE, ENCODING_RAW),
    "OT_FAULT_LIMIT":                (0x4f, MODE_WORD, ENCODING_L11),
    "OT_FAULT_RESPONSE":             (0x50, MODE_BYTE, ENCODING_RAW),
    "OT_WARN_LIMIT":                 (0x51, MODE_WORD, ENCODING_L11),
    "UT_WARN_LIMIT":                 (0x52, MODE_WORD, ENCODING_L11),
    "UT_FAULT_LIMIT":                (0x53, MODE_WORD, ENCODING_L11),
    "UT_FAULT_RESPONSE":             (0x54, MODE_BYTE, ENCODING_RAW),
    "VIN_OV_FAULT_LIMIT":            (0x55, MODE_WORD, ENCODING_L11),
    "VIN_OV_FAULT_RESPONSE":         (0x56, MODE_BYTE, ENCODING_RAW),
    "VIN_OV_WARN_LIMIT":             (0x57, MODE_WORD, ENCODING_L11),
    "VIN_UV_WARN_LIMIT":             (0x58, MODE_WORD, ENCODING_L11),
    "VIN_UV_FAULT_LIMIT":            (0x59, MODE_WORD, ENCODING_L11),
    "VIN_UV_FAULT_RESPONSE":         (0x5a, MODE_BYTE, ENCODING_RAW),
    "POWER_GOOD_ON":                 (0x5e, MODE_WORD, ENCODING_L16),
    "POWER_GOOD_OFF":                (0x5f, MODE_WORD, ENCODING_L16),
    "TON_DELAY":                     (0x60, MODE_WORD, ENCODING_L11),
    "TON_RISE":                      (0x61, MODE_WORD, ENCODING_L11),
    "TON_MAX_FAULT_LIMIT":           (0x62, MODE_WORD, ENCODING_L11),
    "TON_MAX_FAULT_RESPONSE":        (0x63, MODE_BYTE, ENCODING_RAW),
    "TOFF_DELAY":                    (0x64, MODE_WORD, ENCODING_L11),
    "STATUS_BYTE":                   (0x78, MODE_BYTE, ENCODING_RAW),
    "STATUS_WORD":                   (0x79, MODE_WORD, ENCODING_RAW),
    "STATUS_VOUT":                   (0x7a, MODE_BYTE, ENCODING_RAW),
    "STATUS_IOUT":                   (0x7b, MODE_BYTE, ENCODING_RAW),
    "STATUS_INPUT":                  (0x7c, MODE_BYTE, ENCODING_RAW),
    "STATUS_TEMPERATURE":            (0x7d, MODE_BYTE, ENCODING_RAW),
    "STATUS_CML":                    (0x7e, MODE_BYTE, ENCODING_RAW),
    "STATUS_MFR_SPECIFIC":           (0x80, MODE_BYTE, ENCODING_RAW),
    "READ_VIN":                      (0x88, MODE_WORD, ENCODING_L11),
    "READ_IIN":                      (0x89, MODE_WORD, ENCODING_L11),
    "READ_VOUT":                     (0x8b, MODE_WORD, ENCODING_L16),
    "READ_IOUT":                     (0x8c, MODE_WORD, ENCODING_L11),
    "READ_TEMPERATURE_1":            (0x8d, MODE_WORD, ENCODING_L11),
    "READ_TEMPERATURE_2":            (0x8e, MODE_WORD, ENCODING_L11),
    "READ_POUT":                     (0x96, MODE_WORD, ENCODING_L11),
    "READ_PIN":                      (0x97, MODE_WORD, ENCODING_L11),
    "PMBUS_REVISION":                (0x98, MODE_BYTE, ENCODING_RAW),
    "USER_DATA_00":                  (0xb0, MODE_WORD, ENCODING_RAW),
    "USER_DATA_01":                  (0xb1, MODE_WORD, ENCODING_RAW),
    "USER_DATA_02":                  (0xb2, MODE_WORD, ENCODING_RAW),
    "USER_DATA_03":                  (0xb3, MODE_WORD, ENCODING_RAW),
    "USER_DATA_04":                  (0xb4, MODE_WORD, ENCODING_RAW),
    "MFR_LTC_RESERVED_1":            (0xb5, MODE_WORD, ENCODING_RAW),
    "MFR_T_SELF_HEAT":               (0xb8, MODE_WORD, ENCODING_L11),
    "MFR_IOUT_CAL_GAIN_TAU_INV":     (0xb9, MODE_WORD, ENCODING_L11),
    "MFR_IOUT_CAL_GAIN_THETA":       (0xba, MODE_WORD, ENCODING_L11),
    "MFR_READ_IOUT":                 (0xbb, MODE_WORD, ENCODING_RAW),
    "MFR_LTC_RESERVED_2":            (0xbc, MODE_WORD, ENCODING_RAW),
    "MFR_EE_UNLOCK":                 (0xbd, MODE_BYTE, ENCODING_RAW),
    "MFR_EE_ERASE":                  (0xbe, MODE_BYTE, ENCODING_RAW),
    "MFR_EE_DATA":                   (0xbf, MODE_WORD, ENCODING_RAW),
    "MFR_EIN":                       (0xc0, MODE_BLOCK, ENCODING_RAW),
    "MFR_EIN_CONFIG":                (0xc1, MODE_BYTE, ENCODING_RAW),
    "MFR_SPECIAL_LOT":               (0xc2, MODE_BYTE, ENCODING_RAW),
    "MFR_IIN_CAL_GAIN_TC":           (0xc3, MODE_WORD, ENCODING_RAW),
    "MFR_IIN_PEAK":                  (0xc4, MODE_WORD, ENCODING_L11),
    "MFR_IIN_MIN":                   (0xc5, MODE_WORD, ENCODING_L11),
    "MFR_PIN_PEAK":                  (0xc6, MODE_WORD, ENCODING_L11),
    "MFR_PIN_MIN":                   (0xc7, MODE_WORD, ENCODING_L11),
    "MFR_COMMAND_PLUS":              (0xc8, MODE_WORD, ENCODING_RAW),
    "MFR_DATA_PLUS0":                (0xc9, MODE_WORD, ENCODING_RAW),
    "MFR_DATA_PLUS1":                (0xca, MODE_WORD, ENCODING_RAW),
    "MFR_CONFIG_LTM4673":            (0xd0, MODE_WORD, ENCODING_RAW),
    "MFR_CONFIG_ALL_LTM4673":        (0xd1, MODE_WORD, ENCODING_RAW),
    "MFR_FAULTB0_PROPAGATE":         (0xd2, MODE_BYTE, ENCODING_RAW),
    "MFR_FAULTB1_PROPAGATE":         (0xd3, MODE_BYTE, ENCODING_RAW),
    "MFR_PWRGD_EN":                  (0xd4, MODE_WORD, ENCODING_RAW),
    "MFR_FAULTB0_RESPONSE":          (0xd5, MODE_BYTE, ENCODING_RAW),
    "MFR_FAULTB1_RESPONSE":          (0xd6, MODE_BYTE, ENCODING_RAW),
    "MFR_IOUT_PEAK":                 (0xd7, MODE_WORD, ENCODING_L11),
    "MFR_IOUT_MIN":                  (0xd8, MODE_WORD, ENCODING_L11),
    "MFR_CONFIG2_LTM4673":           (0xd9, MODE_BYTE, ENCODING_RAW),
    "MFR_CONFIG3_LTM4673":           (0xda, MODE_BYTE, ENCODING_RAW),
    "MFR_RETRY_DELAY":               (0xdb, MODE_WORD, ENCODING_L11),
    "MFR_RESTART_DELAY":             (0xdc, MODE_WORD, ENCODING_L11),
    "MFR_VOUT_PEAK":                 (0xdd, MODE_WORD, ENCODING_L16),
    "MFR_VIN_PEAK":                  (0xde, MODE_WORD, ENCODING_L11),
    "MFR_TEMPERATURE_1_PEAK":        (0xdf, MODE_WORD, ENCODING_L11),
    "MFR_DAC":                       (0xe0, MODE_WORD, ENCODING_RAW),
    "MFR_POWERGOOD_ASSERTION_DELAY": (0xe1, MODE_WORD, ENCODING_L11),
    "MFR_WATCHDOG_T_FIRST":          (0xe2, MODE_WORD, ENCODING_L11),
    "MFR_WATCHDOG_T":                (0xe3, MODE_WORD, ENCODING_L11),
    "MFR_PAGE_FF_MASK":              (0xe4, MODE_BYTE, ENCODING_RAW),
    "MFR_PADS":                      (0xe5, MODE_WORD, ENCODING_RAW),
    "MFR_I2C_BASE_ADDRESS":          (0xe6, MODE_BYTE, ENCODING_RAW),
    "MFR_SPECIAL_ID":                (0xe7, MODE_WORD, ENCODING_RAW),
    "MFR_IIN_CAL_GAIN":              (0xe8, MODE_WORD, ENCODING_L11),
    "MFR_VOUT_DISCHARGE_THRESHOLD":  (0xe9, MODE_WORD, ENCODING_L11),
    "MFR_FAULT_LOG_STORE":           (0xea, MODE_SEND, ENCODING_RAW),
    "MFR_FAULT_LOG_RESTORE":         (0xeb, MODE_SEND, ENCODING_RAW),
    "MFR_FAULT_LOG_CLEAR":           (0xec, MODE_SEND, ENCODING_RAW),
    "MFR_FAULT_LOG_STATUS":          (0xed, MODE_BYTE, ENCODING_RAW),
    "MFR_FAULT_LOG":                 (0xee, MODE_BLOCK, ENCODING_RAW),
    "MFR_COMMON":                    (0xef, MODE_BYTE, ENCODING_RAW),
    "MFR_IOUT_CAL_GAIN_TC":          (0xf6, MODE_WORD, ENCODING_RAW),
    "MFR_RETRY_COUNT":               (0xf7, MODE_BYTE, ENCODING_RAW),
    "MFR_TEMP_1_GAIN":               (0xf8, MODE_WORD, ENCODING_RAW),
    "MFR_TEMP_1_OFFSET":             (0xf9, MODE_WORD, ENCODING_L11),
    "MFR_IOUT_SENSE_VOLTAGE":        (0xfa, MODE_WORD, ENCODING_RAW),
    "MFR_VOUT_MIN":                  (0xfb, MODE_WORD, ENCODING_L16),
    "MFR_VIN_MIN":                   (0xfc, MODE_WORD, ENCODING_L11),
    "MFR_TEMPERATURE_1_MIN":         (0xfd, MODE_WORD, ENCODING_L11),
}

for name, arg in commands.items():
    globals()[name] = arg[0]


def get_command_name(cmd_byte):
    for name, arg in commands.items():
        cmd = arg[0]
        if cmd == cmd_byte:
            return name
    return "{:02x}".format(cmd_byte)


def _int(x):
    try:
        return int(x)
    except ValueError:
        pass
    return int(x, 16)


def print_commands_c():
    for name, arg in commands.items():
        cmd = arg[0]
        print(f"#define LTM4673_{name:32s} (0x{cmd:02x})")
    encodings = [None]*0x100
    for name, arg in commands.items():
        cmd = arg[0]
        enc = arg[2]
        if enc == ENCODING_RAW:
            s = "LTM4673_ENCODING_RAW"
        elif enc == ENCODING_L11:
            s = "LTM4673_ENCODING_L11"
        elif enc == ENCODING_L16:
            s = "LTM4673_ENCODING_L16"
        else:
            s = "UNKNOWN_ENCODING"
        encodings[cmd] = f"  {s}, // LTM4673_{name}"
    print("uint8_t encodings[256] = {")
    for n in range(len(encodings)):
        enc = encodings[n]
        if enc is None:
            print(f"  LTM4673_UNUSED, // 0x{n:02x}");
        else:
            print(enc)
    print("};")
    return


def calc_pec(*args):
    # TODO We can allow hardware to calculate packet error checking byte
    # or we can calculate it manually here.
    # Use the polynomial x^8 + x^2 + x + 1 = 0b100000111 = 0x107
    return 0


def write_byte(cmd, val):
    #   WRITE_BYTE:         | cmd + wr  | command code | data byte |
    args = vet_args_byte(cmd, val)
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
    args = vet_args_word(cmd, val)
    if args is None:
        return False
    else:
        cmd, val = args
    if not hasattr(val, '__len__'):
        # val should be list of bytes (lsb to msb)
        val = (val & 0xff, (val >> 8) & 0xff)
    msg = (addr_dev + WR, cmd, val[0], val[1])
    return (msg,)


def send_byte(cmd, val):
    # val ignored
    #   SEND_BYTE:          | addr + wr  | command code |
    args = vet_args_byte(cmd, 0)
    if args is None:
        return False
    else:
        cmd, val = args
    msg = (addr_dev + WR, cmd)
    return (msg,)


def write_byte_pec(cmd, val):
    #   WRITE_BYTE w/ PEC:  | addr + wr  | command code | data byte | PEC |
    args = vet_args_byte(cmd, val)
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
    args = vet_args_word(cmd, val)
    if args is None:
        return False
    else:
        cmd, val = args
    if not hasattr(val, '__len__'):
        # val should be list of bytes (lsb to msb)
        val = (val & 0xff, (val >> 8) & 0xff)
    msg = [addr_dev + WR, cmd, val[0], val[1]]
    pec = calc_pec(*msg)
    return (msg + [pec],)


def send_byte_pec(cmd, val):
    # val ignored
    #   SEND_BYTE w/ PEC:   | addr + wr  | command code | PEC |
    args = vet_args_byte(cmd, 0)
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
    args = vet_args_byte(cmd, 0)
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
    args = vet_args_word(cmd, 0)
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
    args = vet_args_word(cmd, 0)
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
    args = vet_args_byte(cmd, 0)
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
    if hasattr(cmd, 'lower'):  # it's a string
        arg = commands.get(cmd, None)
        if arg is None:
            return None
        cmd_code, mode = arg[0:2]
        return (cmd, cmd_code, mode)
    else:
        for name, val in commands.items():
            vcmd, mode = val[0:2]
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


def vet_args(cmd, val, mode_word=False):
    if mode_word:
        max_val = 0xffff
    else:
        max_val = 0xff
    try:
        cmd = int(cmd)
    except ValueError:
        cmd = commands.get(cmd, (None,))[0]
    if cmd is None:
        return None
    cmd = cmd & 0xff
    # val can be int or list of ints (LSB-first)
    try:
        val = int(val) & max_val
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


def vet_args_byte(cmd, val):
    return vet_args(cmd, val, mode_word=False)


def vet_args_word(cmd, val):
    return vet_args(cmd, val, mode_word=True)

# =============================== PROGRAM API ================================
# Each returns nested list of bytes
#   ((addr_byte + rnw, cmd_code, ...), (addr_byte + rnw, cmd_code, ...), ...)
# If multiple transactions are present, they should be joined by repeated start
# If a msg byte is None, it is a placeholder for a single byte read
# If a msg byte is a list (empty), it means an unknown number of bytes should be
# read (determined by the N value returned by the first read byte).


def write(cmd, val, sleep=True):
    return get_xact(cmd, val, pec=False)


def read(cmd):
    return get_xact(cmd, None, pec=False)


def translate_xact_mmc(xact):
    line = [MMC_COMMAND_CHAR_PMBRIDGE]
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
                line.append(f"0x{mbyte:02x}")
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


def translate_i2cbridge(xacts):
    lines = []
    for xact in xacts:
        line = ' '.join(translate_xact_i2cbridge(xact))
        lines.append(line)
    return lines


MMC_REPEAT_START    = '!'
MMC_READ_ONE        = '?'
MMC_READ_BLOCK      = '*'

# MMC console syntax
# Each line is a list of any of the following (whitespace-separated)
#   ! : Repeated start
#   ? : Read 1 byte from the target device
#   * : Read 1 byte, then use that as N and read the next N bytes
#   0xHH: Use hex value 0xHH as the next transaction byte
#   DDD : Use decimal value DDD as the next transaction byte

# Program derived from LTC PMBus Project Text File Version:1.1
_program = (
    # Select PAGE 0xff
    (0xff, (
        (WRITE_PROTECT, 0x00),
        (VIN_ON, 0xCA40),
        (VIN_OFF, 0xCA33),
        (VIN_OV_FAULT_LIMIT, 0xD3C0),
        (VIN_OV_FAULT_RESPONSE, 0x80),
        (VIN_OV_WARN_LIMIT, 0xD380),
        (VIN_UV_WARN_LIMIT, 0x8000),
        (VIN_UV_FAULT_LIMIT, 0x8000),
        (VIN_UV_FAULT_RESPONSE, 0x00),
        (USER_DATA_00, 0x0000),
        (USER_DATA_02, 0x0000),
        (USER_DATA_04, 0x0000),
        (MFR_EIN_CONFIG, 0x00),
        (MFR_IIN_CAL_GAIN_TC, 0x0000),
        (MFR_CONFIG_ALL_LTM4673, 0x0F73),
        (MFR_PWRGD_EN, 0x0000),
        (MFR_FAULTB0_RESPONSE, 0x00),
        (MFR_FAULTB1_RESPONSE, 0x00),
        (MFR_CONFIG2_LTM4673, 0x00),
        (MFR_CONFIG3_LTM4673, 0x00),
        (MFR_RETRY_DELAY, 0xF320),
        (MFR_RESTART_DELAY, 0xFB20),
        (MFR_POWERGOOD_ASSERTION_DELAY, 0xEB20),
        (MFR_WATCHDOG_T_FIRST, 0x8000),
        (MFR_WATCHDOG_T, 0x8000),
        (MFR_PAGE_FF_MASK, 0x0F),
        (MFR_I2C_BASE_ADDRESS, 0x5C),
        (MFR_IIN_CAL_GAIN, 0xCA80),
        (MFR_RETRY_COUNT, 0x07),
    )),
    # Select PAGE 0
    (0x00, (
        (OPERATION, 0x80),
        (ON_OFF_CONFIG, 0x1E),
        (VOUT_COMMAND, 0x2000),
        (VOUT_MAX, 0x8000),
        (VOUT_MARGIN_HIGH, 0x219A),
        (VOUT_MARGIN_LOW, 0x1E66),
        (VOUT_OV_FAULT_LIMIT, 0x2333),
        (VOUT_OV_FAULT_RESPONSE, 0x80),
        (VOUT_OV_WARN_LIMIT, 0x223D),
        (VOUT_UV_WARN_LIMIT, 0x1DC3),
        (VOUT_UV_FAULT_LIMIT, 0x1CCD),
        (VOUT_UV_FAULT_RESPONSE, 0x7F),
        (IOUT_OC_FAULT_LIMIT, 0xDA20),
        (IOUT_OC_FAULT_RESPONSE, 0x00),
        (IOUT_OC_WARN_LIMIT, 0xD340),
        (IOUT_UC_FAULT_LIMIT, 0xC500),
        (IOUT_UC_FAULT_RESPONSE, 0x00),
        (OT_FAULT_LIMIT, 0xF200),
        (OT_FAULT_RESPONSE, 0xB8),
        (OT_WARN_LIMIT, 0xEBE8),
        (UT_WARN_LIMIT, 0xDD80),
        (UT_FAULT_LIMIT, 0xE530),
        (UT_FAULT_RESPONSE, 0xB8),
        (POWER_GOOD_ON, 0x1EB8),
        (POWER_GOOD_OFF, 0x1E14),
        (TON_DELAY, 0xBA00),
        (TON_RISE, 0xE320),
        (TON_MAX_FAULT_LIMIT, 0xF258),
        (TON_MAX_FAULT_RESPONSE, 0xB8),
        (TOFF_DELAY, 0xBA00),
        (USER_DATA_01, 0x0000),
        (USER_DATA_03, 0x0000),
        (MFR_IOUT_CAL_GAIN_TAU_INV, 0x8000),
        (MFR_IOUT_CAL_GAIN_THETA, 0x8000),
        (MFR_CONFIG_LTM4673, 0x0088),
        (MFR_FAULTB0_PROPAGATE, 0x00),
        (MFR_FAULTB1_PROPAGATE, 0x00),
        (MFR_DAC, 0x01FF),
        (MFR_VOUT_DISCHARGE_THRESHOLD, 0xC200),
        (MFR_IOUT_CAL_GAIN_TC, 0x0000),
        (MFR_TEMP_1_GAIN, 0x4000),
        (MFR_TEMP_1_OFFSET, 0x8000),
    )),
    # Select PAGE 1
    (0x01, (
        (OPERATION, 0x80),
        (ON_OFF_CONFIG, 0x1E),
        (VOUT_COMMAND, 0x399A),
        (VOUT_MAX, 0xFFFF),
        (VOUT_MARGIN_HIGH, 0x3C7B),
        (VOUT_MARGIN_LOW, 0x36B9),
        (VOUT_OV_FAULT_LIMIT, 0x3F5D),
        (VOUT_OV_FAULT_RESPONSE, 0x80),
        (VOUT_OV_WARN_LIMIT, 0x3DEC),
        (VOUT_UV_WARN_LIMIT, 0x3548),
        (VOUT_UV_FAULT_LIMIT, 0x33D7),
        (VOUT_UV_FAULT_RESPONSE, 0x7F),
        (IOUT_OC_FAULT_LIMIT, 0xD200),
        (IOUT_OC_FAULT_RESPONSE, 0x00),
        (IOUT_OC_WARN_LIMIT, 0xCB00),
        (IOUT_UC_FAULT_LIMIT, 0xBD00),
        (IOUT_UC_FAULT_RESPONSE, 0x00),
        (OT_FAULT_LIMIT, 0xF200),
        (OT_FAULT_RESPONSE, 0xB8),
        (OT_WARN_LIMIT, 0xEBE8),
        (UT_WARN_LIMIT, 0xDD80),
        (UT_FAULT_LIMIT, 0xE530),
        (UT_FAULT_RESPONSE, 0xB8),
        (POWER_GOOD_ON, 0x372F),
        (POWER_GOOD_OFF, 0x3643),
        (TON_DELAY, 0xEB20),
        (TON_RISE, 0xE320),
        (TON_MAX_FAULT_LIMIT, 0xF258),
        (TON_MAX_FAULT_RESPONSE, 0xB8),
        (TOFF_DELAY, 0xBA00),
        (USER_DATA_01, 0x0000),
        (USER_DATA_03, 0x0000),
        (MFR_IOUT_CAL_GAIN_TAU_INV, 0x8000),
        (MFR_IOUT_CAL_GAIN_THETA, 0x8000),
        (MFR_CONFIG_LTM4673, 0x1088),
        (MFR_FAULTB0_PROPAGATE, 0x00),
        (MFR_FAULTB1_PROPAGATE, 0x00),
        (MFR_DAC, 0x0245),
        (MFR_VOUT_DISCHARGE_THRESHOLD, 0xC200),
        (MFR_IOUT_CAL_GAIN_TC, 0x0000),
        (MFR_TEMP_1_GAIN, 0x4000),
        (MFR_TEMP_1_OFFSET, 0x8000),
    )),
    # Select PAGE 2
    (0x02, (
        (OPERATION, 0x80),
        (ON_OFF_CONFIG, 0x1E),
        (VOUT_COMMAND, 0x5000),
        (VOUT_MAX, 0xFFFF),
        (VOUT_MARGIN_HIGH, 0x5400),
        (VOUT_MARGIN_LOW, 0x4C00),
        (VOUT_OV_FAULT_LIMIT, 0x5800),
        (VOUT_OV_FAULT_RESPONSE, 0x80),
        (VOUT_OV_WARN_LIMIT, 0x563D),
        (VOUT_UV_WARN_LIMIT, 0x4A3D),
        (VOUT_UV_FAULT_LIMIT, 0x4800),
        (VOUT_UV_FAULT_RESPONSE, 0x7F),
        (IOUT_OC_FAULT_LIMIT, 0xD200),
        (IOUT_OC_FAULT_RESPONSE, 0x00),
        (IOUT_OC_WARN_LIMIT, 0xCB00),
        (IOUT_UC_FAULT_LIMIT, 0xBD00),
        (IOUT_UC_FAULT_RESPONSE, 0x00),
        (OT_FAULT_LIMIT, 0xF200),
        (OT_FAULT_RESPONSE, 0xB8),
        (OT_WARN_LIMIT, 0xEBE8),
        (UT_WARN_LIMIT, 0xDD80),
        (UT_FAULT_LIMIT, 0xE530),
        (UT_FAULT_RESPONSE, 0xB8),
        (POWER_GOOD_ON, 0x4CE1),
        (POWER_GOOD_OFF, 0x4B1F),
        (TON_DELAY, 0xF320),
        (TON_RISE, 0xE320),
        (TON_MAX_FAULT_LIMIT, 0xF258),
        (TON_MAX_FAULT_RESPONSE, 0xB8),
        (TOFF_DELAY, 0xBA00),
        (USER_DATA_01, 0xB48F),
        (USER_DATA_03, 0x0000),
        (MFR_IOUT_CAL_GAIN_TAU_INV, 0x8000),
        (MFR_IOUT_CAL_GAIN_THETA, 0x8000),
        (MFR_CONFIG_LTM4673, 0x2088),
        (MFR_FAULTB0_PROPAGATE, 0x00),
        (MFR_FAULTB1_PROPAGATE, 0x00),
        (MFR_DAC, 0x0218),
        (MFR_VOUT_DISCHARGE_THRESHOLD, 0xC200),
        (MFR_IOUT_CAL_GAIN_TC, 0x0000),
        (MFR_TEMP_1_GAIN, 0x4000),
        (MFR_TEMP_1_OFFSET, 0x8000),
    )),
    # Select PAGE 3
    (0x03, (
        (OPERATION, 0x80),
        (ON_OFF_CONFIG, 0x1E),
        (VOUT_COMMAND, 0x699A),
        (VOUT_MAX, 0xFFFF),
        (VOUT_MARGIN_HIGH, 0x6EE2),
        (VOUT_MARGIN_LOW, 0x6452),
        (VOUT_OV_FAULT_LIMIT, 0x7429),
        (VOUT_OV_FAULT_RESPONSE, 0x80),
        (VOUT_OV_WARN_LIMIT, 0x71D7),
        (VOUT_UV_WARN_LIMIT, 0x615D),
        (VOUT_UV_FAULT_LIMIT, 0x5F0B),
        (VOUT_UV_FAULT_RESPONSE, 0x7F),
        (IOUT_OC_FAULT_LIMIT, 0xDA20),
        (IOUT_OC_FAULT_RESPONSE, 0x00),
        (IOUT_OC_WARN_LIMIT, 0xD340),
        (IOUT_UC_FAULT_LIMIT, 0xC500),
        (IOUT_UC_FAULT_RESPONSE, 0x00),
        (OT_FAULT_LIMIT, 0xF200),
        (OT_FAULT_RESPONSE, 0xB8),
        (OT_WARN_LIMIT, 0xEBE8),
        (UT_WARN_LIMIT, 0xDD80),
        (UT_FAULT_LIMIT, 0xE530),
        (UT_FAULT_RESPONSE, 0xB8),
        (POWER_GOOD_ON, 0x64F5),
        (POWER_GOOD_OFF, 0x63B0),
        (TON_DELAY, 0xFA58),
        (TON_RISE, 0xE320),
        (TON_MAX_FAULT_LIMIT, 0xF258),
        (TON_MAX_FAULT_RESPONSE, 0xB8),
        (TOFF_DELAY, 0xBA00),
        (USER_DATA_01, 0x3322),
        (USER_DATA_03, 0x0000),
        (MFR_IOUT_CAL_GAIN_TAU_INV, 0x8000),
        (MFR_IOUT_CAL_GAIN_THETA, 0x8000),
        (MFR_CONFIG_LTM4673, 0x3088),
        (MFR_FAULTB0_PROPAGATE, 0x00),
        (MFR_FAULTB1_PROPAGATE, 0x00),
        (MFR_DAC, 0x01CB),
        (MFR_VOUT_DISCHARGE_THRESHOLD, 0xC200),
        (MFR_IOUT_CAL_GAIN_TC, 0x0000),
        (MFR_TEMP_1_GAIN, 0x4000),
        (MFR_TEMP_1_OFFSET, 0x8000),
    ))
)


def program():
    """Program derived from LTC PMBus Project Text File Version:1.1"""
    xacts = []
    for page, prog in _program:
        # Select the page
        xacts.append(write(PAGE, page))
        for reg, val in prog:
            # Write each register
            xacts.append(write(reg, val))
    lines = translate_mmc(xacts)
    return lines


def readback():
    """Readback of all that is written by program()"""
    xacts = []
    for page, prog in _program:
        # Select the page
        xacts.append(write(PAGE, page))
        for reg, val in prog:
            # Read each register
            xacts.append(read(reg))
    lines = translate_mmc(xacts)
    return lines


def test_program():
    xacts = []
    xacts.append(write(PAGE, 0xff))
    xacts.append(write(VIN_ON, 0xCA40))
    xacts.append(write(VIN_OFF, 0xCA33))
    xacts.append(write(VIN_OFF, 0xCA33))
    xacts.append(write(VIN_OV_FAULT_LIMIT, 0xD3C0))
    xacts.append(write(VIN_OV_FAULT_RESPONSE, 0x80))
    lines = translate_mmc(xacts)
    return lines


def test_readback():
    xacts = []
    xacts.append(write(PAGE, 0xff))
    xacts.append(read(VIN_ON))
    xacts.append(read(VIN_OFF))
    xacts.append(read(VIN_OV_FAULT_LIMIT))
    xacts.append(read(VIN_OV_FAULT_RESPONSE))
    lines = translate_mmc(xacts)
    return lines


def parse_readback_file(filename):
    lines = []
    with open(filename, 'r') as fd:
        line = True
        while line:
            line = fd.readline()
            lines.append(line)
    return parse_readback(lines)


def parse_readback(lines, do_print=False):
    _readback = []
    _prog = []
    newpage = None
    for line in lines:
        rval = match_readback(line)
        if rval is not None:
            command, val = rval
            _prog.append((command, val))
        else:
            rval = match_page(line)
            if rval is not None:
                if newpage is not None:
                    _readback.append((newpage, _prog))
                newpage = rval
                _prog = []
    _readback.append((newpage, _prog))
    compare_progs(_program, _readback)
    if do_print:
        print_prog(_readback)
    return


def match_readback(line):
    res = r"\((0x[0-9a-fA-F]+)\)\s+(0x[0-9a-fA-F]+):\s+(0x[0-9a-fA-F]+)\s*(0x[0-9a-fA-F]+)?"
    _match = re.match(res, line)
    if _match:
        groups = _match.groups()
        devaddr, command, val_lo, val_hi = groups
        command = _int(command)
        val = _int(val_lo)
        if val_hi is not None and len(val_hi) > 0:
            val_hi = _int(val_hi)
            val += (val_hi << 8)
        return command, val
    return None


def match_page(line):
    res = r"#\s+LTM4673_PAGE\s+([0-9a-fA-Fx]+)"
    _match = re.match(res, line)
    if _match:
        page = _match.groups()[0]
        page = _int(page)
        # print(f"Write page: 0x{page:02x}")
        return page
    return None


def compare_progs(ref, dut, strict=False):
    if strict:
        if ref == dut:
            print("Strict Readback Comparison PASS")
            return True
        else:
            print("Strict Readback Comparison FAIL")
            return False
    _pass = True
    if len(dut) == 0:
        _pass = False
    prog_results = []
    # diff = {page : {reg: (ref_val, dut_val), }, }
    diff = {}
    for page, prog in dut:
        page_diff = {}
        _page_results = []
        ref_prog = None
        for _page, _prog in ref:
            if _page == page:
                ref_prog = _prog
                break
        if ref_prog is None:
            _page_results.append((page, None))
            diff[page] = None
            _pass = False
            continue
        for reg, val in prog:
            found = False
            reg_good = False
            for _reg, _val in ref_prog:
                if reg == _reg:
                    found = True
                    if val == _val:
                        reg_good = True
                    else:
                        page_diff[reg] = (_val, val)
                        _pass = False
                    break
            if not found:
                _page_results.append((reg, None))
                page_diff[reg] = (None, val)
                _pass = False
            else:
                _page_results.append((reg, reg_good))
        prog_results.append((page, _page_results))
        if len(page_diff) > 0:
            diff[page] = page_diff
    if _pass:
        print("Permissive Readback Comparison PASS")
    else:
        print("Permissive Readback Comparison FAIL")
        print_diff(diff)
        # print(prog_results)
    return


def print_prog(_prog):
    for page, prog in _prog:
        print("# LTM4673_PAGE 0x{:02x}".format(page))
        for cmd, val in prog:
            cmdname = get_command_name(cmd) + ':'
            print("{:30s} 0x{:x}".format(cmdname, val))
    return


def print_diff(diff):
    """diff is nested dict"""
    print("{:30s} ref_val -> dut_val".format("DIFF:"))
    for page, page_diff in diff.items():
        if len(page_diff) > 0:
            print("==== PAGE 0x{:x} ==== ".format(page))
        for cmd, vals in page_diff.items():
            cmdname = get_command_name(cmd) + ':'
            ref_val, dut_val = vals
            print("{:30s} 0x{:x} -> 0x{:x}".format(cmdname, ref_val, dut_val))
    return


def tc(val, bits=5):
    """Interpret 'val' as two's complement integer of width 'bits'."""
    if val & (1<<(bits-1)):
        ival = (~val & ((1<<bits)-1)) + 1
        return -ival
    return val


def L11(val):
    """PMBus Linear11 Floating-Point format.
    https://en.wikipedia.org/wiki/Power_Management_Bus#Linear11_Floating-Point_Format
    """
    n = tc(val >> 11, 5)
    a = tc(val & ((1<<11)-1), 11)
    print(f"0x{val:x} = {a}*2^{n} = {a*(2**n)}")
    return a*(2**n)


def VtoL11(val):
    # n is 5 bits (signed), can range from -16 to +15; 2**n can range from 15.26u to 32.768k
    # y is 11 bits (signed), can range from -1024 to +1023
    n = -16
    orig = val
    val = val*(2**16)
    while (val > 1023) or (val < -1024):
        val /= 2
        n += 1
    packed = ((int(n) & 0x1f) << 11) + (int(val) & 0x7ff)
    print(f"{orig} ~ {int(val)}*2^{n} = {int(val)*(2**n)} = 0x{packed:x}")
    #print("L11(0x{:x}) = {}".format(packed, L11(packed)))
    L11(packed)
    return packed


def mVtoL11(val):
    """val is integer number of millivolts (or milliamps, milliwatts, etc)"""
    # n is 5 bits (signed), can range from -16 to +15; 2**n can range from 15.26u to 32.768k
    # y is 11 bits (signed), can range from -1024 to +1023
    n = -16
    val = (val << 16)//1000  # signed shift
    while (val > 1023) or (val < -1024):
        val = val >> 1
        n += 1
    packed = ((int(n) & 0x1f) << 11) + (int(val) & 0x7ff)
    return packed


def testVtoL11(argv):
    if len(argv) < 2:
        print("value to convert")
        return
    val = None
    decode = False
    for arg in argv[1:]:
        if arg == '-d':
            decode = True
        else:
            val = arg
    if decode:
        val = _int(val)
        #print("L11(0x{:x}) = {}".format(val, L11(val)))
        L11(val)
    else:
        val = float(val)
        VtoL11(val)
        mvl11 = mVtoL11(int(val*1000))
        print(f"Using mVtoL11: 0x{mvl11:x}")


def main(argv):
    import load
    # 100ms between commands for conservative program timing constraints
    load.INTERCOMMAND_SLEEP = 0.1
    # NOTE! This intercommand timing works for all except MFR_EE_ERASE which needs 0.4s sleep
    parser = load.ArgParser()
    parser.add_argument('--test', default=False, action="store_true", help='Test; not full program')
    parser.add_argument('-r', '--read', default=False, action="store_true", help='Read instead of write')
    parser.add_argument('--check', default=False, action="store_true", help='Compare values of readback with program')
    parser.add_argument('--print', default=False, action="store_true", help='Print values to write or read')
    parser.add_argument('-v', '--verbose', default=False, action="store_true", help='Print console chatter')
    args = parser.parse_args()
    if args.test:
        if args.read:
            lines = test_readback()
        else:
            lines = test_program()
    else:
        if args.read:
            print("Readback of full program (this may take several seconds)")
            lines = readback()
        else:
            print("Write of full program (this may take several seconds)")
            lines = program()
    if args.dev is not None:
        load.loadCommands(args.dev, args.baud, lines, do_print=args.verbose, do_log=args.check)
        if args.check:
            readback_log = load.get_log()
            parse_readback(readback_log, args.print)
    else:
        if args.print:
            print_prog(_program)
        else:
            print("PMBridge MMC Console Encoding:")
            for line in lines:
                print(line)


if __name__ == "__main__":
    import sys
    #main(sys.argv)
    #testVtoL11(sys.argv)
    print_commands_c()
