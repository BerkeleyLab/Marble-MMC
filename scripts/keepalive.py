# Minimum viable keep-alive server
# I don't like how much setup gets re-done every iteration
# Too much chatter
# Needs updating once the handshake logic in watchdog.c turns non-placeholder.

import mboxexchange as me
import argparse
from sys import argv
import os
import time
import mkmbox
import socket
LBUS_ACCESS_FOUND = True
SPI_MBOX_ADDR = 0x200000  # needs to match spi_mbox base_addr in static_regmap.json


def transform(rval):
    return rval  # placeholder only


def word_do(ipAddr, pageNo, wordName, val):
    elementAddr, size = mi.getElementOffsetAddressAndSize(pageNo, wordName)
    if elementAddr is None:
        print("Internal error 1")
        return None
    rvals = me.mailboxReadWrite(ipAddr, SPI_MBOX_ADDR+elementAddr, val, size=size)
    rvals = list(rvals)
    # rvals.reverse()
    rval = mi._combine(*rvals)
    return rval


def refresh(ipAddr, mi):
    rval_l = word_do(ipAddr, 7, "WD_NONCE_L", None)
    rval_h = word_do(ipAddr, 7, "WD_NONCE_H", None)
    print("Returned {:x} {:x}".format(rval_l, rval_h))
    oval = transform((rval_l, rval_h))  # key step
    print("Writing {:x} {:x}".format(oval[0], oval[1]))
    rval_l = word_do(ipAddr, 8, "WD_HASH_L", oval[0])
    rval_h = word_do(ipAddr, 8, "WD_HASH_H", oval[1])
    print("Returned {:x} {:x}".format(rval_l, rval_h))
    return 0


if __name__ == "__main__":
    scriptPath = os.path.split(argv[0])[0]
    defaultDefFile = os.path.join(scriptPath, "../inc/mbox.def")
    parser = argparse.ArgumentParser(description="Watchdog keep-alive server")
    parser.add_argument('-i', '--ipAddr', default=None, help='IP Address of marble board')
    parser.add_argument('-t', '--time', default=8, help='Refresh time interval')
    parser.add_argument('-d', '--def_file', default=defaultDefFile,
                        help='File name for mailbox definition file to be loaded')
    args = parser.parse_args()

    if args.ipAddr is None:
        print("Please specify IP address with '-i'")
        exit(1)

    mi = mkmbox.MailboxInterface(inFilename=args.def_file)
    mi.interpret()
    try:
        while True:
            try:
                rc = refresh(args.ipAddr, mi)
                if rc:
                    break
            except socket.timeout:
                print("Disconnected?", args.ipAddr)
            time.sleep(args.time)
    except KeyboardInterrupt:
        print("\nExiting")
