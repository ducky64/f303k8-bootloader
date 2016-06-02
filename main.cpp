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
   * erase() will be guaranteed to complete when the size is exactly this.
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
   * length must be equal to get_erase_size().
   *
   * Returns zero when successful, otherwise an error code.
   */
  virtual uint8_t erase(void* start_addr, size_t length) = 0;

  /**
   * Writes the data to the specified start address.
   *
   * length must be equal to get_write_size().
   *
   * Returns zero when successful, otherwise an error code.
   */
  virtual uint8_t wrte(void* start_addr, void* data, size_t length) = 0;
};

class F303K8ISP : public ISPBase {
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
    return 2048;
  }

  size_t get_write_size() {
    return 2;
  }

  uint8_t erase(void* start_addr, size_t length) {
    return 255;
  }

  uint8_t wrte(void* start_addr, void* data, size_t length) {
    return 255;
  }
};

extern char _FlashStart, _FlashEnd;
const void* BL_BEGIN_PTR = &_FlashStart;
const void* APP_BEGIN_PTR = &_FlashEnd;

int main() {
  F303K8ISP isp;

  sw.mode(PullUp);

  Serial uart(SERIAL_TX, SERIAL_RX);
  uart.baud(115200);
  uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ ")\r\n");

  uart.printf("Device ID: 0x% 8x, Device Serial: 0x% 8x\r\n",
      isp.get_device_id(), isp.get_device_serial());
  uart.printf("Bootloader address range: 0x% 8x - 0x% 8x\r\n",
      (uint32_t)BL_BEGIN_PTR, (uint32_t)APP_BEGIN_PTR);

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

