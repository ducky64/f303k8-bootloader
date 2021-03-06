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
#include "isp.h"
#include "bootloader.h"

RawSerial usb_uart(SERIAL_TX, SERIAL_RX);
RawSerial ext_uart(D1, D0);

DigitalIn masterRunAppPin(D2, PullUp);
DigitalIn bootInPin(D3, PullUp);
DigitalOut bootOutPin(D6);

ActivityLED statusLED(LED1);

const uint32_t kI2CFrequency = 1000000;

const uint32_t kActivityPulseTimeMs = 25;

const uint32_t kHeartbeatPeriodMs = 1000;
const uint32_t kInitHeartbeatPeriodMs = 500;
const uint32_t kHeartbeatPulseTimeMs = kActivityPulseTimeMs;

extern char _AppStart, _AppEnd, _BootloaderDataStart, _BootloaderDataEnd, _BootVectorBegin, _BootVectorEnd, _BootloaderVector;
uint8_t* const kAppBeginPtr = (uint8_t*)&_AppStart;
uint8_t* const kAppEndPtr = (uint8_t*)&_AppEnd;
uint8_t* const kBootloaderDataBeginPtr = (uint8_t*)&_BootloaderDataStart;
uint8_t* const kBootloaderDataEndPtr = (uint8_t*)&_BootloaderDataEnd;
uint8_t* const kBootVectorPtr = (uint8_t*)&_BootVectorBegin;
const size_t kBootVectorSize = (uint8_t*)&_BootVectorEnd - (uint8_t*)&_BootVectorBegin;
uint8_t* const kBootloaderVectorPtr = (uint8_t*)&_BootloaderVector;

ISP this_isp;
Bootloader bootloader(this_isp, kAppBeginPtr, kAppEndPtr - kAppBeginPtr,
    kBootloaderDataBeginPtr, kBootloaderDataEndPtr - kBootloaderDataBeginPtr,
    kBootVectorPtr, kBootloaderVectorPtr, kBootVectorSize);

/**
 * Runs an application at the specified address. Should not return under normal
 * circumstances.
 */
void runApp(void* app_ptrs, void* isr_vectors) {
  // Use statics since the stack pointer gets reset without the compiler knowing.
  static uint32_t stack_ptr = 0;
  static void (*target)(void) = 0;

  stack_ptr = (*(uint32_t*)((uint8_t*)app_ptrs + 0));
  target = (void (*)(void))(*(uint32_t*)((uint8_t*)app_ptrs + 4));

  // Just to be extra safe
  for (uint8_t i=0; i<NVIC_NUM_VECTORS; i++) {
    NVIC_DisableIRQ((IRQn_Type)i);
  }

  // Reset the interrupt table so the application can re-load it
  SCB->VTOR = (uint32_t)isr_vectors;

  __set_MSP(stack_ptr);
  __set_PSP(stack_ptr);

  goto *target;

  // TODO: replace with standard function call instead of goto.
  // For some reason, this increments the stack pointer past the top of memory,
  // causing a hardfault when it tries to access those locations.
  // target();
}

void runApp(void* app_ptr) {
  runApp(app_ptr, app_ptr);
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

BootProto::RespStatus process_bootloader_command(I2C &i2c, MemoryPacketReader& packet) {
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
      if (computed_crc != crc) {
        return BootProto::kRespInvalidChecksum;
      }

      return bootloader.write(addr, data, data_length);

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
      return bootloader.erase(addr, length);
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
      bootloader.run_app(addr);
    }

    return BootProto::kRespDone;
  } else {  // unknown command
    return BootProto::kRespInvalidFormat;
  }
}

int bootloaderMaster() {
  DigitalIn i2cUp1 = DigitalIn(D4);
  DigitalIn i2cUp2 = DigitalIn(D5);

  I2C i2c(D4, D5);
  uint8_t i2cData[BootProto::kMaxPayloadLength];

  i2cUp1.mode(PullUp);
  i2cUp2.mode(PullUp);

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

  // TODO: account for more than 10 devices
  usb_uart.putc(numDevices + '1');
  ext_uart.putc(numDevices + '1');
  usb_uart.puts(" devices in chain\r\n");
  ext_uart.puts(" devices in chain\r\n");

  if (!masterRunAppPin) {
    for (size_t i=0; i<numDevices; i++) {
      BufferedPacketBuilder<BootProto::kMaxPayloadLength> i2cPacket;
      i2cPacket.put<uint8_t>(BootProto::kCmdRunApp);
      i2cPacket.put<uint32_t>(0);
      i2c.write(BootProto::GetDeviceAddr(i),
          (char*)i2cPacket.getBuffer(), i2cPacket.getLength());
    }
    runApp(kBootloaderDataBeginPtr, kAppBeginPtr);
  }

  statusLED.setIdlePolarity(true);

  Timer heartbeatTimer;
  heartbeatTimer.start();

  BufferedPacketReader<BootProto::kMaxPayloadLength> packet;
  COBSDecoder decoder;
  decoder.set_buffer(&packet);

  while (1) {
    if (bootInPin == 0) {
      NVIC_SystemReset();
    }

    while (usb_uart.readable() || ext_uart.readable()) {
      uint8_t rx = usb_uart.readable() ? (uint8_t)usb_uart.getc() : (uint8_t)ext_uart.getc();
      size_t bytes_decoded;
      COBSDecoder::COBSResult result = decoder.decode(&rx, 1, &bytes_decoded);

      if (result == COBSDecoder::kResultDone) {
        BootProto::RespStatus status = process_bootloader_command(i2c, packet);
        if (status == BootProto::kRespDone) {
          usb_uart.puts("D\n");
          ext_uart.puts("D\n");
        } else if (status == BootProto::kRespInvalidFormat) {
          usb_uart.puts("I\n");
          ext_uart.puts("I\n");
        } else if (status == BootProto::kRespInvalidArgs) {
          usb_uart.puts("A\n");
          ext_uart.puts("A\n");
        } else if (status == BootProto::kRespInvalidChecksum) {
          usb_uart.puts("C\n");
          ext_uart.puts("C\n");
        } else if (status == BootProto::kRespFlashError) {
          usb_uart.puts("F\n");
          ext_uart.puts("F\n");
        } else if (status == BootProto::kRespUnknownError) {
          usb_uart.puts("U\n");
          ext_uart.puts("U\n");
        } else {
          usb_uart.puts("?\n");
          ext_uart.puts("?\n");
        }
        packet.reset();
        decoder.set_buffer(&packet);
      }

      statusLED.pulse(kActivityPulseTimeMs);
    }

    statusLED.update();
    if (heartbeatTimer.read_ms() >= (int)kHeartbeatPeriodMs) {
        heartbeatTimer.reset();
      statusLED.pulse(kHeartbeatPulseTimeMs);
    }
  }
  return 0;
}

int bootloaderSlaveInit() {
  statusLED.setIdlePolarity(false);

  Timer heartbeatTimer;
  heartbeatTimer.start();

  // Wait for BOOT pin to go high
  while (1) {
    if (bootInPin == 1) {
      break;
    }

    statusLED.update();
    if (heartbeatTimer.read_ms() >= (int)kInitHeartbeatPeriodMs) {
        heartbeatTimer.reset();
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
    if (heartbeatTimer.read_ms() >= (int)kInitHeartbeatPeriodMs) {
      heartbeatTimer.reset();
      statusLED.pulse(kHeartbeatPulseTimeMs);
    }
  }

  i2c.address(address);

  BufferedPacketReader<BootProto::kMaxPayloadLength> i2cPacket;
  // If anything other than kRespDone, this is a command parser status
  // and takes priority over the actual bootloader status (which should be
  // "not running").
  BootProto::RespStatus lastStatus = BootProto::kRespDone;

  // Main bootloader loop
  while (1) {
    if (bootInPin == 0) {
      NVIC_SystemReset();
    }

    BootProto::RespStatus status = bootloader.async_update();

    if (status == BootProto::kRespBusy) {
      statusLED.pulse(kActivityPulseTimeMs);
    }

    switch (i2c.receive()) {
    case I2CSlave::ReadAddressed:
      statusLED.pulse(kActivityPulseTimeMs);
      if (lastCommand == BootProto::kCmdStatus) {
        if (lastStatus == BootProto::kRespDone) {
          i2c.write(status);
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

          bootloader.async_erase(startAddr, len);
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
              bootloader.async_write(startAddr, data, len);
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
          bootloader.run_app(addr);
        }
      } else {
        // Drop everything else
      }
      break;
    }

    statusLED.update();
    if (heartbeatTimer.read_ms() >= (int)kHeartbeatPeriodMs) {
      heartbeatTimer.reset();
      statusLED.pulse(kHeartbeatPulseTimeMs);
    }
  }
}

int main() {
  bootOutPin = 0;

  wait_ms(BootProto::kBootscanDelayMs);  // wait for some time to let boot in stabilize

  usb_uart.baud(115200);
  ext_uart.baud(115200);

  // floating or high means master mode
  if (bootInPin == 1) {
    wait_ms(5*BootProto::kBootscanDelayMs);

    usb_uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ "), master\r\n");
    ext_uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ "), master\r\n");

    return bootloaderMaster();
  } else {

	usb_uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ "), slave\r\n");
	ext_uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ "), slave\r\n");

    return bootloaderSlaveInit();
  }
}
