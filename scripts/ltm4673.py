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

_L16_EXPONENT=13

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

def _hexint(s):
    n = int(s)
    if n > 10:
        return hex(n)
    return str(n)

_decoder = {
    # Register: bitlist
    STATUS_WORD: (
        #bitlow, bithigh, field name, field description, decoding function
        (15,15, "Status_word_vout", "An output voltage fault or warning has occurred. See STATUS_VOUT.", _hexint),
        (14,14, "Status_word_iout", "An output current fault or warning has occurred. See STATUS_IOUT.", _hexint),
        (13,13, "Status_word_input", "An input voltage fault or warning has occurred. See STATUS_INPUT.", _hexint),
        (12,12, "Status_word_mfr", "A manufacturer specific fault has occurred. See STATUS_MFR._SPECIFIC.", _hexint),
        (11,11, "Status_word_power_not_good", " The PWRGD pin, if enabled, is negated. Power is not good.", _hexint),
        (7, 7,  "Status_word_busy", "Device busy when PMBus command received. See OPERATION: Processing Commands.", _hexint),
        (6, 6,  "Status_word_off", "This bit is asserted if the unit is not providing power to the output, regardless" \
                + " of the reason, including simply not being enabled.", _hexint),
        (5, 5,  "Status_word_vout_ov", "An output overvoltage fault has occurred.", _hexint),
        (4, 4,  "Status_word_iout_oc", "An output overcurrent fault has occurred.", _hexint),
        (3, 3,  "Status_word_vin_uv", "A VIN undervoltage fault has occurred.", _hexint),
        (2, 2,  "Status_word_temp", "A temperature fault or warning has occurred. See STATUS_TEMPERATURE.", _hexint),
        (1, 1,  "Status_word_cml", "A communication, memory or logic fault has occurred. See STATUS_CML.", _hexint),
        (0, 0,  "Status_word_high_byte", "A fault/warning not listed in b[7:1] has occurred.", _hexint),
        ),
    STATUS_VOUT: (
        (7,7, "Status_vout_ov_fault", "Overvoltage fault.", _hexint),
        (6,6, "Status_vout_ov_warn", "Overvoltage warning.", _hexint),
        (5,5, "Status_vout_uv_warn", "Undervoltage warning", _hexint),
        (4,4, "Status_vout_uv_fault", "Undervoltage fault.", _hexint),
        (3,3, "Status_vout_max_warn",
            "VOUT_MAX warning. An attempt has been made to set the output voltage to a value higher than allowed by" \
            + " the VOUT_MAX command", _hexint),
        (2,2, "Status_vout_ton_max_fault", "TON_MAX_FAULT sequencing fault.", _hexint),
        ),
    STATUS_IOUT: (
        (7,7, "Status_iout_oc_fault", "Overcurrent fault.", _hexint),
        (5,5, "Status_iout_oc_warn", "Overcurrent warning", _hexint),
        (4,4, "Status_iout_uc_fault", "Undercurrent fault.", _hexint),
        ),
    STATUS_TEMPERATURE: (
        (7,7, "Status_temperature_ot_fault", "Overtemperature fault.", _hexint),
        (6,6, "Status_temperature_ot_warn", "Overtemperature warning.", _hexint),
        (5,5, "Status_temperature_ut_warn", "Undertemperature warning.", _hexint),
        (4,4, "Status_temperature_ut_fault", "Undertemperature fault.", _hexint),
        ),
    STATUS_MFR_SPECIFIC: (
        (7, 7, "Status_mfr_discharge", "1 = a VOUT discharge fault occurred while attempting to enter the ON state.", _hexint),
        (6, 6, "Status_mfr_fault1_in",
            "This channel attempted to turn on while the FAULT1 pin was asserted low, or this channel has shut down" \
            + " at least once in response to a FAULT1 pin asserting low since the last CONTROL pin toggle, OPERATION" \
            + " command ON/OFF cycle or CLEAR_FAULTS command.", _hexint),
        (5, 5, "Status_mfr_fault0_in",
            "This channel attempted to turn on while the FAULT0 pin was asserted low, or this channel has shut down" \
            + "at least once in response to a FAULT0 pin asserting low since the last CONTROL pin toggle, OPERATION" \
            + " command ON/OFF cycle or CLEAR_FAULTS command.", _hexint),
        (4, 4, "Status_mfr_servo_target_reached", "Servo target has been reached.", _hexint),
        (3, 3, "Status_mfr_dac_connected", "DAC is connected and driving VDAC pin.", _hexint),
        (2, 2, "Status_mfr_dac_saturated", "A previous servo operation terminated with maximum or minimum DAC value.", _hexint),
        (1, 1, "Status_mfr_auxfaultb_faulted_off", "AUXFAULT has been de-asserted due to a VOUT or IOUT fault.", _hexint),
        (0, 0, "Status_mfr_watchdog_fault", "1 = A watchdog fault has occurred. 0 = No watchdog fault has occurred.", _hexint),
        ),
    STATUS_INPUT: (
        (7,7, "Status_input_ov_fault", "VIN overvoltage fault", _hexint),
        (6,6, "Status_input_ov_warn", "VIN overvoltage warning", _hexint),
        (5,5, "Status_input_uv_warn", "VIN undervoltage warning", _hexint),
        (4,4, "Status_input_uv_fault", "VIN undervoltage fault", _hexint),
        (3,3, "Status_input_off", "Unit is off for insufficient input voltage.", _hexint),
        ),
    STATUS_CML: (
        (7,7, "Status_cml_cmd_fault", "1 = An illegal or unsupported command fault has occurred. 0 = No fault has occurred.", _hexint),
        (6,6, "Status_cml_data_fault", "1 = Illegal or unsupported data received. 0 = No fault has occurred.", _hexint),
        (5,5, "Status_cml_pec_fault", "1 = A packet error check fault has occurred. 0 = No fault has occurred.", _hexint),
        (4,4, "Status_cml_memory_fault", "1 = A fault has occurred in the EEPROM. 0 = No fault has occurred.", _hexint),
        (1,1, "Status_cml_pmbus_fault", "1 = A communication fault other than ones listed in this table has occurred. 0 = No fault has occurred.", _hexint),
        ),
    MFR_PADS: (
        (15,15, "Mfr_pads_pwrgd_drive", "0 = PWRGD pad is being driven low by this chip. 1 = PWRGD pad is not being driven low by this chip.", _hexint),
        (14,14, "Mfr_pads_alertb_drive", "0 = ALERT pad is being driven low by this chip. 1 = ALERT pad is not being driven low by this chip.", _hexint),
        (12,13, "Mfr_pads_faultb_drive[1:0]", "bit[1] used for FAULT0 pad, bit[0] used for FAULT1 pad",
            lambda x: " ".join(["FAULT{} pad is {}being driven low by this chip.".format(b, "not " if (x & (1<<b)) else "") for b in range(2)])),
        (8,9, "Mfr_pads_asel1[1:0]", "11: Logic high detected on ASEL1 input pad. 10: ASEL1 input pad is floating. 01: Reserved. 00: Logic low detected on ASEL1 input pad.", _hexint),
        (7,6, "Mfr_pads_asel0[1:0]", "11: Logic high detected on ASEL0 input pad. 10: ASEL0 input pad is floating. 01: Reserved. 00: Logic low detected on ASEL0 input pad.", _hexint),
        (5,5, "Mfr_pads_control1", "1: Logic high detected on CONTROL1 pad. 0: Logic low detected on CONTROL1 pad.", _hexint),
        (4,4, "Mfr_pads_control0", "1: Logic high detected on CONTROL0 pad. 0: Logic low detected on CONTROL0 pad.", _hexint),
        (3,2, "Mfr_pads_faultb[1:0]", "bit[1] used for FAULT0 pad, bit[0] used for FAULT1 pad as follows: 1: Logic high detected on FAULT pad. 0: Logic low detected on FAULT pad.", _hexint),
        (1,1, "Mfr_pads_control2", "1: Logic high detected on CONTROL2 pad. 0: Logic low detected on CONTROL2 pad.", _hexint),
        (0,0, "Mfr_pads_control3", "1: Logic high detected on CONTROL3 pad. 0: Logic low detected on CONTROL3 pad.", _hexint),
        ),
    MFR_COMMON: (
        (7,7," Mfr_common_alertb", "Returns alert status. 1: ALERT is de-asserted high. 0: ALERT is asserted low.", _hexint),
        (6,6," Mfr_common_busyb", "Returns device busy status. 1: The device is available to process PMBus commands. 0: The device is busy and will NACK PMBus commands.", _hexint),
        (1,1," Mfr_common_share_clk", "Returns the status of the share-clock pin. 1: Share-clock pin is being held low. 0: Share-clock pin is active.", _hexint),
        (0,0," Mfr_common_write_protect", "Returns the status of the write-protect pin. 1: Write-protect pin is high. 0: Write-protect pin is low.", _hexint),
        ),
}


def _slice(val, low, high):
    _low = min(low, high)
    _high = max(low, high)
    return (val & ((1 << _high)-1)) >> _low


def decode_bits(reg, val, verbose=False):
    bitlist = _decoder.get(reg, None)
    regname = get_command_name(reg)
    indent = " "*2
    if bitlist is None:
        decoded = _hexint(val)
    else:
        ll = [""] # Start with a newline
        for bitlow, bithigh, field_name, desc, fn in bitlist:
            v = _slice(val, bitlow, bithigh)
            if verbose:
                # TODO use 'desc' somehow
                ss = "{}: {}".format(field_name, fn(v))
            else:
                ss = "{}: {}".format(field_name, fn(v))
            ll.append(indent + ss)
        decoded = "\n".join(ll)
    return regname, decoded


def get_command_name(cmd_byte):
    for name, arg in commands.items():
        cmd = arg[0]
        if cmd == cmd_byte:
            return name
    return "{:02x}".format(cmd_byte)


def get_encoding(cmd_byte):
    for name, arg in commands.items():
        cmd = arg[0]
        if cmd == cmd_byte:
            return arg[2]
    return ENCODING_RAW


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


def _tc(val, bits=5):
    """Interpret 'val' as two's complement integer of width 'bits'."""
    if val & (1<<(bits-1)):
        ival = (~val & ((1<<bits)-1)) + 1
        return -ival
    return val


def L11_TO_V(l):
    """PMBus Linear11 Floating-Point format.
    https://en.wikipedia.org/wiki/Power_Management_Bus#Linear11_Floating-Point_Format
    """
    n = _tc(l >> 11, 5)
    a = _tc(l & ((1<<11)-1), 11)
    return a*(2**n)


def V_TO_L11(val):
    # n is 5 bits (signed), can range from -16 to +15; 2**n can range from 15.26u to 32.768k
    # y is 11 bits (signed), can range from -1024 to +1023
    n = -16
    val = val*(2**16)
    while (val > 1023) or (val < -1024):
        val /= 2
        n += 1
    packed = ((int(n) & 0x1f) << 11) + (int(val) & 0x7ff)
    #print("L11(0x{:x}) = {}".format(packed, L11(packed)))
    #L11_TO_V(packed)
    return packed


def MV_TO_L11(val):
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


def V_TO_L16(val):
    return int(val*(1<<_L16_EXPONENT))


def L16_TO_V(l):
    return l/(1<<_L16_EXPONENT)


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


_test_program = (
    (0xff, (
        (VIN_ON, 0xCA40),
        (VIN_OFF, 0xCA33),
        (VIN_OV_FAULT_LIMIT, 0xD3C0),
        (VIN_OV_FAULT_RESPONSE, 0x80),
    ))
)


# Must be synchronized with const _ltm4673_limits_t ltm4673_limits[]; in ltm4673.c
ltm4673_limits = {
    # page: limit_dict
    0 : {
        # cmd: (mask, min, max)
        VOUT_COMMAND: (0xffff, V_TO_L16(0.95), V_TO_L16(1.05)),  # 0.95V to 1.05V
    },
    1 : {
        VOUT_COMMAND: (0xffff, V_TO_L16(1.75), V_TO_L16(1.85)),  # 1.75V to 1.85V
    },
    2 : {
        VOUT_COMMAND: (0xffff, V_TO_L16(2.45), V_TO_L16(2.55)),  # 2.45V to 2.55V
    },
    3 : {
        VOUT_COMMAND: (0xffff, V_TO_L16(3.25), V_TO_L16(3.35)),  # 3.25V to 3.35V
    },
}


def translate_program(program, rnw=True):
    """Program derived from LTC PMBus Project Text File Version:1.1"""
    xacts = []
    for page, prog in program:
        # Select the page
        xacts.append(write(PAGE, page))
        for reg, val in prog:
            if rnw:
                # Read each register
                xacts.append(read(reg))
            else:
                # Write each register
                xacts.append(write(reg, val))
    lines = translate_mmc(xacts)
    return lines


def read_telem():
    # Read for each page
    cmds = (
        READ_VIN,
        READ_IIN,
        READ_PIN,
        READ_VOUT,
        READ_IOUT,
        READ_TEMPERATURE_1,
        READ_TEMPERATURE_2,
        READ_POUT,
        MFR_READ_IOUT,
        MFR_IIN_PEAK,
        MFR_IIN_MIN,
        MFR_PIN_PEAK,
        MFR_PIN_MIN,
        MFR_IOUT_SENSE_VOLTAGE,
        MFR_VIN_PEAK,
        MFR_VOUT_PEAK,
        MFR_IOUT_PEAK,
        MFR_TEMPERATURE_1_PEAK,
        MFR_VIN_MIN,
        MFR_VOUT_MIN,
        MFR_IOUT_MIN,
        MFR_TEMPERATURE_1_MIN,
    )
    xacts = []
    for page in range(4):
        xacts.append(write(PAGE, page))
        for cmd in cmds:
            xacts.append(read(cmd))
    lines = translate_mmc(xacts)
    return lines


def read_status():
    # Non-paged registers
    xacts = [
        write(PAGE, 0xff),
        read(STATUS_INPUT),
        read(STATUS_CML),
        read(MFR_PADS),
        read(MFR_COMMON),
    ]
    # Paged registers
    cmds = (
        STATUS_WORD,
        STATUS_VOUT,
        STATUS_IOUT,
        STATUS_TEMPERATURE,
        STATUS_MFR_SPECIFIC,
    )
    for page in range(4):
        xacts.append(write(PAGE, page))
        for cmd in cmds:
            xacts.append(read(cmd))
    lines = translate_mmc(xacts)
    return lines


def _init_sim_mem():
    print("void init_sim_ltm4673(void) {")
    page0 = [0]*0x100
    page1 = [0]*0x100
    page2 = [0]*0x100
    page3 = [0]*0x100
    pages = (page0, page1, page2, page3)
    lines = []
    for page, prog in _program:
        lines.append(f"  // page 0x{page:02x}")
        for cmd, val in prog:
            name = get_command_name(cmd)
            if page == 0xff:
                page0[cmd] = val
                page1[cmd] = val
                page2[cmd] = val
                page3[cmd] = val
                # Write all pages
                lines.append(f"  ltm4673.page0[LTM4673_{name}] = 0x{val:02x};")
                lines.append(f"  ltm4673.page1[LTM4673_{name}] = 0x{val:02x};")
                lines.append(f"  ltm4673.page2[LTM4673_{name}] = 0x{val:02x};")
                lines.append(f"  ltm4673.page3[LTM4673_{name}] = 0x{val:02x};")
            else:
                pages[page][cmd] = val
                # Write to just the selected page
                lines.append(f"  ltm4673.page{page}[LTM4673_{name}] = 0x{val:02x};")
    lines.append("  return;\r\n}")
    function = False
    if function:
        for line in lines:
            print(line)
    else:
        for page_n in range(len(pages)):
            print(f"uint32_t page{page_n}[] = " + "{");
            page = pages[page_n]
            cols = 16
            print("  ", end="")
            for cmd in range(len(page)):
                val = page[cmd]
                print("{:7s} ".format(hex(val)+','), end="")
                if cmd % cols == cols-1:
                    print(f" // 0x{cmd-cols+1:x}-0x{cmd:x}\r\n  ", end="")
            print("};")
    return


def _init_sim_telem():
    # LTM4673_PAGE 0x00
    page0_dict = {
        READ_VIN:                      0xd33c,
        READ_IIN:                      0xaa48,
        READ_PIN:                      0xc3b0,
        READ_VOUT:                     0x2001,
        READ_IOUT:                     0x9b54,
        READ_TEMPERATURE_1:            0xe20d,
        READ_TEMPERATURE_2:            0xdbe8,
        READ_POUT:                     0x9b3c,
        MFR_READ_IOUT:                 0x28,
        MFR_IIN_PEAK:                  0xab73,
        MFR_IIN_MIN:                   0x9313,
        MFR_PIN_PEAK:                  0xcac2,
        MFR_PIN_MIN:                   0xb289,
        MFR_IOUT_SENSE_VOLTAGE:        0x61,
        MFR_VIN_PEAK:                  0xd34c,
        MFR_VOUT_PEAK:                 0x2003,
        MFR_IOUT_PEAK:                 0x9b7d,
        MFR_TEMPERATURE_1_PEAK:        0xe210,
        MFR_VIN_MIN:                   0xd332,
        MFR_VOUT_MIN:                  0x1fdf,
        MFR_IOUT_MIN:                  0x931f,
        MFR_TEMPERATURE_1_MIN:         0xdb45,
    }
    # LTM4673_PAGE 0x01
    page1_dict = {
        READ_VIN:                      0xd33c,
        READ_IIN:                      0xaa4b,
        READ_PIN:                      0xc3b8,
        READ_VOUT:                     0x399a,
        READ_IOUT:                     0xa27d,
        READ_TEMPERATURE_1:            0xe238,
        READ_TEMPERATURE_2:            0xdbec,
        READ_POUT:                     0xaa3f,
        MFR_READ_IOUT:                 0x3e,
        MFR_IIN_PEAK:                  0xab73,
        MFR_IIN_MIN:                   0x9313,
        MFR_PIN_PEAK:                  0xcac2,
        MFR_PIN_MIN:                   0xb289,
        MFR_IOUT_SENSE_VOLTAGE:        0x307,
        MFR_VIN_PEAK:                  0xd34c,
        MFR_VOUT_PEAK:                 0x39fb,
        MFR_IOUT_PEAK:                 0xa280,
        MFR_TEMPERATURE_1_PEAK:        0xe238,
        MFR_VIN_MIN:                   0xd332,
        MFR_VOUT_MIN:                  0x393e,
        MFR_IOUT_MIN:                  0x8082,
        MFR_TEMPERATURE_1_MIN:         0xdb57,
    }
    # LTM4673_PAGE 0x02
    page2_dict = {
        READ_VIN:                      0xd33c,
        READ_IIN:                      0xaa52,
        READ_PIN:                      0xc3c0,
        READ_VOUT:                     0x5001,
        READ_IOUT:                     0x9afb,
        READ_TEMPERATURE_1:            0xe240,
        READ_TEMPERATURE_2:            0xdbef,
        READ_POUT:                     0xa3bd,
        MFR_READ_IOUT:                 0x25,
        MFR_IIN_PEAK:                  0xab73,
        MFR_IIN_MIN:                   0x9313,
        MFR_PIN_PEAK:                  0xcac2,
        MFR_PIN_MIN:                   0xb289,
        MFR_IOUT_SENSE_VOLTAGE:        0x1cb,
        MFR_VIN_PEAK:                  0xd34c,
        MFR_VOUT_PEAK:                 0x5007,
        MFR_IOUT_PEAK:                 0x9b01,
        MFR_TEMPERATURE_1_PEAK:        0xe245,
        MFR_VIN_MIN:                   0xd332,
        MFR_VOUT_MIN:                  0x4f54,
        MFR_IOUT_MIN:                  0x8004,
        MFR_TEMPERATURE_1_MIN:         0xdb64,
    }
    # LTM4673_PAGE 0x03
    page3_dict = {
        READ_VIN:                      0xd33b,
        READ_IIN:                      0xaa48,
        READ_PIN:                      0xc3b4,
        READ_VOUT:                     0x6998,
        READ_IOUT:                     0xb27f,
        READ_TEMPERATURE_1:            0xe226,
        READ_TEMPERATURE_2:            0xdbf3,
        READ_POUT:                     0xc219,
        MFR_READ_IOUT:                 0xfe,
        MFR_IIN_PEAK:                  0xab73,
        MFR_IIN_MIN:                   0x9313,
        MFR_PIN_PEAK:                  0xcac2,
        MFR_PIN_MIN:                   0xb289,
        MFR_IOUT_SENSE_VOLTAGE:        0x26d,
        MFR_VIN_PEAK:                  0xd34c,
        MFR_VOUT_PEAK:                 0x6a1b,
        MFR_IOUT_PEAK:                 0xb387,
        MFR_TEMPERATURE_1_PEAK:        0xe238,
        MFR_VIN_MIN:                   0xd332,
        MFR_VOUT_MIN:                  0x697c,
        MFR_IOUT_MIN:                  0x8725,
        MFR_TEMPERATURE_1_MIN:         0xdb49,
    }
    page_dicts = (page0_dict, page1_dict, page2_dict, page3_dict)
    for npage in range(len(page_dicts)):
        for cmd, val in page_dicts[npage].items():
            name = get_command_name(cmd)
            print(f"  ltm4673.page{npage}[LTM4673_{name}] = 0x{val:x};")
    return


def attempt_limit_break(factor=0.1):
    """Try to write outside the limits in the firmware."""
    xacts = []
    xacts.append(write(PAGE, 0xff))
    xacts.append(write(WRITE_PROTECT, 0x00))
    for page, limit_dict in ltm4673_limits.items():
        xacts.append(write(PAGE, page))
        for cmd, arg in limit_dict.items():
            mask, _min, _max = arg[:3]
            if factor > 0:
                val = _max*(1+factor)
            else:
                val = _min*(1+factor)
            xacts.append(write(cmd, val))
            if mask < 0xffff:
                # Try to write to masked-out bits
                xacts.append(write(cmd, (~mask & 0xffff)))
    lines = translate_mmc(xacts)
    return lines


def xact_store_eeprom():
    """Get the transaction for storing to EEPROM."""
    xacts = []
    xacts.append(write(PAGE, 0xff))
    xacts.append(write(STORE_USER_ALL, 0x00))
    lines = translate_mmc(xacts)
    return lines


def xact_restore_eeprom():
    """Get the transaction for storing to EEPROM."""
    xacts = []
    xacts.append(write(PAGE, 0xff))
    xacts.append(write(RESTORE_USER_ALL, 0x00))
    lines = translate_mmc(xacts)
    return lines


def compare_to_limits(readback, limits):
    fail = False
    for page, _prog in readback:
        limit_page = limits.get(page, None)
        if limit_page is None:
            continue
        for cmd, val in _prog:
            limit_page_cmd = limit_page.get(cmd, None)
            if limit_page_cmd is None:
                continue
            enc = get_encoding(cmd)
            if enc == ENCODING_L11:
                val = L11_TO_V(val)
                limit_min = L11_TO_V(limit_page_cmd[1])
                limit_max = L11_TO_V(limit_page_cmd[2])
                fmt = "{:.3f}"
            elif enc == ENCODING_L16:
                val = L16_TO_V(val)
                limit_min = L16_TO_V(limit_page_cmd[1])
                limit_max = L16_TO_V(limit_page_cmd[2])
                fmt = "{:.3f}"
            else:
                limit_min = limit_page_cmd[1]
                limit_max = limit_page_cmd[2]
                fmt = "0x{:x}"
            cmdname = get_command_name(cmd) + ':'
            val_fmt = fmt.format(val)
            min_fmt = fmt.format(limit_min)
            max_fmt = fmt.format(limit_max)
            _pass = (limit_min <= val) and (val <= limit_max)
            print(f"[{page}] {cmdname:30s} {min_fmt} <= {val_fmt} <= {max_fmt} ? {_pass}")
            if not _pass:
                fail = True
    return (not fail)


def parse_readback(lines, compare_prog=None, do_print=False):
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
    if newpage is not None:
        _readback.append((newpage, _prog))
    compare_pass = True
    if compare_prog is not None:
        compare_pass = compare_progs(_program, _readback)
    if do_print:
        print_prog(_readback)
    return (_readback, compare_pass)


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
    return _pass


def print_prog(_prog):
    for page, prog in _prog:
        print("# LTM4673_PAGE 0x{:02x}".format(page))
        for cmd, val in prog:
            cmdname = get_command_name(cmd) + ':'
            enc = get_encoding(cmd)
            if enc == ENCODING_L11:
                val = to_si(L11_TO_V(val))
                print("{:30s} {}".format(cmdname, val))
            elif enc == ENCODING_L16:
                val = to_si(L16_TO_V(val))
                print("{:30s} {}".format(cmdname, val))
            else:
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


def testV_TO_L11(argv):
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
        V_TO_L11(val)
    else:
        val = float(val)
        V_TO_L11(val)
        mvl11 = MV_TO_L11(int(val*1000))
        print(f"Using MV_TO_L11: 0x{mvl11:x}")


def test_get_program_from_file(argv):
    if len(argv) < 2:
        print("gimme filename")
        return
    filename = argv[1]
    prog = get_program_from_file(filename)
    print_prog(prog)
    return


class ParserSyntaxError(Exception):
    def __init__(self, s):
        super().__init__(s)


def get_program_from_file(filename):
    prog = []
    #0x60,-1,WB,0x10,0x00,WRITE_PROTECT
    res = "^([0-9a-fA-Fx]+)\s*,([0-9a-fA-Fx\-]+)\s*,(WB|WW|RB|RW),([0-9a-fA-Fx]+)\s*,([0-9a-fA-Fx]+)\s*,(\w+)"
    with open(filename, 'r') as fd:
        line = True
        _page = None
        pagelist = []
        nline = 1
        while line:
            line = fd.readline()
            if line.strip().startswith("#"):
                # Ignore comments
                continue
            if len(line.strip()) == 0:
                # Ignore empty lines
                continue
            _match = re.match(res, line)
            if _match:
                #0x60,-1,WB,0x10,0x00,WRITE_PROTECT
                devaddr, page, oper, reg, val, name = _match.groups()
                page = _int(page) & 0xff # Gotta turn -1 into 0xff
                reg = _int(reg)
                val = _int(val)
                if _page is None:
                    _page = page
                elif _page != page:
                    prog.append((_page, pagelist))
                    pagelist = []
                    _page = page
                else:
                    pagelist.append((reg, val))
            else:
                raise ParserSyntaxError("Syntax error on line {}: {}".format(nline, line) + \
                                        "Expected format: 0xHH,[-]D,(WB|WW|RB|RW),0xHH,0xHH,NAME")
            nline += 1
        # Append the last page list
        prog.append((_page, pagelist))
    # Return nested-list
    #   ((page, ((reg, val),...)),...)
    return prog


def _count_ops(program):
    ops = 0
    for page, prog in program:
        # A page write
        ops += 1
        for reg, val in prog:
            # A read or write for each reg,val pair
            ops += 1
    return ops


def get_limits_from_file(filename):
    print("TODO")
    # Return nested dict
    # {page: {cmd: (mask, min, max),...},...}
    return {}


def handle_write(args):
    load_rval = 1
    if args.test:
        print("Writing test program")
        program = _test_program
    elif args.file is not None:
        print("Writing program from file {}.".format(args.file))
        program = get_program_from_file(args.file)
    else:
        print("Writing default program")
        program = _program
    lines = translate_program(program, rnw=False)
    if args.dev is not None:
        import load
        runtime = _count_ops(program)*load.INTERCOMMAND_SLEEP
        print("Estimated {:.1f}s to complete.".format(runtime))
        load_rval = load.loadCommands(args.dev, args.baud, lines, do_print=args.verbose, do_log=False)
    else:
        # Just print the program
        print_prog(program)
        # TODO - Do I want to enable this with another switch?
        if False:
            print("PMBridge MMC Console Encoding:")
            for line in lines:
                print(line)
    return load_rval


def handle_read(args):
    load_rval = 1
    if args.test:
        print("Reading test program")
        program = _test_program
    elif args.file is not None:
        print("Reading program from file {}.".format(args.file))
        program = get_program_from_file(args.file)
    else:
        print("Reading default program")
        program = _program
    lines = translate_program(program, rnw=True)
    if args.dev is not None:
        import load
        runtime = _count_ops(program)*load.INTERCOMMAND_SLEEP
        print("Estimated {:.1f}s to complete.".format(runtime))
        load_rval = load.loadCommands(args.dev, args.baud, lines, do_print=args.verbose, do_log=True)
        readback_log = load.get_log()
        if args.check:
            readback, compare_pass = parse_readback(readback_log, compare_prog=program, do_print=args.print)
        else:
            readback, compare_pass = parse_readback(readback_log, compare_prog=None, do_print=True)
    else:
        # Just print the program
        print_prog(program)
        # TODO - Do I want to enable this with another switch?
        if False:
            print("PMBridge MMC Console Encoding:")
            for line in lines:
                print(line)
    # Convert pass = True to pass = 0
    compare_rval = int(not compare_pass)
    return load_rval | compare_rval


def test_to_si(argv):
    if len(argv) < 1:
        print("float plz")
        return
    f = float(argv[1])
    to_si(f)
    return


def to_si(n, sigfigs=4):
    """Use SI prefixes to represent 'n' up to sigfigs significant figures."""
    import math
    if n == 0:
        return "0"
    si = ((30, ("Q", "quetta")),
          (27, ("R", "ronna")),
          (24, ("Y", "yotta")),
          (21, ("Z", "zetta")),
          (18, ("E", "exa")),
          (15, ("P", "peta")),
          (12, ("T", "tera")),
          ( 9, ("G", "giga")),
          ( 6, ("M", "mega")),
          ( 3, ("k", "kilo")),
          ( 0, ("", "")),
          (-3, ("m", "milli")),
          (-6, ("u", "micro")),   # mu?
          (-9, ("n", "nano")),
         (-12, ("p", "pico")),
         (-15, ("f", "femto")),
         (-18, ("a", "atto")),
         (-21, ("z", "zepto")),
         (-24, ("y", "yocto")),
         (-27, ("r", "ronto")),
         (-30, ("q", "quecto")))
    sign = ""
    if n < 0:
        n = abs(n)
        sign = "-"
    npwr = math.log10(n)
    # x^N = y*10^N = z
    # log(x^N) = log(y*10^N) = log(y) + log(10^N) = log(y) + N = log(z)
    fmt = "{:." + str(int(sigfigs)) + "}"
    for pwr, pfx in si:
        if npwr >= pwr:
            mant = 10**(npwr-pwr)
            s = sign + fmt.format(mant) + pfx[0]
            break
    return s


def from_si(s):
    print("TODO")
    return 1


def handle_limits(args):
    print("TESTING LIMITS")
    if args.dev is None:
        print("No valid device handed to limits_test")
        return False
    if args.file is not None:
        limits = get_limits_from_file(args.file)
    else:
        limits = ltm4673_limits
    import load
    factors = (0.1, -0.1)
    passed = False
    for n in range(len(factors)):
        factor = factors[n]
        if factor > 0:
            print("=== Trying to break upper limits ===")
        elif factor < 0:
            print("=== Trying to break lower limits ===")
        else:
            print("=== Factor is zero. Is this intended? ===")
        # Get the lines in PMBridge syntax to write above the limits
        lines = attempt_limit_break(factor=factor)
        # Perform the write to the serial device
        load.loadCommands(args.dev, args.baud, lines, do_print=args.verbose, do_log=True)
        # Get the raw session log
        readback_log = load.get_log()
        # Parse the session log
        readback, compare_pass = parse_readback(lines, do_print=False)
        # Compare read values with limits
        if compare_to_limits(readback=readback, limits=limits):
            print("=== PASS ===")
            passed = True
        else:
            print("=== FAIL ===")
    # Convert pass = True to pass = 0
    passed_rval = int(not passed)
    return passed_rval


def handle_store(args):
    return _handle_store_restore(args, restore=False)


def handle_restore(args):
    return _handle_store_restore(args, restore=True)


def _handle_store_restore(args, restore=False):
    if args.dev is None:
        print("No valid device")
        return False
    import load
    # Need to sleep 0.5s after issuing the STORE_USER_ALL command.
    load.INTERCOMMAND_SLEEP = 0.5
    # Get the lines in PMBridge syntax to store to EEPROM
    if restore:
        print("Attempting restore from EEPROM")
        lines = xact_restore_eeprom()
    else:
        print("Attempting store to EEPROM")
        lines = xact_store_eeprom()
    # Perform the write to the serial device
    load_rval = load.loadCommands(args.dev, args.baud, lines, do_print=args.verbose, do_log=False)
    # Just waiting on load via the "get_log" command
    log = load.get_log()
    if load_rval == 0:
        print("Success")
    else:
        print("Failed to load transaction to MMC console")
    return load_rval


def handle_telem(args):
    if args.dev is None:
        print("No valid device handed to handle_telem")
        return False
    print("Reading telemetry data")
    import load
    # Might be able to shorten this more. But uart is slow, ain't it?
    load.INTERCOMMAND_SLEEP = 0.01
    # Get the lines in PMBridge syntax to read telemetry registers
    lines = read_telem()
    # Perform the write to the serial device
    load_rval = load.loadCommands(args.dev, args.baud, lines, do_print=args.verbose, do_log=True)
    # Just waiting on load via the "get_log" command
    log = load.get_log()
    readback, compare_pass = parse_readback(log, compare_prog=None, do_print=True)
    #print(readback)
    if load_rval == 0:
        print("Success")
    else:
        print("Failed to load transaction to MMC console")
    return load_rval


def handle_status(args):
    if args.dev is None:
        print("No valid device handed to handle_telem")
        return False
    # TODO - Use args.clear_faults
    print("Reading status registers")
    import load
    load.INTERCOMMAND_SLEEP = 0.01
    # Get the lines in PMBridge syntax to read telemetry registers
    lines = read_status()
    # Perform the write to the serial device
    load_rval = load.loadCommands(args.dev, args.baud, lines, do_print=args.verbose, do_log=True)
    # Just waiting on load via the "get_log" command
    log = load.get_log()
    readback, compare_pass = parse_readback(log, compare_prog=None, do_print=False)
    # TODO parse status bits
    #print(readback)
    for page, regvals in readback:
        print("PAGE: {}".format(_hexint(page)))
        for regnum, val in regvals:
            regname, decoded = decode_bits(regnum, val)
            print("{}:{}".format(regname, decoded))
            #print("  {}: {}".format(regnum, val))
    if load_rval == 0:
        print("Success")
    else:
        print("Failed to load transaction to MMC console")
    return load_rval


def main(argv):
    import load
    # 100ms between commands for conservative program timing constraints
    load.INTERCOMMAND_SLEEP = 0.1
    # NOTE! This intercommand timing works for all except MFR_EE_ERASE which needs 0.4s sleep
    parser = load.ArgParser()
    parser.add_argument('--print', default=False, action="store_true", help='Print values to write or read')
    parser.add_argument('-v', '--verbose', default=False, action="store_true", help='Print console chatter')
    subparsers = parser.add_subparsers(title="Actions", dest="subcmd", required=True)

    parser_write = subparsers.add_parser("write", help="Write a program")
    write_group = parser_write.add_mutually_exclusive_group()
    write_group.add_argument("--test", default=False, action="store_true", help='Test; write just a few registers')
    write_group.add_argument("-f", "--file", help="File to use for program values to write")
    parser_write.set_defaults(handler=handle_write)

    parser_read = subparsers.add_parser("read", help="Read current value of registers in program")
    parser_read.add_argument("--check", default=False, action="store_true", help="Compare values of readback with values in program")
    read_group = parser_read.add_mutually_exclusive_group()
    read_group.add_argument("--test", default=False, action="store_true", help="Test; read just a few registers")
    read_group.add_argument("-f", "--file", default=None, help="Read values of registers parsed from FILE")
    parser_read.set_defaults(handler=handle_read)

    parser_limits = subparsers.add_parser("limits", help="Test hard-coded limits")
    parser_limits.add_argument("-f", "--file", default=None, help="Read limits from FILE")
    parser_limits.set_defaults(handler=handle_limits)

    parser_store = subparsers.add_parser("store", help="Store existing configuration to LTM4673 EEPROM")
    parser_store.set_defaults(handler=handle_store)

    parser_restore = subparsers.add_parser("restore", help="Restore (reload) configuration from LTM4673 EEPROM")
    parser_restore.set_defaults(handler=handle_restore)

    parser_telem = subparsers.add_parser("telemetry", help="Read LTM4673 telemetry registers.")
    parser_telem.set_defaults(handler=handle_telem)

    parser_status = subparsers.add_parser("status", help="Read LTM4673 status registers.")
    parser_limits.add_argument("-c", "--clear_faults", default=False, action="store_true", help="Clear faults before reading status")
    parser_status.set_defaults(handler=handle_status)

    args = parser.parse_args()
    return args.handler(args)

"""
Common PMBridge commands
// Select page 0
t 0xc0 0x0 0x0
// Select page 1
t 0xc0 0x0 0x1
// Select page 2
t 0xc0 0x0 0x2
// Select page 3
t 0xc0 0x0 0x3
// Select page 0xff
t 0xc0 0x0 0xff
// Attempt to set VOUT_COMMAND to 1.1V (1100 mV = 0x2333)
t 0xc0 0x21 0x33 0x23
// Attempt to set VOUT_COMMAND to 1.5V (1500 mV = 0x3000)
t 0xc0 0x21 0x00 0x30
// Attempt to set VOUT_COMMAND to 2.65V (2650 mV = 0x54cc)
t 0xc0 0x21 0xcc 0x54
"""

if __name__ == "__main__":
    import sys
    main(sys.argv)
    #testV_TO_L11(sys.argv)
    #print_commands_c()
    #_init_sim_mem()
    #_init_sim_telem()
    #test_get_program_from_file(sys.argv)
    #test_to_si(sys.argv)
