#include "../health_menu.h"
#include "../health_window.h"
#define CHECK(condition)                                                               \
  do {                                                                                 \
    if (!(condition))                                                                  \
      return __LINE__;                                                                 \
  } while (0)
extern "C" int run_tests() {
  {
    HealthMenu menu;
    CHECK(!menu.enter({TAP, 2, 2}));
    CHECK(!menu.enter({DOUBLE_TAP, 0, 0}));
    CHECK(menu.move(1));
    CHECK(menu.selected == OXYGEN_SATURATION);
    CHECK(!menu.move(1));
    CHECK(menu.enter({DOUBLE_TAP, 8, 8}));
    CHECK(menu.measuring);
    CHECK(!menu.move(-1));
    CHECK(!menu.back({LONG_PRESS, 2, 2}));
    CHECK(menu.back({LONG_PRESS, 6, 6}));
    CHECK(!menu.measuring);
    CHECK(menu.move(-1));
    CHECK(menu.selected == HEART_RATE);
    menu.enter({DOUBLE_TAP, 3, 3});
    menu.reset();
    CHECK(!menu.measuring);
  }
  CHECK(!HealthWindow::contact(0, 60000));
  CHECK(!HealthWindow::contact(60000, 0));
  CHECK(HealthWindow::contact(60000, 60000));
  CHECK(HealthWindow::saturated(262143, 60000));
  CHECK(HealthWindow::saturated(60000, 262143));
  {
    HealthWindow w;
    for (int i = 0; i < 99; ++i)
      CHECK(!w.add(60000 + (i % 10) * 40, 65000 + (i % 10) * 40));
    CHECK(!w.usable());
    CHECK(w.add(60360, 65360));
    CHECK(w.usable());
    CHECK(!w.add(60360, 65360));
    CHECK(w.count == 100);
    uint32_t saved = w.red[25];
    w.advance();
    CHECK(w.count == 75);
    CHECK(w.red[0] == saved);
    w.clear();
    CHECK(w.count == 0);
    CHECK(!w.usable());
  }
  {
    HealthWindow w;
    for (int i = 0; i < 100; ++i)
      w.add(60000, 60000);
    CHECK(!w.usable());
  }
  {
    HealthWindow w;
    for (int i = 0; i < 100; ++i)
      w.add(i % 2 ? 180000 : 10000, 60000);
    CHECK(!w.usable());
  }
  return 0;
}
