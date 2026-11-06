#include "analog_touch.h"

void AnalogTouch::reset() {
  calibrationCount = 0;
  for (int i = 0; i < 12; ++i) {
    idleSum[i] = 0;
    idle[i] = 0;
    noisePeak[i] = 0;
  }
  for (int row = 0; row < 2; ++row) {
    weakAt[row] = 0;
    weakCandidate[row] = 0;
    weakActive[row] = false;
    for (int i = 0; i < 6; ++i)
      previous[row][i] = -1;
  }
}
AnalogFrame AnalogTouch::update(const AnalogSample &sample, uint32_t now) {
  AnalogFrame frame = {};
  frame.rowPosition[0] = frame.rowPosition[1] = -1;
  for (int i = 0; i < 12; ++i)
    frame.delta[i] = int(sample.baseline[i]) - int(sample.filtered[i]);
  // Startup calibration must be hands-off. Require 16 successive samples with
  // no hardware touches; do not adapt the baseline underneath a held finger.
  if (calibrationCount < 16) {
    if (sample.touched) {
      reset();
      return frame;
    }
    for (int i = 0; i < 12; ++i) {
      idleSum[i] += frame.delta[i];
      if (!calibrationCount || frame.delta[i] > noisePeak[i])
        noisePeak[i] = frame.delta[i];
    }
    if (++calibrationCount == 16)
      for (int i = 0; i < 12; ++i)
        idle[i] = idleSum[i] / 16;
    frame.calibrated = calibrationCount == 16;
    return frame;
  }
  frame.calibrated = true;
  for (int row = 0; row < 2; ++row) {
    int strength[6] = {};
    const int base = row * 6;
    for (int col = 0; col < 6; ++col) {
      int i = base + col;
      strength[col] = frame.delta[i] - idle[i];
      if (strength[col] < 0)
        strength[col] = 0;
      // Hardware classification wins even if baseline quantization makes a
      // touched electrode's externally visible delta very small.
      if ((sample.touched & (1u << i)) && !strength[col])
        strength[col] = 1;
    }
    // A pronounced valley between two peaks is evidence of two contacts,
    // not a broad single finger. Keep broad, single-peaked contacts together.
    for (int col = 1; col < 5; ++col) {
      if (strength[col] * 3 < strength[col - 1] &&
          strength[col] * 3 < strength[col + 1])
        strength[col] = 0;
    }
    // Qualify adjacent weak contacts by a combined calibrated-threshold budget,
    // above the observed idle peaks on BOTH electrodes, with a 60 ms dwell.
    // No interpolation is performed between electrodes 5 and 6 (different rows).
    uint8_t eligible = 0;
    for (int col = 0; col < 5; ++col) {
      int a = base + col, b = a + 1;
      int threshold = weakActive[row] ? releaseThreshold : touchThreshold;
      if (strength[col] > 0 && strength[col + 1] > 0 && frame.delta[a] > noisePeak[a] &&
          frame.delta[b] > noisePeak[b] &&
          strength[col] + strength[col + 1] >= threshold)
        eligible |= 1u << col;
    }
    if (eligible != weakCandidate[row]) {
      weakCandidate[row] = eligible;
      weakAt[row] = now;
      weakActive[row] = false;
    }
    if (!eligible)
      weakActive[row] = false;
    else if (allowWeakPairs && uint32_t(now - weakAt[row]) >= 60)
      weakActive[row] = true;
    int contacts = 0, lastPosition = -1;
    int8_t nextPrevious[6] = {-1, -1, -1, -1, -1, -1};
    for (int start = 0; start < 6;) {
      if (!strength[start]) {
        ++start;
        continue;
      }
      int end = start, sum = 0, weighted = 0;
      bool hardware = false;
      while (end < 6 && strength[end] > 0) {
        sum += strength[end];
        weighted += strength[end] * end * 200;
        hardware = hardware || (sample.touched & (1u << (base + end)));
        ++end;
      }
      bool weak = false;
      if (weakActive[row])
        for (int i = start; i < end - 1; ++i)
          weak = weak || (eligible & (1u << i));
      if (hardware || weak) {
        // Position in hundredths of a virtual interval (physical pitch=200).
        int position = weighted / sum;
        int pad = (position + 50) / 100;
        int old = -1;
        for (int i = start; i < end; ++i)
          if (previous[row][i] >= 0) {
            old = previous[row][i];
            break;
          }
        // 15% interval hysteresis prevents boundary chatter without smoothing
        // away a fast swipe. A clear move immediately selects the new pad.
        if (old >= 0 && position >= old * 100 - 65 && position <= old * 100 + 65)
          pad = old;
        for (int i = start; i < end; ++i)
          nextPrevious[i] = pad;
        frame.mask |= 1ul << (row * 11 + pad);
        lastPosition = row == 0 ? position : 1000 - position;
        ++contacts;
      }
      start = end;
    }
    for (int i = 0; i < 6; ++i)
      previous[row][i] = nextPrevious[i];
    if (contacts == 1)
      frame.rowPosition[row] = lastPosition;
  }
  return frame;
}
