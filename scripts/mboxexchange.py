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
except:
    print("Please append path to lbus_access module to PYTHONPATH")
    LBUS_ACCESS_FOUND = False

def getPageAndName(s):
    """
    Accepts mailbox member names in the following formats
                      (returns  npage,  name,   part)
        MB3_FOO => Resolves to  3,      FOO,    None
        FOO     => Resolves to  None,   FOO,    None
        FOO_3   => Resolves to  None,   FOO,    3
        FOO_4   => Resolves to  None,   FOO_4,  None
    """
    match = re.search('MB(\d+)_(\w+)', s)
    if match:
        groups = match.groups()
        npage = int(groups[0])
        name = groups[1]
    else:
        name = s
        npage = None
    matchPart = re.search('(_(\d+))$', name)
    if matchPart:
        groups = matchPart.groups()
        part = int(groups[1])
        if part > 3:
            part = None
            name = s
        else:
            name = name.strip(groups[0])
    else:
        part = None
    return npage, name, part

def getIPAddr(s):
    # IPv4
    match = re.search('(\d+.\d+.\d+.\d+)', s)
    if match:
        print(f"{match.groups()}")
        return match.groups()[0]
    return False

def _int(s):
    # Try to interpret as decimal-string/integer/float
    try:
        return int(s)
    except:
        pass
    # Assuming s is string from this point
    if hasattr(s, 'lower'):
        # Try hex format 0x...
        if 'x' in s.lower():
            try:
                return int(s, 16)
            except:
                pass
        # Try hex format ...h
        elif 'h' in s.lower():
            hindex = s.index('h') # Assuming this method exists
            try:
                return int(s[:hindex], 16)
            except:
                pass
    return None

def _splitBytes(val, size):
    l = [(val >> 8*m) & 0xff for m in range(size)]
    l.reverse()  # big-endian, network byte order
    return l

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

def testGetPageAndName(argv):
    if len(argv) < 2:
        print("python3 {} string".format(argv[0]))
        return 1
    s = argv[1]
    npage, name, part = getPageAndName(s)
    if npage is not None:
        print(f"Extracted {name} in page {npage} (part {part})")
    elif part is not None:
        print(f"Extracted part {part} from name {name}")
    else:
        print(f"No extraction: {s}")
    return 0

def doMailboxReadWrite(argv):
    if not LBUS_ACCESS_FOUND:
        return 1
    scriptPath = os.path.split(argv[0])[0]
    defaultDefFile = os.path.join(scriptPath, "../inc/mbox.def")
    parser = argparse.ArgumentParser(description="Mailbox write interface")
    parser.add_argument('-d', '--def_file', default=defaultDefFile, help='File name for mailbox definition file to be loaded')
    parser.add_argument('-p', '--page', default=None, help='Mailbox page number')
    parser.add_argument('-i', '--ipAddr', default=None, help='IP Address of marble board')
    parser.add_argument('--port', default=803, help='UDP port number')
    parser.add_argument('-x', '--hex', default=False, action="store_true",  help="Print output as hex")
    parser.add_argument('name', nargs='+', default=None, help='Mailbox entry name (and optionally value for write)')
    args = parser.parse_args()
    value = None
    if len(args.name) > 1:
        rname, value = args.name
    else:
        rname = args.name[0]
    if args.ipAddr == None:
        print("Please specify IP address with '-i'")
        return 1
    nPage, name, part = getPageAndName(rname)
    if nPage == None and args.page == None:
        print("Please specify page number with '-p' or with fully-resolved mailbox enum constant.")
        return 1
    elif nPage == None:
        nPage = int(args.page)
    else:
        if args.page is not None and nPage != int(args.page):
            nPage = int(args.page)
            print("Using page {}".format(nPage))
    mi = mkmbox.MailboxInterface(inFilename=args.def_file)
    mi.interpret()
    elementAddr, size = mi.getElementOffsetAddressAndSize(nPage, name)
    if elementAddr == None:
        print("Could not find {} in page {}".format(name, nPage))
        return 1
    rvals = mailboxReadWrite(args.ipAddr, SPI_MBOX_ADDR+elementAddr, _int(value), size=size, port=int(args.port))
    if value == None:
        rvals = list(rvals)
        rvals.reverse()
        rval = mi._combine(*rvals)
        if args.hex:
            print("Returned 0x{:x}".format(rval))
        else:
            print("Returned {}".format(rval))
    return 0

if __name__ == "__main__":
    import sys
    sys.exit(doMailboxReadWrite(sys.argv))
    # sys.exit(testGetPageAndName(sys.argv))
