/*
 * main.cpp
 *
 *  Created on: Mar 23, 2016
 *      Author: ducky
 */

#include "mbed.h"

DigitalIn sw(D7);

DigitalOut led0(D11);
DigitalOut led1(D12);

Serial uart(SERIAL_TX, SERIAL_RX);

class ISPBase {
public:
  /**
   * Call before beginning ISP operations. Unlocks the flash write.
   */
  virtual bool isp_begin() = 0;

  /**
   * Call at the end of ISP operations. Re-locks the flash write.
   */
  virtual bool isp_end() = 0;

  /**
   * Returns the part's device ID. Meaning is vendor-specific.
   */
  virtual uint32_t get_device_id() = 0;

  /**
   * Returns the low 32 bits of the part's serial number (if it has one) or zero
   * (if it doesn't).
   */
  virtual uint32_t get_device_serial() = 0;

  /**
   * Returns the size, in bytes, of a erase sector. Will return a power of two.
   * erase() will be guaranteed to complete when the size is a multiple of this.
   */
  virtual size_t get_erase_size() = 0;

  /**
   * Returns the size, in bytes, of a write page. Will return a power of two.
   * write() will be guaranteed to complete when the size is a multiple of this.
   */
  virtual size_t get_write_size() = 0;
  /**
   * Erase a page in flash.
   *
   * length must be a multiple of get_erase_size().
   *
   * Returns zero when successful, otherwise an error code.
   */
  virtual uint8_t erase(void* start_addr, size_t length) = 0;

  /**
   * Writes the data to the specified start address.
   *
   * length must be a multiple of get_write_size().
   *
   * Returns zero when successful, otherwise an error code.
   */
  virtual uint8_t write(void* start_addr, void* data, size_t length) = 0;
};

class F303K8ISP : public ISPBase {
protected:
  const size_t FLASH_START = 0x8000000;
  const size_t FLASH_END = 0x800ffff;
  const size_t ERASE_SIZE = 2048;
  const size_t WRITE_SIZE = 2l;

public:
  bool isp_begin() {
    return HAL_FLASH_Unlock() == HAL_OK;
  }

  bool isp_end() {
    return HAL_FLASH_Lock() == HAL_OK;
  }

  uint32_t get_device_id() {
    return DBGMCU->IDCODE;
  }

  uint32_t get_device_serial() {
    return 0;
  }

  size_t get_erase_size() {
    return ERASE_SIZE;
  }

  size_t get_write_size() {
    return WRITE_SIZE;
  }

  uint8_t erase(void* start_addr, size_t length) {
    if (length % ERASE_SIZE != 0) {
      return 255;
    }
    size_t addr = (uint32_t)start_addr;
    if (addr % ERASE_SIZE != 0) {
      return 254;
    }
    if (!(addr >= FLASH_START && addr <= FLASH_END)) {
      return 253;
    }

    size_t page = addr - FLASH_START;
    size_t page_end = page + length;
    while (page < page_end) {
      FLASH_PageErase(page);
      HAL_StatusTypeDef status = FLASH_WaitForLastOperation(HAL_MAX_DELAY);
      static_assert(HAL_OK == 0, "HAL_OK must be zero");
      if (status != HAL_OK) {
        return status;
      }
      page += ERASE_SIZE;
    }
    return 0;
  }

  uint8_t write(void* start_addr, void* data, size_t length) {
    if (length % WRITE_SIZE != 0) {
      return 255;
    }
    size_t addr = (uint32_t)start_addr;
    if (addr % WRITE_SIZE != 0) {
      return 254;
    }
    if (!(addr >= FLASH_START && addr <= FLASH_END)) {
      return 253;
    }

  }
};

extern char _FlashStart, _FlashEnd;
void* const BL_BEGIN_PTR = &_FlashStart;
void* const APP_BEGIN_PTR = &_FlashEnd;

int main() {
  F303K8ISP isp;

  sw.mode(PullUp);

  uart.baud(115200);
  uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ ")\r\n");

  uart.printf("Device ID: 0x% 8x, Device Serial: 0x% 8x\r\n",
      isp.get_device_id(), isp.get_device_serial());
  uart.printf("Bootloader address range: 0x% 8x - 0x% 8x\r\n",
      (uint32_t)BL_BEGIN_PTR, (uint32_t)APP_BEGIN_PTR);
  uart.printf("Bootloader unlock: %i\r\n",
      isp.isp_begin());

  uart.printf("Bootloader erase: %i\r\n",
      isp.erase(BL_BEGIN_PTR + 16384, 16384));

  I2C i2c(D4, D5);

  const size_t RPC_BUFSIZE = 256;
  char rpc_inbuf[RPC_BUFSIZE], rpc_outbuf[RPC_BUFSIZE];
  char* rpc_inptr = rpc_inbuf;  // next received byte pointer

  uint8_t led = 0;

  while (1) {
    while (uart.readable()) {
      char rx = uart.getc();
      if (rx == '\n' || rx == '\r') {
        *rpc_inptr = '\0';  // optionally append the string terminator
        uart.puts(rpc_inbuf);
        uart.puts("\r\n");
        rpc_inptr = rpc_inbuf;  // reset the received byte pointer
      } else {
        *rpc_inptr = rx;
        rpc_inptr++;  // advance the received byte pointer
        if (rpc_inptr >= rpc_inbuf + RPC_BUFSIZE) {
          // you should emit some helpful error on overflow
          rpc_inptr = rpc_inbuf;  // reset the received byte pointer, discarding what we have
        }
      }
    }

    led = !led;
    led0 = led;
    led1 = !led;

    wait(0.25);
  }
  return 0;
}

