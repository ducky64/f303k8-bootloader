#ifndef ENDIAN_H_
#define ENDIAN_H_

/**
 * Serialize a 32-bit integer to network order (big-endian) in a array of bytes.
 */
void serialize_uint32(uint8_t* addr, uint32_t data) {
  addr[3] = data & 0xff;
  data = data >> 8;
  addr[2] = data & 0xff;
  data = data >> 8;
  addr[1] = data & 0xff;
  data = data >> 8;
  addr[0] = data & 0xff;
}

/**
 * Deserialize a 32-bit integer from an array of bytes in network order (big-endian).
 */
uint32_t deserialize_uint32(uint8_t* addr) {
  uint32_t data = 0;
  data |= addr[0];
  data = data << 8;
  data |= addr[1];
  data = data << 8;
  data |= addr[2];
  data = data << 8;
  data |= addr[3];
  return data;
}

/**
 * Serialize a 16-bit integer to network order (big-endian) in a array of bytes.
 */
void serialize_uint16(uint8_t* addr, uint16_t data) {
  addr[1] = data & 0xff;
  data = data >> 8;
  addr[0] = data & 0xff;
}

/**
 * Deserialize a 16-bit integer from an array of bytes in network order (big-endian).
 */
uint16_t deserialize_uint16(uint8_t* addr) {
  uint16_t data = 0;
  data |= addr[0];
  data = data << 8;
  data |= addr[1];
  return data;
}

#endif
