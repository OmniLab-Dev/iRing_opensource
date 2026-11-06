# Experimental analog touch

The release uses digital MPR121 input. To enable interpolation, set
`VRING_ANALOG_TOUCH` to `1` in `touch_handler.h` or define it at compile time.
Hardware thresholds remain 2/1, as defined in `board_config.h`.

The analog backend estimates 11 positions per six-electrode row: six physical
pads plus five midpoints. It subtracts an idle offset from each channel's
`baseline - filtered` value, groups contiguous contacts, and quantizes their
weighted centroids with boundary hysteresis. It does not interpolate across the
row boundary between electrodes 5 and 6.

## Calibration and weak contacts

Keep fingers off the electrodes for approximately half a second after startup.
The backend collects 16 samples without hardware touch bits, recording each
channel's mean offset and peak idle level. Hardware touches restart calibration.
A subthreshold finger can be learned as idle, so hands-off startup is required.

`VRING_WEAK_PAIRS=1` also qualifies adjacent weak signals when both exceed their
idle peaks, their combined corrected strength reaches the touch threshold, and
the qualifying pair persists for 60 ms. Set it to `0` to require a hardware touch
bit in each contact cluster while retaining position interpolation.

Correlated noise, moisture, and adjacent fingers can be ambiguous. This backend
needs tuning against real sensor traces; it does not increase the ADC resolution
or guarantee linear physical position. It is disabled by default.

## Coordinates

The gesture layer retains nearest-physical-pad fields and adds virtual start/end
IDs plus a `high_resolution` flag. Row-local even positions are real electrodes;
odd positions are midpoints. Virtual IDs 0–10 follow electrodes 0–5; IDs 11–21
follow electrodes 6–11 in wiring order.

`mode8` shows raw deltas and the estimated row positions. Positions range from
0 to 1000 in physical left-to-right order. Physical centers are 0, 200, 400, 600,
800, 1000; midpoints are 100, 300, 500, 700, 900. `-1` means no qualified contact
or more than one separated contact on that row.

Readout uses three I²C block reads at intervals of at least 10 ms. The blocks are
not an atomic ADC snapshot. An I²C failure suppresses gestures until a successful
release sample. Restore `VRING_ANALOG_TOUCH=0` to use digital input without startup
calibration.
