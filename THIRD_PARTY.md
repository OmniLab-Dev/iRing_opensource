# Third-party components

The following components retain their upstream notices. This document does not
assign a license to project-authored code or replace the terms in individual files.

| Component | Location | Upstream / notices |
| --- | --- | --- |
| Adafruit MPR121 driver, locally modified | `firmware/iRing_OS/Adafruit_MPR121.*` | [Adafruit MPR121](https://github.com/adafruit/Adafruit_MPR121); BSD notice in source headers |
| MicroView-derived fonts | `firmware/iRing_OS/util/` | Files identifying [MicroView](https://github.com/geekammo/MicroView-Arduino-Library) carry GPL-3.0-or-later notices |
| nRF52 / Bluefruit / DFU | Board package dependency | [Adafruit nRF52 Arduino](https://github.com/adafruit/Adafruit_nRF52_Arduino) |
| MAX3010x driver and Maxim SpO₂ algorithm | Library dependency | [SparkFun MAX3010x library](https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library); see its source and algorithm notices |
| BusIO | Library dependency | [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO) |
| Time | Library dependency | [Time](https://github.com/PaulStoffregen/Time) |

The SSD1320 driver and bitmap assets are included with the firmware. Preserve any
source attribution and license headers in these files. No repository-wide license
has been specified; the project maintainer must choose terms for the original
code before publishing a licensed release.
