#pragma once
#include "Adafruit_MPR121.h"
#include "gesture_engine.h"
#include "board_config.h"

#ifndef VRING_ANALOG_TOUCH
#define VRING_ANALOG_TOUCH 0
#endif
#ifndef VRING_WEAK_PAIRS
#define VRING_WEAK_PAIRS 1
#endif
class TouchHandler {
public:
  static constexpr uint8_t TOUCH_THRESHOLD = board::kTouchThreshold,
                           RELEASE_THRESHOLD = board::kReleaseThreshold;
  bool begin(uint8_t address = board::kTouchAddress) {
    if (!cap.begin(address))
      return false;
    cap.setThresholds(TOUCH_THRESHOLD, RELEASE_THRESHOLD);
#if VRING_ANALOG_TOUCH
    analog.reset();
    lastSample = millis() - 10;
#endif
    gestures = GestureEngine();
    return true;
  }
  TouchInfo read_touch() {
    uint32_t now = millis();
#if VRING_ANALOG_TOUCH
    if (uint32_t(now - lastSample) < 10)
      return {NONE, -1, -1};
    lastSample = now;
    AnalogSample sample;
    if (!cap.readTouchSample(sample.touched, sample.filtered, sample.baseline)) {
      // Never turn an I2C fault into a tap, hold, or synthetic touch.
      gestures.suppressUntilRelease();
      frame.mask = 0;
      return {NONE, -1, -1};
    }
    frame = analog.update(sample, now);
    return gestures.updateVirtual(frame.mask, now);
#else
    return gestures.update(cap.touched(), now);
#endif
  }
  const AnalogFrame &diagnostics() const {
    return frame;
  }
  void suppressUntilRelease() {
    gestures.suppressUntilRelease();
  }
  Adafruit_MPR121 cap;

private:
  GestureEngine gestures;
#if VRING_ANALOG_TOUCH
  AnalogTouch analog{TOUCH_THRESHOLD, RELEASE_THRESHOLD, VRING_WEAK_PAIRS != 0};
#endif
  AnalogFrame frame{};
  uint32_t lastSample = 0;
};
