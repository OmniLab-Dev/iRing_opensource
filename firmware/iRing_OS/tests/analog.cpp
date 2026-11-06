#include "../analog_touch.h"
#include "../gesture_engine.h"
#define CHECK(condition)                                                               \
  do {                                                                                 \
    if (!(condition))                                                                  \
      return __LINE__;                                                                 \
  } while (0)
AnalogSample blank() {
  AnalogSample s = {};
  for (int i = 0; i < 12; ++i)
    s.baseline[i] = s.filtered[i] = 500;
  return s;
}
void calibrate(AnalogTouch &hal, int offset = 0) {
  AnalogSample s = blank();
  for (int i = 0; i < 12; ++i)
    s.filtered[i] -= offset;
  for (int i = 0; i < 16; ++i)
    hal.update(s, i * 10);
}
extern "C" int run_tests() {
  {
    AnalogTouch h;
    AnalogSample s = blank();
    for (int i = 0; i < 15; ++i)
      CHECK(!h.update(s, i * 10).calibrated);
    CHECK(h.update(s, 150).calibrated);
    CHECK(!h.update(s, 160).mask);
  }
  {
    AnalogTouch h;
    AnalogSample s = blank();
    s.touched = 1;
    for (int i = 0; i < 30; ++i)
      CHECK(!h.update(s, i * 10).calibrated);
    s.touched = 0;
    for (int i = 0; i < 16; ++i)
      h.update(s, 400 + i * 10);
    CHECK(h.update(s, 600).calibrated);
  }
  // All physical electrodes retain their IDs, including reversed row geometry.
  for (int i = 0; i < 12; ++i) {
    AnalogTouch h;
    calibrate(h);
    AnalogSample s = blank();
    s.filtered[i] = 490;
    s.touched = 1u << i;
    AnalogFrame f = h.update(s, 1000);
    CHECK(f.mask == (1ul << virtualPad(i)));
    CHECK(physicalPad(virtualPad(i)) == i);
    CHECK(f.rowPosition[i / 6] == padColumn(i) * 200);
  }
  // All ten midpoints, with two equally weighted adjacent electrodes.
  for (int row = 0; row < 2; ++row)
    for (int col = 0; col < 5; ++col) {
      AnalogTouch h;
      calibrate(h);
      AnalogSample s = blank();
      int i = row * 6 + col;
      s.filtered[i] = s.filtered[i + 1] = 494;
      s.touched = (1u << i) | (1u << (i + 1));
      AnalogFrame f = h.update(s, 1000);
      CHECK(f.mask == (1ul << (row * 11 + col * 2 + 1)));
      CHECK(f.rowPosition[row] == (row ? 1000 - col * 200 - 100 : col * 200 + 100));
    }
  {
    AnalogTouch h;
    calibrate(h);
    AnalogSample s = blank();
    s.filtered[2] = 499;
    s.filtered[3] = 499; // Both below the calibrated touch=2.
    CHECK(!h.update(s, 1000).mask);
    CHECK(!h.update(s, 1059).mask);
    CHECK(h.update(s, 1060).mask == (1ul << 5));
    s = blank();
    CHECK(!h.update(s, 1070).mask);
  }
  {
    AnalogTouch h(2, 1, false);
    calibrate(h);
    AnalogSample s = blank();
    s.filtered[2] = s.filtered[3] = 499;
    h.update(s, 1000);
    CHECK(!h.update(s, 1100).mask);
    s.touched = 1u << 2;
    CHECK(h.update(s, 1110).mask == (1ul << 5));
  }
  {
    AnalogTouch h;
    calibrate(h, 1);
    AnalogSample s = blank();
    for (int i = 0; i < 12; ++i)
      s.filtered[i] = 499;
    h.update(s, 1000);
    CHECK(!h.update(s, 1100).mask); // Idle offset cannot create a contact.
  }
  {
    AnalogTouch h;
    calibrate(h);
    AnalogSample s = blank();
    s.filtered[5] = s.filtered[6] = 499;
    h.update(s, 1000);
    CHECK(!h.update(s, 1100).mask); // Never bridge row ends.
  }
  {
    AnalogTouch h;
    calibrate(h);
    AnalogSample s = blank();
    s.filtered[0] = 499;
    h.update(s, 1000);
    CHECK(!h.update(s, 1100).mask); // Lone subthreshold signal ignored.
  }
  {
    AnalogTouch h;
    calibrate(h);
    AnalogSample s = blank();
    s.touched = 3;
    s.filtered[0] = 400;
    CHECK(h.update(s, 1000).mask == 1);
    s.filtered[0] = 425;
    s.filtered[1] = 475;
    CHECK(h.update(s, 1010).mask == 1); // 50: within hysteresis.
    s.filtered[0] = 430;
    s.filtered[1] = 470;
    CHECK(h.update(s, 1020).mask == 1); // 60: still real pad.
    s.filtered[0] = 435;
    s.filtered[1] = 465;
    CHECK(h.update(s, 1030).mask == 2); // 70: midpoint.
    s.filtered[0] = 425;
    s.filtered[1] = 475;
    CHECK(h.update(s, 1040).mask == 2); // No chatter.
  }
  {
    AnalogTouch h;
    calibrate(h);
    AnalogSample s = blank();
    s.filtered[0] = s.filtered[2] = 490;
    s.filtered[1] = 499;
    s.touched = 5;
    CHECK(h.update(s, 1000).mask ==
          ((1ul << 0) | (1ul << 4))); // Deep valley separates contacts.
  }
  {
    AnalogTouch h;
    calibrate(h);
    AnalogSample s = blank();
    s.filtered[1] = s.filtered[3] = 495;
    s.filtered[2] = 490;
    s.touched = 14;
    CHECK(h.update(s, 1000).mask == (1ul << 4)); // Broad single peak stays one contact.
  }
  {
    AnalogTouch h;
    calibrate(h);
    AnalogSample s = blank();
    s.filtered[0] = s.filtered[1] = 499;
    h.update(s, 0xfffffff0u);
    CHECK(h.update(s, 0x30u).mask == 2);
  }
  {
    GestureEngine g;
    g.updateVirtual(1, 0);
    g.updateVirtual(1, 20);
    g.updateVirtual(2, 30);
    TouchInfo event = g.updateVirtual(2, 50);
    CHECK(event.event == SLIDE_RIGHT);
    CHECK(event.high_resolution);
    CHECK(event.virtual_position == 0 && event.end_virtual_position == 1);
  }
  // Center midpoint is an enter region; near-corner midpoint cannot exit.
  {
    TouchInfo e = {DOUBLE_TAP, 3, 3, 5, 5, true};
    CHECK(touchCenter(e));
    e = {LONG_PRESS, 0, 0, 1, 1, true};
    CHECK(!touchCorner(e));
    e = {LONG_PRESS, 6, 6, 11, 11, true};
    CHECK(touchCorner(e));
  }
  return 0;
}
