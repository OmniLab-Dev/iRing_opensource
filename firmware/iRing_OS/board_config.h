#pragma once
#include <stdint.h>

namespace board {
constexpr int kDisplayWidth = 160;
constexpr int kDisplayHeight = 32;
constexpr int kStatusWidth = 16;
constexpr uint8_t kOledCs = 15;
constexpr uint8_t kOledReset = 16;
constexpr uint8_t kOledClock = 12;
constexpr uint8_t kOledData = 13;
constexpr uint8_t kTouchAddress = 0x5B;
constexpr uint8_t kTouchThreshold = 2;
constexpr uint8_t kReleaseThreshold = 1;
constexpr uint8_t kOpticalAddress = 0x57;
constexpr uint8_t kOpticalLedAmplitude = 60;
constexpr uint8_t kOpticalAverage = 4;
constexpr int kOpticalSampleRate = 100;
constexpr int kOpticalPulseWidth = 411;
constexpr int kOpticalAdcRange = 4096;
} // namespace board
