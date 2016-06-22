/*
 * main.cpp
 *
 *  Created on: Mar 23, 2016
 *      Author: ducky
 */

#include "mbed.h"

#include "crc.h"
#include "packet.h"
#include "cobs.h"

#include "blproto.h"

#include "ActivityLED.h"
#include "isp_f303k8.h"

RawSerial uart(SERIAL_TX, SERIAL_RX);

DigitalIn masterRunAppPin(D2, PullUp);
DigitalIn bootInPin(D3, PullDown);
DigitalOut bootOutPin(D6);

ActivityLED statusLED(LED1);

const uint32_t kI2CFrequency = 1000000;

const uint32_t kActivityPulseTimeMs = 25;

const uint32_t kHeartbeatPeriodMs = 1000;
const uint32_t kInitHeartbeatPeriodMs = 500;
const uint32_t kHeartbeatPulseTimeMs = kActivityPulseTimeMs;

extern char _FlashStart, _FlashEnd;
void* const kBootloaderBeginPtr = &_FlashStart;
void* const kAppBeginPtr = &_FlashEnd;

/**
 * Runs an application at the specified address. Should not return under normal
 * circumstances.
 */
void runApp(void* addr) {
  // Use statics since the stack pointer gets reset without the compiler knowing.
  static uint32_t stack_ptr = 0;
  static void (*target)(void) = 0;

  stack_ptr = (*(uint32_t*)((uint8_t*)addr + 0));
  target = (void (*)(void))(*(uint32_t*)((uint8_t*)addr + 4));

  // Just to be extra safe
  for (uint8_t i=0; i<NVIC_NUM_VECTORS; i++) {
    NVIC_DisableIRQ((IRQn_Type)i);
  }

  // Reset the interrupt table so the application can re-load it
  SCB->VTOR = (uint32_t)addr;

  __set_MSP(stack_ptr);
  __set_PSP(stack_ptr);

  goto *target;

  // TODO: replace with standard function call instead of goto.
  // For some reason, this increments the stack pointer past the top of memory,
  // causing a hardfault when it tries to access those locations.
  // target();
}

BootProto::RespStatus blstatus_from_ispstatus(ISPBase::ISPStatus status) {
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

BootProto::RespStatus get_slave_status(I2C &i2c, uint8_t device) {
  uint8_t i2cData[1];
  BootProto::RespStatus resp = BootProto::kRespBusy;
  while (resp == BootProto::kRespBusy) {
    i2cData[0] = BootProto::kCmdStatus;
    // TODO: debug why I2C device resets are necessary...
    i2c.frequency(kI2CFrequency); // reset the I2C device
    i2c.write(BootProto::GetDeviceAddr(device), (char*)i2cData, 1);
    i2c.read(BootProto::GetDeviceAddr(device), (char*)i2cData, 1);
    resp = (BootProto::RespStatus)i2cData[0];
  }
  return resp;
}

BootProto::RespStatus process_bootloader_command(ISPBase &isp, I2C &i2c, MemoryPacketReader& packet) {
  BufferedPacketBuilder<BootProto::kMaxPayloadLength> i2cPacket;
  uint8_t opcode = packet.read<uint8_t>();

  if (opcode == 'W') {
    uint8_t device = packet.read<uint8_t>();
    uint32_t addr = packet.read<uint32_t>();
    uint32_t crc = packet.read<uint32_t>();
    size_t data_length = packet.getRemainingBytes();
    if (data_length < 1) {
      return BootProto::kRespInvalidFormat;
    }

    if (device > 0) {
      device = device - 1;

      i2cPacket.put<uint8_t>(BootProto::kCmdWrite);
      i2cPacket.put<uint32_t>(addr);
      i2cPacket.put<uint16_t>((uint16_t)data_length);
      i2cPacket.put<uint32_t>(crc);
      while (packet.getRemainingBytes() > 0) {
        i2cPacket.put<uint8_t>(packet.read<uint8_t>());
      }
      i2c.write(BootProto::GetDeviceAddr(device),
          (char*)i2cPacket.getBuffer(), i2cPacket.getLength());

      return get_slave_status(i2c, device);
    } else {
      uint8_t* data = packet.read_buf(data_length);
      uint32_t computed_crc = CRC32::compute_crc(data, data_length);
      if (computed_crc == crc) {
        return blstatus_from_ispstatus(isp.write((void*)addr, data, data_length));
      } else {
        return BootProto::kRespInvalidChecksum;
      }

    }
  } else if (opcode == 'E') {
    uint8_t device = packet.read<uint8_t>();
    uint32_t addr = packet.read<uint32_t>();
    uint32_t length = packet.read<uint32_t>();
    if (packet.getRemainingBytes() > 0) {
      return BootProto::kRespInvalidFormat;
    }

    if (device > 0) {
      device = device - 1;

      i2cPacket.put<uint8_t>(BootProto::kCmdErase);
      i2cPacket.put<uint32_t>(addr);
      i2cPacket.put<uint32_t>(length);
      i2c.write(BootProto::GetDeviceAddr(device),
          (char*)i2cPacket.getBuffer(), i2cPacket.getLength());

      return get_slave_status(i2c, device);
    } else {
      return blstatus_from_ispstatus(isp.erase((void*)addr, length));
    }
  } else if (opcode == 'J') {
    uint8_t device = packet.read<uint8_t>();
    uint32_t addr = packet.read<uint32_t>();
    if (packet.getRemainingBytes() > 0) {
      return BootProto::kRespInvalidFormat;
    }

    if (device > 0) {
      device = device - 1;
      i2cPacket.put<uint8_t>(BootProto::kCmdRunApp);
      i2cPacket.put<uint32_t>(addr);
      i2c.write(BootProto::GetDeviceAddr(device),
          (char*)i2cPacket.getBuffer(), i2cPacket.getLength());
    } else {
      isp.isp_end();
      runApp((void*)addr);
    }

    return BootProto::kRespDone;
  } else {  // unknown command
    return BootProto::kRespInvalidFormat;
  }
}

int bootloaderMaster() {
  F303K8ISP isp;
  isp.isp_begin();

  I2C i2c(D4, D5);
  uint8_t i2cData[BootProto::kMaxPayloadLength];

  i2c.frequency(kI2CFrequency);

  bootOutPin = 1;

  uint8_t numDevices = 0;
  while (1) {
    wait_ms(BootProto::kBootToAddrDelayMs);

    i2c.frequency(kI2CFrequency); // reset the I2C device
    i2cData[0] = BootProto::kCmdStatus;
    if (i2c.write(BootProto::kAddressGlobal, (char*)i2cData, 1, true) != 0) {
      // Next device in chain didn't respond to ping, reached end of chain
      break;
    }
    i2c.read(BootProto::kAddressGlobal, (char*)i2cData, 1);
    // TODO: better I2C robustness, like if device doesn't respond
    if (i2cData[0] != BootProto::kRespDone) { break; }

    uint8_t thisDeviceAddress = BootProto::GetDeviceAddr(numDevices);
    i2cData[0] = BootProto::kCmdSetAddress;
    i2cData[1] = thisDeviceAddress;
    i2c.write(BootProto::kAddressGlobal, (char*)i2cData, 2);

    i2cData[0] = BootProto::kCmdSetBootOut;
    i2c.write(thisDeviceAddress, (char*)i2cData, 1);

    numDevices += 1;
  }

  if (!masterRunAppPin) {
    for (size_t i=0; i<numDevices; i++) {
      BufferedPacketBuilder<BootProto::kMaxPayloadLength> i2cPacket;
      i2cPacket.put<uint8_t>(BootProto::kCmdRunApp);
      // TODO: allow heterogeneous systems, use relative addressing
      i2cPacket.put<uint32_t>((uint32_t)kAppBeginPtr);
      i2c.write(BootProto::GetDeviceAddr(i),
          (char*)i2cPacket.getBuffer(), i2cPacket.getLength());
    }
    runApp((void*)kAppBeginPtr);
  }

  statusLED.setIdlePolarity(true);

  Timer heartbeatTimer;
  int nextHeartbeatTime = 0;
  heartbeatTimer.start();

  BufferedPacketReader<BootProto::kMaxPayloadLength> packet;
  COBSDecoder decoder;
  decoder.set_buffer(&packet);

  while (1) {
    if (bootInPin == 0) {
      NVIC_SystemReset();
    }

    while (uart.readable()) {
      uint8_t rx = (uint8_t)uart.getc();
      size_t bytes_decoded;
      COBSDecoder::COBSResult result = decoder.decode(&rx, 1, &bytes_decoded);

      if (result == COBSDecoder::kResultDone) {
        BootProto::RespStatus status = process_bootloader_command(isp, i2c, packet);
        if (status == BootProto::kRespDone) {
          uart.puts("D\n");
        } else {
          uart.puts("N\n");
        }
        packet.reset();
        decoder.set_buffer(&packet);
      }

      statusLED.pulse(kActivityPulseTimeMs);
    }

    statusLED.update();
    if (heartbeatTimer.read_ms() >= nextHeartbeatTime) {
      nextHeartbeatTime += kHeartbeatPeriodMs;
      statusLED.pulse(kHeartbeatPulseTimeMs);
    }
  }
  return 0;
}

int bootloaderSlaveInit() {
  statusLED.setIdlePolarity(false);

  Timer heartbeatTimer;
  int nextHeartbeatTime = 0;
  heartbeatTimer.start();

  // Wait for BOOT pin to go high
  while (1) {
    if (bootInPin == 1) {
      break;
    }

    statusLED.update();
    if (heartbeatTimer.read_ms() >= nextHeartbeatTime) {
      nextHeartbeatTime += kInitHeartbeatPeriodMs;
      statusLED.pulse(kHeartbeatPulseTimeMs);
    }
  }

  statusLED.setIdlePolarity(true);

  I2CSlave i2c(D4, D5);
  i2c.frequency(kI2CFrequency);
  i2c.address(BootProto::kAddressGlobal);
  uint8_t address = BootProto::kAddressGlobal;

  BootProto::BootCommand lastCommand = BootProto::kCmdInvalid;

  // Wait for initialization I2C command to get address
  while (address == BootProto::kAddressGlobal) {
    if (bootInPin == 0) {
      NVIC_SystemReset();
    }

    switch (i2c.receive()) {
    case I2CSlave::ReadAddressed:
      if (lastCommand == BootProto::kCmdStatus) {
        i2c.write(BootProto::kRespDone);
      } else {
        // Drop everything else
      }
      break;
    case I2CSlave::WriteAddressed:
      lastCommand = (BootProto::BootCommand)i2c.read();
      if (lastCommand == BootProto::kCmdSetAddress) {
        uint8_t payload = i2c.read();
        if (payload != -1) {
          address = payload;
        }
      } else {
        // Drop everything else
      }
      break;
    }

    statusLED.update();
    if (heartbeatTimer.read_ms() >= nextHeartbeatTime) {
      nextHeartbeatTime += kInitHeartbeatPeriodMs;
      statusLED.pulse(kHeartbeatPulseTimeMs);
    }
  }

  i2c.address(address);

  F303K8ISP isp;
  isp.isp_begin();

  BufferedPacketReader<BootProto::kMaxPayloadLength> i2cPacket;
  // Override status. DONE means to read from ISP status.
  BootProto::RespStatus lastStatus = BootProto::kRespDone;

  // Main bootloader loop
  while (1) {
    if (bootInPin == 0) {
      NVIC_SystemReset();
    }

    if (isp.async_update()) {
      statusLED.pulse(kActivityPulseTimeMs);
    }

    switch (i2c.receive()) {
    case I2CSlave::ReadAddressed:
      statusLED.pulse(kActivityPulseTimeMs);
      if (lastCommand == BootProto::kCmdStatus) {
        if (lastStatus == BootProto::kRespDone) {
          ISPBase::ISPStatus ispStatus;
          if (isp.get_last_async_status(&ispStatus)) {
            i2c.write(blstatus_from_ispstatus(ispStatus));
          } else {
            i2c.write(BootProto::kRespBusy);
          }
        } else {
          i2c.write(lastStatus);
        }

      } else {
        // Drop everything else
      }
      break;

    case I2CSlave::WriteAddressed:
      statusLED.pulse(kActivityPulseTimeMs);
      lastCommand = (BootProto::BootCommand)i2c.read();
      i2cPacket.reset();
      if (lastCommand == BootProto::kCmdSetBootOut) {
        bootOutPin = 1;
      } else if (lastCommand == BootProto::kCmdErase) {
        if (!i2c.read((char*)i2cPacket.ptrPutBytes(8), 8)) {
          uint32_t startAddr = i2cPacket.read<uint32_t>();
          uint32_t len = i2cPacket.read<uint32_t>();

          isp.async_erase((void*)startAddr, len);
          lastStatus = BootProto::kRespDone;
        } else {
          lastStatus = BootProto::kRespInvalidFormat;
        }
      } else if (lastCommand == BootProto::kCmdWrite) {
        if (!i2c.read((char*)i2cPacket.ptrPutBytes(10), 10)) {
          uint32_t startAddr = i2cPacket.read<uint32_t>();
          uint16_t len = i2cPacket.read<uint16_t>();
          uint32_t crc = i2cPacket.read<uint32_t>();
          // TODO: allow len >= 256 transfers by breaking i2c read calls into
          // smaller chunks
          if (!i2c.read((char*)i2cPacket.ptrPutBytes(len), len)) {
            uint8_t* data = i2cPacket.read_buf(len);
            uint32_t computed_crc = CRC32::compute_crc(data, len);
            if (computed_crc == crc) {
              isp.async_write((void*)startAddr, data, len);
              lastStatus = BootProto::kRespDone;
            } else {
              lastStatus = BootProto::kRespInvalidChecksum;
            }
          } else {
            lastStatus = BootProto::kRespInvalidFormat;
          }
        } else {
          lastStatus = BootProto::kRespInvalidFormat;
        }
      } else if (lastCommand == BootProto::kCmdRunApp) {
        if (!i2c.read((char*)i2cPacket.ptrPutBytes(4), 4)) {
          uint32_t addr = i2cPacket.read<uint32_t>();
          isp.isp_end();
          runApp((void*)addr);
        }
      } else {
        // Drop everything else
      }
      break;
    }

    statusLED.update();
    if (heartbeatTimer.read_ms() >= nextHeartbeatTime) {
      nextHeartbeatTime += kHeartbeatPeriodMs;
      statusLED.pulse(kHeartbeatPulseTimeMs);
    }
  }
}

int main() {
  bootOutPin = 0;

  uart.baud(115200);
  uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ ")\r\n");

  wait_ms(BootProto::kBootscanDelayMs);  // wait for some time to let boot in stabilize

  if (bootInPin == 1) {
    wait_ms(BootProto::kBootscanDelayMs);
    return bootloaderMaster();
  } else {
    return bootloaderSlaveInit();
  }
}
