#pragma once
#include <stdint.h>
#include "analog_touch.h"
enum TouchEvent {
  NONE,
  TAP,
  DOUBLE_TAP,
  SLIDE_LEFT,
  SLIDE_RIGHT,
  LONG_PRESS,
  PINCH_INWARD,
  PINCH_OUTWARD
};
struct TouchInfo {
  TouchEvent event;
  int position, end_position; // Nearest physical electrodes for legacy callers.
  int virtual_position, end_virtual_position;
  bool high_resolution;
};
inline int padColumn(int pad) {
  return pad < 6 ? pad : 11 - pad;
}
inline bool centerPad(int pad) {
  return pad >= 0 && pad < 12 && (padColumn(pad) == 2 || padColumn(pad) == 3);
}
inline bool cornerPad(int pad) {
  return pad >= 0 && pad < 12 && (padColumn(pad) == 0 || padColumn(pad) == 5);
}
inline bool touchCenter(const TouchInfo &event) {
  if (!event.high_resolution)
    return centerPad(event.position);
  int col = virtualColumn(event.virtual_position);
  return col >= 4 && col <= 6;
}
inline bool touchCorner(const TouchInfo &event) {
  if (!event.high_resolution)
    return cornerPad(event.position);
  int col = virtualColumn(event.virtual_position);
  return col == 0 || col == 10;
}
class GestureEngine {
public:
  TouchInfo update(uint16_t mask, uint32_t now);
  TouchInfo updateVirtual(uint32_t mask, uint32_t now);
  void suppressUntilRelease() {
    suppressed = true;
    pendingTap = false;
  }

private:
  TouchInfo updateMask(uint32_t mask, uint32_t now, bool highResolution);
  uint32_t candidate = 0, stable = 0;
  uint32_t candidateAt = 0, began = 0, releasedAt = 0;
  bool active = false, moved = false, held = false, suppressed = false;
  bool pendingTap = false, centerOnly = false, pinching = false;
  int first = -1, last = -1, anchor = 0, span = 0;
};
