#include "mbed.h"
#include "blproto.h"

namespace BootProto {
  uint8_t GetDeviceAddr(uint8_t device_num) {
    return (device_num + 1) << 1;
  }
}
