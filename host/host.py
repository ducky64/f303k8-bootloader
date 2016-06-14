import argparse
import binascii
import logging
import serial
import time

from duckycobs import *
from duckypacket import *

CHUNK_SIZE = 128

logging.basicConfig(format='%(asctime)s %(levelname)s: %(message)s', datefmt='%H:%M:%S', level=logging.INFO)

parser = argparse.ArgumentParser(description='Bootloader host')
parser.add_argument('bin_file', type=str,
                    help='bin file to load')
parser.add_argument('serial', type=str,
                    help='serial port to use, like COM1 (Windows) or /dev/ttyACM0 (Linux)')
parser.add_argument('--baud', type=int, default=115200,
                    help='serial baud rate')
parser.add_argument('--address', type=int, default=0x8000000 + 16*1024,
                    help='starting address')
parser.add_argument('--memsize', type=int, default=48*1024,
                    help='memory size')
parser.add_argument('--devices', type=int, nargs='+',
                    help='device number, 0 is master, slaves start at 1')

args = parser.parse_args()

ser = serial.Serial(args.serial, args.baud)
logging.info("Opened serial port '%s'", args.serial)

# General exception when the bootloader returns a non-success error code.
class BootloaderResponseError(Exception):
  pass

class BootloaderComms(object):
  def __init__(self, ser):
    self.serial = ser
    self.serial.write(b'\x00')

  def command(self, packet, reply_expected=True):
    self.serial.write(cobs_encode(packet.get_bytes()) + b'\x00')
    if reply_expected:
      line = ser.readline().strip()
      logging.debug("Serial <- '%s'", line)  # discard the ending space
      if (line != 'D'):
        raise BootloaderResponseError("Got '%s' error code from bootloader" % line)

  def erase(self, device, address, length):
    packet = PacketBuilder()
    packet.put_uint8(ord('E'))
    packet.put_uint8(device)
    packet.put_uint32(address)
    packet.put_uint32(length)
    self.command(packet)

  def write(self, device, address, data):
    packet = PacketBuilder()
    packet.put_uint8(ord('W'))
    packet.put_uint8(device)
    packet.put_uint32(address)
    packet.put_uint32(binascii.crc32(data) & 0xffffffff)
    packet.put_bytes(data, len(data))
    self.command(packet)

  def run_app(self, device, address):
    packet = PacketBuilder()
    packet.put_uint8(ord('J'))
    packet.put_uint8(device)
    packet.put_uint32(address)
    self.command(packet, reply_expected=False)

  def program_and_run(self, device, memsize, address, program_bin):
    time.sleep(0.1) # wait for some time to initialize the serial object, otherwise the initial flush doesn't work
    bytes_read = ser.read(ser.inWaiting())
    logging.debug("Serial: flushed %i bytes", len(bytes_read))

    self.erase(device, address, memsize)
    logging.info("Erased %i bytes from device %i", memsize, device)
    curr_address = address

    while True:
      chunk = program_bin.read(CHUNK_SIZE)
      if chunk:
        self.write(device, curr_address, chunk)
        curr_address += len(chunk)
      else:
        break
    logging.info("Write %i bytes to device %i", curr_address - address, device)

    self.run_app(device, address)
    logging.info("Started app on device %i", device)

bootloader = BootloaderComms(ser)

# TODO: ensure device 0 isn't programmed except at end

for device in args.devices:
  bin_file = open(args.bin_file, 'r')
  bootloader.program_and_run(device, args.memsize, args.address, bin_file)
  bin_file.close()

ser.close()
