// User settings. The only file worth editing before flashing.
#pragma once

// --- Connection ---------------------------------------------------------------
// Data arrives over USB-C - the same cable you flash the firmware with.
// Nothing to configure here: the PC finds the Tab5 by itself.

// How long without a frame before we call the link dead (ms)
#define LINK_TIMEOUT_MS 1500

// How often we shout "ACT5HELLO" until the PC finds us (ms)
#define HELLO_PERIOD_MS 500

// --- Appearance ---------------------------------------------------------------
#define SCREEN_BRIGHTNESS 200  // 0..255
#define TARGET_FPS 45

// Startup screen: 0 = RACE (speed/rpm), 1 = DRIFT
#define DEFAULT_PAGE 0

// Shift light thresholds as a fraction of maxRpm
#define SHIFT_LIGHT_START 0.80f
#define SHIFT_LIGHT_RED 0.93f

// Full-scale deflection of the drift angle gauge (degrees)
#define DRIFT_GAUGE_MAX 70.0f

// How long the peak-angle marker stays on the gauge after the last new peak
#define PEAK_HOLD_MS 5000
