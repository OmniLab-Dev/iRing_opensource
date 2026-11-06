#pragma once
#include <stdint.h>

// Virtual IDs follow wiring: 0..10 on electrodes 0..5, 11..21 on 6..11.
// Even row-local indices are real electrodes; odd indices lie between them.
inline int virtualColumn(int id) {
  return id < 11 ? id : 21 - id;
}
inline int physicalPad(int id) {
  return id < 11 ? (id + 1) / 2 : 6 + (id - 11 + 1) / 2;
}
inline int virtualPad(int pad) {
  return pad < 6 ? pad * 2 : 11 + (pad - 6) * 2;
}

struct AnalogSample {
  uint16_t touched;
  uint16_t filtered[12];
  uint16_t baseline[12];
};
struct AnalogFrame {
  uint32_t mask;          // 22 virtual pads; one position per contiguous contact.
  int16_t delta[12];      // Raw signed baseline minus filtered, for tuning.
  int16_t rowPosition[2]; // 0..1000 in physical left-to-right order, -1 if
                          // ambiguous/absent.
  bool calibrated;
};
class AnalogTouch {
public:
  // Preserve the real device thresholds. Weak-pair synthesis can be disabled
  // independently without changing interpolation of hardware-qualified contacts.
  AnalogTouch(uint8_t touch = 2, uint8_t release = 1, bool weakPairs = true)
      : touchThreshold(touch), releaseThreshold(release), allowWeakPairs(weakPairs) {}
  AnalogFrame update(const AnalogSample &sample, uint32_t now);
  void reset();

private:
  uint8_t touchThreshold, releaseThreshold;
  bool allowWeakPairs;
  uint8_t calibrationCount = 0;
  int32_t idleSum[12] = {};
  int16_t idle[12] = {}, noisePeak[12] = {};
  int8_t previous[2][6] = {{-1, -1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1, -1}};
  uint32_t weakAt[2] = {};
  uint8_t weakCandidate[2] = {};
  bool weakActive[2] = {};
};
