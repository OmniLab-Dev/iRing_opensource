#pragma once
#include "gesture_engine.h"
#include "media_queue.h"
// Hardware layout: bottom 0..5, top 11..6 (both left-to-right).
inline bool topPad(int pad) {
  return pad >= 6 && pad <= 11;
}
inline bool bottomPad(int pad) {
  return pad >= 0 && pad <= 5;
}
// Shared menu/slider direction: right is positive, left is negative.
inline int swipeDirection(TouchEvent event) {
  return event == SLIDE_RIGHT ? 1 : event == SLIDE_LEFT ? -1 : 0;
}
inline int sliderStep(TouchEvent event) {
  return swipeDirection(event);
}
// Brickles uses the opposite paddle direction to the menu and slider controls.
inline int brickleStep(TouchEvent event) {
  return -swipeDirection(event);
}
inline uint16_t mediaCommand(const TouchInfo &event) {
  int step = sliderStep(event.event);
  if (step && topPad(event.position) && topPad(event.end_position))
    return step > 0 ? MEDIA_VOLUME_UP : MEDIA_VOLUME_DOWN;
  if (event.event != TAP || !bottomPad(event.position) ||
      !bottomPad(event.end_position))
    return 0;
  if (touchCenter(event))
    return MEDIA_PLAY_PAUSE;
  int column = event.high_resolution ? virtualColumn(event.virtual_position) / 2
                                     : padColumn(event.position);
  return column < 2 ? MEDIA_PREVIOUS : MEDIA_NEXT;
}
