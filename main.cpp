/*
 * main.cpp
 *
 *  Created on: Mar 23, 2016
 *      Author: ducky
 */

#include "mbed.h"
#include "endian.h"

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

void dumpmem(Serial& serial, void* start_addr, size_t length, size_t bytes_per_line) {
  serial.printf("\r\n");
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
 * Reads through a array containing ASCII hex bytes and converts it into raw hex
 * bytes. length is the number of bytes in the output (2x the number of
 * characters in the input). Returns true if all bytes were converted, otherwise
 * false (for example, if detected non-hex bytes).
 * out may alias with in, doing an in-place decode.
 */
bool ascii_hex_to_uint8_array(uint8_t *out, uint8_t *in, size_t length) {
  while (length > 0) {
    uint8_t high_nibble, low_nibble;
    if (!hex_char_to_nibble(in[0], &high_nibble)
        || !hex_char_to_nibble(in[1], &low_nibble)) {
      return false;
    }
    *out = (high_nibble << 4) | low_nibble;
    in += 2;
    out += 1;
    length -= 1;
  }
  return true;
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
  __set_PSP(stack_ptr);

  goto *target;

  // TODO: replace with standard function call instead of goto.
  // For some reason, this increments the stack pointer past the top of memory,
  // causing a hardfault when it tries to access those locations.
  // target();
}

uint8_t process_bootloader_command(ISPBase &isp, I2C &i2c, uint8_t* cmd, size_t length) {
  uint8_t i2cData[BLPROTO::MAX_PAYLOAD_LEN];

  if (cmd[0] == 'W') {
    size_t data_length = length - 17;
    if (data_length % 2 != 0) {
      return 127; // not byte aligned
    }
    data_length = data_length / 2;

    if (!ascii_hex_to_uint8_array(cmd, cmd+1, 2)) {
      return 125;
    }
    uint16_t device = deserialize_uint16(cmd) - 1;

    if (!ascii_hex_to_uint8_array(cmd, cmd+5, 4)) {
      return 124;
    }
    uint32_t addr = deserialize_uint32(cmd);

    if (!ascii_hex_to_uint8_array(cmd, cmd+13, 2)) {
      return 123;
    }
    uint16_t crc = deserialize_uint16(cmd);

    if (!ascii_hex_to_uint8_array(cmd, cmd+17, data_length)) {
      return 126;
    }

//    uart.printf("I2C Write %x: ptr 0x% 8x, crc 0x% 4x, len %i  ", device, addr, crc, data_length);

    i2cData[0] = BLPROTO::CMD_WRITE;
    serialize_uint32(i2cData+1, addr);
    serialize_uint16(i2cData+5, (uint16_t)data_length);
    serialize_uint16(i2cData+7, crc);
    for (size_t i=0; i<data_length; i++) {
      i2cData[i+9] = cmd[i];
    }

    i2c.write(BLPROTO::DEVICE_ADDR(device), (char*)i2cData, data_length + 9);

    uint8_t resp = BLPROTO::RESP_STATUS_BUSY;
    while (resp == BLPROTO::RESP_STATUS_BUSY) {
      i2c.frequency(I2C_FREQUENCY); // reset the I2C device
      i2cData[0] = BLPROTO::CMD_STATUS;
      i2c.write(BLPROTO::DEVICE_ADDR(device), (char*)i2cData, 1);
      i2c.read(BLPROTO::DEVICE_ADDR(device), (char*)i2cData, 1);
      resp = i2cData[0];
    }

    return resp;
  } else if (cmd[0] == 'E') {
    if (!ascii_hex_to_uint8_array(cmd, cmd+1, 2)) {
      return 125;
    }
    uint16_t device = deserialize_uint16(cmd) - 1;

    if (!ascii_hex_to_uint8_array(cmd, cmd+5, 4)) {
      return 124;
    }
    uint32_t addr = deserialize_uint32(cmd);

    if (!ascii_hex_to_uint8_array(cmd, cmd+13, 4)) {
      return 123;
    }
    uint32_t length = deserialize_uint32(cmd);

    i2cData[0] = BLPROTO::CMD_ERASE;
    serialize_uint32(i2cData+1, addr);
    serialize_uint32(i2cData+5, length);

    i2c.write(BLPROTO::DEVICE_ADDR(device), (char*)i2cData, 9);

    uint8_t resp = BLPROTO::RESP_STATUS_BUSY;
    while (resp == BLPROTO::RESP_STATUS_BUSY) {
      i2c.frequency(I2C_FREQUENCY); // reset the I2C device
      i2cData[0] = BLPROTO::CMD_STATUS;
      i2c.write(BLPROTO::DEVICE_ADDR(device), (char*)i2cData, 1);
      i2c.read(BLPROTO::DEVICE_ADDR(device), (char*)i2cData, 1);
      resp = i2cData[0];
    }

    return resp;
  } else if (cmd[0] == 'J') {
    if (!ascii_hex_to_uint8_array(cmd, cmd+1, 2)) {
      return 125;
    }
    uint16_t device = deserialize_uint16(cmd) - 1;

    if (!ascii_hex_to_uint8_array(cmd, cmd+5, 4)) {
      return 124;
    }
    uint32_t addr = deserialize_uint32(cmd);

    i2cData[0] = BLPROTO::CMD_JUMP;
    serialize_uint32(i2cData+1, (uint32_t)APP_BEGIN_PTR);

    i2c.write(BLPROTO::DEVICE_ADDR(device), (char*)i2cData, 5);

    return 0;
  } else {  // unknown command
    return 63;
  }
}

int bootloaderMaster() {
  F303K8ISP isp;
  uart.printf("Device ID: 0x% 8x, Device Serial: 0x% 8x\r\n",
      isp.get_device_id(), isp.get_device_serial());
  uart.printf("Bootloader address range: 0x% 8x - 0x% 8x\r\n",
      (uint32_t)BL_BEGIN_PTR, (uint32_t)APP_BEGIN_PTR);
  uart.printf("Flash unlock: %i\r\n",
      isp.isp_begin());

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

  uart.printf("Discovered %i devices\n", numDevices);

  statusLED.setIdlePolarity(true);

  Timer heartbeatTimer;
  uint32_t nextHeartbeatTime = 0;
  heartbeatTimer.start();

  const size_t RPC_BUFSIZE = 1024;
  static char rpc_inbuf[RPC_BUFSIZE], rpc_outbuf[RPC_BUFSIZE];
  char* rpc_inptr = rpc_inbuf;  // next received byte pointer

  while (1) {
    while (uart.readable() || uart_in.readable()) {
      char rx = uart.readable() ? uart.getc() : uart_in.getc();
      if (rx == '\n' || rx == '\r') {
        *rpc_inptr = '\0';  // optionally append the string terminator
        uint8_t rtn = process_bootloader_command(isp, i2c,
            (uint8_t*)rpc_inbuf, rpc_inptr - rpc_inbuf);
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

    if (isp.async_update()) {
      statusLED.pulse(LED_PULSE_TIME);
    }

    switch (i2c.receive()) {
    case I2CSlave::ReadAddressed:
      statusLED.pulse(LED_PULSE_TIME);
      if (lastI2CCommand == BLPROTO::CMD_PING) {
        i2c.write(BLPROTO::RESP_PING);
      } else if (lastI2CCommand == BLPROTO::CMD_STATUS) {
        uint8_t status;
        if (isp.get_last_async_status(&status)) {
          if (status == 0) {
            i2c.write(BLPROTO::RESP_STATUS_DONE);
          } else {
            // TODO: differentiate argcheck / other error reporting
            i2c.write(BLPROTO::RESP_STATUS_ERR_FLASH);
          }
        } else {
          i2c.write(BLPROTO::RESP_STATUS_BUSY);
        }
      } else {
        // Drop everything else
      }
      break;
    case I2CSlave::WriteAddressed:
      statusLED.pulse(LED_PULSE_TIME);
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
      } else if (lastI2CCommand == BLPROTO::CMD_WRITE) {
        if (!i2c.read((char*)i2cData, 8)) {
          uint32_t startAddr = deserialize_uint32(i2cData+0);
          uint16_t len = deserialize_uint16(i2cData+4);
          uint16_t crc = deserialize_uint16(i2cData+6);
          // TODO: allow len >= 256 transfers by breaking i2c read calls into
          // smaller chunks
          if (!i2c.read((char*)i2cData, len)) {
            // TODO: CRC check
            isp.async_write((void*)startAddr, i2cData, len);
          } else {
            // TODO: better error handling
          }
        } else {
          // TODO: better error handling
        }
      } else if (lastI2CCommand == BLPROTO::CMD_JUMP) {
        if (!i2c.read((char*)i2cData, 4)) {
          uint32_t addr = deserialize_uint32(i2cData);
          isp.isp_end();
          dumpmem(uart, (void*)addr, 1024, 16);
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

  wait_ms(BLPROTO::DELAY_MS_BOOTSCAN);  // wait for some time to let boot in stabilize

  if (bootInPin == 1) {
    wait_ms(BLPROTO::DELAY_MS_BOOTSCAN);
    return bootloaderMaster();
  } else {
    return bootloaderSlaveInit();
  }
}
