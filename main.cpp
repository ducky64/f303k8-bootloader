/*
 * main.cpp
 *
 *  Created on: Mar 23, 2016
 *      Author: ducky
 */

#include "mbed.h"
#include "mbed_rpc.h"

PwmOut led0(D11);
PwmOut led1(D12);

int main() {
  Serial uart(SERIAL_TX, SERIAL_RX);
  uart.baud(115200);
  uart.printf("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ ")\r\n");

  RPC::add_rpc_class<RpcDigitalOut>();

  const size_t RPC_BUFSIZE = 256;
  char rpc_inbuf[RPC_BUFSIZE], rpc_outbuf[RPC_BUFSIZE];
  char* rpc_inptr = rpc_inbuf;  // next received byte pointer

  int step = 0;
  bool dir = true;

  while (1) {
    while (uart.readable()) {
      char rx = uart.getc();
      if (rx == '\n' || rx == '\r') {
        *rpc_inptr = '\0';  // optionally append the string terminator
        bool rpc_rtn = RPC::call(rpc_inbuf, rpc_outbuf);
        uart.printf("%s >> (%i) %s\r\n", rpc_inbuf, rpc_rtn, rpc_outbuf);
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

    if (dir) {
      step += 1;
    } else {
      step -= 1;
    }
    if (step == 128) {
      dir = false;
    } else if (step == 0) {
      dir = true;
    }
    int inv_step = 128 - step;

    led0 = (step * step) / 16384.f;
    led1 = (inv_step * inv_step) / 16384.f;

    wait(0.01);
  }
  return 0;
}

