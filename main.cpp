/*
 * main.cpp
 *
 *  Created on: Mar 23, 2016
 *      Author: ducky
 */

#include "mbed.h"
#include "endian.h"

#include "ActivityLED.h"
#include "isp_f303k8.h"

DigitalIn bootInPin(D3, PullDown);
DigitalOut bootOutPin(D6);

ActivityLED statusLED(LED1);

const uint32_t I2C_FREQUENCY = 400000;

const uint32_t LED_PULSE_TIME = 50;

const uint32_t HEARTBEAT_PERIOD = 1000;
const uint32_t HEARTBEAT_INIT_PERIOD = 500;
const uint32_t HEARTBEAT_PULSE_TIME = LED_PULSE_TIME;

namespace BLPROTO {
  const uint32_t DELAY_MS_BOOTSCAN = 50;  // delay, in ms, before doing the initial BOOT_IN check to determine master/slave
  const uint32_t DELAY_MS_BOOT_TO_ADDR = 10;

  const uint8_t ADDRESS_GLOBAL = 0x42 << 1;

  const size_t MAX_PAYLOAD_LEN = 512; // TODO: size this more optimally

  // CMD_PING
  // <- RESP_PING
  // Global and addressed mode.
  const uint8_t CMD_PING = 0x02;
  const uint8_t RESP_PING = 0x06;

  // CMD_SET_ADDRESS (uint8 newAddress)
  // Global mode only. Sets the I2C address of the device.
  const uint8_t CMD_SET_ADDRESS = 0x04;

  // CMD_SET_BOOTOUT
  // Sets the boot out pin high.
  const uint8_t CMD_SET_BOOTOUT = 0x03;

  // CMD_ERASE (uint32 startAddress) (uint32 length)
  // Erases a block.
  const uint8_t CMD_ERASE = 0x06;

  // CMD_WRITE_DATA (uint32 startAddress) (uint16 length) (uint16 CRC16)
  //   (length*uint8 data)
  // Writes data of length starting at the specified address.
  // CRC is of the data only.
  const uint8_t CMD_WRITE_DATA = 0x06;

  // CMD_RUN (uint32 address)
  // Runs the program beginning at the specified address. Not a simple jump,
  // also sets up the initial PC, stack pointer, and vector table pointer.
  const uint8_t CMD_JUMP = 0x08;

  // CMD_LAST_STATUS
  // <- RESP_STATUS_BUSY 0x00
  // or RESP_STATUS_DONE (uint8 status)
  // or RESP_STATUS_NONE 0x00
  const uint8_t CMD_STATUS = 0x10;

  const uint8_t RESP_STATUS_BUSY = 0x01;
  const uint8_t RESP_STATUS_DONE = 0x02;

  // Given the device number (position in chain), return the mbed-style I2C
  // address.
  constexpr uint8_t DEVICE_ADDR(uint8_t device_num) {
    return (device_num + 1) << 1;
  }
}

void dumpmem(Serial& serial, void* start_addr, size_t length, size_t bytes_per_line) {
  for (size_t maj=0; maj<length; maj+=bytes_per_line) {
    serial.printf("% 8x: ", (uint32_t)start_addr);
    // TODO: allow partial line prints
    for (size_t min=0; min<bytes_per_line; min+=1) {
      serial.printf("%02x ", *(uint8_t*)start_addr);
      start_addr += 1;
    }
    serial.printf("\r\n");
  }
}

extern char _FlashStart, _FlashEnd;
void* const BL_BEGIN_PTR = &_FlashStart;
void* const APP_BEGIN_PTR = &_FlashEnd;

bool hex_char_to_nibble(uint8_t hex_char, uint8_t* nibble_out) {
  if (hex_char >= '0' && hex_char <= '9') {
    *nibble_out = hex_char - '0';
    return true;
  } else if (hex_char >= 'A' && hex_char <= 'F') {
    *nibble_out = hex_char - 'A' + 0xA;
    return true;
  } else if (hex_char >= 'a' && hex_char <= 'f') {
    *nibble_out = hex_char - 'a' + 0xA;
    return true;
  } else {
    return false;
  }
}

/**
 * Runs an application at the specified address. Should not return under normal
 * circumstances.
 */
void runApp(void* addr) {
  // Use statics since the stack pointer gets reset without the compiler knowing.
  static uint32_t stack_ptr = 0;
  static void (*target)(void) = 0;

  stack_ptr = (*(uint32_t*)(addr + 0));
  target = (void (*)(void))(*(uint32_t*)(addr + 4));

  // Just to be extra safe
  for (uint8_t i=0; i<NVIC_NUM_VECTORS; i++) {
    NVIC_DisableIRQ((IRQn_Type)i);
  }

  // Reset the interrupt table so the application can re-load it
  SCB->VTOR = (uint32_t)addr;

  __set_MSP(stack_ptr);

  target();
}


// TODO FIX THIS and make this not awful global state
void* app_write_ptr = &_FlashEnd;

uint8_t process_bootloader_command(ISPBase &isp, uint8_t* cmd, size_t length) {
  if (cmd[0] == 'W') {
    size_t data_length = length - 1;
    if (data_length % 2 != 0) {
      return 127; // not byte aligned
    }
    data_length = data_length / 2;

    // translate ASCII hex cmd into raw bytes
    uint8_t* data_str = cmd + 1;
    uint8_t* data_bytes = cmd;

    for (size_t data_ind=0; data_ind<data_length; data_ind++) {
      uint8_t high_nibble, low_nibble;
      if (!hex_char_to_nibble(data_str[0], &high_nibble)
          || !hex_char_to_nibble(data_str[1], &low_nibble)) {
        return 126;
      }
      *data_bytes = (high_nibble << 4) | low_nibble;
      data_str += 2;
      data_bytes += 1;
    }
    uint8_t rtn = isp.write(app_write_ptr, cmd, data_length);
    app_write_ptr += data_length;
    return rtn;
  } else if (cmd[0] == 'J') {
    runApp(APP_BEGIN_PTR);
    return 63;
  } else {  // unknown command
    return 125;
  }
}

int bootloaderMaster() {
  Serial uart(SERIAL_TX, SERIAL_RX);
  uart.baud(115200);
  uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ ")\r\n");

  F303K8ISP isp;
  uart.printf("Device ID: 0x% 8x, Device Serial: 0x% 8x\r\n",
      isp.get_device_id(), isp.get_device_serial());
  uart.printf("Bootloader address range: 0x% 8x - 0x% 8x\r\n",
      (uint32_t)BL_BEGIN_PTR, (uint32_t)APP_BEGIN_PTR);
  uart.printf("Flash unlock: %i\r\n",
      isp.isp_begin());
  uart.printf("Flash erase: %i\r\n",
      isp.erase(APP_BEGIN_PTR, 32768));

  I2C i2c(D4, D5);
  uint8_t i2cData[BLPROTO::MAX_PAYLOAD_LEN];

  i2c.frequency(I2C_FREQUENCY);

  wait_ms(500);
  bootOutPin = 1;

  uint8_t numDevices = 0;

  while (1) {
    wait_ms(BLPROTO::DELAY_MS_BOOT_TO_ADDR);

    i2c.frequency(I2C_FREQUENCY); // reset the I2C device
    i2cData[0] = BLPROTO::CMD_PING;
    if (i2c.write(BLPROTO::ADDRESS_GLOBAL, (char*)i2cData, 1, true) != 0) {
      // Next device in chain didn't respond to ping, reached end of chain
      break;
    }
    i2c.read(BLPROTO::ADDRESS_GLOBAL, (char*)i2cData, 1);
    // TODO: better I2C robustness, like if device doesn't respond
    if (i2cData[0] != BLPROTO::RESP_PING) { break; }

    uint8_t thisDeviceAddress = BLPROTO::DEVICE_ADDR(numDevices);
    i2cData[0] = BLPROTO::CMD_SET_ADDRESS;
    i2cData[1] = thisDeviceAddress;
    i2c.write(BLPROTO::ADDRESS_GLOBAL, (char*)i2cData, 2);

    i2cData[0] = BLPROTO::CMD_SET_BOOTOUT;
    i2c.write(thisDeviceAddress, (char*)i2cData, 1);

    numDevices += 1;
  }

  i2cData[0] = BLPROTO::CMD_ERASE;
  serialize_uint32(i2cData+1, (uint32_t)APP_BEGIN_PTR);
  serialize_uint32(i2cData+5, 16384);

  i2c.write(BLPROTO::DEVICE_ADDR(1), (char*)i2cData, 9);

  uint8_t resp = BLPROTO::RESP_STATUS_BUSY;
  uint8_t status;
  while (resp == BLPROTO::RESP_STATUS_BUSY) {
    i2c.frequency(I2C_FREQUENCY); // reset the I2C device
    i2cData[0] = BLPROTO::CMD_STATUS;
    i2c.write(BLPROTO::DEVICE_ADDR(1), (char*)i2cData, 1);
    i2c.read(BLPROTO::DEVICE_ADDR(1), (char*)i2cData, 2);
    resp = i2cData[0];
    status = i2cData[1];
  }

  uart.printf("Status <- %i\n", status);

  statusLED.setIdlePolarity(true);

  Timer heartbeatTimer;
  uint32_t nextHeartbeatTime = 0;
  heartbeatTimer.start();

  const size_t RPC_BUFSIZE = 4096;
  static char rpc_inbuf[RPC_BUFSIZE], rpc_outbuf[RPC_BUFSIZE];
  char* rpc_inptr = rpc_inbuf;  // next received byte pointer

  while (1) {
    while (uart.readable()) {
      char rx = uart.getc();
      if (rx == '\n' || rx == '\r') {
        *rpc_inptr = '\0';  // optionally append the string terminator
        uint8_t rtn = process_bootloader_command(isp, (uint8_t*)rpc_inbuf, rpc_inptr - rpc_inbuf);
        uart.printf("D%i\n", rtn);
        rpc_inptr = rpc_inbuf;  // reset the received byte pointer
      } else {
        *rpc_inptr = rx;
        rpc_inptr++;  // advance the received byte pointer
        if (rpc_inptr >= rpc_inbuf + RPC_BUFSIZE) {
          // you should emit some helpful error on overflow
          rpc_inptr = rpc_inbuf;  // reset the received byte pointer, discarding what we have
        }
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
  Serial uart(SERIAL_TX, SERIAL_RX);
  uart.baud(115200);
  uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ ")\r\n");

  statusLED.setIdlePolarity(false);

  Timer heartbeatTimer;
  uint32_t nextHeartbeatTime = 0;
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
  i2c.address(BLPROTO::ADDRESS_GLOBAL);
  uint8_t address = BLPROTO::ADDRESS_GLOBAL;

  uint8_t lastI2CCommand = 0;

  // Wait for initialization I2C command to get address
  while (address == BLPROTO::ADDRESS_GLOBAL) {
    if (bootInPin == 0) {
      NVIC_SystemReset();
    }

    switch (i2c.receive()) {
    case I2CSlave::ReadAddressed:
      if (lastI2CCommand == BLPROTO::CMD_PING) {
        i2c.write(BLPROTO::RESP_PING);
      } else {
        // Drop everything else
      }
      break;
    case I2CSlave::WriteAddressed:
      lastI2CCommand = i2c.read();
      if (lastI2CCommand == BLPROTO::CMD_SET_ADDRESS) {
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


  uint8_t i2cData[BLPROTO::MAX_PAYLOAD_LEN];

  // Main bootloader loop
  while (1) {
    if (bootInPin == 0) {
      NVIC_SystemReset();
    }

    isp.async_update();

    switch (i2c.receive()) {
    case I2CSlave::ReadAddressed:
      if (lastI2CCommand == BLPROTO::CMD_PING) {
        i2c.write(BLPROTO::RESP_PING);
      } else if (lastI2CCommand == BLPROTO::CMD_STATUS) {
        uint8_t status;
        if (isp.get_last_async_status(&status)) {
          i2c.write(BLPROTO::RESP_STATUS_DONE);
          i2c.write(status);
        } else {
          i2c.write(BLPROTO::RESP_STATUS_BUSY);
          i2c.write(0xff);
        }
      } else {
        // Drop everything else
      }
      break;
    case I2CSlave::WriteAddressed:
      lastI2CCommand = i2c.read();
      if (lastI2CCommand == BLPROTO::CMD_SET_BOOTOUT) {
        bootOutPin = 1;
      } else if (lastI2CCommand == BLPROTO::CMD_ERASE) {
        if (!i2c.read((char*)i2cData, 8)) {
          uint32_t startAddr = deserialize_uint32(i2cData+0);
          uint32_t len = deserialize_uint32(i2cData+4);
          isp.async_erase((void*)startAddr, len);
        } else {
          // TODO: better error handling
        }
      } else if (lastI2CCommand == BLPROTO::CMD_JUMP) {
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

  wait_ms(BLPROTO::DELAY_MS_BOOTSCAN);  // wait for some time to let boot in stabilize

  if (bootInPin == 1) {
    wait_ms(BLPROTO::DELAY_MS_BOOTSCAN);
    return bootloaderMaster();
  } else {
    return bootloaderSlaveInit();
  }
}
