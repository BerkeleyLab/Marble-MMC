#! /usr/bin/python3

# Read from or write to a mailbox item via LBUS_ACCESS

import argparse
import re
import os

LBUS_ACCESS_FOUND = True
SPI_MBOX_ADDR = 0x200000

import mkmbox
try:
    from lbus_access import lbus_access
except ModuleNotFoundError:
    LBUS_ACCESS_FOUND = False


def getPageAndName(s):
    """
    Accepts mailbox member names in the following formats
                      (returns  npage,  name)
        MB3_FOO => Resolves to  3,      FOO
        FOO     => Resolves to  None,   FOO
        FOO_3   => Resolves to  None,   FOO_3
    """
    match = re.search(r'MB(\d+)_(\w+)', s)
    if match:
        groups = match.groups()
        npage = int(groups[0])
        name = groups[1]
    else:
        name = s
        npage = None
    return npage, name


def getIPAddr(s):
    # IPv4
    match = re.search(r'(\d+.\d+.\d+.\d+)', s)
    if match:
        print(f"{match.groups()}")
        return match.groups()[0]
    return False


def _int(s):
    # Try to interpret as decimal-string/integer/float
    try:
        return int(s)
    except (TypeError, ValueError):
        pass
    # Assuming s is string from this point
    if hasattr(s, 'lower'):
        # Try hex format 0x...
        if 'x' in s.lower():
            try:
                return int(s, 16)
            except (TypeError, ValueError):
                pass
        # Try hex format ...h
        elif 'h' in s.lower():
            hindex = s.index('h')  # Assuming this method exists
            try:
                return int(s[:hindex], 16)
            except (TypeError, ValueError):
                pass
    return None


def _splitBytes(val, size):
    ll = [(val >> 8*m) & 0xff for m in range(size)]
    ll.reverse()  # big-endian, network byte order
    return ll


def mailboxReadWrite(ipAddr, address, value, size=1, verbose=False, port=803):
    """Write to mailbox page number 'nPage', element 'name' via lbus_access low-level protocol
    with IP address 'ipAddr'"""
    if not LBUS_ACCESS_FOUND:
        print("lbus_access not imported")
        return []
    readNWrite = False
    if value is None:
        readNWrite = True
    dev = lbus_access(ipAddr, port=port, allow_burst=False)
    addrs = [address]
    if readNWrite:
        vals = [None]*size
    else:
        vals = _splitBytes(value, size)
    for n in range(1, size):
        addrs.append(address + n)
    if verbose:
        if readNWrite:
            print("Reading from address 0x{:x}".format(address))
        else:
            print("Writing {} to address 0x{:x}".format(value, address))
    rets = dev.exchange(addrs, vals)
    return rets


def doMailboxReadWrite(argv):
    if not LBUS_ACCESS_FOUND:
        print("Please append path to lbus_access module to PYTHONPATH")
        return 1
    scriptPath = os.path.split(argv[0])[0]
    defaultDefFile = os.path.join(scriptPath, "../inc/mbox.def")
    parser = argparse.ArgumentParser(description="Mailbox write interface")
    parser.add_argument('-d', '--def_file', default=defaultDefFile,
                        help='File name for mailbox definition file to be loaded')
    parser.add_argument('-p', '--page', default=None, help='Mailbox page number')
    parser.add_argument('-i', '--ipAddr', default=None, help='IP Address of marble board')
    parser.add_argument('--port', default=803, help='UDP port number')
    parser.add_argument('-x', '--hex', default=False, action="store_true",  help="Print output as hex")
    parser.add_argument('name', default=None, help='Mailbox entry name (and optionally value for write)')
    args = parser.parse_args()
    value = None
    if "=" in args.name:
        rname, value = args.name.split('=')
    else:
        rname = args.name
    if args.ipAddr is None:
        print("Please specify IP address with '-i'")
        return 1
    nPage, name = getPageAndName(rname)
    if nPage is None and args.page is None:
        print("Please specify page number with '-p' or with fully-resolved mailbox enum constant.")
        return 1
    elif nPage is None:
        nPage = int(args.page)
    else:
        if args.page is not None and nPage != int(args.page):
            nPage = int(args.page)
            print("Using page {}".format(nPage))
    mi = mkmbox.MailboxInterface(inFilename=args.def_file)
    mi.interpret()
    elementAddr, size = mi.getElementOffsetAddressAndSize(nPage, name)
    if elementAddr is None:
        print("Could not find {} in page {}".format(name, nPage))
        return 1
    rvals = mailboxReadWrite(args.ipAddr, SPI_MBOX_ADDR+elementAddr, _int(value), size=size, port=int(args.port))
    if value is None:
        rvals = list(rvals)
        rvals.reverse()
        rval = mi._combine(*rvals)
        if args.hex:
            print("0x{:x}".format(rval))
        else:
            print("{}".format(rval))
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(doMailboxReadWrite(sys.argv))
