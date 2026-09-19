#include "telemetry.h"

#include <Arduino.h>
#include <math.h>

#include "config.h"

namespace {

// Exponential smoothing that does not depend on the frame interval.
inline void smooth(float& value, float target, float tau_s, float dt) {
  const float a = 1.0f - expf(-dt / tau_s);
  value += (target - value) * a;
}

}  // namespace

float DashState::gForce() const { return sqrtf(p.g_lat * p.g_lat + p.g_lon * p.g_lon); }

const char* DashState::gearLabel() const {
  static const char* kGears[] = {"R", "N", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
  int idx = p.gear + 1;
  if (idx < 0) idx = 1;
  if (idx > 10) idx = 10;
  return kGears[idx];
}

float DashState::rpmFraction() const {
  if (p.max_rpm <= 100.0f) return 0.0f;
  float f = rpm_smooth / p.max_rpm;
  return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

void TelemetryLink::begin() {
  // USB CDC ignores the baud rate, but Serial.begin is still required.
  Serial.setRxBufferSize(8192);
  Serial.begin(921600);
  last_hello_ms_ = 0;
}

void TelemetryLink::update(DashState& st) {
  const uint32_t now = millis();

  // Until the PC finds us we announce ourselves. Once linked we go quiet, so
  // no text gets mixed into the binary stream flowing the other way.
  if (!st.linked && now - last_hello_ms_ >= HELLO_PERIOD_MS) {
    last_hello_ms_ = now;
    Serial.print(AC_HELLO_LINE);
  }

  // Drain EVERYTHING that arrived and keep the freshest frame - at 60 Hz a
  // queue of stale frames has no value.
  bool got = false;
  while (int avail = Serial.available()) {
    const size_t want = (avail > (int)sizeof(rx_)) ? sizeof(rx_) : (size_t)avail;
    const size_t n = Serial.read(rx_, want);
    if (n == 0) break;

    AcPacket in;
    if (parser_.feed(rx_, n, in)) {
      if (have_seq_) {
        const uint16_t gap = (uint16_t)(in.seq - last_seq_);
        if (gap > 1 && gap < 1000) st.dropped += gap - 1;
      }
      last_seq_ = in.seq;
      have_seq_ = true;

      st.p = in;
      got = true;
      st.packets++;
      hz_count_++;
    }
  }

  if (got) st.last_rx_ms = now;
  st.linked = (st.last_rx_ms != 0) && (now - st.last_rx_ms < LINK_TIMEOUT_MS);
  st.game_live = st.linked && (st.p.flags & F_LIVE);

  if (now - hz_window_start_ >= 1000) {
    st.rx_hz = hz_count_ * 1000.0f / (float)(now - hz_window_start_);
    hz_window_start_ = now;
    hz_count_ = 0;
  }

  // --- smoothing and history ---
  static uint32_t last_ms = 0;
  float dt = (last_ms == 0) ? 0.02f : (now - last_ms) / 1000.0f;
  last_ms = now;
  if (dt <= 0.0f) dt = 0.001f;
  if (dt > 0.25f) dt = 0.25f;

  if (!st.game_live) {
    smooth(st.rpm_smooth, 0.0f, 0.25f, dt);
    smooth(st.speed_smooth, 0.0f, 0.25f, dt);
    smooth(st.drift_smooth, 0.0f, 0.15f, dt);
    smooth(st.yaw_smooth, 0.0f, 0.15f, dt);
  } else {
    // Barely smooth the revs: lag on the shift lights is worse than a
    // slightly jittery number.
    smooth(st.rpm_smooth, st.p.rpm, 0.030f, dt);
    smooth(st.speed_smooth, st.p.speed_kmh, 0.060f, dt);
    smooth(st.drift_smooth, st.p.drift_angle, 0.045f, dt);
    smooth(st.yaw_smooth, st.p.yaw_rate, 0.045f, dt);
  }

  st.trace_head = (st.trace_head + 1) % DashState::TRACE_LEN;
  st.trace[st.trace_head] = st.drift_smooth;

  // Hold the peak angle for 4 s after the last significant deflection.
  const float mag = fabsf(st.drift_smooth);
  if (mag > 6.0f && mag > fabsf(st.peak_angle)) {
    st.peak_angle = st.drift_smooth;
    st.peak_ms = now;
  } else if (now - st.peak_ms > 4000) {
    st.peak_angle = 0.0f;
  }
}

void formatLapTime(uint32_t ms, char* out, size_t len) {
  if (ms == 0 || ms > 3600000UL) {
    snprintf(out, len, "--:--.---");
    return;
  }
  const uint32_t m = ms / 60000UL;
  const uint32_t s = (ms / 1000UL) % 60UL;
  const uint32_t f = ms % 1000UL;
  snprintf(out, len, "%lu:%02lu.%03lu", (unsigned long)m, (unsigned long)s, (unsigned long)f);
}
