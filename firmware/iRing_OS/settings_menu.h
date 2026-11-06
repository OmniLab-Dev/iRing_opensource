#pragma once
#include "gesture_engine.h"
// Extend this table when a new settings page is implemented.
enum SettingPage { SETTING_BRIGHTNESS };
struct SettingEntry {
  SettingPage page;
  const char *title;
};
static const SettingEntry settingEntries[] = {{SETTING_BRIGHTNESS, "Brightness"}};
constexpr int SETTING_COUNT = sizeof(settingEntries) / sizeof(settingEntries[0]);
class SettingsMenu {
public:
  int selected = 0;
  bool editing = false;
  void reset() {
    editing = false;
  }
  bool move(int direction) {
    if (editing)
      return false;
    int next = selected + direction;
    if (next < 0)
      next = 0;
    if (next >= SETTING_COUNT)
      next = SETTING_COUNT - 1;
    bool changed = next != selected;
    selected = next;
    return changed;
  }
  bool enter(TouchInfo event) {
    if (!editing && event.event == DOUBLE_TAP && touchCenter(event)) {
      editing = true;
      return true;
    }
    return false;
  }
  bool back(TouchInfo event) {
    if (editing && event.event == LONG_PRESS && touchCorner(event)) {
      editing = false;
      return true;
    }
    return false;
  }
};
