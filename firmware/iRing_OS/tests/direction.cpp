#include "../control_mapping.h"
#include "../health_menu.h"
#include "../settings_menu.h"
#define CHECK(condition)                                                               \
  do {                                                                                 \
    if (!(condition))                                                                  \
      return __LINE__;                                                                 \
  } while (0)
TouchInfo swipe(int from, int to) {
  GestureEngine g;
  g.update(1u << from, 0);
  g.update(1u << from, 20);
  g.update(1u << to, 40);
  return g.update(1u << to, 60);
}
extern "C" int run_tests() {
  const int rows[2][6] = {{0, 1, 2, 3, 4, 5}, {11, 10, 9, 8, 7, 6}};
  for (int row = 0; row < 2; ++row)
    for (int col = 0; col < 5; ++col) {
      TouchInfo right = swipe(rows[row][col], rows[row][col + 1]);
      TouchInfo left = swipe(rows[row][col + 1], rows[row][col]);
      CHECK(right.event == SLIDE_RIGHT && left.event == SLIDE_LEFT);
      CHECK(swipeDirection(right.event) == 1 && swipeDirection(left.event) == -1);
      // Paddle controls are reversed relative to menu navigation.
      // Sliders and HID volume must retain their existing direction.
      CHECK(brickleStep(right.event) == -1 && brickleStep(left.event) == 1);
      CHECK(57 + 15 * brickleStep(right.event) < 57);
      CHECK(57 + 15 * brickleStep(left.event) > 57);
      CHECK(sliderStep(right.event) == 1 && sliderStep(left.event) == -1);
      CHECK(mediaCommand(right) == (row ? MEDIA_VOLUME_UP : 0));
      CHECK(mediaCommand(left) == (row ? MEDIA_VOLUME_DOWN : 0));
      HealthMenu menu;
      CHECK(menu.move(swipeDirection(right.event)));
      CHECK(menu.selected == OXYGEN_SATURATION);
      CHECK(menu.move(swipeDirection(left.event)) && menu.selected == HEART_RATE);
    }
  // Virtual half-steps use the same direction if the experimental HAL is enabled.
  for (int row = 0; row < 2; ++row) {
    GestureEngine g;
    int from = row ? 21 : 0, to = row ? 20 : 1;
    g.updateVirtual(1ul << from, 0);
    g.updateVirtual(1ul << from, 20);
    g.updateVirtual(1ul << to, 40);
    TouchInfo e = g.updateVirtual(1ul << to, 60);
    CHECK(e.event == SLIDE_RIGHT && swipeDirection(e.event) == 1);
  }
  CHECK(swipeDirection(TAP) == 0 && swipeDirection(LONG_PRESS) == 0);
  CHECK(brickleStep(TAP) == 0 && brickleStep(LONG_PRESS) == 0);
  return 0;
}
