#include "packet.h"

namespace internal {
  template<> size_t buf_put<uint8_t>(uint8_t* loc, size_t max_len, uint8_t data) {
    if (max_len < 1) {
      return 0;
    }
    *loc = data;
    return 1;
  }

  template<> size_t buf_put<uint16_t>(uint8_t* loc, size_t max_len, uint16_t data) {
    if (max_len < 2) {
      return 0;
    }
    *(loc + 0) = (data >> 8) & 0xff;
    *(loc + 1) = (data >> 0) & 0xff;
    return 2;
  }

  template<> size_t buf_put<uint32_t>(uint8_t* loc, size_t max_len, uint32_t data) {
    if (max_len < 4) {
      return 0;
    }
    *(loc + 0) = (data >> 24) & 0xff;
    *(loc + 1) = (data >> 16) & 0xff;
    *(loc + 2) = (data >> 8) & 0xff;
    *(loc + 3) = (data >> 0) & 0xff;
    return 4;
  }

  template<> size_t buf_put<float>(uint8_t* loc, size_t max_len, float data) {
    if (max_len < 4) {
      return 0;
    }
    // TODO: check if this is platform dependent, perhaps abstract elsewhere
    uint8_t *float_array = reinterpret_cast<uint8_t*>(&data);
    *(loc + 0) = float_array[3];
    *(loc + 1) = float_array[2];
    *(loc + 2) = float_array[1];
    *(loc + 3) = float_array[0];
    return 4;
  }
}

template<> uint8_t PacketReader::read<uint8_t>() {
  return read_uint8();
}

template<> uint16_t PacketReader::read<uint16_t>() {
  return read_uint16();
}

template<> uint32_t PacketReader::read<uint32_t>() {
  return read_uint32();
}

template<> float PacketReader::read<float>() {
  return read_float();
}

uint8_t BufferedPacketReader::read_uint8() {
  if (getRemainingBytes() < 1) {
    return 0;
  }
  readPtr += 1;
  return readPtr[-1];
}
uint16_t BufferedPacketReader::read_uint16() {
  if (getRemainingBytes() < 2) {
    return 0;
  }
  return ((uint16_t)readPtr[-2] << 8)
       | ((uint16_t)readPtr[-1] << 0);
}
uint32_t BufferedPacketReader::read_uint32() {
  if (getRemainingBytes() < 4) {
    return 0;
  }
  readPtr += 4;
  return ((uint16_t)readPtr[-4] << 24)
       | ((uint16_t)readPtr[-3] << 16)
       | ((uint16_t)readPtr[-2] << 8)
       | ((uint16_t)readPtr[-1] << 0);
}

float BufferedPacketReader::read_float() {
  if (getRemainingBytes() < 4) {
    return 0;
  }
  float data_out;
  uint8_t* data_out_bytes = reinterpret_cast<uint8_t*>(&data_out);
  // TODO: check if this is platform dependent, perhaps abstract elsewhere
  data_out_bytes[0] = readPtr[3];
  data_out_bytes[1] = readPtr[2];
  data_out_bytes[2] = readPtr[1];
  data_out_bytes[3] = readPtr[0];
  readPtr += 4;

  return data_out;
}

