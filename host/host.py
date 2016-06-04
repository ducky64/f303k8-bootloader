import argparse
import logging
import serial
import time

CHUNK_SIZE = 1024

logging.basicConfig(format='%(asctime)s %(levelname)s: %(message)s', datefmt='%H:%M:%S', level=logging.DEBUG)

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
logging.info("Opened serial port '%s'", args.serial)

# flush the receive buffer
time.sleep(0.2) # wait for some time to initialize the serial object, otherwise the initial flush doesn't work
bytes_read = ser.read(ser.inWaiting())
logging.debug("Serial: flushed %i bytes", len(bytes_read))

while True:
  chunk = args.bin_file.read(CHUNK_SIZE)
  if chunk:
    cmd = bytearray()
    cmd += b'W'
    cmd += chunk.encode('hex')
    cmd += b'\n'
    logging.debug("Serial -> Write(%i)", len(chunk))
    ser.write(cmd)
    line = ser.readline()
    logging.debug("Serial <- '%s'", line.strip())  # discard the ending space
  else:
    break

ser.write('J\n')
logging.debug("Serial -> Jump")

ser.close()
