/*
 * main.cpp
 *
 *  Created on: Mar 23, 2016
 *      Author: ducky
 */

#include "mbed.h"

#include "crc.h"
#include "endian.h"
#include "packet.h"
#include "cobs.h"

#include "blproto.h"

#include "ActivityLED.h"
#include "isp_f303k8.h"

Serial uart(SERIAL_TX, SERIAL_RX);
Serial uart_in(D1, D0);

DigitalIn bootInPin(D3, PullDown);
DigitalOut bootOutPin(D6);

ActivityLED statusLED(LED1);

const uint32_t I2C_FREQUENCY = 400000;

const uint32_t LED_PULSE_TIME = 50;

const uint32_t HEARTBEAT_PERIOD = 1000;
const uint32_t HEARTBEAT_INIT_PERIOD = 500;
const uint32_t HEARTBEAT_PULSE_TIME = LED_PULSE_TIME;

extern char _FlashStart, _FlashEnd;
void* const BL_BEGIN_PTR = &_FlashStart;
void* const APP_BEGIN_PTR = &_FlashEnd;

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
    i2c.frequency(I2C_FREQUENCY); // reset the I2C device
    i2c.write(BootProto::GetDeviceAddr(device), (char*)i2cData, 1);
    i2c.read(BootProto::GetDeviceAddr(device), (char*)i2cData, 1);
    resp = (BootProto::RespStatus)i2cData[0];
  }
  return resp;
}

BootProto::RespStatus process_bootloader_command(ISPBase &isp, I2C &i2c, BufferedPacketReader& packet) {
  uint8_t i2cData[BootProto::kMaxPayloadLength];
  uint8_t opcode = packet.read<uint8_t>();

  if (opcode == 'W') {
    uint8_t device = packet.read<uint8_t>();
    uint32_t addr = packet.read<uint32_t>();
    uint32_t crc = packet.read<uint32_t>();
    size_t data_length = packet.getRemainingBytes();
    if (data_length < 1) {
      return BootProto::kRespInvalidFormat;
    }

    size_t i = 0;
    while (packet.getRemainingBytes() > 0) {
      i2cData[11 + i] = packet.read<uint8_t>();
      i += 1;
    }

    if (device > 0) {
      device = device - 1;

      i2cData[0] = BootProto::kCmdWrite;
      serialize_uint32(i2cData+1, addr);
      serialize_uint16(i2cData+5, (uint16_t)data_length);
      serialize_uint32(i2cData+7, crc);

      i2c.write(BootProto::GetDeviceAddr(device), (char*)i2cData, data_length +11);

      return get_slave_status(i2c, device);
    } else {
      uint32_t computed_crc = CRC32::compute_crc(i2cData + 11, data_length);
      if (computed_crc == crc) {
        return blstatus_from_ispstatus(isp.write((void*)addr, i2cData + 11, data_length));
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

      i2cData[0] = BootProto::kCmdErase;
      serialize_uint32(i2cData+1, addr);
      serialize_uint32(i2cData+5, length);

      i2c.write(BootProto::GetDeviceAddr(device), (char*)i2cData, 9);

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
      i2cData[0] = BootProto::kCmdRunApp;
      serialize_uint32(i2cData+1, (uint32_t)APP_BEGIN_PTR);

      i2c.write(BootProto::GetDeviceAddr(device), (char*)i2cData, 5);
    } else {
      runApp((void*)addr);
    }

    return BootProto::kRespDone;
  } else {  // unknown command
    return BootProto::kRespInvalidFormat;
  }
}

int bootloaderMaster() {
  F303K8ISP isp;

  I2C i2c(D4, D5);
  uint8_t i2cData[BootProto::kMaxPayloadLength];

  i2c.frequency(I2C_FREQUENCY);

  bootOutPin = 1;

  uint8_t numDevices = 0;
  while (1) {
    wait_ms(BootProto::kBootToAddrDelayMs);

    i2c.frequency(I2C_FREQUENCY); // reset the I2C device
    i2cData[0] = BootProto::kCmdPing;
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

  uart.printf("Discovered %i devices\n", numDevices);

  statusLED.setIdlePolarity(true);

  Timer heartbeatTimer;
  int nextHeartbeatTime = 0;
  heartbeatTimer.start();

  COBSPacketReader packet;
  COBSDecoder decoder;
  decoder.set_buffer(&packet);

  while (1) {
    while (uart.readable() || uart_in.readable()) {
      uint8_t rx = uart.readable() ? (uint8_t)uart.getc() : (uint8_t)uart_in.getc();
      size_t bytes_decoded;
      COBSDecoder::COBSResult result = decoder.decode(&rx, 1, &bytes_decoded);

      if (result == COBSDecoder::kResultDone) {
        BootProto::RespStatus status = process_bootloader_command(isp, i2c, packet);
        uart.printf("D%i\n", status);
        packet.reset();
        decoder.set_buffer(&packet);
      }

      statusLED.pulse(LED_PULSE_TIME);
    }

    statusLED.update();
    if (heartbeatTimer.read_ms() >= nextHeartbeatTime) {
      nextHeartbeatTime += HEARTBEAT_PERIOD;
      statusLED.pulse(HEARTBEAT_PULSE_TIME);
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
      nextHeartbeatTime += HEARTBEAT_INIT_PERIOD;
      statusLED.pulse(HEARTBEAT_PULSE_TIME);
    }
  }

  statusLED.setIdlePolarity(true);

  I2CSlave i2c(D4, D5);
  i2c.frequency(I2C_FREQUENCY);
  i2c.address(BootProto::kAddressGlobal);
  uint8_t address = BootProto::kAddressGlobal;

  uint8_t lastI2CCommand = 0;

  // Wait for initialization I2C command to get address
  while (address == BootProto::kAddressGlobal) {
    if (bootInPin == 0) {
      NVIC_SystemReset();
    }

    switch (i2c.receive()) {
    case I2CSlave::ReadAddressed:
      if (lastI2CCommand == BootProto::kCmdPing) {
        i2c.write(BootProto::kRespDone);
      } else {
        // Drop everything else
      }
      break;
    case I2CSlave::WriteAddressed:
      lastI2CCommand = i2c.read();
      if (lastI2CCommand == BootProto::kCmdSetAddress) {
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
      nextHeartbeatTime += HEARTBEAT_INIT_PERIOD;
      statusLED.pulse(HEARTBEAT_PULSE_TIME);
    }
  }

  i2c.address(address);

  F303K8ISP isp;
  isp.isp_begin();

  uint8_t i2cData[BootProto::kMaxPayloadLength];
  // Override status. DONE means to read from ISP status.
  BootProto::RespStatus lastStatus = BootProto::kRespDone;

  // Main bootloader loop
  while (1) {
    if (bootInPin == 0) {
      NVIC_SystemReset();
    }

    if (isp.async_update()) {
      statusLED.pulse(LED_PULSE_TIME);
    }

    switch (i2c.receive()) {
    case I2CSlave::ReadAddressed:
      statusLED.pulse(LED_PULSE_TIME);
      if (lastI2CCommand == BootProto::kCmdPing) {
        i2c.write(BootProto::kRespDone);
      } else if (lastI2CCommand == BootProto::kCmdStatus) {
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
      statusLED.pulse(LED_PULSE_TIME);
      lastI2CCommand = i2c.read();
      if (lastI2CCommand == BootProto::kCmdSetBootOut) {
        bootOutPin = 1;
      } else if (lastI2CCommand == BootProto::kCmdErase) {
        if (!i2c.read((char*)i2cData, 8)) {
          uint32_t startAddr = deserialize_uint32(i2cData+0);
          uint32_t len = deserialize_uint32(i2cData+4);
          isp.async_erase((void*)startAddr, len);
          lastStatus = BootProto::kRespDone;
        } else {
          lastStatus = BootProto::kRespInvalidFormat;
        }
      } else if (lastI2CCommand == BootProto::kCmdWrite) {
        if (!i2c.read((char*)i2cData, 10)) {
          uint32_t startAddr = deserialize_uint32(i2cData+0);
          uint16_t len = deserialize_uint16(i2cData+4);
          uint32_t crc = deserialize_uint32(i2cData+6);
          // TODO: allow len >= 256 transfers by breaking i2c read calls into
          // smaller chunks
          if (!i2c.read((char*)i2cData, len)) {
            uint32_t computed_crc = CRC32::compute_crc(i2cData, len);
            if (computed_crc == crc) {
              isp.async_write((void*)startAddr, i2cData, len);
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
      } else if (lastI2CCommand == BootProto::kCmdRunApp) {
        if (!i2c.read((char*)i2cData, 4)) {
          uint32_t addr = deserialize_uint32(i2cData);
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
      nextHeartbeatTime += HEARTBEAT_PERIOD;
      statusLED.pulse(HEARTBEAT_PULSE_TIME);
    }
  }
}

int main() {
  bootOutPin = 0;

  uart.baud(115200);
  uart_in.baud(115200);
  uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ ")\r\n");

  wait_ms(BootProto::kBootscanDelayMs);  // wait for some time to let boot in stabilize

  if (bootInPin == 1) {
    wait_ms(BootProto::kBootscanDelayMs);
    return bootloaderMaster();
  } else {
    return bootloaderSlaveInit();
  }
}
