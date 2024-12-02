#! python3

# Set Marble power supplies to slightly above/below their nominal values to
# test engineering margin on hardware

import ltm4673
import re

nominal_volts = {
    # page: nominal rail voltage
    0: 1.0, # 1.0V is page 0
    1: 1.8, # 1.8V is page 1
    2: 2.5, # 2.5V is page 2
    3: 3.3, # 3.3V is page 3
}


def get_voltage(vs, page):
    nominal_voltage = nominal_volts[page]
    _rePercent = "^([0-9.]+)\%"
    _reVoltage = "^([0-9.]+)V|v"
    _match = re.match(_rePercent, vs)
    voltage = None
    if _match:
        percent = float(_match.groups()[0])
        voltage = (percent/100)*nominal_voltage
    else:
        _match = re.match(_reVoltage, vs)
        if _match:
            voltage = float(_match.groups()[0])
        else:
            voltage = float(vs) # will error out if garbage is given
    if voltage is None:
        raise Exception("Invalid voltage string {}".format(vs))
    # Check desired voltage against limits
    limits = ltm4673.ltm4673_limits[page][ltm4673.VOUT_COMMAND] # (mask, min, max)
    mask, _min, _max = limits
    vmin = ltm4673.L16_TO_V(_min)
    vmax = ltm4673.L16_TO_V(_max)
    if voltage < vmin or voltage > vmax:
        raise Exception("Requested voltage ({:.2f}V) for {:.1f}V rail is beyond hard-coded limits ({:.2f}V : {:.2f}V)".format(
            voltage, nominal_voltage, vmin, vmax))
    return voltage


def read_voltages():
    # Read for each page
    cmds = (
        ltm4673.READ_VOUT,
        ltm4673.READ_IOUT,
    )
    xacts = []
    for page in range(4):
        xacts.append(ltm4673.write(ltm4673.PAGE, page))
        for cmd in cmds:
            xacts.append(ltm4673.read(cmd))
    lines = ltm4673.translate_mmc(xacts)
    return lines


def handle_args(args):
    xacts = []
    _rails = (
        # Argstr, page, nominal voltage
        ("s1v0", 0), # 1.0V is page 0
        ("s1v8", 1), # 1.8V is page 1
        ("s2v5", 2), # 2.5V is page 2
        ("s3v3", 3), # 3.3V is page 3
    )
    for arg, page in _rails:
        sarg = getattr(args, arg)
        if sarg is not None:
            val = get_voltage(sarg, page)
            xacts.append(ltm4673.write(ltm4673.PAGE, page))
            xacts.append(ltm4673.write(ltm4673.VOUT_COMMAND, ltm4673.V_TO_L16(val)))
    import load
    # I'll give it a bit more time for writes.
    load.INTERCOMMAND_SLEEP = 0.1
    load_rval = 0
    if len(xacts) > 0:
        print("Attempting voltage override. Ensure write-protect switch (SW4) is off.")
        lines = ltm4673.translate_mmc(xacts)
        # Perform the write to the serial device
        load_rval = load.loadCommands(args.dev, args.baud, lines, do_print=args.verbose, do_log=False)
        # Just waiting on load via the "get_log" command
        log = load.get_log()
        if load_rval == 0:
            print("Success")
        else:
            print("Failed to load transaction to MMC console")
    else:
        print("No voltages provided. No writes will be performed.")
    if not args.confirm:
        return load_rval
    # Reads don't need extra time
    load.INTERCOMMAND_SLEEP = 0.01
    lines = read_voltages()
    load_rval = load.loadCommands(args.dev, args.baud, lines, do_print=args.verbose, do_log=True)
    log = load.get_log()
    readback, compare_pass = ltm4673.parse_readback(log, compare_prog=None, do_print=True)
    #print(log)
    if load_rval == 0:
        print("Success")
    else:
        print("Failed to load transaction to MMC console")
    return load_rval


def main(argv):
    import load
    # 100ms between commands for conservative program timing constraints
    load.INTERCOMMAND_SLEEP = 0.1
    parser = load.ArgParser()
    parser.add_argument('-v', '--verbose', default=False, action="store_true", help='Print console chatter')
    parser.add_argument('--1V0', default=None, dest="s1v0",
                        help="Set 1.0V rail voltage (voltage or percent with '%%').")
    parser.add_argument('--1V8', default=None, dest="s1v8",
                        help="Set 1.8V rail voltage (voltage or percent with '%%').")
    parser.add_argument('--2V5', default=None, dest="s2v5",
                        help="Set 2.5V rail voltage (voltage or percent with '%%').")
    parser.add_argument('--3V3', default=None, dest="s3v3",
                        help="Set 3.3V rail voltage (voltage or percent with '%%').")
    parser.add_argument('-c', '--confirm', default=False, action="store_true",
                        help="Confirm voltages after programing by reading back telemetry.")
    args = parser.parse_args()
    return handle_args(args)


if __name__ == "__main__":
    import sys
    sys.exit(main(sys.argv))

