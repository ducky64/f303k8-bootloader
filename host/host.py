import argparse
import binascii
import logging
import math
import os
import serial
import sys
import time

from duckycobs import *
from duckypacket import *

CHUNK_SIZE = 128
ERASE_SIZE = 2048

logging.basicConfig(format='%(asctime)s %(levelname)s: %(message)s', datefmt='%H:%M:%S', level=logging.INFO)

parser = argparse.ArgumentParser(description='Bootloader host')
parser.add_argument('bin_files', type=str, nargs='+',
                    help='bin files to load')
parser.add_argument('serial', type=str,
                    help='serial port to use, like COM1 (Windows) or /dev/ttyACM0 (Linux)')
parser.add_argument('--baud', type=int, default=115200,
                    help='serial baud rate')
parser.add_argument('--address', type=int, default=0x8000000 + 32*1024,
                    help='starting address')
parser.add_argument('--memsize', type=int, default=(256-32)*1024,
                    help='memory size')
parser.add_argument('--devices', type=int, nargs='+',
                    help='device number, 0 is master, slaves start at 1 (optional, defaults to 0...len(bin_files)-1)')

args = parser.parse_args()

ser = serial.Serial(args.serial, args.baud, timeout=1)
logging.info("Opened serial port '%s'", args.serial)

def pbar(curr, max, sym='=', space=' ', arrow='>', nsyms=32):
  assert curr <= max
  if curr == 0:
    combined = space * nsyms
  elif curr == max:
    combined = sym * nsyms
  else:
    syms = int(curr * nsyms / max)
    if syms > 0:
      combined = (syms-1) * sym + arrow + (nsyms - syms) * space
    else:
      combined = arrow + (nsyms-1) * space

  return "[{0}] {1}/{2}".format(combined, curr, max)

# General exception when the bootloader returns a non-success error code.
class BootloaderResponseError(Exception):
  pass

class BootloaderComms(object):
  def __init__(self, ser, retries=3):
    self.serial = ser
    self.serial.write(b'\x00')
    self.retries = retries

  def command(self, packet, debug_text="", reply_expected=True):
    retry = 0
    while retry <= self.retries:
      if retry > 0:
        logging.error("Retrying command (try %i of max %i): %s", retry, self.retries, debug_text)
      self.serial.write(cobs_encode(packet.get_bytes()) + b'\x00')
      if reply_expected:
        line = ser.readline().strip()
        logging.debug("Serial <- '%s'", line)  # discard the ending space
        if (line == b'D'):
          return
        else:
          logging.error("Got response '%s' from bootloader", line)
          retry += 1
      else:
        return
    raise BootloaderResponseError("Hit max retries for command: %s" % (debug_text))

  def erase(self, device, address, length):
    packet = PacketBuilder()
    packet.put_uint8(ord('E'))
    packet.put_uint8(device)
    packet.put_uint32(address)
    packet.put_uint32(length)
    self.command(packet, "Erase %i bytes @ %08x" % (length, address))

  def write(self, device, address, data):
    packet = PacketBuilder()
    packet.put_uint8(ord('W'))
    packet.put_uint8(device)
    packet.put_uint32(address)
    packet.put_uint32(binascii.crc32(data) & 0xffffffff)
    packet.put_bytes(data, len(data))
    self.command(packet, "Program %i bytes @ %08x" % (len(data), address))

  def run_app(self, device, address):
    packet = PacketBuilder()
    packet.put_uint8(ord('J'))
    packet.put_uint8(device)
    packet.put_uint32(address)
    self.command(packet, "Run app @ %08x" % address, reply_expected=False)

  def program(self, device, memsize, address, program_bin_filename):
    time.sleep(0.1) # wait for some time to initialize the serial object, otherwise the initial flush doesn't work
    bytes_read = ser.read(ser.inWaiting())
    logging.info("Serial: flushed %i bytes: %s", len(bytes_read), bytes_read)

    program_size = os.path.getsize(program_bin_filename)
    erase_size = int(math.ceil(program_size / float(ERASE_SIZE)) * ERASE_SIZE)

    logging.info("Erase %i bytes from device %i ...", erase_size, device)
    start = time.time()
    self.erase(device, address, erase_size)
    logging.info("  done (%s s)", time.time() - start)
    curr_address = address

    curr_file_loc = 0
    program_bin = open(program_bin_filename, 'rb')
    logging.info("Write %i bytes to device %i", program_size, device)
    sys.stdout.write("...")
    start = time.time()
    while True:
      chunk = program_bin.read(CHUNK_SIZE)
      if chunk:
        self.write(device, curr_address, chunk)
        curr_address += len(chunk)
        curr_file_loc += len(chunk)
        sys.stdout.write('\r' + pbar(curr_file_loc, program_size))
        sys.stdout.flush()
      else:
        break
    sys.stdout.write('\n')
    elapsed = time.time() - start
    logging.info("  done (%.03f s, %.03fKiB/s)", elapsed, program_size / 1024.0 / elapsed)

    program_bin.close()

bootloader = BootloaderComms(ser)

if not args.devices:
  devices = range(0, len(args.bin_files))
else :
  devices = args.devices
assert len(devices) == len(args.bin_files)

for device, bin_filename in zip(devices, args.bin_files):
  logging.info("Programming '%s' onto device %i", bin_filename, device)

  bootloader.program(device, args.memsize, args.address, bin_filename)

for device in devices:
  if device != 0:
    logging.info("Start app on device %i", device)
    bootloader.run_app(device, args.address)

if 0 in devices:
    logging.info("Start app on device 0")
    bootloader.run_app(0, args.address)

ser.close()
