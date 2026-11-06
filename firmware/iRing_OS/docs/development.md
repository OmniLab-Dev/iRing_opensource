# Firmware development

## Source layout

| File | Responsibility |
| --- | --- |
| `iRing_OS.ino` | Arduino entry points |
| `board_config.h` | Board pins, sensor addresses, acquisition settings |
| `ring_os.cpp` | App state, input dispatch, rendering, UART commands |
| `gesture_engine.*` | Debounce, taps, holds, slides, pinch gestures |
| `touch_handler.h` | MPR121 access and touch backend selection |
| `control_mapping.h` | Row roles, slider and paddle direction, media key mapping |
| `phone_link.*`, `media_queue.h` | HID service, connection state, press/release queue |
| `health_sensor.*` | MAX30101 setup and checked FIFO acquisition |
| `health_window.h`, `health_waveform.h` | Measurement windows and display history |
| `health_menu.h`, `settings_menu.h` | Submenu navigation |
| `analog_touch.*` | Optional interpolated touch backend |

Rendering targets a 33 ms interval. Touch, UART, sensor polling, and game updates
run between frames. Display transfer time determines the actual frame rate.
A transition uses two 640-byte snapshots and redraws the left status strip last.

App mode numbers are part of the UART protocol. Keep those values stable when
adding apps. Add settings entries to `settingEntries` and implement the matching
page renderer and input handler. Keep corner-hold navigation in `handleTouch()`.

## Sensor settings

The default touch backend reads MPR121 touch bits with thresholds 2/1. It does not
perform startup calibration. Raw values are available on UART-selected `mode8`.

MAX30101 configuration is in `board_config.h`: red/IR mode, LED amplitude 60,
100 Hz sampling, averaging 4, 411 µs pulse width, ADC range 4096. The resulting
25 samples/s must match the reference algorithm's `FreqS` constant. Sensor setup
is synchronous; acquisition is polled afterward. Each poll reads at most eight
FIFO samples. Overflow resets the window; read failure or 1.2 seconds without
samples stops acquisition.

`health_window.h` uses provisional contact and signal-quality heuristics. Contact
requires red >= 1000 and IR >= 10000; raw values >= 260000 are treated as saturated.
The algorithm receives both channels divided by four to constrain 32-bit products.
A full 100-sample window must have modulation in both channels, and two consecutive
valid algorithm windows are required before displaying a result. These checks do
not establish clinical accuracy; tune them against captured data and a reference
instrument when changing the optics or LED settings.

The waveform retains 60 raw IR samples independently of the calculation window.
Its minimum scaling span is 32 ADC counts to limit magnification of small noise.

## Tests

Host tests require only Python 3.8+ and a C++11 compiler:

```sh
python3 firmware/iRing_OS/tests/run_tests.py
python3 firmware/iRing_OS/tests/run_tests.py --suite direction --cxx clang++
```

Temporary executables are removed after the run. On Windows, use Python and a
GNU-compatible C++ compiler, such as MinGW g++. `--cxx` can include compiler flags.

The same suites can run as compiled Cortex-M4 code in Unicorn:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install unicorn==2.1.4
python firmware/iRing_OS/tests/run_arm.py /path/to/arm-none-eabi/bin
```

On Windows activate `.venv/Scripts/activate` instead. The runner accepts toolchains
with or without `.exe` suffixes and writes only temporary build files. Emulated
tests do not cover BLE radio behavior, I²C transactions, display orientation, or
measurement accuracy.

## Hardware checks

After changes to input or board configuration, check swipes on both rows, center
double-taps, corner holds, and paddle direction. Also check HID pairing/reconnect,
time commands, live brightness, sensor removal during acquisition, and exit while
measurements are running. The application exposes DFU over GATT; the bootloader
and OTA package must match the board and SoftDevice.

## Third-party code

`Adafruit_MPR121.*` includes local changes for touch configuration and block
readout. The SSD1320 driver and `util/` fonts are maintained separately from the
app code. Preserve their notices when editing or redistributing them. See the
repository's [third-party notes](../../../THIRD_PARTY.md).
