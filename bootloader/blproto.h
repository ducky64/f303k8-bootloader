#ifndef BLPROTO_H_
#define BLPROTO_H_

namespace BootProto {
  const uint32_t kBootscanDelayMs = 100;  // delay, in ms, before doing the initial BOOT_IN check to determine master/slave
  const uint32_t kBootToAddrDelayMs = 10;

  const uint8_t kAddressGlobal = 0x42 << 1;

  const size_t kMaxPayloadLength = 512; // TODO: size this more optimally

  enum BootCommand {
    // kCmdStatus
    // <- RespStatus
    kCmdStatus = 0x08,

    // kCmdSetAddress (uint8 newAddress)
    // Global mode only. Sets the I2C address of the device.
    kCmdSetAddress,

    // kCmdSetBootOut
    // Sets the boot out pin high.
    kCmdSetBootOut = 0x10,

    // kCmdErase (uint32 startAddress) (uint32 length)
    // Erases a block.
    kCmdErase,

    // kCmdWrite (uint32 startAddress) (uint16 length) (uint16 CRC16)
    //   (length*uint8 data)
    // Writes data of length starting at the specified address.
    // CRC is of the data only.
    kCmdWrite,

    // kCmdRunApp (uint32 address)
    // Runs the program beginning at the specified address. Not a simple jump,
    // also sets up the initial PC, stack pointer, and vector table pointer.
    kCmdRunApp,

    kCmdInvalid
  };

  enum RespStatus {
    kRespBusy = 0x00,
    kRespInvalidFormat = 0x10,
    kRespInvalidArgs,
    kRespInvalidChecksum,
    kRespFlashError,
    kRespUnknownError,
    kRespDone = 0x5A
  };

  // Given the device number (position in chain), return the mbed-style I2C
  // address.
  uint8_t GetDeviceAddr(uint8_t device_num) {
    return (device_num + 1) << 1;
  }
}

#endif
