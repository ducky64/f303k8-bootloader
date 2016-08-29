#ifndef ISP_L432KC_H_
#define ISP_L432KC_H_

#ifdef TARGET_NUCLEO_L432KC

// in stm32l4xx_hal_flash_ex.c
extern "C" void FLASH_PageErase(uint32_t Page, uint32_t Banks);

// from stm32l4xx_hal_flash.c
static void FLASH_Program_DoubleWord(uint32_t Address, uint64_t Data)
{
  /* Check the parameters */
  assert_param(IS_FLASH_PROGRAM_ADDRESS(Address));

  /* Set PG bit */
  SET_BIT(FLASH->CR, FLASH_CR_PG);

  /* Program the double word */
  *(__IO uint32_t*)Address = (uint32_t)Data;
  *(__IO uint32_t*)(Address+4) = (uint32_t)(Data >> 32);
}


class ISP : public ISPBase {
private:
  const static size_t kFlashStartAddr = 0x8000000;
  const static size_t kFlashEndAddr = 0x803ffff;
  const static size_t kEraseSize = 2048;
  const static size_t kWriteSize = 8;

public:
  ISP() : async_op(OP_NONE) {
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

  size_t get_flash_addr_start() {
    return kFlashStartAddr;
  }

  size_t get_flash_addr_end() {
    return kFlashEndAddr;
  }

  size_t get_erase_size() {
    return kEraseSize;
  }

  size_t get_write_size() {
    return kWriteSize;
  }

  ISPStatus erase(void* start_addr, size_t length) {
    async_erase(start_addr, length);
    ISPStatus status;
    while (!get_last_async_status(&status)) {
      async_update();
    }
    return status;
  }

  ISPStatus write(void* start_addr, void* data, size_t length) {
    async_write(start_addr, data, length);
    ISPStatus status;
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
    if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_EOP)) {
      __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
    }
    if (async_op == OP_ERASE) {
      if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_ALL_ERRORS)) {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        CLEAR_BIT(FLASH->CR, FLASH_CR_PER);
        async_op = OP_NONE;
        async_status = kISPFlashError;
        return false;
      }

      if (async_length_remaining > 0) {
        FLASH_PageErase((async_addr_current - kFlashStartAddr) / kEraseSize, FLASH_BANK_1);

        async_addr_current += kEraseSize;
        async_length_remaining -= kEraseSize;
        return true;
      } else {
        CLEAR_BIT(FLASH->CR, FLASH_CR_PER);
        async_op = OP_NONE;
        async_status = kISPOk;
        return false;
      }
    } else if (async_op == OP_WRITE) {
      if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_ALL_ERRORS)) {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        CLEAR_BIT(FLASH->CR, FLASH_CR_PG);
        async_op = OP_NONE;
        async_status = kISPFlashError;
        return false;
      }

      if (async_length_remaining > 0) {
        CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

        uint64_t doubleword = ((uint64_t)(async_data_ptr[0]) << 0)
            + ((uint64_t)(async_data_ptr[1]) << 8)
            + ((uint64_t)(async_data_ptr[2]) << 16)
            + ((uint64_t)(async_data_ptr[3]) << 24)
            + ((uint64_t)(async_data_ptr[4]) << 32)
            + ((uint64_t)(async_data_ptr[5]) << 40)
            + ((uint64_t)(async_data_ptr[6]) << 48)
            + ((uint64_t)(async_data_ptr[7]) << 56);

        FLASH_Program_DoubleWord(async_addr_current, doubleword);

        async_addr_current += kWriteSize;
        async_data_ptr += kWriteSize;
        async_length_remaining -= kWriteSize;
        return true;
      } else {
        CLEAR_BIT(FLASH->CR, FLASH_CR_PG);
        async_op = OP_NONE;
        async_status = kISPOk;
        return false;
      }
    } else {
      // This shouldn't happen, so make it fail obviously when it does.
      return false;
    }
  }

  virtual bool get_last_async_status(ISPStatus* statusOut) {
    *statusOut = async_status;
    return async_op == OP_NONE;
  }

  virtual bool async_erase(void* start_addr, size_t length) {
    if (async_op != OP_NONE) {
      return false;
    }
    async_status = kISPOk;

    if (length % kEraseSize != 0) {
      async_status = kISPInvalidArgs;
    }
    size_t addr = (uint32_t)start_addr;
    if (addr % kEraseSize != 0) {
      async_status = kISPInvalidArgs;
    }
    if (!(addr >= kFlashStartAddr && addr <= kFlashEndAddr)) {
      async_status = kISPInvalidArgs;
    }
    if (addr + length > kFlashEndAddr + 1) {
      async_status = kISPInvalidArgs;
    }
    if (async_status != kISPOk) {
      return true;
    }

    async_op = OP_ERASE;
    async_addr_current = (uint32_t)start_addr;
    async_length_remaining = length;

    async_update();

    return true;
  }


  virtual bool async_write(void* start_addr, void* data, size_t length) {
    if (async_op != OP_NONE) {
      return false;
    }

    if (length % kWriteSize != 0) {
      async_status = kISPInvalidArgs;
    }
    size_t addr = (uint32_t)start_addr;
    if (addr % kWriteSize != 0) {
      async_status = kISPInvalidArgs;
    }
    if (!(addr >= kFlashStartAddr && addr <= kFlashEndAddr)) {
      async_status = kISPInvalidArgs;
    }
    if (async_status != kISPOk) {
      return true;
    }

    async_op = OP_WRITE;
    async_addr_current = (uint32_t)start_addr;
    async_data_ptr = (uint8_t*)data;
    async_length_remaining = length;
    async_update();

    return true;
  }

private:
  enum AsyncOp {OP_NONE, OP_ERASE, OP_WRITE};
  AsyncOp async_op;
  uint32_t async_addr_current;
  uint8_t* async_data_ptr;
  size_t async_length_remaining;
  ISPStatus async_status;

};

#endif
#endif
