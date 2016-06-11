#include "packet.h"

template<> bool PacketBuilder::put<uint8_t>(uint8_t data) {
  return put_uint8(data);
}

template<> bool PacketBuilder::put<uint16_t>(uint16_t data) {
  return put_uint16(data);
}

template<> bool PacketBuilder::put<uint32_t>(uint32_t data) {
  return put_uint32(data);
}

template<> bool PacketBuilder::put<float>(float data) {
  return put_float(data);
}

bool BufferedPacketBuilder::put_uint8(uint8_t data) {
  if (getFreeBytes() < 1) {
    return false;
  }
  *writePtr = data;
  writePtr += 1;
  return true;
}

bool BufferedPacketBuilder::put_uint16(uint16_t data) {
  if (getFreeBytes() < 2) {
    return false;
  }
  writePtr[0] = (data >> 8) & 0xff;
  writePtr[1] = (data >> 0) & 0xff;
  writePtr += 2;
  return true;
}

bool BufferedPacketBuilder::put_uint32(uint32_t data) {
  if (getFreeBytes() < 4) {
    return false;
  }
  writePtr[0] = (data >> 24) & 0xff;
  writePtr[1] = (data >> 16) & 0xff;
  writePtr[2] = (data >> 8) & 0xff;
  writePtr[3] = (data >> 0) & 0xff;
  writePtr += 4;
  return true;
}

bool BufferedPacketBuilder::put_float(float data) {
  if (getFreeBytes() < 4) {
    return false;
  }
  // TODO: check if this is platform dependent, perhaps abstract elsewhere
  uint8_t *float_array = reinterpret_cast<uint8_t*>(&data);
  writePtr[0] = float_array[3];
  writePtr[1] = float_array[2];
  writePtr[2] = float_array[1];
  writePtr[3] = float_array[0];
  writePtr += 4;
  return true;
}

template<> uint8_t PacketReader::read<uint8_t>() {
  uint8_t out;
  read_uint8(&out);
  return out;
}

template<> uint16_t PacketReader::read<uint16_t>() {
  uint16_t out;
  read_uint16(&out);
  return out;
}

template<> uint32_t PacketReader::read<uint32_t>() {
  uint32_t out;
  read_uint32(&out);
  return out;
}

template<> float PacketReader::read<float>() {
  float out;
  read_float(&out);
  return out;
}

bool BufferedPacketReader::read_uint8(uint8_t* out) {
  if (getRemainingBytes() < 1) {
    return false;
  }
  *out = readPtr[0];
  readPtr += 1;
  return true;
}
bool BufferedPacketReader::read_uint16(uint16_t* out) {
  if (getRemainingBytes() < 2) {
    return false;
  }
  *out = ((uint16_t)readPtr[0] << 8)
       | ((uint16_t)readPtr[1] << 0);
  readPtr += 2;
  return true;
}
bool BufferedPacketReader::read_uint32(uint32_t* out) {
  if (getRemainingBytes() < 4) {
    return false;
  }
  *out = ((uint16_t)readPtr[0] << 24)
       | ((uint16_t)readPtr[1] << 16)
       | ((uint16_t)readPtr[2] << 8)
       | ((uint16_t)readPtr[3] << 0);
  readPtr += 4;
  return true;
}

bool BufferedPacketReader::read_float(float* out) {
  if (getRemainingBytes() < 4) {
    return false;
  }
  uint8_t* out_bytes = reinterpret_cast<uint8_t*>(out);
  // TODO: check if this is platform dependent, perhaps abstract elsewhere
  out_bytes[0] = readPtr[3];
  out_bytes[1] = readPtr[2];
  out_bytes[2] = readPtr[1];
  out_bytes[3] = readPtr[0];
  readPtr += 4;

  return true;
}

