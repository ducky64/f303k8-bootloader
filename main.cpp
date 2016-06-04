/*
 * main.cpp
 *
 *  Created on: Mar 23, 2016
 *      Author: ducky
 */

#include "mbed.h"
#include "isp_f303k8.h"

Serial uart(SERIAL_TX, SERIAL_RX);

DigitalIn sw(D7);

DigitalOut led0(D11);
DigitalOut led1(D12);

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
    // Use statics since the stack pointer gets reset without the compiler knowing.
    static uint32_t stack_ptr = 0;
    static void (*target)(void) = 0;

    stack_ptr = (*(uint32_t*)(APP_BEGIN_PTR + 0));
    target = (void (*)(void))(*(uint32_t*)(APP_BEGIN_PTR + 4));

    // Just to be extra safe
    for (uint8_t i=0; i<NVIC_NUM_VECTORS; i++) {
      NVIC_DisableIRQ((IRQn_Type)i);
    }

    // Reset the interrupt table so the application can re-load it
    SCB->VTOR = (uint32_t)APP_BEGIN_PTR;

    __set_MSP(stack_ptr);

    target();
    return 63;
  } else {  // unknown command
    return 125;
  }
}

int main() {
  F303K8ISP isp;

  sw.mode(PullUp);

  uart.baud(115200);
  uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ ")\r\n");

  uart.printf("Device ID: 0x% 8x, Device Serial: 0x% 8x\r\n",
      isp.get_device_id(), isp.get_device_serial());
  uart.printf("Bootloader address range: 0x% 8x - 0x% 8x\r\n",
      (uint32_t)BL_BEGIN_PTR, (uint32_t)APP_BEGIN_PTR);
  uart.printf("Bootloader unlock: %i\r\n",
      isp.isp_begin());

  uart.printf("Bootloader erase: %i\r\n",
      isp.erase(APP_BEGIN_PTR, 32768));

  dumpmem(uart, BL_BEGIN_PTR, 256, 16);

  I2C i2c(D4, D5);

  const size_t RPC_BUFSIZE = 4096;
  char rpc_inbuf[RPC_BUFSIZE], rpc_outbuf[RPC_BUFSIZE];
  char* rpc_inptr = rpc_inbuf;  // next received byte pointer

  uint8_t led = 0;

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
    }
  }
  return 0;
}

