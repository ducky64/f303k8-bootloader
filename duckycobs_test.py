import unittest

from duckycobs import *

class TestCOBSEncode(unittest.TestCase):
  def check_encode_decode(self, in_bytearray):
    encoded = cobs_encode(in_bytearray)
    self.assertEqual(in_bytearray, cobs_decode(encoded),
                     "failed: %s -> %s" % (in_bytearray, encoded))

  def test_manual(self):
    self.assertEquals(b'\x01', cobs_encode(b''))
    self.assertEquals(b'\x01\x01', cobs_encode(b'\x00'))
    self.assertEquals(b'\x01\x02\xff\x02\xff', cobs_encode(b'\x00\xff\x00\xff'))
    self.assertEquals(b'\x02\xff\x02\xff\x01', cobs_encode(b'\xff\x00\xff\x00'))
    self.assertEquals(b'\xfd' + b'\xff'*252, cobs_encode(b'\xff'*252))
    self.assertEquals(b'\xfe' + b'\xff'*253, cobs_encode(b'\xff'*253))
    self.assertEquals(b'\xff' + b'\xff'*253 + b'\x02\xff', cobs_encode(b'\xff'*254))

  def test_basic(self):
    self.check_encode_decode(b'')
    self.check_encode_decode(b'\x00')
    self.check_encode_decode(b'\x01')
    self.check_encode_decode(b'\xff')
    self.check_encode_decode(b'\x00\x00')
    self.check_encode_decode(b'\x00\x01\x00\x02')
    self.check_encode_decode(b'\x00\xff\x00\xff')
    self.check_encode_decode(b'\xff\x00\xff\x00')

  def test_longruns(self):
    self.check_encode_decode(b'\xff'*252)
    self.check_encode_decode(b'\xff'*253)
    self.check_encode_decode(b'\xff'*254)
    self.check_encode_decode(b'\x00'*512)
    self.check_encode_decode(b'\xff'*512)
