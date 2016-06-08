import argparse
import logging
import serial
import time

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

ser.write('E%04x%08x%08x\n' % (args.device, args.address, args.memsize))
logging.debug("Serial -> Erase(0x% 8x, %i)", args.address, args.memsize)

line = ser.readline()
logging.debug("Serial <- '%s'", line.strip())  # discard the ending space

while True:
  chunk = args.bin_file.read(CHUNK_SIZE)
  if chunk:
    cmd = bytearray()
    cmd += b'W'
    cmd += ("%04x" % args.device).encode('ascii')
    cmd += ("%08x" % curr_address).encode('ascii')
    crc = 0 # TODO IMPLEMENT ME
    cmd += ("%04x" % crc).encode('ascii')
    cmd += chunk.encode('hex')
    cmd += b'\n'
    logging.debug("Serial -> Write(0x% 8x -> %i)", curr_address, len(chunk))
    curr_address += CHUNK_SIZE

    ser.write(cmd)

    line = ser.readline()
    logging.debug("Serial <- '%s'", line.strip())  # discard the ending space
  else:
    break

ser.write('J%04x%08x\n' % (args.device, args.address))
logging.debug("Serial -> Jump")

ser.close()
