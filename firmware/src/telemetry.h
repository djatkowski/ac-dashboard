// Telemetry reception over USB CDC plus smoothing for display.
#pragma once

#include "frame_parser.h"
#include "protocol.h"

struct DashState {
  AcPacket p{};           // last received packet
  bool linked = false;    // frames are arriving
  bool game_live = false; // ...and the game is actually driving (F_LIVE)
  uint32_t last_rx_ms = 0;
  uint32_t packets = 0;
  uint32_t dropped = 0;   // gaps in the seq numbering
  float rx_hz = 0.0f;     // measured frame rate

  // Smoothed values - the raw ones jitter too much to animate nicely.
  float rpm_smooth = 0.0f;
  float speed_smooth = 0.0f;
  float drift_smooth = 0.0f;
  float yaw_smooth = 0.0f;

  // Drift angle history for the scrolling trace.
  // TRACE_LEN samples every TRACE_INTERVAL_MS => 220 * 25 ms = 5.5 s,
  // independent of how fast the main loop happens to run.
  static constexpr int TRACE_LEN = 220;
  static constexpr uint32_t TRACE_INTERVAL_MS = 25;
  float trace[TRACE_LEN] = {0};
  int trace_head = 0;

  // Peak angle of the current slide - nice to see how far it went.
  float peak_angle = 0.0f;
  uint32_t peak_ms = 0;

  float gForce() const;
  const char* gearLabel() const;
  float rpmFraction() const;
};

class TelemetryLink {
 public:
  void begin();
  void update(DashState& st);  // call once per frame

 private:
  FrameParser parser_;
  uint32_t last_hello_ms_ = 0;
  uint16_t last_seq_ = 0;
  bool have_seq_ = false;
  uint32_t hz_window_start_ = 0;
  uint32_t hz_count_ = 0;
  uint8_t rx_[1024];
};

// Lap time formatting: "1:23.456" or "--:--.---".
void formatLapTime(uint32_t ms, char* out, size_t len);
