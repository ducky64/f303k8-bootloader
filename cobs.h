#ifndef COBS_H_
#define COBS_H_

#include "packet.h"

class COBSDecoder;

class COBSPacketReader : public BufferedPacketReader {
  friend COBSDecoder;
public:
  COBSPacketReader() : BufferedPacketReader(NULL, 0), writePtr(buffer) {}

  /**
   * Resets this to be ready for a new incoming packet.
   */
  void reset() {
    writePtr = buffer;
    length = 0;
  }

protected:
  /**
   * Call when finished writing to this packet, so it's ready for reading.
   */
  void finish() {
    length = writePtr - buffer;
    readPtr = buffer;
    writePtr = NULL;
  }

  /**
   * Write a new byte to the end of this packet. Returns true if successful,
   * false if not (like in buffer overflow).
   */
  bool putByte(uint8_t byte) {
    if (writePtr >= buffer + kPacketMaxLen) {
      return false;
    }
    *writePtr = byte;
    writePtr++;
    return true;
  }

  uint8_t buffer[kPacketMaxLen];
  uint8_t* writePtr;
};

class COBSDecoder {
public:
  COBSDecoder() :
    decoderStatus(kDecodeError),
    nextSpecial(0), insertZeroAtNextSpecial(false) {
  }

  /**
   * Sets the current buffer (a COBSPacketReader object) to write to.
   */
  void set_buffer(COBSPacketReader* reader) {
    currReader = reader;
  }

  enum COBSResult {
    kResultWorking,  // assembling a packet
    kResultDone,  // a new packet is ready
    kErrorOverflow,  // packet discarded because of length overflow
    kErrorInvalidFormat,  // packet discarded because of invalid format
    kErrorNoBuffer,  // no buffer assigned
  };

  /**
   * Decodes a chunk of data from the input stream, returning the decoder
   * status.
   *
   * If a full packet is decoded (kResultDone), the assigned COBSPacketReader is
   * valid for reading. A new COBSPacketReader must be assigned before the next decode
   * operation.
   *
   * Attempting to call this with an unassigned COBSPacketReader will do nothing
   * and read no data.
   */
  COBSResult decode(uint8_t* chunk, size_t length, size_t *read_out);

protected:
  COBSPacketReader* currReader;  // current buffer, can be NULL if none assigned

  enum DecoderStatus {
    kDecodeBegin,  // last byte was a flag byte
    kDecodeNormal,  // currently reading a packet
    kDecodeError,  // current packet in error, discard remaining bytes
  };

  DecoderStatus decoderStatus;
  size_t nextSpecial;  // number of bytes to the next special byte
  bool insertZeroAtNextSpecial;  // whether the next special byte represents a
                                 // modified zero
};

#endif
