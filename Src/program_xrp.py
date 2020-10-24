#!/usr/bin/env python

import serial


def read_and_print_till_empty(ser):
    for x in iter(ser.readline, b''):
        print(x.decode('ascii'), end='')

def print_write_get(ser, write_val, print_input=True):
    if print_input:
        print(write_val)
    ser.write(bytes(write_val, 'ascii'))
    read_and_print_till_empty(ser)


def program(hex_file, serial_port):
    '''
    Goal here is to open the serial port to MMC on Marble
    And exectute a sequence of steps to program the XR chip
    '''
    # Note the timeout here set for readback is important
    # And needs to be atleast 0.2sec during the XRP programming phase
    with serial.Serial(serial_port, 115200, timeout=0.2) as ser:
        print_write_get(ser, '?')
        print_write_get(ser, '3')
        print_write_get(ser, 'h')

        for line in open(hex_file, 'r').readlines():
            if line.startswith(':') and len(line) > 3:
                print_write_get(ser, line, False)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Program XR via MMC")
    parser.add_argument("-u", "--uart-device",
                        help="MMC UART device")
    parser.add_argument("-hf", "--hex-file", default="Marble_runtime_3A.hex",
                        help="Intex hex recort program file")
    args = parser.parse_args()
    program(args.hex_file, args.uart_device)
