#include "../health_waveform.h"
#define CHECK(condition)                                                               \
  do {                                                                                 \
    if (!(condition))                                                                  \
      return __LINE__;                                                                 \
  } while (0)
extern "C" int run_tests() {
  HealthWaveform wave;
  uint8_t y[144];
  wave.buildY(y, 144, 32);
  for (int i = 0; i < 144; ++i)
    CHECK(y[i] == 15);
  CHECK(wave.size() == 0);
  wave.add(50000);
  wave.add(51000);
  wave.buildY(y, 144, 32);
  CHECK(y[0] == 31 && y[143] == 0);
  for (int i = 1; i < 144; ++i)
    CHECK(y[i] <= y[i - 1]);
  wave.clear();
  CHECK(wave.size() == 0 && wave.at(0) == 0);
  for (int i = 0; i < 100; ++i)
    wave.add(40000 + i * 100);
  CHECK(wave.size() == 60 && wave.at(0) == 44000 && wave.at(59) == 49900);
  CHECK(wave.at(-1) == 0 && wave.at(60) == 0);
  wave.buildY(y, 144, 32);
  CHECK(y[0] == 31 && y[143] == 0);
  for (int i = 0; i < 144; ++i)
    CHECK(y[i] < 32);
  wave.clear();
  for (int i = 0; i < 60; ++i)
    wave.add(50000);
  wave.buildY(y, 144, 32);
  for (int i = 0; i < 144; ++i)
    CHECK(y[i] == 16);
  wave.clear();
  wave.add(50000);
  wave.add(50001);
  wave.buildY(y, 144, 32);
  CHECK(y[0] - y[143] <= 1); // One ADC count cannot look like a full-height beat.
  wave.clear();
  wave.add(0);
  wave.add(262143);
  wave.buildY(y, 144, 32);
  CHECK(y[0] == 31 && y[143] == 0);
  wave.buildY(y, 1, 32);
  CHECK(y[0] == 31);
  wave.buildY(y, 144, 1);
  for (int i = 0; i < 144; ++i)
    CHECK(y[i] == 0);
  return 0;
}
