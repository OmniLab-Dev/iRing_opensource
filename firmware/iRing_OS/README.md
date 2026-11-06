# iRing firmware

Arduino firmware for the nRF52832 iRing, with a 160 × 32 OLED, capacitive touch
controls, BLE media keys, and optical heart-rate and SpO₂ acquisition.

## Hardware

| Component | Connection |
| --- | --- |
| SSD1320 OLED | CS 15, reset 16, clock 12, data 13; 3-wire SPI |
| MPR121 touch controller | I²C address `0x5B`, 12 electrodes |
| MAX30101 optical sensor | I²C address `0x57`, red and infrared channels |

Pins, addresses, touch thresholds, and optical sensor settings are defined in
[board_config.h](board_config.h). The OLED driver uses a 1-bit framebuffer and
bit-bangs the ninth SPI bit. SDA/SCL follow the selected nRF52 board variant.

## Build

Open **iRing_OS.ino** in Arduino IDE and select **Adafruit Feather nRF52832**
with SoftDevice **S132 6.1.1**. This release is built against Adafruit nRF52 core
**1.6.0**. Board variants with different pin assignments need corresponding
changes in `board_config.h`.

Add the [Adafruit board package index](https://adafruit.github.io/arduino-board-index/package_adafruit_index.json)
to Arduino IDE's Additional Boards Manager URLs, then install the board package
and these libraries through Library Manager:

| Library | Tested version |
| --- | --- |
| Adafruit BusIO | 1.17.0 |
| Time | 1.6.1 |
| SparkFun MAX3010x Pulse and Proximity Sensor Library | 1.1.2 |

SPI, Wire, Bluefruit, and DFU support come with the board package. The modified
MPR121 driver and SSD1320 driver are included beside the sketch.

From the repository root, the equivalent CLI build is:

```sh
arduino-cli compile --fqbn adafruit:nrf52:feather52832 firmware/iRing_OS
```

Upload with your board's existing bootloader or SWD programmer. The application
exposes Adafruit's DFU service, but OTA also requires a compatible bootloader and
DFU package. Compiling the sketch does not install a bootloader.

## Controls

Electrodes are ordered from left to right as follows:

```text
Top:    11  10   9   8   7   6
Bottom:  0   1   2   3   4   5
```

- Swipe to select an app or submenu item.
- Double-tap a center pad (`2`, `3`, `8`, `9`) to enter. The double-tap window is
  320 ms between releases.
- Hold a corner pad (`0`, `5`, `6`, `11`) for 700 ms to go back one level.
- Release after entering or leaving a page before issuing another gesture.

The launcher contains Media, Clock, Health, Settings, and Brickles.

### Media

Pair **VisRing** in the phone's Bluetooth settings. The ring sends BLE HID Consumer
Control reports; playback controls work without a companion app.

| Input | Action |
| --- | --- |
| Top-row swipe | Volume up/down |
| Bottom pads `0`, `1` | Previous track |
| Bottom pads `2`, `3` | Play/pause |
| Bottom pads `4`, `5` | Next track |

Top-row taps and bottom-row swipes are ignored. The volume bar tracks commands
sent by the ring, not the phone's absolute volume. It shows **Volume unavailable**
until the connection is encrypted and the phone subscribes to HID reports.
Media keys control whichever player Android routes them to; track and playback
state are not read back.

The left status strip shows Bluetooth state: crossed when disconnected, blinking
while pairing/subscribing, solid with a dot when HID is ready. The battery icon is
a fixed placeholder; the firmware does not measure battery voltage.

### Clock and Settings

Clock time is set over BLE UART. The display shows `--:--:--` until a valid time
command arrives; time is lost on reboot. Brightness is adjusted in Settings and
applies immediately. Brightness is also lost on reboot.

### Health

Select **Heart Rate** or **SpO₂**, then double-tap the center to start. Keep the
sensor covered and hold still. The first result needs about five seconds of
usable data; subsequent estimates update once per second. Double-tap again to
restart, or hold a corner to stop and return to the Health menu.

The MAX30101 supplies red/IR samples at 25 samples/s after FIFO averaging. The
Maxim reference algorithm uses a four-second window. Heart Rate also shows the
latest 60 IR samples as a scrolling PPG waveform. Missing hardware, contact loss,
read errors, and rejected signal windows clear or suppress results.

These measurements are experimental and have not been medically validated for
this ring's optical geometry. `EXP` identifies the measurement pages. Do not use
them for medical decisions. See [development notes](docs/development.md) for the
acquisition settings and quality checks.

### Brickles

Swipe or tap to position the paddle; tap to launch. The board resets after a miss
or after all bricks are cleared. The game's paddle direction is configured
separately from menu navigation in `control_mapping.h`.

## BLE UART

The UART service remains available alongside HID for time synchronization,
configuration, and diagnostics.

| Endpoint | UUID |
| --- | --- |
| Service | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| RX (write) | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| TX (notify) | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |

Send newline-terminated ASCII commands. The parser also accepts a command after
an 80 ms idle gap for unframed senders. Lines longer than 127 bytes are discarded.

| Command | Meaning |
| --- | --- |
| `mode0` | Launcher |
| `mode1` ... `mode5` | Media, Clock, Health, Settings, Brickles |
| `mode6`, `mode7` | Sample message notification and reader |
| `mode8` | Touch diagnostics |
| `mode9` | Media view alias |
| `bri30` | Brightness (`2`–`126`) |
| `time1430` | 14:30:00 |
| `time143045` | 14:30:45 |
| `time14304531102026` | 14:30:45 on 31 October 2026 |

The short time commands use 1 January 1970 as the date. Full commands use
`timeHHMMSSDDMMYYYY`. Invalid numeric fields and calendar dates are rejected.

After TX notifications are enabled, the ring sends `time?\r\n` once per connection.
A companion should reply with the phone's local date/time, then resend after time
or time-zone changes. HID pairing alone does not provide the phone's time.
The companion app is not included in this repository.

## Development

Run the hardware-independent tests with Python 3.8+ and a C++11 compiler:

```sh
python3 firmware/iRing_OS/tests/run_tests.py
```

Use `--cxx clang++` or set `CXX` to select a compiler. The tests cover gestures,
media reports, control direction, menu navigation, sample-window quality checks,
and waveform scaling. See [development notes](docs/development.md) for optional
Cortex-M4 emulation and [analog touch notes](docs/analog-touch.md) for the disabled
interpolation experiment.
