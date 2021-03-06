#ifndef ISP_H_
#define ISP_H_

class ISPBase {
public:
  enum ISPStatus {
    kISPOk = 0x00,
    kISPInvalidArgs = 0x10,
    kISPFlashError
  };

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
   * Returns the address of the first byte in flash.
   * TODO: define meaning for multibank / non-continuous flash address.
   */
  virtual size_t get_flash_addr_start() = 0;

  /**
   * Returns the address of the last byte in flash.
   * TODO: define meaning for multibank / non-continuous flash address.
   */
  virtual size_t get_flash_addr_end() = 0;

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
  ISPStatus erase(void* start_addr, size_t length) {
    async_erase(start_addr, length);
    ISPStatus status;
    while (!get_last_async_status(&status)) {
      async_update();
    }
    return status;
  }


  /**
   * Writes the data to the specified start address.
   *
   * length must be a multiple of get_write_size().
   *
   * Returns zero when successful, otherwise an error code.
   */
  ISPStatus write(void* start_addr, void* data, size_t length) {
    async_write(start_addr, data, length);
    ISPStatus status;
    while (!get_last_async_status(&status)) {
      async_update();
    }
    return status;
  }

  /**
   * During an async operation, this must be called periodically.
   *
   * Return true if currently busy.
   */
  virtual bool async_update() = 0;

  /**
   * Returns true if the last async operation has finished, false if it is still
   * ongoing.
   *
   * If the operation has finished, the output status code will be in statusOut.
   */
  virtual bool get_last_async_status(ISPStatus* statusOut) = 0;

  /**
   * Asynchronous versions of the above blocking operations. Returns true if the
   * operation was started (and a valid result will be on get_last_async_status,
   * false if not (like if an operation was pending).
   *
   * Data buffers must not be modified until the operation is complete!
   */
  virtual bool async_erase(void* start_addr, size_t length) = 0;
  virtual bool async_write(void* start_addr, void* data, size_t length) = 0;
};

#include "isp_stm32f303k8.h"
#include "isp_stm32l432kc.h"

#endif
