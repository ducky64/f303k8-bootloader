#ifndef PACKET_H_
#define PACKET_H_

#include <stddef.h>
#include <stdint.h>

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
template <size_t size>
class BufferedPacketBuilder : PacketBuilder {
public:
  BufferedPacketBuilder() : writePtr(buffer), endPtr(buffer+size) {
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

  uint8_t buffer[size];
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
