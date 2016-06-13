#ifndef COBS_H_
#define COBS_H_

#include "packet.h"

class COBSDecoder {
public:
  COBSDecoder() :
    decoderStatus(kDecodeError),
    nextSpecial(0), insertZeroAtNextSpecial(false) {
  }

  /**
   * Sets the current buffer (a COBSPacketReader object) to write to.
   */
  void set_buffer(BufferedPacketReaderInterface* reader) {
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
  BufferedPacketReaderInterface* currReader;  // current buffer, can be NULL if none assigned

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
