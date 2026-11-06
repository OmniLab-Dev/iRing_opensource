#pragma once
#include <stdint.h>
// Consumer usages from the USB HID Consumer page (also used by BLE HID).
enum MediaKey : uint16_t {
  MEDIA_PLAY_PAUSE = 0xCD,
  MEDIA_NEXT = 0xB5,
  MEDIA_PREVIOUS = 0xB6,
  MEDIA_VOLUME_UP = 0xE9,
  MEDIA_VOLUME_DOWN = 0xEA
};

// Transport-independent sender: never send another press before releasing.
class MediaQueue {
public:
  void reset() {
    head = tail = count = 0;
    active = false;
    neutral = true;
    gap = false;
  }
  bool push(uint16_t key, uint32_t now) {
    if (!key || count == 8)
      return false;
    keys[tail] = key;
    times[tail] = now;
    tail = (tail + 1) % 8;
    ++count;
    return true;
  }
  template <class Send> uint16_t tick(uint32_t now, bool ready, Send send) {
    if (!ready) {
      reset();
      return 0;
    }
    if (neutral) {
      if (send(0))
        neutral = false;
      return 0;
    }
    if (active) {
      if (uint32_t(now - pressedAt) >= 35 && send(0)) {
        active = false;
        gap = true;
        releasedAt = now;
      }
      return 0;
    }
    if (gap && uint32_t(now - releasedAt) < 25)
      return 0;
    gap = false;
    while (count && uint32_t(now - times[head]) > 300)
      pop();
    if (!count || !send(keys[head]))
      return 0;
    uint16_t sent = keys[head];
    pop();
    active = true;
    pressedAt = now;
    return sent;
  }

private:
  uint16_t keys[8] = {};
  uint32_t times[8] = {}, pressedAt = 0, releasedAt = 0;
  uint8_t head = 0, tail = 0, count = 0;
  bool active = false, neutral = true, gap = false;
  void pop() {
    head = (head + 1) % 8;
    --count;
  }
};
