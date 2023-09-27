#! /usr/bin/python3

# Load a script of marble_mmc commands to the target device via UART

# Goals:
#   1. Handle esc/backspace gracefully
#   2. Show both input and output
#   3. Sleep a bit between lines to allow blocking functions with output to return
#       3a. Try reducing the sleep and seeing what happens

import argparse
import serial
import os
import time
from concurrent.futures import ThreadPoolExecutor as Executor

INTERCOMMAND_SLEEP = 0.01 # seconds
POST_SLEEP = 1.0 # seconds

class StreamSerial():
    def __init__(self, port = None, baud = 115200):
        self._ready = False
        self.line = None
        if (port == None):
            return None
        try:
            self.dev = serial.Serial(port=port, baudrate=baud, timeout=0.1)
            self._ready = True
        except Exception as e:
            print(e)

    def failed(self):
        return not self._ready

    def writeline(self, line):
        line = line.encode('ascii')
        #print(f"writing {line}")
        try:
            self.dev.write(line)
        except Exception as e:
            print(e)
        return

    def readline(self):
        if not self._ready:
            return None
        buf = []
        timeout = 10
        while True:
            r = self.dev.read()
            if not r:
                timeout -= 1
                if timeout == 0:
                    return None
                continue
            else:
                r = r.decode('utf-8')
                #print("r = {}".format(r))
                buf.append(r)
                if r in ('\n', '\r'):
                    self.line = ''.join(buf)
                    if self.line.strip():
                        # prevent breaking on empty lines
                        break
        return self.line

    def getline(self):
        return self.line

    def close(self):
        self.dev.close()

def readDevice(sdev, wait_on):
    while True:
        try:
            if wait_on.done():
                print(">   done")
                break
            #line = await ss.readline()
            line = sdev.readline()
            if line:
                line = line.strip()
                print(line)
        except Exception as e:
            print(e)
    print(">   closing")
    sdev.close()
    return True

def getLines(filename):
    if not os.path.exists(filename):
        print("File {} does not exist".format(filename))
        return None
    lines = []
    with open(filename, 'r') as fd:
        line = True
        while (line):
            line = fd.readline()
            if (line):
                lines.append(line)
    return lines

def serveFile(sdev, filename):
    lines = getLines(filename)
    nlines = 0
    for line in lines:
        if len(line) > 0 and not line.strip().startswith('#'):
            sdev.writeline(line)
            nlines += 1
            time.sleep(INTERCOMMAND_SLEEP)
    print(f">   Wrote {nlines} lines")
    return

def serveCommands(sdev, *commands):
    nlines = 0
    for line in commands:
        if len(line) > 0 and not line.strip().startswith('#'):
            print("writing {}".format(line))
            sdev.writeline(line + '\r\n')
            nlines += 1
            time.sleep(INTERCOMMAND_SLEEP)
    time.sleep(POST_SLEEP)
    print(f">   Wrote {nlines} lines")
    return

def testReadLines(argv):
    USAGE = "python3 {} scriptname".format(argv[0])
    if len(argv) < 2:
        print(USAGE)
        return False
    filename = argv[1]
    lines = getLines(filename)
    print("got {}".format(lines))
    return True

def doLoad(argv):
    # Unused 'argv' since argparse is handling everything
    parser = argparse.ArgumentParser(description="Script loader for marble_mmc")
    parser.add_argument('-f', '--filename', default=None, help='File name for command script to be loaded')
    parser.add_argument('-d', '--dev', default='/dev/ttyUSB3',
                        help='Device descriptor of TTY/COM port for marble_mmc')
    parser.add_argument('-b', '--baud', default=115200, help="UART Baud rate")
    # Any ordered args will be assumed to be commands to pass to device
    parser.add_argument("commands", nargs=argparse.REMAINDER, help="Strings to be sent directly to device")
    args = parser.parse_args()
    if args.filename is None and len(args.commands) == 0:
        print("Missing mandatory filename or ordered args")
        return 1
    sdev = StreamSerial(args.dev, args.baud)
    if sdev.failed():
        return 1
    executor = Executor(max_workers = 2)
    if args.filename is not None:
        task1 = executor.submit(serveFile, sdev, args.filename)
    else:
        task1 = executor.submit(serveCommands, sdev, *args.commands)
    task2 = executor.submit(readDevice, sdev, task1)
    return 0

if __name__ == "__main__":
    import sys
    sys.exit(doLoad(sys.argv))
    #testReadLines(sys.argv)
