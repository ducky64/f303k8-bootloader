#include "mbed.h"

#ifndef ACTIVITY_LED_H_
#define ACTIVITY_LED_H_

/**
 * A realtime polled (non-interrupt, requires periodic explicit update) blinky
 * LED. Can be asynchronously toggled and set to fire for some amount of time.
 *
 * TODO: allow timer overflow
 */
class ActivityLED {
public:
  ActivityLED(PinName ledPin, bool inIdleValue=true) :
      led(ledPin), idleValue(inIdleValue),
      inPulse(false), nextPulseLength(0), nextResetLength(0) {
    timer.start();
    led = idleValue;
  }

  /**
   * Pulses the LED. If the LED is in the active state, does nothing. If the LED
   * is in the idle state, either turns the LED active immediately (if one pulse
   * length past the reset edge) or queues another LED pulse.
   */
  void pulse(uint32_t pulseLengthMs) {
    nextPulseLength = pulseLengthMs;
    nextResetLength = pulseLengthMs;
  }

  /**
   * Sets the LED value at idle. Does not affect the current LED value if
   * currently in a pulse.
   */
  void setIdlePolarity(bool inIdlePolarity) {
    idleValue = inIdlePolarity;
    if (!inPulse) {
      led = idleValue;
    }
  }

  /**
   * Call this periodically to update the LED status.
   */
  void update() {
    uint32_t currentTime = timer.read_ms();

    if (inPulse) {
      if (currentTime >= nextResetEdge) {
        if (nextPulseLength > 0) {
          led = !idleValue;
          nextIdleEdge = nextResetEdge + nextPulseLength;
          nextResetEdge = nextIdleEdge + nextResetLength;
          nextPulseLength = 0;
        } else {
          led = idleValue;
          inPulse = false;
        }
      } else if (currentTime >= nextIdleEdge) {
        led = idleValue;
      }
    } else if (nextPulseLength > 0) {
      led = !idleValue;
      nextIdleEdge = currentTime + nextPulseLength;
      nextResetEdge = nextIdleEdge + nextResetLength;
      nextPulseLength = 0;
      inPulse = true;
    }
  }

private:
  DigitalOut led;
  Timer timer;
  bool idleValue;

  bool inPulse; // whether currently in a pulse
  uint32_t nextIdleEdge;   // timer value to return the LED to the idle state
  uint32_t nextResetEdge;  // timer value at end of full pulse cycle (active + idle), must be >= nextIdleEdge

  uint32_t nextPulseLength; // after the current pulse, the active length of the next one, or 0 if none queued
  uint32_t nextResetLength; // after the current pulse, the idle length of the next one
};

#endif
