#ifndef ISP_H_
#define ISP_H_

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

#endif