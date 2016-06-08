#ifndef BLPROTO_H_
#define BLPROTO_H_

namespace BLPROTO {
  const uint32_t DELAY_MS_BOOTSCAN = 50;  // delay, in ms, before doing the initial BOOT_IN check to determine master/slave
  const uint32_t DELAY_MS_BOOT_TO_ADDR = 10;

  const uint8_t ADDRESS_GLOBAL = 0x42 << 1;

  const size_t MAX_PAYLOAD_LEN = 512; // TODO: size this more optimally

  // CMD_PING
  // <- RESP_PING
  // Global and addressed mode.
  const uint8_t CMD_PING = 0x02;
  const uint8_t RESP_PING = 0x06;

  // CMD_SET_ADDRESS (uint8 newAddress)
  // Global mode only. Sets the I2C address of the device.
  const uint8_t CMD_SET_ADDRESS = 0x04;

  // CMD_SET_BOOTOUT
  // Sets the boot out pin high.
  const uint8_t CMD_SET_BOOTOUT = 0x03;

  // CMD_ERASE (uint32 startAddress) (uint32 length)
  // Erases a block.
  const uint8_t CMD_ERASE = 0x06;

  // CMD_WRITE_DATA (uint32 startAddress) (uint16 length) (uint16 CRC16)
  //   (length*uint8 data)
  // Writes data of length starting at the specified address.
  // CRC is of the data only.
  const uint8_t CMD_WRITE = 0x07;

  // CMD_RUN (uint32 address)
  // Runs the program beginning at the specified address. Not a simple jump,
  // also sets up the initial PC, stack pointer, and vector table pointer.
  const uint8_t CMD_JUMP = 0x08;

  // CMD_LAST_STATUS
  // <- RESP_STATUS_*
  const uint8_t CMD_STATUS = 0x10;

  enum RESP_STATUS {
    BUSY = 0x00,
    DONE = 0x5A,
    ERR_INVALID_FORMAT = 0x10,
    ERR_INVALID_ARGS,
    ERR_INVALID_CHECKSUM,
    ERR_FLASH,
    ERR_UNKNOWN
  };

  // Given the device number (position in chain), return the mbed-style I2C
  // address.
  constexpr uint8_t DEVICE_ADDR(uint8_t device_num) {
    return (device_num + 1) << 1;
  }
}

#endif
