#include "mbed.h"

#include "bootloader.h"

BootProto::RespStatus Bootloader::async_update() {
  return BootProto::kRespBusy;
}

bool Bootloader::async_erase(void* start_rel_addr, size_t length) {
  return true;
}

bool Bootloader::async_write(void* start_rel_addr, void* data, size_t length) {
  return true;
}
