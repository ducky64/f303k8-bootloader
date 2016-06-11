#ifndef PACKET_H_
#define PACKET_H_

#include <stddef.h>
#include <stdint.h>

#ifndef PACKET_MAX_LEN
const size_t kPacketMaxLen = 512;
#else
const size_t kPacketMaxLen = PACKET_MAX_LEN;
#endif

/**
 * Packet builder interface.
 */
class PacketBuilder {
protected:
  virtual bool put_uint8(uint8_t data) = 0;
  virtual bool put_uint16(uint16_t data) = 0;
  virtual bool put_uint32(uint32_t data) = 0;
  virtual bool put_float(float data) = 0;

public:
  // Generic templated write operations.
  template<typename T> bool put(T data);
};

/**
 * Packet reader interface.
 */
class PacketReader {
protected:
  virtual bool read_uint8(uint8_t* out) = 0;
  virtual bool read_uint16(uint16_t* out) = 0;
  virtual bool read_uint32(uint32_t* out) = 0;
  virtual bool read_float(float* out) = 0;

public:
  // Generic templated read operations.
  // TODO: return error on underflow
  template<typename T> T read();
};

/**
 * Packet builder that stores data in a statically allocated buffer.
 */
class BufferedPacketBuilder : PacketBuilder {
public:
  BufferedPacketBuilder() : writePtr(buffer), endPtr(buffer+kPacketMaxLen) {
  }

  // Returns the number of bytes in the packet and stores the pointer to the
  // beginning of the buffer in out_ptr.
  size_t get_bytes(const uint8_t ** out_ptr) {
    *out_ptr = buffer;
    return writePtr - buffer;
  }

protected:
  bool put_uint8(uint8_t data) = 0;
  bool put_uint16(uint16_t data) = 0;
  bool put_uint32(uint32_t data) = 0;
  bool put_float(float data) = 0;

  size_t getFreeBytes() {
    return endPtr - writePtr;
  }

  uint8_t buffer[kPacketMaxLen];
  uint8_t* writePtr;
  uint8_t* endPtr;
};

class BufferedPacketReader : public PacketReader {
public:
  BufferedPacketReader(uint8_t* readPtr, size_t length) :
    readPtr(readPtr), endPtr(readPtr + length) {
  }

  size_t getRemainingBytes() {
    return endPtr - readPtr;
  }

protected:
  bool read_uint8(uint8_t* out);
  bool read_uint16(uint16_t* out);
  bool read_uint32(uint32_t* out);
  bool read_float(float* out);

  uint8_t* readPtr;
  uint8_t* endPtr;
};

#endif
