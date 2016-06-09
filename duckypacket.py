import struct
from numbers import Number

class PacketBuilder(object):
  """Class for stuffing data into a network-order (big-endian) byte-oriented
  packet.
  """
  def __init__(self):
    """Initialize an empty packet.
    """
    self.bytes = bytearray()

  def put_uint8(self, value):
    """Append a 8-bit integer to the end of this packet.
    """
    if (not isinstance(value, int)) or (value < 0 or value > 255):
      raise ValueError("Invalid uint8: %s" % value)
    self.bytes.extend(struct.pack('!B', value))

  def put_uint16(self, value):
    """Append a 16-bit integer to the end of this packet.
    """
    if (not isinstance(value, int)) or (value < 0 or value > 65535):
      raise ValueError("Invalid uint16: %s" % value)
    self.bytes.extend(struct.pack('!H', value))

  def put_uint32(self, value):
    """Append a 32-bit integer to the end of this packet.
    """
    if (not isinstance(value, int)) or (value < 0 or value > 2 ** 32 - 1):
      raise ValueError("Invalid uint32: %s" % value)
    self.bytes.extend(struct.pack('!L', value))

  def put_float(self, value):
    """Append a 32-bit float to the end of this packet.
    """
    if not isinstance(value, Number):
      raise ValueError("Invalid float: %s" % value)
    self.bytes.extend(struct.pack('!f', value))

  def get_bytes(self):
    return self.bytes

class PacketUnderrunError(Exception):
  pass

class PacketReader(object):
  """Class for reading data from a network-order (big-endian) byte-oriented
  packet.
  All read operations may throw exceptions in cases like data underrun.
  """
  def __init__(self, data):
    """Create a packet from some data.
    """
    self.bytes = bytearray(data)

  def read_bytes(self, num_bytes):
    if num_bytes > len(self.bytes):
      raise PacketUnderrunError("Requested %i bytes of %i bytes left" % (num_bytes, len(self.bytes)))
    out = self.bytes[:num_bytes]
    self.bytes = self.bytes[num_bytes:]
    return out

  def read_uint8(self):
    """Read a 8-bit integer from the beginning of the packet, then advance the
    packet past the read data.
    """
    return struct.unpack('!B', self.read_bytes(1))[0]

  def read_uint16(self):
    """Read a 16-bit integer from the beginning of the packet, then advance the
    packet past the read data.
    """
    return struct.unpack('!H', self.read_bytes(2))[0]

  def read_uint32(self):
    """Read a 32-bit integer from the beginning of the packet, then advance the
    packet past the read data.
    """
    return struct.unpack('!L', self.read_bytes(4))[0]

  def read_float(self):
    """Read a 32-bit float from the beginning of the packet, then advance the
    packet past the read data.
    """
    return struct.unpack('!f', self.read_bytes(4))[0]

  def empty(self):
    """Returns true if the packet is empty.
    """
    return len(self.bytes) == 0
