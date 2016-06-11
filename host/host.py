import argparse
import binascii
import logging
import serial
import time

from duckycobs import *
from duckypacket import *

CHUNK_SIZE = 128

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
parser.add_argument('--memsize', type=int, default=32768,
                    help='memory size')
parser.add_argument('--device', type=int, default=1,
                    help='device number, 0 is master, slaves start at 1')

args = parser.parse_args()

ser = serial.Serial(args.serial, args.baud)
logging.info("Opened serial port '%s'", args.serial)

# flush the receive buffer
time.sleep(0.1) # wait for some time to initialize the serial object, otherwise the initial flush doesn't work
bytes_read = ser.read(ser.inWaiting())
logging.debug("Serial: flushed %i bytes", len(bytes_read))

curr_address = args.address

logging.debug("Serial -> Erase(0x% 8x, %i)", args.address, args.memsize)

packet = PacketBuilder()
packet.put_uint8(ord('E'))
packet.put_uint8(args.device)
packet.put_uint32(args.address)
packet.put_uint32(args.memsize)
ser.write(b'\x00' + cobs_encode(packet.get_bytes()) + b'\x00')
line = ser.readline()

logging.debug("Serial <- '%s'", line.strip())  # discard the ending space

while True:
  chunk = args.bin_file.read(CHUNK_SIZE)
  if chunk:
    logging.debug("Serial -> Write(0x% 8x -> %i)", curr_address, len(chunk))

    packet = PacketBuilder()
    packet.put_uint8(ord('W'))
    packet.put_uint8(args.device)
    packet.put_uint32(curr_address)
    packet.put_uint32(binascii.crc32(chunk) & 0xffffffff)
    packet.put_bytes(chunk, len(chunk))
    ser.write(cobs_encode(packet.get_bytes()) + b'\x00')

    line = ser.readline().strip()
    logging.debug("Serial <- '%s'", line)  # discard the ending space
    if (line != 'D90'):
      logging.error("Bad response from bootloader")
      exit()

    curr_address += CHUNK_SIZE
  else:
    break

logging.debug("Serial -> Jump")

packet = PacketBuilder()
packet.put_uint8(ord('J'))
packet.put_uint8(args.device)
packet.put_uint32(args.address)
ser.write(cobs_encode(packet.get_bytes()) + b'\x00')

ser.close()
