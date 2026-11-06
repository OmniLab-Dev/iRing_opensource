#include "health_sensor.h"
#include "board_config.h"
#include <spo2_algorithm.h>

bool HealthSensor::read(uint8_t reg, uint8_t *bytes, uint8_t count) {
  Wire.beginTransmission(board::kOpticalAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
    return false;
  if (Wire.requestFrom(board::kOpticalAddress, count) != count)
    return false;
  for (uint8_t i = 0; i < count; ++i)
    bytes[i] = Wire.read();
  return true;
}
void HealthSensor::resetResults() {
  window.clear();
  trace.clear();
  bpm = spo2 = -1;
  bpmValid = spo2Valid = false;
  goodBpm = goodSpo2 = 0;
}
bool HealthSensor::start() {
  stop();
  resetResults();
  // The SparkFun MAX30105 driver supports MAX30101 red/IR operation.
  initialized = sensor.begin(Wire, I2C_SPEED_STANDARD, board::kOpticalAddress);
  if (!initialized) {
    state = HEALTH_NO_SENSOR;
    Serial.println("MAX30101 not found at 0x57");
    return false;
  }
  sensor.setup(board::kOpticalLedAmplitude, board::kOpticalAverage, 2,
               board::kOpticalSampleRate, board::kOpticalPulseWidth,
               board::kOpticalAdcRange);
  sensor.setPulseAmplitudeGreen(0);
  sensor.disableFIFORollover();
  sensor.clearFIFO();
  active = true;
  state = HEALTH_CONTACT;
  lastPoll = lastData = millis();
  Serial.println("MAX30101 measurement started");
  return true;
}
void HealthSensor::stop() {
  if (initialized)
    sensor.shutDown();
  active = false;
  resetResults();
  state = HEALTH_IDLE;
}
void HealthSensor::error() {
  stop();
  state = HEALTH_ERROR;
  Serial.println("MAX30101 read error; double tap center to retry");
}
void HealthSensor::process(uint32_t red, uint32_t ir) {
  if (!HealthWindow::contact(red, ir)) {
    resetResults();
    state = HEALTH_CONTACT;
    return;
  }
  if (HealthWindow::saturated(red, ir)) {
    resetResults();
    state = HEALTH_SIGNAL;
    return;
  }
  trace.add(ir); // Plot each accepted FIFO sample, independent of BPM calculation.
  if (!window.add(red, ir)) {
    if (state == HEALTH_CONTACT || state == HEALTH_SIGNAL)
      state = HEALTH_COLLECTING;
    return;
  }
  if (!window.usable()) {
    bpmValid = spo2Valid = false;
    goodBpm = goodSpo2 = 0;
    state = HEALTH_SIGNAL;
  } else {
    int8_t hrOk = 0, oxygenOk = 0;
    int32_t heart = -1, oxygen = -1;
    maxim_heart_rate_and_oxygen_saturation(window.ir, HealthWindow::SIZE, window.red,
                                           &oxygen, &oxygenOk, &heart, &hrOk);
    if (hrOk && heart > 0 && heart <= 300) {
      if (goodBpm < 2)
        ++goodBpm;
      bpm = heart;
    } else {
      goodBpm = 0;
      bpm = -1;
    }
    if (oxygenOk && oxygen > 0 && oxygen <= 100) {
      if (goodSpo2 < 2)
        ++goodSpo2;
      spo2 = oxygen;
    } else {
      goodSpo2 = 0;
      spo2 = -1;
    }
    bpmValid = goodBpm >= 2;
    spo2Valid = goodSpo2 >= 2;
    state = (bpmValid || spo2Valid) ? HEALTH_RUNNING : HEALTH_SIGNAL;
  }
  window.advance(); // Retain 3 seconds; update once per second.
}
void HealthSensor::poll(uint32_t now) {
  if (!active || uint32_t(now - lastPoll) < 10)
    return;
  lastPoll = now;
  // Read hardware FIFO directly. The bundled library's four-slot software FIFO
  // can silently drop samples after a display transfer; do not use getIR().
  uint8_t pointers[3];
  if (!read(0x04, pointers, 3)) {
    error();
    return;
  }
  if (pointers[1]) {
    sensor.clearFIFO();
    resetResults();
    state = HEALTH_COLLECTING;
    lastData = now;
    return;
  }
  int pending = (int(pointers[0]) - int(pointers[2])) & 31;
  int budget = pending < 8
                   ? pending
                   : 8; // Bounded work, so exit gestures and BLE stay responsive.
  for (int i = 0; i < budget; ++i) {
    uint8_t sample[6];
    if (!read(0x07, sample, 6)) {
      error();
      return;
    }
    uint32_t red =
        ((uint32_t(sample[0]) << 16) | (uint32_t(sample[1]) << 8) | sample[2]) &
        0x3ffff;
    uint32_t ir =
        ((uint32_t(sample[3]) << 16) | (uint32_t(sample[4]) << 8) | sample[5]) &
        0x3ffff;
    process(red, ir);
    lastData = now;
  }
  if (uint32_t(now - lastData) > 1200)
    error(); // Never leave stale values on-screen.
}
