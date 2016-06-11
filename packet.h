#ifndef PACKET_H_
#define PACKET_H_

#include <stddef.h>
#include <stdint.h>

#ifndef PACKET_MAX_LEN
const size_t kPacketMaxLen = 512;
#else
const size_t kPacketMaxLen = PACKET_MAX_LEN;
#endif

namespace internal {
  // Stores data of type T into loc in network order, returns the number of
  // bytes written.
  template<typename T> size_t buf_put(uint8_t* loc, size_t max_len, T data);
}

/**
 * Packet builder interface.
 */
class PacketBuilder {
protected:
//  virtual void put_uint8(uint8_t data) = 0;
//  virtual void put_uint16(uint16_t data) = 0;
//  virtual void put_uint32(uint32_t data) = 0;
//  virtual void put_float(float data) = 0;

public:
  // Generic templated write operations.
//  template<typename T> void put(T data);
};

/**
 * Packet reader interface.
 */
class PacketReader {
protected:
  virtual uint8_t read_uint8() = 0;
  virtual uint16_t read_uint16() = 0;
  virtual uint32_t read_uint32() = 0;
  virtual float read_float() = 0;

public:
  // Generic templated write operations.
  template<typename T> T read();
};

/**
 * Packet builder that stores data in a statically allocated buffer.
 */
class BufferedPacketBuilder : PacketBuilder {
public:
  template<typename T> void put(T data) {
   length += internal::buf_put<T>(buffer, kPacketMaxLen - length, data);
   // TODO: overflow detection
 }

  // Returns the number of bytes in the packet and stores the pointer to the
  // beginning of the buffer in out_ptr.
  size_t get_bytes(const uint8_t ** out_ptr) {
    *out_ptr = buffer;
    return length;
  }

private:
  uint8_t buffer[kPacketMaxLen];
  size_t length;
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
  uint8_t read_uint8();
  uint16_t read_uint16();
  uint32_t read_uint32();
  float read_float();

  uint8_t* readPtr;
  uint8_t* endPtr;
};

#endif
