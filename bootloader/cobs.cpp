#include "cobs.h"

COBSDecoder::COBSResult COBSDecoder::decode(uint8_t* chunk, size_t length, size_t *read_out) {
  *read_out = 0;
  if (currReader == NULL) {
    return kErrorNoBuffer;
  }

  while (length > 0) {
    uint8_t byte = *chunk;
    (*read_out) += 1;

    if (byte == 0x00) {
      if (decoderStatus == kDecodeNormal) {
        nextSpecial -= 1;
        if (nextSpecial == 0 && insertZeroAtNextSpecial) {
          decoderStatus = kDecodeBegin;
          return kResultDone;
        } else {
          currReader->reset();

          decoderStatus = kDecodeBegin;
          return kErrorInvalidFormat;
        }
      } else {
        decoderStatus = kDecodeBegin;
      }
    } else {
      if (decoderStatus == kDecodeBegin) {
        if (byte == 0xff) {
          nextSpecial = byte - 1;
          insertZeroAtNextSpecial = false;
        } else {
          nextSpecial = byte;
          insertZeroAtNextSpecial = true;
        }
        decoderStatus = kDecodeNormal;
      } else if (decoderStatus == kDecodeNormal) {
        nextSpecial -= 1;
        if (nextSpecial == 0) {
          if (insertZeroAtNextSpecial) {
            if (!currReader->putByte(0)) {
              decoderStatus = kDecodeError;
              return kErrorOverflow;
            }
          }
          if (byte == 0xff) {
            nextSpecial = byte - 1;
            insertZeroAtNextSpecial = false;
          } else {
            nextSpecial = byte;
            insertZeroAtNextSpecial = true;
          }
        } else {
          if (!currReader->putByte(byte)) {
            decoderStatus = kDecodeError;
            return kErrorOverflow;
          }
        }
      } else {
        // Drop bytes otherwise
      }
    }

    chunk += 1;
    length -= 1;
  }
  return kResultWorking;
}
