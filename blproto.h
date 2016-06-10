#ifndef BLPROTO_H_
#define BLPROTO_H_

namespace BootProto {
  const uint32_t kBootscanDelayMs = 50;  // delay, in ms, before doing the initial BOOT_IN check to determine master/slave
  const uint32_t kBootToAddrDelayMs = 10;

  const uint8_t kAddressGlobal = 0x42 << 1;

  const size_t kMaxPayloadLength = 512; // TODO: size this more optimally

  // kCmdPing
  // <- kRespDone
  // Global and addressed mode.
  const uint8_t kCmdPing = 0x02;

  // kCmdSetAddress (uint8 newAddress)
  // Global mode only. Sets the I2C address of the device.
  const uint8_t kCmdSetAddress = 0x04;

  // kCmdSetBootOut
  // Sets the boot out pin high.
  const uint8_t kCmdSetBootOut = 0x03;

  // kCmdErase (uint32 startAddress) (uint32 length)
  // Erases a block.
  const uint8_t kCmdErase = 0x06;

  // kCmdWrite (uint32 startAddress) (uint16 length) (uint16 CRC16)
  //   (length*uint8 data)
  // Writes data of length starting at the specified address.
  // CRC is of the data only.
  const uint8_t kCmdWrite = 0x07;

  // kCmdRunApp (uint32 address)
  // Runs the program beginning at the specified address. Not a simple jump,
  // also sets up the initial PC, stack pointer, and vector table pointer.
  const uint8_t kCmdRunApp = 0x08;

  // kCmdStatus
  // <- RespStatus
  const uint8_t kCmdStatus = 0x10;

  enum RespStatus {
    kRespBusy = 0x00,
    kRespDone = 0x5A,
    kRespInvalidFormat = 0x10,
    kRespInvalidArgs,
    kRespInvalidChecksum,
    kRespFlashError,
    kRespUnknownError
  };

  // Given the device number (position in chain), return the mbed-style I2C
  // address.
  uint8_t GetDeviceAddr(uint8_t device_num) {
    return (device_num + 1) << 1;
  }
}

#endif
