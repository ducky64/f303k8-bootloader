#include "mbed.h"

#include "bootloader.h"

static BootProto::RespStatus blstatus_from_ispstatus(ISPBase::ISPStatus status) {
  if (status == ISPBase::kISPOk) {
    return BootProto::kRespDone;
  } else if (status == ISPBase::kISPInvalidArgs) {
    return BootProto::kRespInvalidArgs;
  } else if (status == ISPBase::kISPFlashError) {
    return BootProto::kRespFlashError;
  } else {
    return BootProto::kRespUnknownError;
  }
}

BootProto::RespStatus Bootloader::async_update() {
  if (current_command == BootProto::kCmdErase) {
    if (current_stage == 0) {
      isp.isp_begin();
      if (current_start_addr < boot_vector + boot_vector_length
          && current_start_addr + current_length > boot_vector) {
        // TODO: allow operations overlapping with, but not aligned to, boot vector
        if (current_start_addr != boot_vector) {
          isp.isp_end();
          current_command = BootProto::kCmdInvalid;
          last_response = BootProto::kRespInvalidArgs;
          return BootProto::kRespInvalidArgs;
        }
        // Redirect requests on boot vector to bootloader data segment
        isp.async_erase(bootloader_data, bootloader_data_length);
        current_stage = 1;
      } else {
        isp.async_erase(current_start_addr, current_length);
        current_stage = 255;
      }
    }

    isp.async_update();
    ISPBase::ISPStatus status;
    if (!isp.get_last_async_status(&status)) {
      return BootProto::kRespBusy;
    }
    if (status != ISPBase::kISPOk) {
      isp.isp_end();
      current_command = BootProto::kCmdInvalid;
      last_response = blstatus_from_ispstatus(status);
    }

    if (current_stage == 1) {
      // Erase boot vector page
      isp.async_erase(boot_vector, isp.get_erase_size());
      current_stage = 2;
    } else if (current_stage == 2) {
      // ... then immediately reprogram it
      isp.async_write(boot_vector, bootloader_vector, boot_vector_length);
      current_stage = 3;
    } else if (current_stage == 3) {
      // Erase everything else
      isp.async_erase(current_start_addr + isp.get_erase_size(),
          current_length - isp.get_erase_size());
      current_stage = 255;
    } else if (current_stage == 255) {
      // Done with everything
      isp.isp_end();
      current_command = BootProto::kCmdInvalid;
      last_response = BootProto::kRespDone;
    } else {  // should never happen
      isp.isp_end();
      current_command = BootProto::kCmdInvalid;
      last_response = BootProto::kRespUnknownError;
    }
  } else if (current_command == BootProto::kCmdWrite) {

    if (current_stage == 0) {
      isp.isp_begin();
      if (current_start_addr < boot_vector + boot_vector_length
          && current_start_addr + current_length > boot_vector) {
        // TODO: allow operations overlapping with, but not aligned to, boot vector
        if (current_start_addr != boot_vector) {
          isp.isp_end();
          current_command = BootProto::kCmdInvalid;
          last_response = BootProto::kRespInvalidArgs;
          return BootProto::kRespInvalidArgs;
        }
        // Redirect requests on boot vector to bootloader data segment
        isp.async_write(bootloader_data, current_data, boot_vector_length);
        current_stage = 1;
      } else {
        isp.async_write(current_start_addr, current_data, current_length);
        current_stage = 255;
      }
    }

    isp.async_update();
    ISPBase::ISPStatus status;
    if (!isp.get_last_async_status(&status)) {
      return BootProto::kRespBusy;
    }
    if (status != ISPBase::kISPOk) {
      isp.isp_end();
      current_command = BootProto::kCmdInvalid;
      last_response = blstatus_from_ispstatus(status);
    }

    if (current_stage == 1) {
      // Write everything else
      isp.async_write(current_start_addr + boot_vector_length,
          current_data + boot_vector_length,
          current_length - boot_vector_length);
      current_stage = 255;
    } else if (current_stage == 255) {
      isp.isp_end();
      current_command = BootProto::kCmdInvalid;
      last_response = BootProto::kRespDone;
    } else {  // should never happen
      isp.isp_end();
      current_command = BootProto::kCmdInvalid;
      last_response = BootProto::kRespUnknownError;
    }
  }

  return last_response;
}

bool Bootloader::async_erase(size_t start_offset, size_t length) {
  if (current_command != BootProto::kCmdInvalid) {
    return false;
  }

  current_start_addr = app + start_offset;
  current_length = length;

  last_response = BootProto::kRespBusy;
  if (current_start_addr < app
      || current_start_addr + current_length > app + app_length) {
    last_response = BootProto::kRespInvalidArgs;
  }
  if (last_response != BootProto::kRespBusy) {
    return true;
  }

  current_stage = 0;
  current_command = BootProto::kCmdErase;

  async_update();
  return true;
}

bool Bootloader::async_write(size_t start_offset, void* data, size_t length) {
  if (current_command != BootProto::kCmdInvalid) {
    return false;
  }

  current_start_addr = app + start_offset;
  current_length = length;
  current_data = (uint8_t*)data;

  last_response = BootProto::kRespBusy;
  if (current_start_addr < app
      || current_start_addr + current_length > app + app_length) {
    last_response = BootProto::kRespInvalidArgs;
  }
  if (last_response != BootProto::kRespBusy) {
    return true;
  }

  current_stage = 0;
  current_command = BootProto::kCmdWrite;

  async_update();
  return true;
}

bool Bootloader::run_app(size_t start_offset) {
  // Use statics since the stack pointer gets reset without the compiler knowing.
  static uint32_t stack_ptr = 0;
  static void (*target)(void) = 0;

  // Just to be extra safe
  for (uint8_t i=0; i<NVIC_NUM_VECTORS; i++) {
    NVIC_DisableIRQ((IRQn_Type)i);
  }

  uint8_t* start_addr = app + start_offset;

  if (start_addr == boot_vector) {
    stack_ptr = (*(uint32_t*)((uint8_t*)bootloader_data + 0));
    target = (void (*)(void))(*(uint32_t*)((uint8_t*)bootloader_data + 4));
  } else {
    stack_ptr = (*(uint32_t*)((uint8_t*)start_addr + 0));
    target = (void (*)(void))(*(uint32_t*)((uint8_t*)start_addr + 4));
  }

  __set_MSP(stack_ptr);
  __set_PSP(stack_ptr);
  SCB->VTOR = (uint32_t)start_addr;

  goto *target;

  // TODO: replace with standard function call instead of goto.
  // For some reason, this increments the stack pointer past the top of memory,
  // causing a hardfault when it tries to access those locations.
  // target();

  // TODO: meaningful errors
  return true;
}
