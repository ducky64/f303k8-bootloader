/*
 * main.cpp
 *
 *  Created on: Mar 23, 2016
 *      Author: ducky
 */

#include "mbed.h"

PwmOut led0(D9);
PwmOut led1(D10);

DigitalOut mainLed(LED1);

DigitalIn bootInPin(D3, PullUp);

int main() {
  RawSerial uart(SERIAL_TX, SERIAL_RX);
  uart.baud(115200);
  uart.puts("\r\n\r\nBuilt " __DATE__ " " __TIME__ " (" __FILE__ ")\r\n");

  int step = 0;
  bool dir = true;

  while (1) {
    if (bootInPin == 0) {
      NVIC_SystemReset();
    }

    if (dir) {
      step += 1;
    } else {
      step -= 1;
    }
    if (step == 128) {
      dir = false;
      mainLed = !mainLed;
    } else if (step == 0) {
      dir = true;
      mainLed = !mainLed;
    }
    int inv_step = 128 - step;

    led0 = (step * step) / 16384.f;
    led1 = (inv_step * inv_step) / 16384.f;

    wait(0.01);
  }
  return 0;
}

