#pragma once
#include <stdint.h>
// Scrolling IR waveform with an independent sample history and display scaling.
class HealthWaveform {
public:
  static constexpr int CAPACITY = 60;
  void clear() {
    head = count = 0;
  }
  int size() const {
    return count;
  }
  void add(uint32_t value) {
    samples[head] = value;
    head = (head + 1) % CAPACITY;
    if (count < CAPACITY)
      ++count;
  }
  uint32_t at(int index) const {
    if (index < 0 || index >= count)
      return 0;
    return samples[(head + CAPACITY - count + index) % CAPACITY];
  }
  void buildY(uint8_t *output, int width, int height) const {
    if (!output || width <= 0 || height <= 0 || height > 256)
      return;
    if (!count) {
      for (int x = 0; x < width; ++x)
        output[x] = (height - 1) / 2;
      return;
    }
    uint32_t low = at(0), high = low;
    for (int i = 1; i < count; ++i) {
      uint32_t value = at(i);
      if (value < low)
        low = value;
      if (value > high)
        high = value;
    }
    // Avoid magnifying a constant signal or a few ADC counts into a full pulse.
    if (high - low < 32) {
      uint32_t center = low + (high - low) / 2;
      low = center > 16 ? center - 16 : 0;
      high = low + 32;
    }
    uint32_t range = high - low;
    for (int x = 0; x < width; ++x) {
      uint32_t value;
      if (width == 1 || count == 1)
        value = at(0);
      else {
        int denominator = width - 1;
        int offset = x * (count - 1), index = offset / denominator;
        int fraction = offset % denominator;
        uint32_t a = at(index), b = at(index + 1 < count ? index + 1 : index);
        value = (uint64_t(a) * (denominator - fraction) + uint64_t(b) * fraction) /
                denominator;
      }
      output[x] = (height - 1) - uint64_t(value - low) * (height - 1) / range;
    }
  }

private:
  uint32_t samples[CAPACITY] = {};
  uint8_t head = 0, count = 0;
};
