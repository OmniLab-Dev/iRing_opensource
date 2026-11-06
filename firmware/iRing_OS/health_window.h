#pragma once
#include <stdint.h>
// Acquisition uses 100 Hz / FIFO averaging 4 = 25 samples/s.
// These are optical acquisition heuristics, not clinical quality validation.
class HealthWindow {
public:
  static constexpr int SIZE = 100;
  uint32_t red[SIZE] = {}, ir[SIZE] = {};
  int count = 0;
  void clear() {
    count = 0;
  }
  bool add(uint32_t redRaw, uint32_t irRaw) {
    if (count == SIZE)
      return false;
    // Scale both channels equally into 16 bits to keep the reference algorithm's
    // 32-bit AC*DC arithmetic in range. Ratio-of-ratios is scale invariant.
    red[count] = redRaw >> 2;
    ir[count] = irRaw >> 2;
    ++count;
    return count == SIZE;
  }
  static bool contact(uint32_t redRaw, uint32_t irRaw) {
    return redRaw >= 1000 && irRaw >= 10000;
  }
  static bool saturated(uint32_t redRaw, uint32_t irRaw) {
    return redRaw >= 260000 || irRaw >= 260000;
  }
  bool usable() const {
    if (count != SIZE)
      return false;
    return usableChannel(red) && usableChannel(ir);
  }
  void advance() {
    if (count != SIZE)
      return;
    for (int i = 25; i < SIZE; ++i) {
      red[i - 25] = red[i];
      ir[i - 25] = ir[i];
    }
    count = 75;
  }

private:
  bool usableChannel(const uint32_t *values) const {
    uint32_t low = values[0], high = values[0], sum = 0;
    for (int i = 0; i < SIZE; ++i) {
      if (values[i] < low)
        low = values[i];
      if (values[i] > high)
        high = values[i];
      sum += values[i];
    }
    uint32_t mean = sum / SIZE, range = high - low;
    return range >= 8 && mean > 0 && range <= mean / 3;
  }
};
