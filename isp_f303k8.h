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
  F303K8ISP() : async_op(OP_NONE) {
  }

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
    async_erase(start_addr, length);
    uint8_t status;
    while (!get_last_async_status(&status)) {
      async_update();
    }
    return status;
  }

  uint8_t write(void* start_addr, void* data, size_t length) {
    async_write(start_addr, data, length);
    uint8_t status;
    while (!get_last_async_status(&status)) {
      async_update();
    }
    return status;
  }

  bool async_update() {
    if (async_op == OP_NONE) {
      return false;
    }
    if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY)) {
      return true;
    }
    if (async_op == OP_ERASE) {
      if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_WRPERR) || __HAL_FLASH_GET_FLAG(FLASH_FLAG_PGERR)) {
        CLEAR_BIT(FLASH->CR, FLASH_CR_PER);
        async_op = OP_NONE;
        async_status = 1;
        return false;
      }

      if (async_length_remaining > 0) {
        FLASH_PageErase(async_addr_current);

        async_addr_current += ERASE_SIZE;
        async_length_remaining -= ERASE_SIZE;
        return true;
      } else {
        CLEAR_BIT(FLASH->CR, FLASH_CR_PER);
        async_op = OP_NONE;
        async_status = 0;
        return false;
      }
    } else if (async_op == OP_WRITE) {
      if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_WRPERR) || __HAL_FLASH_GET_FLAG(FLASH_FLAG_PGERR)) {
        CLEAR_BIT(FLASH->CR, FLASH_CR_PG);
        async_op = OP_NONE;
        async_status = 1;
        return false;
      }

      if (async_length_remaining > 0) {
        if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_EOP)) {
          __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
        }
        CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

        uint16_t halfword = ((*(uint8_t*)(async_data_ptr+0)) << 0)
            + ((*(uint8_t*)(async_data_ptr+1)) << 8);

        FLASH_Program_HalfWord(async_addr_current, halfword);

        async_addr_current += WRITE_SIZE;
        async_data_ptr += WRITE_SIZE;
        async_length_remaining -= WRITE_SIZE;
        return true;
      } else {
        CLEAR_BIT(FLASH->CR, FLASH_CR_PG);
        async_op = OP_NONE;
        async_status = 0;
        return false;
      }
    }
  }

  virtual bool get_last_async_status(uint8_t* statusOut) {
    *statusOut = async_status;
    return async_op == OP_NONE;
  }

  virtual void async_erase(void* start_addr, size_t length) {
    if (length % ERASE_SIZE != 0) {
      async_status = 255;
      return;
    }
    size_t addr = (uint32_t)start_addr;
    if (addr % ERASE_SIZE != 0) {
      async_status = 254;
      return;
    }
    if (!(addr >= FLASH_START && addr <= FLASH_END)) {
      async_status = 253;

      return;
    }
    if (addr + length > FLASH_END + 1) {
      async_status = 252;
      return;
    }

    async_op = OP_ERASE;
    async_addr_current = (uint32_t)start_addr;
    async_length_remaining = length;

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR);
    async_update();
  }


  virtual void async_write(void* start_addr, void* data, size_t length) {
    if (length % WRITE_SIZE != 0) {
      async_status = 255;
      return;
    }
    size_t addr = (uint32_t)start_addr;
    if (addr % WRITE_SIZE != 0) {
      async_status = 254;
      return;
    }
    if (!(addr >= FLASH_START && addr <= FLASH_END)) {
      async_status = 253;
      return;
    }

    async_op = OP_WRITE;
    async_addr_current = (uint32_t)start_addr;
    async_data_ptr = (uint8_t*)data;
    async_length_remaining = length;

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR);
    async_update();
  }

private:
  enum AsyncOp {OP_NONE, OP_ERASE, OP_WRITE};
  AsyncOp async_op;
  uint32_t async_addr_current;
  uint8_t* async_data_ptr;
  size_t async_length_remaining;
  uint8_t async_status;

  // for some reason, this isn't exposed in stm32f3xx_hal_flash.h
  static void FLASH_Program_HalfWord(uint32_t Address, uint16_t Data)
  {
    /* Clear pending flags (if any) */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR);

    /* Proceed to program the new data */
    SET_BIT(FLASH->CR, FLASH_CR_PG);

    *(__IO uint16_t*)Address = Data;
  }
};

#endif
