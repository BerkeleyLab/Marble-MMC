#! /usr/bin/python3

# Read and print contents of mailbox in human-readable format

import sys
import re
import os
import argparse

JSON_TOP_DIR="mailbox"

MBOX_REGNAMES = ("spi_mbox", "marble_mbox_buf")

import mkmbox
def getFromLeep(ipAddr, port=803):
    try:
        import leep
    except:
        print("To use LEEP, append path to leep module to PYTHONPATH")
        return None
    leep_addr = "leep://{}:{}".format(ipAddr, port)
    print("Getting from {}".format(leep_addr))
    dev = leep.open(leep_addr, timeout=5.0) # Returns LEEPDevice instance
    regname = None
    for _regname in MBOX_REGNAMES:
        if _regname in dev.regmap.keys():
            regname = _regname
            break
    if regname is None:
        raise Exception("Could not find mailbox in memory map.")
    mbox = dev.reg_read([regname])[0]
    dev.sock.close()
    return mbox

def get_MSB(l, offset, size):
    ll = [(x & 0xff) for x in l[offset:offset+size]]
    return int.from_bytes(bytes(ll))

def get_4(l, offset):
    return get_MSB(l, offset, 4)

def load_json(filename):
    import json
    return json.load(filename)

def getJsonMapFromHash(_hash, topdir=JSON_TOP_DIR):
    if not os.path.exists(topdir):
        print("{} doesn't exist".format(topdir))
        return None
    filename = "mailbox_map_{:08x}.json".format(_hash)
    files = os.path.listdir(topdir)
    if filename not in files:
        return None
    filepath = os.path.join(topdir, filename)
    dd = load_json(filepath)
    return dd

def decode(mbox_dump):
    # The only hard-coded number we need is the MBOX_HASH index
    mbox_hash_index = 76
    mhash = get_4(mbox_dump, mbox_hash_index)
    print("HASH = 0x{:x}".format(mhash))
    mmap = getJsonMapFromHash(mhash)
    val_dict = {}
    if mmap is None:
        return None
    for regname, reg_dict in mmap:
        aw = reg_dict["addr_width"]
        base = reg_dict["base_addr"]
        size = 1<<aw
        val = get_MSB(mbox_dump, base, size)
        val_dict["regname"] = val
    return val_dict

def _storeToFile(filename, mbox):
    n = 0
    if filename != None:
        data = mbox.astype('uint8').tobytes()
        with open(filename, 'wb') as fd:
            n = fd.write(data)
    return n

def printDump(ldata, rowLength=16):
    for n in range(len(ldata)):
        if (n+1) % rowLength:
            print(f"{ldata[n]:02x} ", end='')
        else:
            print(f"{ldata[n]:02x}")
    if (n+1) % rowLength:
        print()
    return

def getFromFile(filename):
    """Get mailbox contents from a binary file 'filename' rather than from a
    device (i.e. via LEEP)."""
    mbox = []
    print("Getting from {}".format(filename))
    with open(filename, 'rb') as fd:
        mbox = fd.read(mkmbox.MAILBOX_SIZE)
    return list(mbox)

def getIPAddr(s):
    if s is None:
        return "127.0.0.1", 803
    # IPv4
    match = re.search('(\d+.\d+.\d+.\d+)(:\d+)?', s)
    if match:
        groups = match.groups()
        ip = groups[0]
        port = groups[1]
        if port is None:
            port = 803
        else:
            port = int(port.strip(':'))
        return ip, port
    return "127.0.0.1", 803

def testGetIPAddr(argv):
    if len(argv) < 2:
        print("python3 {} ipString".format(argv[0]))
        return False
    s = argv[1]
    ip = getIPAddr(s)
    print("{} => {}".format(s, ip))
    return True

def decodeMbox(argv):
    scriptPath = os.path.split(argv[0])[0]
    defaultDefFile = os.path.join(scriptPath, "../inc/mbox.def")
    fromFile = False
    parser = argparse.ArgumentParser(description="Mailbox Reader/Parser")
    parser.add_argument('-d', '--def_file', default=None, help='File name for mailbox definition file to be loaded')
    parser.add_argument('-s', '--store_file', default=None, help='File name to store binary data read from mailbox')
    parser.add_argument('-r', '--raw', action='store_true', default=False, help='Print the raw contents of the mailbox memory.')
    inputHelp = "Can be IP address to read device via LEEP or file name to read mailbox data from a binary file"
    parser.add_argument('-i', '--input', default=None, help=inputHelp)
    args = parser.parse_args()
    ipAddr, port = getIPAddr(args.input)
    if port is not None:
        port = int(port)
    else:
        port = 803
    if not ipAddr:
        if not os.path.exists(args.input):
            print("{} does not look like an IP address and also doesn't appear to be an existing file.".format(args.input))
            return 1
        fromFile = True
        inFilename = args.input
    if fromFile:
        mboxContents = getFromFile(inFilename)
    else:
        mboxContents = getFromLeep(ipAddr, port)
        if mboxContents is None:
            return 1
    decode(mboxContents)
    if args.store_file is not None:
        print("Storing raw data to {}".format(args.store_file))
        _storeToFile(args.store_file, mboxContents)
    if args.def_file is None:
        if not args.raw and os.path.exists(defaultDefFile):
            print("Found default mailbox definition file {}".format(defaultDefFile))
            args.def_file = defaultDefFile
    if args.def_file is None:
        if args.store_file is None:
            if not args.raw:
                print("No mailbox definition file; cannot interpret meaning of data. ", end="")
            print("Raw data:")
            printDump(mboxContents, 16)
    else:
        if args.raw:
            print("Raw data:")
            printDump(mboxContents, 16)
        else:
            mi = mkmbox.MailboxInterface(inFilename=args.def_file)
            mi.interpret()
            decoded = mi.decode(mboxContents)
    return 0

if __name__ == "__main__":
    #sys.exit(getFromLeep())
    #sys.exit(testGetIPAddr(sys.argv))
    sys.exit(decodeMbox(sys.argv))
