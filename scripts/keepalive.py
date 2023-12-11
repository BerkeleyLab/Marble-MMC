# Minimum viable keep-alive server
# I don't like how much setup gets re-done every iteration
# Too much chatter
# Needs updating once the handshake logic in watchdog.c turns non-placeholder.

import mboxexchange as me
import argparse
from sys import argv
import os
import time
import datetime
import mkmbox
import socket
from pysiphash import SipHash_2_4
import struct
from binascii import hexlify

LBUS_ACCESS_FOUND = True
SPI_MBOX_ADDR = 0x200000  # needs to match spi_mbox base_addr in static_regmap.json


def transform(rval, key=None):
    if key is None:
        key = "super secret key".encode()
    tkey = bytearray()
    tkey.extend(key)
    xval = SipHash_2_4(tkey, rval).hash_nopad()
    return xval


def word_do(ipAddr, pageNo, wordName, val, port=803):
    elementAddr, size = mi.getElementOffsetAddressAndSize(pageNo, wordName)
    if elementAddr is None:
        print("Internal error 1")
        return None
    rvals = me.mailboxReadWrite(ipAddr, SPI_MBOX_ADDR+elementAddr, val, size=size, port=port)
    rvals = list(rvals)
    rvals.reverse()  # I was trying to get rid of reordering like this
    return rvals


def refresh(ipAddr, mi, port=803, key=None):
    rvals = word_do(ipAddr, 7, "WD_NONCE", None, port=port)
    rval_b = bytearray(rvals)
    rval_s = hexlify(rval_b).decode()
    # print("Returned "+rval_s)
    oval = transform(rval_b, key=key)
    oval_s = hexlify(struct.pack("!Q", oval)).decode()
    # print("Writing  "+oval_s)
    word_do(ipAddr, 8, "WD_HASH", oval, port=port)
    timestamp = datetime.datetime.utcnow().replace(microsecond=0).isoformat()
    print(timestamp + "Z Handshake " + ipAddr + "  " + rval_s + " -> " + oval_s)
    return 0


def hex_key(keystring):
    try:
        key_int = int(keystring, 16)
    except ValueError:
        try:
            key_int = int(keystring, 10)
        except ValueError:
            return None
    key_hex = key_int.to_bytes(16, byteorder='big')
    if False:
        print("key_hex = ", end='')
        for v in key_hex:
            print("{:02x} ".format(v), end='')
        print()
    return key_hex


if __name__ == "__main__":
    scriptPath = os.path.split(argv[0])[0]
    defaultDefFile = os.path.join(scriptPath, "../inc/mbox.def")
    parser = argparse.ArgumentParser(description="Watchdog keep-alive server")
    parser.add_argument('-i', '--ipAddr', default=None, help='IP Address of marble board')
    parser.add_argument('-t', '--time', default=8, help='Refresh time interval')
    parser.add_argument('-d', '--def_file', default=defaultDefFile,
                        help='File name for mailbox definition file to be loaded')
    parser.add_argument('-p', '--port', default=803, help="UDP port number")
    parser.add_argument('-k', '--key', default=None, help="Shared secret key for hash calculation")
    args = parser.parse_args()

    if args.ipAddr is None:
        print("Please specify IP address with '-i'")
        exit(1)

    mi = mkmbox.MailboxInterface(inFilename=args.def_file)
    mi.interpret()
    if args.key is not None:
        key = hex_key(args.key)
        if key is None and hasattr(args.key, 'encode'):
            key = args.key.encode()
    else:
        key = None

    try:
        while True:
            try:
                rc = refresh(args.ipAddr, mi, port=int(args.port), key=key)
                if rc:
                    break
            except socket.timeout:
                print("Disconnected?", args.ipAddr)
            time.sleep(int(args.time))
    except KeyboardInterrupt:
        print("\nExiting")
