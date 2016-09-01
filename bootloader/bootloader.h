#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#include "isp.h"
#include "blproto.h"

extern char _AppStart, _AppEnd, _BootloaderDataStart, _BootloaderDataEnd, _BootVectorBegin, _BootVectorEnd, _BootloaderVector;

/**
 * Asynchronous pooling bootloader, with remapping capaibility for high-memory
 * bootloaders.
 */
class Bootloader {
public:
  Bootloader(ISPBase &isp, uint8_t* app, size_t app_length,
      uint8_t* bootloader_data, size_t bootloader_data_length,
      uint8_t* boot_vector, uint8_t* bootloader_vector,
      size_t boot_vector_length) :
      isp(isp), app(app), app_length(app_length),
      bootloader_data(bootloader_data), bootloader_data_length(bootloader_data_length),
      boot_vector(boot_vector), bootloader_vector(bootloader_vector),
      boot_vector_length(boot_vector_length),
      current_command(BootProto::kCmdInvalid), current_stage(0),
      last_response(BootProto::kRespDone)
      {}
  /**
   * Call this periodically during an async operation.
   * Returns kRespBusy is an operation is ongoing, otherwise returns the last
   * status, either kRespDone if success, or an error code.
   */
  BootProto::RespStatus async_update();

  /**
   * Begins an asynchronous erase operation.
   *
   * Returns true if the operation is started (and the next async_update will
   * return the result of this operation), or false if not (for example, if
   * another operation is running).
   */
  bool async_erase(size_t start_offset, size_t length);

  /**
   * Begins an asynchronous write operation.
   *
   * Data buffer must not be modified until the operation is complete.
   *
   * Returns true if the operation is started (and the next async_update will
   * return the result of this operation), or false if not (for example, if
   * another operation is running).
   */
  bool async_write(size_t start_offset, void* data, size_t length);

  /**
   * Runs the app at the specified app-relative address. Should not return under
   * normal circumstances.
   */
  bool run_app(size_t start_offset);

  /**
   * Blocking variants, weapper around async_*() and async_update().
   */
  BootProto::RespStatus erase(size_t start_offset, size_t length) {
    if (!async_erase(start_offset, length)) {
      return BootProto::kRespUnknownError;
    }
    BootProto::RespStatus status = BootProto::kRespBusy;
    while (status == BootProto::kRespBusy) {
      status = async_update();
    }
    return status;
  }

  BootProto::RespStatus write(size_t start_offset, void* data, size_t length) {
    if (!async_write(start_offset, data, length)) {
      return BootProto::kRespUnknownError;
    }
    BootProto::RespStatus status = BootProto::kRespBusy;
    while (status == BootProto::kRespBusy) {
      status = async_update();
    }
    return status;
  }

private:
  ISPBase &isp;

  uint8_t* const app;
  const size_t app_length;

  uint8_t* const bootloader_data;
  const size_t bootloader_data_length;

  uint8_t* const boot_vector;
  uint8_t* const bootloader_vector;
  const size_t boot_vector_length;

  BootProto::BootCommand current_command;
  uint8_t current_stage;
  BootProto::RespStatus last_response;

  uint8_t* current_start_addr;
  uint8_t* current_data;
  size_t current_length;
};

#endif
