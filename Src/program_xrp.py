#!/usr/bin/env python

import time

import serial

def read_and_print_till_empty(ser):
    while True:
        x = ser.readline().decode('utf-8')
        if x: print(x, end='')
        else: break
    print()

def program(hex_file, serial_port):
    '''
    Goal here is to open the serial port to MMC on Marble
    And exectute a sequence of steps to program the XR chip
    '''
    with serial.Serial(serial_port, 115200, timeout=1) as ser:
        print('?'), ser.write(b'?')
        read_and_print_till_empty(ser)
        print('3'), ser.write(b'3')
        read_and_print_till_empty(ser)
        print('h'), ser.write(b'h')
        read_and_print_till_empty(ser)

        for line in open(hex_file, 'r').readlines():
            line = line.strip()
            if line.startswith(':') and len(line) > 3:
                ser.write(bytes(line, 'ascii'))
                read_and_print_till_empty(ser)



if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Program XR via MMC")
    parser.add_argument("-u", "--uart-device",
                        help="MMC UART device")
    parser.add_argument("-hf", "--hex-file", default="Marble_runtime_3A.hex",
                        help="Intex hex recort program file")
    args = parser.parse_args()
    program(args.hex_file, args.uart_device)
