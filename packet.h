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
class MemoryPacketBuilder : public PacketBuilder {
public:
  MemoryPacketBuilder(uint8_t* writePtr, size_t length) :
    writePtr(writePtr), endPtr(writePtr+length) {
  }

protected:
  bool put_uint8(uint8_t data);
  bool put_uint16(uint16_t data);
  bool put_uint32(uint32_t data);
  bool put_float(float data);

  size_t getFreeBytes() {
    return endPtr - writePtr;
  }

  uint8_t* writePtr;
  uint8_t* endPtr;
};

template <size_t size>
class BufferedPacketBuilder : public MemoryPacketBuilder {
public:
  BufferedPacketBuilder() : MemoryPacketBuilder(buffer, size) {
  }

  // Returns the pointer to the beginning of the buffer.
  const uint8_t * getBuffer() const {
    return buffer;
  }

  // Returns the current size of the packet, in bytes.
  size_t getLength() const {
    return writePtr - buffer;
  }

  // Resets this, like when a new packet is to be written.
  void reset() {
    writePtr = buffer;
    endPtr = buffer + size;
  }

protected:
  uint8_t buffer[size];
};

class MemoryPacketReader : public PacketReader {
public:
  MemoryPacketReader(uint8_t* readPtr, size_t length) :
    readPtr(readPtr), endPtr(readPtr + length) {
  }

  size_t getRemainingBytes() const {
    return endPtr - readPtr;
  }

  // Returns a pointer into the buffer and advance the read pointer by numBytes.
  // Valid as long as this object is valid and has not been reset.
  // Returns NULL if there are not numBytes left.
  uint8_t* read_buf(size_t numBytes) {
    if (readPtr > endPtr || (uint32_t)(endPtr - readPtr) < numBytes) {
      return NULL;
    }
    uint8_t* rtn = readPtr;
    readPtr += numBytes;
    return rtn;
  }

protected:
  bool read_uint8(uint8_t* out);
  bool read_uint16(uint16_t* out);
  bool read_uint32(uint32_t* out);
  bool read_float(float* out);

  uint8_t* readPtr;
  uint8_t* endPtr;
};


class BufferedPacketReaderInterface : public MemoryPacketReader {
public:
  BufferedPacketReaderInterface(uint8_t *buffer) : MemoryPacketReader(buffer, 0) {
  }
  virtual void reset() = 0;
  virtual bool putByte(uint8_t byte) = 0;
};

template <size_t size>
class BufferedPacketReader : public BufferedPacketReaderInterface {
public:
  BufferedPacketReader() : BufferedPacketReaderInterface(buffer) {
  }

  /**
   * Resets this to be ready for a new incoming packet.
   */
  void reset() {
    readPtr = buffer;
    endPtr = buffer;
  }

  /**
   * Write a new byte to the end of this packet. Returns true if successful,
   * false if not (like in buffer overflow).
   */
  bool putByte(uint8_t byte) {
    if (endPtr >= buffer + size) {
      return false;
    }
    *endPtr = byte;
    endPtr++;
    return true;
  }

  /**
   * Returns a pointer into the end of the buffer for another function to stuff
   * data into, and advance the end of the buffer by numBytes.
   *
   * If there are less than numBytes in the buffer, returns NULL.
   */
  uint8_t* ptrPutBytes(size_t numBytes) {
    if (endPtr + numBytes >= buffer + size) {
      return NULL;
    }
    uint8_t* rtn = endPtr;
    endPtr += numBytes;
    return rtn;
  }

protected:
  uint8_t buffer[size];
};

#endif
