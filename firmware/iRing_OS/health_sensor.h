#pragma once
#include <MAX30105.h>
#include "health_window.h"
#include "health_waveform.h"
enum HealthState {
  HEALTH_IDLE,
  HEALTH_NO_SENSOR,
  HEALTH_CONTACT,
  HEALTH_COLLECTING,
  HEALTH_SIGNAL,
  HEALTH_RUNNING,
  HEALTH_ERROR
};
class HealthSensor {
public:
  bool start();
  void stop();
  void poll(uint32_t now);
  HealthState state = HEALTH_IDLE;
  int32_t bpm = -1, spo2 = -1;
  bool bpmValid = false, spo2Valid = false;
  const HealthWaveform &waveform() const {
    return trace;
  }
  int progress() const {
    return window.count;
  }

private:
  MAX30105 sensor;
  HealthWindow window;
  HealthWaveform trace;
  bool initialized = false, active = false;
  uint8_t goodBpm = 0, goodSpo2 = 0;
  uint32_t lastPoll = 0, lastData = 0;
  bool read(uint8_t reg, uint8_t *bytes, uint8_t count);
  void resetResults();
  void error();
  void process(uint32_t red, uint32_t ir);
};
