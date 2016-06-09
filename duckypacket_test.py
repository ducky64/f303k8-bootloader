import unittest

from duckypacket import *

class TestPacketization(unittest.TestCase):
  def test_encode(self):
    packet = PacketBuilder()
    packet.put_uint8(0x42)
    self.assertEquals(b'\x42', packet.get_bytes())

    packet = PacketBuilder()
    packet.put_uint16(0x4200)
    self.assertEquals(b'\x42\x00', packet.get_bytes())

    packet = PacketBuilder()
    packet.put_uint32(0xdeadbeef)
    self.assertEquals(b'\xde\xad\xbe\xef', packet.get_bytes())

    packet = PacketBuilder()
    packet.put_float(0.0)
    self.assertEquals(b'\x00\x00\x00\x00', packet.get_bytes())

    packet = PacketBuilder()
    packet.put_uint8(0x42)
    packet.put_uint8(0xff)
    packet.put_uint32(0xdeadbeef)
    self.assertEquals(b'\x42\xff\xde\xad\xbe\xef', packet.get_bytes())

  def test_encode(self):
    packet = PacketReader(b'\x42')
    self.assertEquals(0x42, packet.read_uint8())
    self.assertTrue(packet.empty())

    packet = PacketReader(b'\x42\x00')
    self.assertEquals(0x4200, packet.read_uint16())
    self.assertTrue(packet.empty())

    packet = PacketReader(b'\xde\xad\xbe\xef')
    self.assertEquals(0xdeadbeef, packet.read_uint32())
    self.assertTrue(packet.empty())

    packet = PacketReader(b'\x00\x00\x00\x00')
    self.assertEquals(0.0, packet.read_float())
    self.assertTrue(packet.empty())

    packet = PacketReader(b'\x42\xff\xde\xad\xbe\xef')
    self.assertEquals(0x42, packet.read_uint8())
    self.assertEquals(0xff, packet.read_uint8())
    self.assertEquals(0xdeadbeef, packet.read_uint32())
    self.assertTrue(packet.empty())
