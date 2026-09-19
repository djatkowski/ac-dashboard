// Odbior telemetrii po USB CDC + wygladzanie wartosci pod wyswietlanie.
#pragma once

#include "frame_parser.h"
#include "protocol.h"

struct DashState {
  AcPacket p{};           // ostatni odebrany pakiet
  bool linked = false;    // ramki przychodza na biezaco
  bool game_live = false; // ...i gra faktycznie jedzie (flaga F_LIVE)
  uint32_t last_rx_ms = 0;
  uint32_t packets = 0;
  uint32_t dropped = 0;   // dziury w numeracji seq
  float rx_hz = 0.0f;     // zmierzona czestotliwosc ramek

  // Wygladzone wartosci - surowe skacza za bardzo, zeby je ladnie animowac.
  float rpm_smooth = 0.0f;
  float speed_smooth = 0.0f;
  float drift_smooth = 0.0f;
  float yaw_smooth = 0.0f;

  // Historia kata driftu do wykresu przewijanego (ok. 5 s przy 45 fps).
  static constexpr int TRACE_LEN = 220;
  float trace[TRACE_LEN] = {0};
  int trace_head = 0;

  // Szczyt kata w biezacym driftcie - fajnie widziec, ile sie wyciagnelo.
  float peak_angle = 0.0f;
  uint32_t peak_ms = 0;

  float gForce() const;
  const char* gearLabel() const;
  float rpmFraction() const;
};

class TelemetryLink {
 public:
  void begin();
  void update(DashState& st);  // wolac w kazdej klatce

 private:
  FrameParser parser_;
  uint32_t last_hello_ms_ = 0;
  uint16_t last_seq_ = 0;
  bool have_seq_ = false;
  uint32_t hz_window_start_ = 0;
  uint32_t hz_count_ = 0;
  uint8_t rx_[1024];
};

// Formatowanie czasu okrazenia: "1:23.456" albo "--:--.---".
void formatLapTime(uint32_t ms, char* out, size_t len);
