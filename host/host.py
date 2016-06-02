import argparse
import serial

CHUNK_SIZE = 128

parser = argparse.ArgumentParser(description='Bootloader host')
parser.add_argument('bin_file', type=file,
                    help='bin file to load')
parser.add_argument('serial', type=str,
                    help='serial port to use, like COM1 (Windows) or /dev/ttyACM0 (Linux)')
parser.add_argument('--baud', type=int, default=115200,
                    help='serial baud rate')
parser.add_argument('--address', type=int, default=0x8008000,
                    help='starting address')

args = parser.parse_args()

ser = serial.Serial(args.serial, args.baud)
