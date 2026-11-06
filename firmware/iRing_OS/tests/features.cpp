#include "../media_queue.h"
#include "../settings_menu.h"
#include "../control_mapping.h"
#define CHECK(condition)                                                               \
  do {                                                                                 \
    if (!(condition))                                                                  \
      return __LINE__;                                                                 \
  } while (0)
struct Sender {
  bool succeeds = true;
  uint16_t reports[64] = {};
  int count = 0;
  bool operator()(uint16_t key) {
    if (!succeeds)
      return false;
    reports[count++] = key;
    return true;
  }
};
extern "C" int run_tests() {
  {
    MediaQueue q;
    Sender s;
    auto send = [&s](uint16_t k) { return s(k); };
    CHECK(q.push(MEDIA_NEXT, 0));
    CHECK(q.push(MEDIA_VOLUME_UP, 0));
    CHECK(q.tick(0, true, send) == 0);
    CHECK(s.reports[0] == 0);
    CHECK(q.tick(1, true, send) == MEDIA_NEXT);
    CHECK(q.tick(35, true, send) == 0);
    CHECK(s.count == 2);
    CHECK(q.tick(36, true, send) == 0);
    CHECK(s.reports[2] == 0);
    CHECK(q.tick(60, true, send) == 0);
    CHECK(s.count == 3);
    CHECK(q.tick(61, true, send) == MEDIA_VOLUME_UP);
    q.tick(96, true, send);
    CHECK(s.reports[4] == 0);
    CHECK(s.count == 5);
  }
  {
    MediaQueue q;
    Sender s;
    auto send = [&s](uint16_t k) { return s(k); };
    q.tick(0, true, send);
    q.push(MEDIA_VOLUME_DOWN, 0);
    q.tick(1, true, send);
    q.push(MEDIA_NEXT, 10);
    s.succeeds = false;
    q.tick(40, true, send);
    q.tick(70, true, send);
    CHECK(s.count == 2);
    s.succeeds = true;
    q.tick(100, true, send);
    CHECK(s.reports[2] == 0);
    CHECK(q.tick(124, true, send) == 0);
    CHECK(q.tick(125, true, send) == MEDIA_NEXT);
  }
  {
    MediaQueue q;
    Sender s;
    auto send = [&s](uint16_t k) { return s(k); };
    q.tick(0, true, send);
    q.push(MEDIA_PLAY_PAUSE, 0);
    q.tick(1, true, send);
    q.push(MEDIA_NEXT, 1);
    q.tick(2, false, send);
    q.tick(3, true, send);
    CHECK(s.reports[2] == 0);
    q.tick(100, true, send);
    CHECK(s.count == 3); // No stale commands on reconnect.
  }
  {
    MediaQueue q;
    Sender s;
    auto send = [&s](uint16_t k) { return s(k); };
    for (int i = 0; i < 8; ++i)
      CHECK(q.push(MEDIA_NEXT, 0));
    CHECK(!q.push(MEDIA_NEXT, 0));
    q.tick(0, true, send);
    q.tick(301, true, send);
    CHECK(s.count == 1);
    CHECK(q.push(MEDIA_PREVIOUS, 302));
    CHECK(q.tick(302, true, send) == MEDIA_PREVIOUS);
  }
  {
    MediaQueue q;
    Sender s;
    auto send = [&s](uint16_t k) { return s(k); };
    q.tick(0xfffffff0u, true, send);
    q.push(MEDIA_NEXT, 0xfffffff0u);
    CHECK(q.tick(0xfffffff1u, true, send) == MEDIA_NEXT);
    q.tick(0x14u, true, send);
    CHECK(s.count == 3);
    CHECK(s.reports[2] == 0);
  }
  {
    SettingsMenu menu;
    CHECK(!menu.move(-1));
    CHECK(menu.selected == 0);
    CHECK(!menu.enter({TAP, 2, 2}));
    CHECK(!menu.enter({DOUBLE_TAP, 0, 0}));
    CHECK(menu.enter({DOUBLE_TAP, 9, 9}));
    CHECK(menu.editing);
    CHECK(!menu.move(1));
    CHECK(!menu.back({LONG_PRESS, 2, 2}));
    CHECK(menu.back({LONG_PRESS, 11, 11}));
    CHECK(!menu.editing);
    CHECK(!menu.back({LONG_PRESS, 11, 11}));
    menu.enter({DOUBLE_TAP, 3, 3});
    menu.reset();
    CHECK(!menu.editing);
    menu.move(100);
    CHECK(menu.selected == SETTING_COUNT - 1);
  }
  // Only top-row slides control volume, with slider direction corrected.
  CHECK(mediaCommand({SLIDE_RIGHT, 11, 10}) == MEDIA_VOLUME_UP);
  CHECK(mediaCommand({SLIDE_LEFT, 10, 11}) == MEDIA_VOLUME_DOWN);
  CHECK(mediaCommand({SLIDE_RIGHT, 0, 1}) == 0);
  CHECK(mediaCommand({SLIDE_LEFT, 1, 0}) == 0);
  CHECK(mediaCommand({SLIDE_RIGHT, 5, 6}) == 0);
  for (int i = 6; i < 12; ++i)
    CHECK(mediaCommand({TAP, i, i}) == 0);
  CHECK(mediaCommand({TAP, 0, 0}) == MEDIA_PREVIOUS);
  CHECK(mediaCommand({TAP, 1, 1}) == MEDIA_PREVIOUS);
  CHECK(mediaCommand({TAP, 2, 2}) == MEDIA_PLAY_PAUSE);
  CHECK(mediaCommand({TAP, 3, 3}) == MEDIA_PLAY_PAUSE);
  CHECK(mediaCommand({TAP, 4, 4}) == MEDIA_NEXT);
  CHECK(mediaCommand({TAP, 5, 5}) == MEDIA_NEXT);
  CHECK(mediaCommand({LONG_PRESS, 0, 0}) == 0);
  CHECK(sliderStep(SLIDE_RIGHT) == 1 && sliderStep(SLIDE_LEFT) == -1);
  return 0;
}
