#! /usr/bin/python3

# Generate a shared secret key for Marble MMC watchdog authentication
# Stores key to local disk and optionally loads to MMC via console.

import os
import sys
import platform
import random
import argparse
import re
import serial
from serial.tools import list_ports

MMC_CONSOLE_CHAR_WATCHDOG_KEY = 'v'
DEFAULT_KEY_FILE = "mmc_key"

system = platform.system()
def get_default_key_path():
    if system == "Linux":
        home = os.environ.get("HOME", None)
        if home is None:
            home = "~"
        return os.path.join(home, ".marble_mmc_keys")
    elif system == "Windows":
        print("Please get some real Windows-specific help here")
        return os.path.join(os.environ.get("APPDATA"), "marble_mmc_keys")
    else:
        print("System {} unsupported".format(system))
    return None

DEFAULT_KEY_PATH = get_default_key_path()

def get_default_key_file(_id=None):
    filename = make_filename(_id)
    filedir = os.environ.get("MMC_KEY_PATH", DEFAULT_KEY_PATH)
    return os.path.join(filedir, filename)

def list_devs():
    ports = list_ports.comports()
    return [p.device for p in ports]

def make_filename(_id=None):
    if _id is None:
        return DEFAULT_KEY_FILE
    else:
        return "{}_{}".format(DEFAULT_KEY_FILE, _id)

def make_key_string():
    return "{:032x}".format(random.randint(0, (1<<128)))

def open_file(filedir=None, _id=None, force=False):
    """Returns (file_descriptor, filepath_exists)"""
    if filedir is None:
        # Check environment
        filedir = os.environ.get("MMC_KEY_PATH", DEFAULT_KEY_PATH)
    filename = make_filename(_id)
    # Create a new file (or overwrite existing)
    filepath = os.path.join(filedir, filename)
    if os.path.exists(filepath) and not force:
        print("Using existing keyfile {}.\nUse --regen to generate new key and overwrite.".format(filepath))
        return (None, filepath)
    # Ensure the path exists
    os.makedirs(filedir, exist_ok=True)
    try:
        fd = open(filepath, 'w')
    except OSError as err:
        print(err)
        return (None, filepath)
    return (fd, filepath)

def store_key_string(fd, ks):
    if fd is None:
        return False
    if len(ks) != 32:
        print("Invalid key string length {} (must be 32)".format(len(ks)))
        return False
    # Write key string
    fd.write(ks)
    return True

def try_serial_device(tty=None, baud=115200):
    try:
        import load
    except ImportError:
        print("Failed to import load.py. Please append its path to PYTHONPATH")
        return False
    if tty is None:
        print("No serial device specified. Available devices: {}".format(list_devs()))
        return False
    try:
        dev = serial.Serial(port=tty, baudrate=int(baud), timeout=0.1)
        dev.close()
    except serial.SerialException as se:
        print(se)
        print("Available devices: {}".format(list_devs()))
        return False
    return True

def load_key_string(ks, tty=None):
    try:
        import load
    except ImportError:
        print("Failed to import load.py. Please append its path to PYTHONPATH")
        return False
    if len(ks) != 32:
        print("Invalid key string length {} (must be 32)".format(len(ks)))
        return False
    commands = [MMC_CONSOLE_CHAR_WATCHDOG_KEY + " " + ks]
    if load.loadCommands(tty, commands=commands, do_log=True) == 0:
        log = load.get_log() # Just a hack to wait for commands to be loaded
        # TODO - Vet the log to confirm success?
        print("Key loaded to MMC\r\n")
        return True
    print("Failed to load to device {}. Verify the device path and permissions.".format(tty))
    print("Available devices: {}".format(list_devs()))
    return False

def get_key_string(filepath):
    if not os.path.exists(filepath):
        print("{} does not exist".format(filepath))
        return None
    with open(filepath, 'r') as fd:
        matched = vet_key_string_line(fd.readline())
    return matched

def vet_key_string_line(line):
    restr = "^[a-fA-F0-9]{32}$"
    matched = re.match(restr, line)
    if matched:
        return line
    return None

def test_key_gen_match():
    ks = make_key_string()
    print(f"ks = {ks}")
    line = vet_key_string_line(ks)
    print(f"line = {line}")
    shortks = "01bde569760982ad"
    print(f"shortks = {shortks}")
    line = vet_key_string_line(shortks)
    print(f"line = {line}")
    badks = "{:-^32s}".format("some line of text that's the w")
    print(f"badks = {badks}")
    print(f"len(badks) = {len(badks)}")
    line = vet_key_string_line(badks)
    print(f"line = {line}")
    corruptks = [x for x in ks]
    corruptks[15] = "X"
    corruptks = ''.join(corruptks)
    print(f"corruptks = {corruptks}")
    line = vet_key_string_line(corruptks)
    print(f"line = {line}")
    return

def main():
    parser = argparse.ArgumentParser(description="Marble MMC Authentication Key Generator and Loader")
    parser.add_argument('-d', '--dev', default=None,
                        help='Device descriptor of TTY/COM port for Marble MMC')
    parser.add_argument('-b', '--baud', default=115200, help="UART Baud rate")
    parser.add_argument('-k', '--keyfile', default=None, help="Existing file to load to MMC")
    parser.add_argument('-i', '--id', default=None, help="Board ID (i.e. serial number)")
    parser.add_argument('-r', '--regen', default=False, action="store_true", help="Regenerate (overwrite key file if exists)")
    args = parser.parse_args()
    if DEFAULT_KEY_PATH == None:
        sys.exit(1)

    # Check serial device
    if args.dev is not None:
        if not try_serial_device(args.dev):
            return 1
    if args.keyfile is None:
        # Generate/store new keyfile
        fd, keyfile_path = open_file(args.keyfile, args.id, force=args.regen)
        if fd is None:
            if keyfile_path is not None:
                ks = get_key_string(keyfile_path)
                if ks is None:
                    print("Invalid key file found at {}".format(keyfile_path))
                    return 1
            else:
                return 1
        else:
            print("Generating new key file")
            ks = make_key_string()
            if store_key_string(fd, ks):
                # Set the file permissions
                os.chmod(keyfile_path, 0o600)
                print("Key stored to: {}".format(keyfile_path))
    else:
        # Use existing key file
        ks = get_key_string(args.keyfile)
        if ks is None:
            print("Invalid key file found at {}".format(args.keyfile))
            return 1
        else:
            print("Using key file {}".format(args.keyfile))
    if args.dev is not None:
        if not load_key_string(ks, args.dev):
            return 1
    print("SUCCESS")
    return 0

if __name__ == "__main__":
    # test_key_gen_match()
    sys.exit(main())
