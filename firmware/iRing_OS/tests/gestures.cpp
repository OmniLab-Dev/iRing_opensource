#include "../gesture_engine.h"
// Return the failing source line, or zero. Runs on a host compiler or bare ARM.
#define CHECK(condition)                                                               \
  do {                                                                                 \
    if (!(condition))                                                                  \
      return __LINE__;                                                                 \
  } while (0)
struct Fixture {
  GestureEngine engine;
  uint32_t now = 0;
  int counts[8] = {0};
  TouchInfo last = {NONE, -1, -1};
  void poll(uint16_t mask, uint32_t dt) {
    now += dt;
    TouchInfo event = engine.update(mask, now);
    if (event.event != NONE) {
      ++counts[event.event];
      last = event;
    }
  }
  void sample(uint16_t mask) {
    poll(mask, 1);
    poll(mask, 20);
  }
  void tap(uint16_t mask) {
    sample(mask);
    poll(mask, 60);
    sample(0);
  }
};
extern "C" int run_tests() {
  for (int pad = 0; pad < 12; ++pad) {
    CHECK(padColumn(pad) == padColumn(11 - pad));
    Fixture f;
    f.tap(1u << pad);
    CHECK(f.counts[TAP] == 1);
    f.tap(1u << pad);
    CHECK(f.counts[DOUBLE_TAP] == (centerPad(pad) ? 1 : 0));
  }
  {
    Fixture f;
    f.tap((1 << 2) | (1 << 3));
    f.tap((1 << 8) | (1 << 9));
    CHECK(f.counts[DOUBLE_TAP] == 1);
  }
  {
    Fixture f;
    f.tap(1 << 2);
    f.poll(0, 400);
    f.tap(1 << 2);
    CHECK(f.counts[DOUBLE_TAP] == 0);
    CHECK(f.counts[TAP] == 2);
  }
  {
    Fixture f;
    f.poll(1 << 2, 1);
    f.poll(0, 10);
    f.poll(0, 40);
    CHECK(f.counts[TAP] == 0);
  }
  for (int pad = 0; pad < 12; ++pad)
    if (cornerPad(pad)) {
      Fixture f;
      f.sample(1 << pad);
      f.poll(1 << pad, 700);
      f.poll(1 << pad, 2000);
      f.sample(0);
      CHECK(f.counts[LONG_PRESS] == 1);
      CHECK(f.counts[TAP] == 0);
    }
  {
    Fixture f;
    f.sample(1 << 0);
    f.sample((1 << 0) | (1 << 1));
    f.sample(1 << 1);
    f.sample(1 << 2);
    f.sample(0);
    CHECK(f.counts[SLIDE_RIGHT] == 2);
    CHECK(f.counts[SLIDE_LEFT] == 0);
    CHECK(f.counts[TAP] == 0);
    CHECK(f.counts[PINCH_OUTWARD] == 0);
  }
  {
    Fixture f;
    f.sample(1 << 11);
    f.sample(1 << 10);
    f.sample(1 << 9);
    f.sample(0);
    CHECK(f.counts[SLIDE_RIGHT] == 2);
    CHECK(f.counts[SLIDE_LEFT] == 0);
  }
  {
    Fixture f;
    f.sample(1 << 6);
    f.sample(1 << 7);
    f.sample(0);
    CHECK(f.counts[SLIDE_LEFT] == 1);
  }
  {
    Fixture f;
    f.sample((1 << 2) | (1 << 9));
    f.sample((1 << 3) | (1 << 8));
    f.sample(0);
    CHECK(f.counts[SLIDE_RIGHT] == 1);
    CHECK(f.counts[PINCH_INWARD] == 0);
  }
  {
    Fixture f;
    f.sample((1 << 0) | (1 << 5));
    f.sample((1 << 1) | (1 << 4));
    f.sample((1 << 2) | (1 << 3));
    f.sample((1 << 1) | (1 << 4));
    f.sample(1 << 4);
    f.poll(1 << 4, 1000);
    f.sample(0);
    CHECK(f.counts[PINCH_INWARD] == 2);
    CHECK(f.counts[PINCH_OUTWARD] == 1);
    CHECK(f.counts[TAP] == 0);
    CHECK(f.counts[LONG_PRESS] == 0);
    CHECK(f.counts[SLIDE_RIGHT] == 0);
  }
  {
    Fixture f;
    f.sample((1 << 0) | (1 << 5));
    f.sample(0);
    CHECK(f.counts[TAP] == 0);
  }
  {
    Fixture f;
    f.tap(1 << 2);
    f.sample(1 << 0);
    f.sample(1 << 1);
    f.sample(0);
    f.tap(1 << 2);
    CHECK(f.counts[DOUBLE_TAP] == 0);
  }
  {
    Fixture f;
    f.sample(1 << 0);
    f.poll(1 << 0, 700);
    f.engine.suppressUntilRelease();
    f.poll(1 << 0, 1000);
    f.sample(1 << 1);
    f.sample(0);
    f.tap(1 << 2);
    CHECK(f.counts[LONG_PRESS] == 1);
    CHECK(f.counts[SLIDE_RIGHT] == 0);
    CHECK(f.counts[TAP] == 1);
  }
  {
    Fixture f;
    f.now = 0xfffffff0u;
    f.tap(1 << 2);
    f.tap(1 << 2);
    CHECK(f.counts[DOUBLE_TAP] == 1);
  }
  {
    Fixture f;
    f.now = 0xffffff00u;
    f.sample(1 << 0);
    f.poll(1 << 0, 700);
    CHECK(f.counts[LONG_PRESS] == 1);
  }
  return 0;
}
