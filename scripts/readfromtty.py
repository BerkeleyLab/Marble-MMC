#! /usr/bin/python3

# Read and print N lines from a tty device

import argparse
import serial
from load import StreamSerial

class TTYReader():
    def __init__(self, port = None, baud = 115200, timeout = 1, encoding = 'utf-8'):
        self._ready = False
        self.line = None
        self._timeout = float(timeout)
        self._encoding = encoding
        if (port == None):
            return None
        try:
            self.dev = serial.Serial(port=port, baudrate=baud, timeout=self._timeout)
            self._ready = True
        except Exception as e:
            print(e)

    def failed(self):
        return not self._ready

    def readLine(self):
        # It seems like neither readline() nor read_until() actually respect the timeout
        l = self.dev.readline()
        #l = self.dev.read_until()
        if l is not None and hasattr(l, "decode"):
            try:
                return l.decode(self._encoding).strip()
            except UnicodeDecodeError:
                return None
        return None

    def close(self):
        self.dev.close()

def readLines(argv):
    parser = argparse.ArgumentParser(description="Read and print N lines from TTY device")
    parser.add_argument('-d', '--dev', default='/dev/ttyUSB2',
                        help='Device descriptor of TTY/COM port for marble_mmc')
    parser.add_argument('-b', '--baud', default=115200, help="UART Baud rate")
    parser.add_argument('-t', '--timeout', default=100, help="TTY device timeout in milliseconds")
    parser.add_argument('-e', '--encoding', default='utf-8', help="Encoding of characters received from TTY.")
    parser.add_argument('-m', '--minchars', default=8, help="The minimum number of characters to read to be considered a valid line.")
    parser.add_argument("nlines", default=1, help="Number of lines to read from device")
    args = parser.parse_args()
    try:
        timeout_s = float(args.timeout)/1000
    except TypeError:
        print("timeout must be a number (not string). Defaulting to 100ms")
        timeout_s = 0.1
    dev = TTYReader(port=args.dev, baud=args.baud, timeout=timeout_s, encoding=args.encoding)
    minchars = int(args.minchars)
    if dev.failed():
        return 1
    try:
        nlines = int(args.nlines)
    except ValueError:
        print("nlines must be an integer, not {}. Defaulting to 1 line.".format(args.nlines))
        nlines = 1
    n = 0
    while True:
        l = dev.readLine()
        if l is not None and len(l.strip()) != 0:
            if len(l) >= minchars:
                print(l)
                n += 1
            if n >= nlines:
                break
    dev.close()

if __name__ == "__main__":
    import sys
    sys.exit(readLines(sys.argv))
