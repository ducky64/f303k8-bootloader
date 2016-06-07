#include "isp.h"

#ifndef ISP_F303K8_H_
#define ISP_F303K8_H_

class F303K8ISP : public ISPBase {
protected:
  const size_t FLASH_START = 0x8000000;
  const size_t FLASH_END = 0x800ffff;
  const size_t ERASE_SIZE = 2048;
  const size_t WRITE_SIZE = 2;

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
    if (addr + length > FLASH_END + 1) {
      return 252;
    }

    size_t page = addr - FLASH_START;
    size_t page_end = page + length;
    while (page < page_end) {
      FLASH_PageErase(page);

      HAL_StatusTypeDef status = FLASH_WaitForLastOperation(HAL_MAX_DELAY);
      static_assert(HAL_OK == 0, "HAL_OK must be zero");
      if (status != HAL_OK) {
        CLEAR_BIT(FLASH->CR, FLASH_CR_PER);
        return status;
      }

      page += ERASE_SIZE;
    }
    CLEAR_BIT(FLASH->CR, FLASH_CR_PER);
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

    size_t addr_end = addr + length;
    while (addr < addr_end) {
      uint16_t halfword = ((*(uint8_t*)(data+0)) << 0) + ((*(uint8_t*)(data+1)) << 8);
      HAL_StatusTypeDef status = HAL_FLASH_Program(TYPEPROGRAM_HALFWORD, addr, halfword);

      if (status != HAL_OK) {
        return status;
      }

      addr += WRITE_SIZE;
      data += WRITE_SIZE;
    }
    return 0;
  }
};

#endif
