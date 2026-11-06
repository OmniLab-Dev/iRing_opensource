#include <SPI.h>
#include <Wire.h>
#include <bluefruit.h>
#include <TimeLib.h>
#include <stdio.h>
#include <string.h>
#include "SSD1320_OLED.h"
#include "touch_handler.h"
#include "ring_os.h"
#include "board_config.h"
#include "phone_link.h"
#include "settings_menu.h"
#include "control_mapping.h"
#include "health_menu.h"
#include "health_sensor.h"
#include "vol0.h"
#include "media.h"
#include "timer.h"
#include "health.h"
#include "settings.h"
#include "game.h"
#include "play.h"
#include "pause.h"
#include "prevsmall.h"
#include "nextsmall.h"
#include "msgopen.h"

namespace {
constexpr int W = board::kDisplayWidth, H = board::kDisplayHeight;
constexpr int STATUS_W = board::kStatusWidth, CONTENT_W = W - STATUS_W;

// Values are part of the BLE UART mode command protocol.
enum Mode : uint8_t {
  MODE_MENU = 0,
  MODE_MEDIA = 1,
  MODE_CLOCK = 2,
  MODE_HEALTH = 3,
  MODE_SETTINGS = 4,
  MODE_BRICKLES = 5,
  MODE_MESSAGE = 6,
  MODE_READER = 7,
  MODE_TOUCH_DEBUG = 8,
  MODE_MEDIA_ALIAS = 9
};
constexpr uint32_t FRAME_MS = 33, GAME_MS = 20, TRANSITION_MS = 180;
BLEUart uart;
BLEDis deviceInfo;
BLEDfu dfu;
SSD1320 display(board::kOledCs, board::kOledReset, board::kOledClock, board::kOledData);
TouchHandler touch;
uint8_t mode = MODE_MENU, selected = 0, brightness = 30, textSize = 0;
int textOffset = 0;
bool touchReady = false, clockSynced = false;
SettingsMenu settingsMenu;
HealthMenu healthMenu;
HealthSensor healthSensor;
// Local indicator only: HID does not report absolute phone volume.
uint8_t volumeIndicator = 6;
uint32_t uartGeneration = 0;
bool timeRequested = false;
uint32_t lastFrame = 0, lastGame = 0, lastTouchRetry = 0;
uint8_t oldFrame[640], nextFrame[640];
bool transitioning = false;
uint32_t transitionAt = 0;
int transitionDirection = 1;
char input[128];
size_t inputLength = 0;
bool inputOverflow = false;
uint32_t inputAt = 0;
const char *message = "Hi John, I may be late for the coffee. I'm on my way.";
uint8_t *icons[] = {media, timer, health, settings, game};
bool bricks[3][10];
int paddle = 57, ballX = 72, ballY = 24, dx = 1, dy = -1;
bool waiting = true;

int clamp(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}
void label(int x, int y, const char *value, uint8_t font = 0) {
  display.setFontType(font);
  display.setColor(WHITE);
  display.setCursor(x, y);
  display.print(value);
}
// Clip signed coordinates before passing them to the uint8_t display API.
void bitmap(uint8_t *data, int x, int y, int width, int height) {
  for (int row = 0; row < height; ++row)
    for (int col = 0; col < width; ++col) {
      int px = x + col, py = y + height - 1 - row;
      if (px >= 0 && px < W && py >= 0 && py < H)
        display.setPixel(px, py,
                         (data[row * ((width + 7) / 8) + col / 8] & (0x80 >> (col % 8)))
                             ? WHITE
                             : BLACK,
                         NORM);
    }
}
void startTransition(int direction = 1) {
  memcpy(oldFrame, display.getScreenBuffer(), sizeof oldFrame);
  transitionAt = millis();
  transitioning = true;
  transitionDirection = direction;
}
void resetGame() {
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 10; ++c)
      bricks[r][c] = true;
  paddle = 57;
  ballX = 72;
  ballY = 24;
  dx = 1;
  dy = -1;
  waiting = true;
  lastGame = millis();
}
void changeMode(uint8_t value) {
  if (value > MODE_MEDIA_ALIAS || value == mode)
    return;
  startTransition(value == 0 ? -1 : 1);
  if (mode == MODE_HEALTH)
    healthSensor.stop();
  mode = value;
  touch.suppressUntilRelease();
  if (mode >= MODE_MEDIA && mode <= MODE_BRICKLES)
    selected = mode - MODE_MEDIA;
  if (mode == MODE_HEALTH)
    healthMenu.reset();
  if (mode == MODE_SETTINGS)
    settingsMenu.reset();
  if (mode == MODE_BRICKLES)
    resetGame();
  if (mode == MODE_READER)
    textOffset = 0;
}
void handleMediaTouch(TouchInfo event) {
  uint16_t key = mediaCommand(event);
  if (key)
    phoneSend(static_cast<MediaKey>(key));
}

void handleHealthTouch(TouchInfo event, int direction) {
  if (healthMenu.enter(event)) {
    startTransition();
    touch.suppressUntilRelease();
    healthSensor.start();
  } else if (!healthMenu.measuring && direction && healthMenu.move(direction))
    startTransition(direction);
  else if (healthMenu.measuring && event.event == DOUBLE_TAP && touchCenter(event)) {
    touch.suppressUntilRelease();
    healthSensor.start();
  }
}

void handleSettingsTouch(TouchInfo event, int direction) {
  if (settingsMenu.enter(event)) {
    startTransition();
    touch.suppressUntilRelease();
  } else if (direction && settingsMenu.editing) {
    brightness = clamp(int(brightness) +
                           sliderStep(event.event) * (event.high_resolution ? 4 : 8),
                       2, 126);
    display.setContrast(brightness);
  } else if (direction && settingsMenu.move(direction))
    startTransition(direction);
}

void handleGameTouch(TouchInfo event, int direction) {
  if (direction)
    paddle = clamp(paddle + brickleStep(event.event) * (event.high_resolution ? 7 : 15),
                   0, CONTENT_W - 30);
  if (event.event == TAP) {
    paddle = event.high_resolution
                 ? clamp(virtualColumn(event.virtual_position) * (CONTENT_W - 30) / 10,
                         0, CONTENT_W - 30)
                 : clamp(padColumn(event.position) * 24 + 12 - 15, 0, CONTENT_W - 30);
    if (waiting) {
      ballX = paddle + 15;
      ballY = 24;
      waiting = false;
      lastGame = millis();
    }
  }
}

void handleReaderTouch(TouchInfo event, int direction) {
  if (event.event == PINCH_INWARD && textSize) {
    startTransition(-1);
    textSize = 0;
    textOffset = 0;
  }
  if (event.event == PINCH_OUTWARD && !textSize) {
    startTransition();
    textSize = 1;
    textOffset = 0;
  }
  if (direction) {
    int limit = int(strlen(message)) - (textSize ? 12 : 54);
    textOffset = clamp(textOffset + direction * 3, 0, limit > 0 ? limit : 0);
  }
}

void handleTouch(TouchInfo event) {
  if (event.event == NONE)
    return;
  if (mode != MODE_MENU && event.event == LONG_PRESS && touchCorner(event)) {
    if (mode == MODE_SETTINGS && settingsMenu.back(event)) {
      startTransition(-1);
      touch.suppressUntilRelease();
    } else if (mode == MODE_HEALTH && healthMenu.back(event)) {
      healthSensor.stop();
      startTransition(-1);
      touch.suppressUntilRelease();
    } else
      changeMode(MODE_MENU);
    return;
  }
  int direction = swipeDirection(event.event);
  switch (mode) {
  case MODE_MENU:
    if (direction) {
      int next = clamp(int(selected) + direction, 0, 4);
      if (next != selected) {
        startTransition(direction);
        selected = next;
      }
    } else if (event.event == DOUBLE_TAP && touchCenter(event))
      changeMode(selected + MODE_MEDIA);
    break;
  case MODE_MEDIA:
  case MODE_MEDIA_ALIAS:
    handleMediaTouch(event);
    break;
  case MODE_HEALTH:
    handleHealthTouch(event, direction);
    break;
  case MODE_SETTINGS:
    handleSettingsTouch(event, direction);
    break;
  case MODE_BRICKLES:
    handleGameTouch(event, direction);
    break;
  case MODE_MESSAGE:
    if (event.event == TAP)
      changeMode(MODE_READER);
    break;
  case MODE_READER:
    handleReaderTouch(event, direction);
    break;
  default:
    break;
  }
}
void tickGame(uint32_t now) {
  if (mode != MODE_BRICKLES)
    return;
  if (waiting) {
    ballX = paddle + 15;
    ballY = 24;
    lastGame = now;
    return;
  }
  if (uint32_t(now - lastGame) < GAME_MS)
    return;
  lastGame = now; // No catch-up burst after slow display/I2C transfers.
  ballX += dx;
  ballY += dy;
  if (ballX <= 1) {
    ballX = 1;
    dx = 1;
  }
  if (ballX >= CONTENT_W - 2) {
    ballX = CONTENT_W - 2;
    dx = -1;
  }
  if (ballY <= 1) {
    ballY = 1;
    dy = 1;
  }
  if (dy > 0 && ballY >= 25 && ballY <= 28 && ballX >= paddle - 1 &&
      ballX <= paddle + 30) {
    ballY = 25;
    dy = -1;
  }
  bool hit = false, remaining = false;
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 10; ++c)
      if (bricks[r][c]) {
        int x = 2 + c * 14, y = 3 + r * 4;
        if (!hit && ballX + 1 >= x && ballX - 1 < x + 12 && ballY + 1 >= y &&
            ballY - 1 < y + 3) {
          bricks[r][c] = false;
          dy = -dy;
          hit = true;
        } else
          remaining = true;
      }
  if (ballY >= 32 || !remaining)
    resetGame();
}
bool digits(const char *value) {
  if (!*value)
    return false;
  for (; *value; ++value)
    if (*value < '0' || *value > '9')
      return false;
  return true;
}
int number(const char *value, int length) {
  int result = 0;
  for (int i = 0; i < length; ++i)
    result = result * 10 + (value[i] - '0');
  return result;
}
bool command(const char *value) {
  size_t length = strlen(value);
  if (length == 5 && strncmp(value, "mode", 4) == 0 && digits(value + 4)) {
    changeMode(value[4] - '0');
    return true;
  }
  if (length >= 4 && length <= 6 && strncmp(value, "bri", 3) == 0 &&
      digits(value + 3)) {
    int b = number(value + 3, length - 3);
    if (b < 2 || b > 126)
      return false;
    brightness = b;
    display.setContrast(brightness);
    return true;
  }
  if ((length == 8 || length == 10 || length == 18) && strncmp(value, "time", 4) == 0 &&
      digits(value + 4)) {
    int h = number(value + 4, 2), m = number(value + 6, 2),
        s = length >= 10 ? number(value + 8, 2) : 0;
    int day = 1, month = 1, year = 1970;
    if (length == 18) {
      day = number(value + 10, 2);
      month = number(value + 12, 2);
      year = number(value + 14, 4);
    }
    if (h > 23 || m > 59 || s > 59 || month < 1 || month > 12 || year < 1970 ||
        year > 2099)
      return false;
    const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maxDay = days[month - 1] +
                 (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (day < 1 || day > maxDay)
      return false;
    setTime(h, m, s, day, month, year);
    clockSynced = true;
    return true;
  }
  return false;
}
void finishInput() {
  if (inputLength || inputOverflow) {
    input[inputLength] = 0;
    if (inputOverflow || !command(input))
      Serial.println("Invalid BLE command");
  }
  inputLength = 0;
  inputOverflow = false;
}
void pollUart(uint32_t now) {
  // Do not join partial commands from separate connections.
  if (uartGeneration != phoneGeneration()) {
    uartGeneration = phoneGeneration();
    inputLength = 0;
    inputOverflow = false;
    timeRequested = false;
  }
  // A companion app can subscribe to UART, receive this request, and reply with
  // timeHHMMSSDDMMYYYY\n using Android's current local date/time.
  if (!timeRequested && uart.notifyEnabled()) {
    uart.println("time?");
    timeRequested = true;
  }
  // Bound work per loop. An idle gap also terminates unframed UART commands.
  for (int budget = 64; budget > 0 && uart.available(); --budget) {
    char c = char(uart.read());
    inputAt = now;
    if (c == '\r' || c == '\n')
      finishInput();
    else if (!inputOverflow) {
      if (inputLength < sizeof(input) - 1)
        input[inputLength++] = c;
      else
        inputOverflow = true;
    }
  }
  if (!uart.available() && (inputLength || inputOverflow) &&
      uint32_t(now - inputAt) >= 80)
    finishInput();
}
void drawMenu() {
  for (int offset = -1; offset <= 1; ++offset) {
    int index = int(selected) + offset;
    if (index >= 0 && index < 5)
      bitmap(icons[index], 74 + offset * 46, 4, 28, 28);
  }
  display.line(71, 1, 104, 1);
  // Dots indicate position; selected icon stays centered.
  for (int i = 0; i < 5; ++i)
    display.rectFill(75 + i * 6, 0, i == selected ? 4 : 2, 1);
}
void drawMedia() {
  // Keep status in the volume strip; leave the bottom button area text-free.
  if (phoneStatus() == PHONE_READY) {
    bitmap(vol0, 42, 24, vol0_w, vol0_h);
    if (volumeIndicator)
      display.rectFill(64, 26, 10 * volumeIndicator, 4);
  } else {
    label(34, 24, "Volume unavailable");
  }
  bitmap(prevsmall, 32, 4, prevsmall_w, prevsmall_h);
  bitmap(nextsmall, 137, 4, nextsmall_w, nextsmall_h);
  bitmap(play, 73, 1, play_w, play_h);
  bitmap(pause, 94, 2, pause_w, pause_h);
}
void drawHealth() {
  bool heart = healthMenu.selected == HEART_RATE;
  char value[24];
  if (!healthMenu.measuring) {
    label(47, 23, heart ? "Heart Rate" : "SpO2");
    if (heart) {
      display.line(68, 12, 77, 12);
      display.line(77, 12, 81, 18);
      display.line(81, 18, 85, 6);
      display.line(85, 6, 89, 12);
      display.line(89, 12, 103, 12);
    } else
      label(76, 7, "O2", 1);
    label(69, 0, heart ? "1 / 2" : "2 / 2");
    if (!heart) {
      display.line(26, 14, 30, 10);
      display.line(26, 14, 30, 18);
    }
    if (heart) {
      display.line(150, 14, 146, 10);
      display.line(150, 14, 146, 18);
    }
    return;
  }
  if (heart && healthSensor.waveform().size() >= 2) {
    // Draw the trace behind an opaque BPM overlay, outside the status rail.
    uint8_t y[CONTENT_W];
    healthSensor.waveform().buildY(y, CONTENT_W, H);
    for (int x = 1; x < CONTENT_W; ++x)
      display.line(STATUS_W + x - 1, y[x - 1], STATUS_W + x, y[x]);
    if (healthSensor.bpmValid)
      snprintf(value, sizeof value, "BPM:%ld", long(healthSensor.bpm));
    else
      snprintf(value, sizeof value, "BPM:--");
    display.rectFill(STATUS_W, 0, 86, 17, BLACK, NORM);
    label(STATUS_W + 3, 0, value, 1);
    const char *caption = healthSensor.bpmValid                     ? "Heart Rate"
                          : healthSensor.state == HEALTH_COLLECTING ? "Hold still..."
                                                                    : "Adjust contact";
    display.rectFill(STATUS_W, 23, 96, 9, BLACK, NORM);
    label(STATUS_W + 3, 24, caption);
    display.rectFill(129, 23, 25, 9, BLACK, NORM);
    label(130, 24, "EXP");
    return;
  }
  label(22, 24, heart ? "Heart Rate" : "SpO2");
  label(130, 24, "EXP");
  const char *prompt = nullptr;
  switch (healthSensor.state) {
  case HEALTH_NO_SENSOR:
    prompt = "Sensor not found";
    break;
  case HEALTH_ERROR:
    prompt = "Sensor read error";
    break;
  case HEALTH_CONTACT:
    prompt = "Cover sensor";
    break;
  case HEALTH_COLLECTING:
    prompt = "Hold still...";
    break;
  default:
    break;
  }
  if (prompt)
    label(28, 12, prompt);
  else if (heart ? healthSensor.bpmValid : healthSensor.spo2Valid) {
    snprintf(value, sizeof value, heart ? "%ld BPM" : "%ld %%",
             long(heart ? healthSensor.bpm : healthSensor.spo2));
    label(48, 6, value, 1);
  } else
    label(28, 12, "Adjust / hold still");
  if (healthSensor.state == HEALTH_COLLECTING) {
    display.rect(28, 1, 116, 4);
    int fill = healthSensor.progress() * 112 / 100;
    if (fill)
      display.rectFill(30, 2, fill, 2);
  } else if (healthSensor.state == HEALTH_ERROR ||
             healthSensor.state == HEALTH_NO_SENSOR)
    label(28, 0, "Double tap to retry");
}
void drawSun(int x, int y) {
  display.circle(x, y, 4);
  display.line(x - 8, y, x - 6, y);
  display.line(x + 6, y, x + 8, y);
  display.line(x, y - 8, x, y - 6);
  display.line(x, y + 6, x, y + 8);
  display.line(x - 6, y - 6, x - 5, y - 5);
  display.line(x + 5, y + 5, x + 6, y + 6);
  display.line(x - 6, y + 6, x - 5, y + 5);
  display.line(x + 5, y - 5, x + 6, y - 6);
}
void drawSettings() {
  char value[16];
  if (!settingsMenu.editing) {
    drawSun(88, 20);
    label(58, 0, settingEntries[settingsMenu.selected].title);
    snprintf(value, sizeof value, "%d/%d", settingsMenu.selected + 1, SETTING_COUNT);
    label(138, 24, value);
    if (settingsMenu.selected > 0) {
      display.line(26, 14, 30, 10);
      display.line(26, 14, 30, 18);
    }
    if (settingsMenu.selected < SETTING_COUNT - 1) {
      display.line(150, 14, 146, 10);
      display.line(150, 14, 146, 18);
    }
    return;
  }
  label(58, 24, "Brightness");
  drawSun(27, 15);
  display.rect(40, 12, 104, 8);
  int fill = (brightness - 2) * 100 / 124;
  if (fill)
    display.rectFill(42, 14, fill, 4);
  snprintf(value, sizeof value, "%d%%", fill);
  label(79, 0, value);
}
void drawStatus(uint32_t now) {
  display.rectFill(0, 0, STATUS_W, H, BLACK, NORM);
  // Placeholder battery level; no voltage measurement is available.
  display.rect(1, 24, 11, 6);
  display.rectFill(12, 26, 2, 2);
  display.rectFill(3, 26, 6, 2);
  PhoneStatus status = phoneStatus();
  bool visible = status != PHONE_PAIRING || (now / 400) % 2 == 0;
  if (visible) {
    // Compact 7 x 11 Bluetooth glyph, centered below the battery.
    display.line(7, 5, 7, 15);
    display.line(7, 15, 10, 12);
    display.line(10, 12, 4, 8);
    display.line(7, 5, 10, 8);
    display.line(10, 8, 4, 12);
  }
  if (status == PHONE_DISCONNECTED)
    display.line(3, 4, 11, 16);
  if (status == PHONE_READY)
    display.rectFill(2, 17, 2, 2);
}
void drawMessage() {
  bitmap(msgopen, 18, 5, msgopen_w, msgopen_h);
  if (mode == MODE_MESSAGE) {
    label(44, 20, "New Message");
    label(44, 10, "Jack");
    label(44, 0, "Tap to read");
    return;
  }
  int width = textSize ? 12 : 18, rows = textSize ? 1 : 3;
  for (int row = 0; row < rows; ++row) {
    char line[19] = {0};
    size_t start = textOffset + row * width;
    if (start < strlen(message))
      strncpy(line, message + start, width);
    label(44, textSize ? 8 : 21 - row * 10, line, textSize);
  }
}
void drawGame() {
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 10; ++c)
      if (bricks[r][c])
        display.rectFill(STATUS_W + 2 + c * 14, 29 - r * 4 - 3, 12, 3);
  display.rectFill(STATUS_W + paddle, 3, 30, 2);
  display.circleFill(STATUS_W + ballX, 31 - ballY, 1);
  if (waiting)
    label(44, 13, "Tap to launch");
}
void render(uint32_t now) {
  display.clearDisplay(CLEAR_BUFFER);
  display.setColor(WHITE);
  display.setDrawMode(NORM);
  char line[32];
  switch (mode) {
  case MODE_MENU:
    drawMenu();
    break;
  case MODE_MEDIA:
    drawMedia();
    break;
  case MODE_CLOCK:
    if (clockSynced)
      snprintf(line, sizeof line, "%02d:%02d:%02d", hour(), minute(), second());
    else
      snprintf(line, sizeof line, "--:--:--");
    label(48, 10, line, 1);
    if (!clockSynced)
      label(28, 0, "Sync via phone app");
    break;
  case MODE_HEALTH:
    drawHealth();
    break;
  case MODE_SETTINGS:
    drawSettings();
    break;
  case MODE_BRICKLES:
    drawGame();
    break;
  case MODE_MESSAGE:
  case MODE_READER:
    drawMessage();
    break;
  case MODE_TOUCH_DEBUG:
    for (int i = 0; i < 12; ++i) {
#if VRING_ANALOG_TOUCH
      snprintf(line, sizeof line, "%d", int(touch.diagnostics().delta[i]));
#else
      snprintf(line, sizeof line, "%d",
               int(touch.cap.baselineData(i)) - int(touch.cap.filteredData(i)));
#endif
      label(18 + padColumn(i) * 23, i < 6 ? 0 : 23, line);
    }
#if VRING_ANALOG_TOUCH
    if (!touch.diagnostics().calibrated)
      label(24, 12, "Lift fingers: calibrate");
    else {
      snprintf(line, sizeof line, "A:%d B:%d", touch.diagnostics().rowPosition[0],
               touch.diagnostics().rowPosition[1]);
      label(24, 12, line);
    }
#else
    label(30, 12, "Touch diagnostics");
#endif
    break;
  case MODE_MEDIA_ALIAS:
    drawMedia();
    break;
  }
  if (!touchReady) {
    display.clearDisplay(CLEAR_BUFFER);
    label(20, 20, "Touch sensor missing");
    label(20, 6, "Retrying MPR121 0x5B");
  }
  if (transitioning) {
    uint32_t elapsed = now - transitionAt;
    if (elapsed >= TRANSITION_MS)
      transitioning = false;
    else {
      memcpy(nextFrame, display.getScreenBuffer(), sizeof nextFrame);
      float t = float(elapsed) / TRANSITION_MS;
      int shift = int(CONTENT_W * (1 - (1 - t) * (1 - t) * (1 - t)));
      display.clearDisplay(CLEAR_BUFFER);
      for (int y = 0; y < H; ++y)
        for (int x = STATUS_W; x < W; ++x) {
          int source = transitionDirection > 0 ? x + shift : x - shift;
          const uint8_t *frame = oldFrame;
          if (source < STATUS_W) {
            source += CONTENT_W;
            frame = nextFrame;
          } else if (source >= W) {
            source -= CONTENT_W;
            frame = nextFrame;
          }
          if (frame[y * 20 + source / 8] & (0x80 >> (source % 8)))
            display.setPixel(x, y);
        }
    }
  }
  drawStatus(now); // Status rail stays fixed during content transitions.
  display.display();
}
} // namespace

void ringSetup() {
  Serial.begin(115200);
  Wire.begin();
  SPI.begin();
  display.begin(W, H);
  display.setContrast(brightness);
  touchReady = touch.begin();
  Bluefruit.begin();
  Bluefruit.setName("VisRing");
  deviceInfo.setManufacturer("OmniLab");
  deviceInfo.setModel("VisRing");
  uart.begin();
  dfu.begin();
  deviceInfo.begin();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  if (!phoneBegin())
    Serial.println("HID initialization failed");
  Bluefruit.Advertising.addName();
  // Primary advertisement identifies a HID device; UART is still discoverable
  // in the scan response for an Android companion app and developer tools.
  Bluefruit.ScanResponse.addService(uart);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
  resetGame();
  render(millis());
}
void ringLoop() {
  uint32_t now = millis();
  uint16_t sent = phonePoll(now);
  if (sent == MEDIA_VOLUME_UP)
    volumeIndicator = clamp(int(volumeIndicator) + 1, 0, 7);
  else if (sent == MEDIA_VOLUME_DOWN)
    volumeIndicator = clamp(int(volumeIndicator) - 1, 0, 7);
  pollUart(now);
  if (touchReady)
    handleTouch(touch.read_touch());
  else if (uint32_t(now - lastTouchRetry) >= 2000) {
    lastTouchRetry = now;
    touchReady = touch.begin();
  }
  now = millis(); // Input handling may have started a transition after the initial
                  // timestamp.
  if (mode == MODE_HEALTH && healthMenu.measuring)
    healthSensor.poll(now);
  tickGame(now);
  if (uint32_t(now - lastFrame) >= FRAME_MS) {
    lastFrame = now;
    render(now);
  }
  delay(1); // Yield to the nRF52 scheduler.
}
