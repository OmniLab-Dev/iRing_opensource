#include "gesture_engine.h"
// Unsigned elapsed times remain correct across millis() rollover.
TouchInfo GestureEngine::update(uint16_t mask, uint32_t now) {
  uint32_t virtualMask = 0;
  for (int i = 0; i < 12; ++i)
    if (mask & (1u << i))
      virtualMask |= 1ul << virtualPad(i);
  return updateMask(virtualMask, now, false);
}
TouchInfo GestureEngine::updateVirtual(uint32_t mask, uint32_t now) {
  return updateMask(mask, now, true);
}
static TouchInfo makeEvent(TouchEvent type, int first, int last, bool highResolution) {
  return {type, physicalPad(first), physicalPad(last), first, last, highResolution};
}
TouchInfo GestureEngine::updateMask(uint32_t mask, uint32_t now, bool highResolution) {
  TouchInfo event = {NONE, -1, -1};
  mask &= 0x003fffff;
  if (mask != candidate) {
    candidate = mask;
    candidateAt = now;
  }
  if (uint32_t(now - candidateAt) >= 18)
    stable = candidate;
  if (suppressed) {
    if (!stable && !candidate) {
      suppressed = false;
      active = false;
    }
    return event;
  }
  if (pendingTap && uint32_t(now - releasedAt) > 320)
    pendingTap = false;
  if (!stable) {
    if (active && !moved && !held && uint32_t(now - began) >= 35 &&
        uint32_t(now - began) < 700) {
      event = makeEvent(TAP, first, last, highResolution);
      if (centerOnly && pendingTap && uint32_t(now - releasedAt) <= 320) {
        event.event = DOUBLE_TAP;
        pendingTap = false;
      } else {
        pendingTap = centerOnly;
        releasedAt = now;
      }
    }
    active = false;
    return event;
  }
  int low = 11, high = -1, sum = 0, count = 0, primary = -1;
  bool allCenter = true;
  for (int i = 0; i < 22; ++i)
    if (stable & (1ul << i)) {
      int col = virtualColumn(i);
      if (col < low)
        low = col;
      if (col > high)
        high = col;
      if (primary < 0)
        primary = i;
      sum += col * 50;
      ++count;
      allCenter = allCenter && col >= 4 && col <= 6;
    }
  int position = sum / count;
  if (!active) {
    active = true;
    moved = held = false;
    began = now;
    first = last = primary;
    anchor = position;
    span = high - low;
    centerOnly = allCenter;
    pinching = span >= 4;
    moved = pinching;
    if (!allCenter)
      pendingTap = false;
    return event;
  }
  centerOnly = centerOnly && allCenter;
  int newSpan = high - low;
  // Adjacent pads form one broad contact. Separated contacts form a pinch.
  if (!held && (pinching || span >= 4 || newSpan >= 4) && span >= 2 && newSpan >= 2 &&
      newSpan != span && count >= 2) {
    event = makeEvent(newSpan < span ? PINCH_INWARD : PINCH_OUTWARD, first, primary,
                      highResolution);
    pinching = true;
    moved = true;
    pendingTap = false;
    anchor = position;
  } else if (!held && !pinching && newSpan <= 2 &&
             (position - anchor >= (highResolution ? 50 : 100) ||
              anchor - position >= (highResolution ? 50 : 100))) {
    // padColumn/virtualColumn already normalize the reversed electrode rows.
    event = makeEvent(position > anchor ? SLIDE_RIGHT : SLIDE_LEFT, last, primary,
                      highResolution);
    moved = true;
    pendingTap = false;
    anchor = position;
  }
  if (newSpan >= 4) {
    pinching = true;
    moved = true;
    pendingTap = false;
  }
  span = newSpan;
  last = primary;
  if (!moved && !held && uint32_t(now - began) >= 700) {
    held = true;
    pendingTap = false;
    event = makeEvent(LONG_PRESS, first, last, highResolution);
  }
  return event;
}
