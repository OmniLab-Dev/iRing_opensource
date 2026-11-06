#pragma once
#include "gesture_engine.h"
enum HealthPage { HEART_RATE, OXYGEN_SATURATION };
class HealthMenu {
public:
  HealthPage selected = HEART_RATE;
  bool measuring = false;
  void reset() {
    measuring = false;
  }
  bool move(int direction) {
    if (measuring)
      return false;
    int next = int(selected) + direction;
    if (next < 0)
      next = 0;
    if (next > 1)
      next = 1;
    bool changed = next != selected;
    selected = static_cast<HealthPage>(next);
    return changed;
  }
  bool enter(const TouchInfo &event) {
    if (!measuring && event.event == DOUBLE_TAP && touchCenter(event)) {
      measuring = true;
      return true;
    }
    return false;
  }
  bool back(const TouchInfo &event) {
    if (measuring && event.event == LONG_PRESS && touchCorner(event)) {
      measuring = false;
      return true;
    }
    return false;
  }
};
