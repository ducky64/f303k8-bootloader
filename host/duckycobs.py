"""
Consistent Overhead Byte Stuffing algorithm
0x00 is used as the start of frame flag
Empty frames are dropped, so 0x00 can also mark end-of-frame is not sending
frames back-to-back
The first byte marks special data.
Special data in the range 0x01 - 0xfe is used to denote the number of bytes
(inclusive of this one) until the next zero. The zero is replaced with another
special data byte, indicating the number of bytes until the next zero.
(so 0x00, 0x01, 0x01, 0x02, 0xaa, 0x00 -> 0x00, 0x00, 0xaa)
As a checking measure, the last one should "point" to an end of frame / next
start-of-frame byte, otherwise it is a decode error.
0xff means that a special data byte will be in 0xfe bytes, but do not insert a
zero at that point. This is used to allow long runs of non-zero bytes.
"""

def cobs_encode(in_bytearray):
  """
  Encodes a bytearray into DuckyCOBS. Does not include the start-of-frame or
  end-of-frame bytes.
  """
  out = bytearray()

  if len(in_bytearray) == 0:
    return b'\x01'

  while len(in_bytearray) > 0:
    next_zero = in_bytearray.find(b'\x00')
    if next_zero < 0:
      chunk_length = len(in_bytearray)
    else:
      chunk_length = next_zero

    while chunk_length > 253:
      out.append(255)
      out.extend(in_bytearray[:253])
      in_bytearray = in_bytearray[253:]
      chunk_length -= 253

    out.append(chunk_length + 1)
    if chunk_length > 0:
      out.extend(in_bytearray[:chunk_length])

    in_bytearray = in_bytearray[chunk_length + 1:]

    if len(in_bytearray) == 0 and next_zero >= 0:
      # if last byte is zero, add it here since there won't be any more iterations
      out.append(1)

  return out

def cobs_decode(in_bytearray):
  """
  Decodes a DuckyCOBS bytearray. The input should not include the
  start-of-frame or end-of-frame bytes, it is an error (assertion) if a zero
  occurs in the sequence.
  Returns None if an invalid packet was detected.
  """
  assert in_bytearray.find(b'\x00') < 0

  out = bytearray()

  while len(in_bytearray) > 0:
    special_byte = in_bytearray[0]
    in_bytearray = in_bytearray[1:]

    if special_byte >= 255:
      chunk_length = 253
      # also invalid to end on 0xff
      if chunk_length >= len(in_bytearray):
        return None

      out.extend(in_bytearray[:chunk_length])
      in_bytearray = in_bytearray[chunk_length:]
    else:
      chunk_length = special_byte - 1
      if chunk_length > len(in_bytearray):
        return None
      out.extend(in_bytearray[:chunk_length])

      # don't append a zero if this points past the end of the input
      if chunk_length < len(in_bytearray):
        out.append(0)

      in_bytearray = in_bytearray[chunk_length:]

  return out
