// Ekran DRIFT: kat poslizgu nadwozia, predkosc obrotu, kontra, poslizg opon.
//
// Konwencja znakow (ustalona w pc-app/acbridge/shared_memory.py):
//   drift_angle > 0  -> wektor predkosci ucieka w PRAWO wzgledem osi auta,
//                       czyli tyl wyszedl w prawo, a nos patrzy w lewo.
//   steer > 0        -> kierownica skrecona w prawo.
// Lapanie poslizgu = skret w strone poslizgu, wiec KONTRA jest prawidlowa,
// gdy znak kierownicy zgadza sie ze znakiem kata.
#include <Arduino.h>
#include <math.h>

#include "config.h"
#include "pages.h"

namespace {

constexpr float kGaugeSpan = 70.0f;  // stopni na kazda strone

class DriftPage : public Page {
 public:
  const char* name() const override { return "DRIFT"; }

  void begin() override {
    gauge_.begin(16, 16, 540, 420);
    trace_.begin(16, 448, 540, 204);
    head_.begin(572, 16, 340, 130);
    steer_.begin(572, 158, 340, 120);
    yaw_.begin(572, 290, 340, 120);
    gball_.begin(572, 422, 340, 230);
    tyres_.begin(928, 16, 336, 394);
    grip_.begin(928, 422, 336, 230);
  }

  void enter() override {
    M5.Display.fillRect(0, 0, theme::W, theme::PAGE_H, theme::BG);
    c_angle_ = c_yaw_ = c_steer_ = c_steer_angle_ = c_gx_ = c_gy_ = -1e9f;
    c_speed_ = -1e9f;
    c_gear_ = INT32_MIN;
    c_tyres_ = INT32_MIN;
    c_grip_ = -1e9f;
  }

  void update(const DashState& st) override {
    drawGauge(st);
    drawTrace(st);
    drawHead(st);
    drawSteer(st);
    drawYaw(st);
    drawGBall(st);
    drawTyres(st);
    drawGrip(st);
  }

 private:
  // --- glowny wskaznik kata --------------------------------------------------
  void drawGauge(const DashState& st) {
    if (!ui::changed(c_angle_, st.drift_smooth, 0.15f)) return;

    Panel& p = gauge_;
    M5Canvas& c = p.c();
    p.tile("KAT POSLIZGU");

    const int cx = p.w() / 2;
    const int cy = 310;
    const int r0 = 132, r1 = 206;

    const float angle = st.drift_smooth;
    const float clamped = fmaxf(-kGaugeSpan, fminf(kGaugeSpan, angle));

    // Luk skladamy z segmentow po 5 stopni. Zapalone sa te miedzy zerem
    // a biezacym katem, wiec od razu widac strone i wielkosc poslizgu.
    constexpr float kStep = 5.0f;
    for (float a = -kGaugeSpan; a < kGaugeSpan - 0.1f; a += kStep) {
      const float mid = a + kStep * 0.5f;
      const bool lit = (clamped >= 0.0f) ? (mid > 0.0f && mid <= clamped)
                                         : (mid < 0.0f && mid >= clamped);
      uint16_t color = theme::PANEL_HI;
      if (lit) {
        // Zielony do ~25 stopni, potem zolty, czerwony blisko wykretu.
        const float t = fabsf(mid) / kGaugeSpan;
        color = t < 0.35f ? theme::GREEN
                          : (t < 0.65f ? theme::lerp565(theme::GREEN, theme::YELLOW,
                                                        (t - 0.35f) / 0.30f)
                                       : theme::lerp565(theme::YELLOW, theme::RED,
                                                        (t - 0.65f) / 0.35f));
      }
      ui::arcSegment(c, cx, cy, r0, r1, a + 0.7f, a + kStep - 0.7f, color);
    }

    // Znacznik szczytu z ostatniego poslizgu.
    if (fabsf(st.peak_angle) > 6.0f) {
      const float pk = fmaxf(-kGaugeSpan, fminf(kGaugeSpan, st.peak_angle));
      ui::arcSegment(c, cx, cy, r0 - 12, r1 + 8, pk - 1.2f, pk + 1.2f, theme::MAGENTA);
    }

    // Kreska zera na gorze.
    ui::arcSegment(c, cx, cy, r0 - 10, r1 + 6, -0.8f, 0.8f, theme::TEXT_DIM);

    // Wskazowka.
    const float rad = clamped * (float)M_PI / 180.0f;
    c.drawLine(cx, cy, cx + (int)(r0 * sinf(rad)), cy - (int)(r0 * cosf(rad)), theme::TEXT);
    c.fillCircle(cx, cy, 6, theme::TEXT);

    // Wartosc liczbowo w srodku luku.
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(fabsf(angle)));
    c.setFont(&fonts::DejaVu72);
    c.setTextSize(1.6f);
    c.setTextDatum(textdatum_t::middle_center);
    c.setTextColor(theme::TEXT, theme::PANEL);
    c.drawString(buf, cx, cy - 76);

    c.setFont(&fonts::DejaVu24);
    c.setTextSize(1.0f);
    c.setTextDatum(textdatum_t::middle_center);
    c.setTextColor(theme::TEXT_DIM, theme::PANEL);
    c.drawString("stopni", cx, cy - 14);

    // Strzalka strony poslizgu - szybciej czytelna niz znak liczby.
    if (fabsf(angle) > 3.0f) {
      const char* side = angle > 0 ? "TYL W PRAWO" : "TYL W LEWO";
      c.setFont(&fonts::DejaVu24);
      c.setTextDatum(textdatum_t::middle_center);
      c.setTextColor(angle > 0 ? theme::ORANGE : theme::ACCENT, theme::PANEL);
      c.drawString(side, cx, cy + 46);
    }

    if (fabsf(st.peak_angle) > 6.0f) {
      snprintf(buf, sizeof(buf), "szczyt %d", (int)lroundf(fabsf(st.peak_angle)));
      c.setFont(&fonts::DejaVu18);
      c.setTextDatum(textdatum_t::middle_center);
      c.setTextColor(theme::MAGENTA, theme::PANEL);
      c.drawString(buf, cx, cy + 78);
    }
    p.push();
  }

  // --- historia kata ---------------------------------------------------------
  void drawTrace(const DashState& st) {
    Panel& p = trace_;
    M5Canvas& c = p.c();
    p.tile("HISTORIA (ok. 5 s)");

    const int x0 = 14, y0 = 40;
    const int w = p.w() - 28, h = p.h() - 54;
    const int mid = y0 + h / 2;
    const float scale = (h / 2.0f) / kGaugeSpan;

    // Siatka: zero grubsze, +-30 stopni cienko.
    for (int g = -60; g <= 60; g += 30) {
      if (g == 0) continue;
      const int y = mid - (int)(g * scale);
      c.drawFastHLine(x0, y, w, theme::LINE);
    }
    c.drawFastHLine(x0, mid, w, theme::TEXT_DIM);

    const float dx = (float)w / (DashState::TRACE_LEN - 1);
    int prev_x = x0, prev_y = mid;
    for (int i = 0; i < DashState::TRACE_LEN; i++) {
      // Czytamy od najstarszej probki: head+1 to najstarsza w buforze cyklicznym.
      const int idx = (st.trace_head + 1 + i) % DashState::TRACE_LEN;
      const float v = fmaxf(-kGaugeSpan, fminf(kGaugeSpan, st.trace[idx]));
      const int x = x0 + (int)(i * dx);
      const int y = mid - (int)(v * scale);
      if (i > 0) {
        const uint16_t col = fabsf(v) > 40.0f ? theme::RED
                                              : (fabsf(v) > 20.0f ? theme::YELLOW : theme::GREEN);
        c.drawLine(prev_x, prev_y, x, y, col);
        c.drawLine(prev_x, prev_y + 1, x, y + 1, col);  // pogrubienie
      }
      prev_x = x;
      prev_y = y;
    }
    p.push();
  }

  // --- predkosc i bieg -------------------------------------------------------
  void drawHead(const DashState& st) {
    const int32_t gear = st.game_live ? st.p.gear : -2;
    const bool dirty = ui::changed(c_speed_, st.speed_smooth, 0.5f) |
                       ui::changedInt(c_gear_, gear);
    if (!dirty) return;

    Panel& p = head_;
    M5Canvas& c = p.c();
    p.tile(nullptr);

    char buf[12];
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(fmaxf(0.0f, st.speed_smooth)));
    c.setFont(&fonts::DejaVu56);
    c.setTextSize(1.0f);
    c.setTextDatum(textdatum_t::middle_left);
    c.setTextColor(theme::TEXT, theme::PANEL);
    c.drawString(buf, 18, 72);
    ui::label(c, "km/h", 18, 18);

    ui::label(c, "BIEG", p.w() - 86, 18);
    c.setFont(&fonts::DejaVu56);
    c.setTextDatum(textdatum_t::middle_right);
    c.setTextColor(theme::ACCENT, theme::PANEL);
    c.drawString(gear == -2 ? "-" : st.gearLabel(), p.w() - 24, 72);
    p.push();
  }

  // --- kierownica i kontra ---------------------------------------------------
  void drawSteer(const DashState& st) {
    const bool dirty = ui::changed(c_steer_, st.p.steer, 0.008f) |
                       ui::changed(c_steer_angle_, st.drift_smooth, 0.3f);
    if (!dirty) return;

    Panel& p = steer_;
    M5Canvas& c = p.c();
    p.tile(nullptr);

    ui::label(c, "KIEROWNICA", 16, 14);
    ui::centerBar(c, 16, 46, p.w() - 32, 26, st.p.steer, theme::ACCENT);

    // Kontra jest prawidlowa, gdy skrecamy w strone poslizgu.
    const float angle = st.drift_smooth;
    const bool sliding = fabsf(angle) > 8.0f;
    const bool matching = (angle > 0.0f) == (st.p.steer > 0.0f);
    const char* text = !sliding ? "-" : (matching ? "KONTRA OK" : "W POSLIZG!");
    const uint16_t color =
        !sliding ? theme::TEXT_DIM : (matching ? theme::GREEN : theme::RED);

    c.setFont(&fonts::DejaVu24);
    c.setTextSize(1.0f);
    c.setTextDatum(textdatum_t::middle_center);
    c.setTextColor(color, theme::PANEL);
    c.drawString(text, p.w() / 2, 96);
    p.push();
  }

  // --- predkosc obrotu -------------------------------------------------------
  void drawYaw(const DashState& st) {
    if (!ui::changed(c_yaw_, st.yaw_smooth, 0.4f)) return;

    Panel& p = yaw_;
    M5Canvas& c = p.c();
    p.tile(nullptr);

    ui::label(c, "OBROT (stopni/s)", 16, 14);
    const float norm = fmaxf(-1.0f, fminf(1.0f, st.yaw_smooth / 120.0f));
    ui::centerBar(c, 16, 46, p.w() - 32, 26, norm, theme::YELLOW);

    char buf[16];
    snprintf(buf, sizeof(buf), "%+d", (int)lroundf(st.yaw_smooth));
    c.setFont(&fonts::DejaVu40);
    c.setTextSize(1.0f);
    c.setTextDatum(textdatum_t::middle_center);
    c.setTextColor(theme::TEXT, theme::PANEL);
    c.drawString(buf, p.w() / 2, 94);
    p.push();
  }

  // --- kula przeciazen -------------------------------------------------------
  void drawGBall(const DashState& st) {
    const bool dirty = ui::changed(c_gx_, st.p.g_lat, 0.02f) |
                       ui::changed(c_gy_, st.p.g_lon, 0.02f);
    if (!dirty) return;

    Panel& p = gball_;
    M5Canvas& c = p.c();
    p.tile(nullptr);

    const int cx = p.w() / 2, cy = p.h() / 2 + 8;
    const float scale = 52.0f;  // pikseli na 1 g

    for (int g = 1; g <= 2; g++) {
      c.drawCircle(cx, cy, (int)(g * scale), theme::LINE);
    }
    c.drawFastHLine(cx - 100, cy, 200, theme::LINE);
    c.drawFastVLine(cx, cy - 100, 200, theme::LINE);
    ui::label(c, "G", 16, 8);

    // Hamowanie (g_lon < 0) wypycha kropke do gory - jak w realnym akcelerometrze.
    int dx = (int)(st.p.g_lat * scale);
    int dy = (int)(st.p.g_lon * scale);
    const int lim = 104;
    if (dx > lim) dx = lim;
    if (dx < -lim) dx = -lim;
    if (dy > lim) dy = lim;
    if (dy < -lim) dy = -lim;

    const float mag = st.gForce();
    const uint16_t col = mag > 1.4f ? theme::RED : (mag > 0.9f ? theme::YELLOW : theme::GREEN);
    c.fillCircle(cx + dx, cy + dy, 11, col);

    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f g", mag);
    c.setFont(&fonts::DejaVu24);
    c.setTextDatum(textdatum_t::top_right);
    c.setTextColor(theme::TEXT, theme::PANEL);
    c.drawString(buf, p.w() - 16, 10);
    p.push();
  }

  // --- opony -----------------------------------------------------------------
  void drawTyres(const DashState& st) {
    // Klucz zmiany: temperatury i poslizgi zaokraglone, zeby nie odrysowywac
    // kafelka przy kazdym drgnieciu ostatniej cyfry.
    int32_t key = 0;
    for (int i = 0; i < 4; i++) {
      key = key * 131 + (int32_t)st.p.tyre_temp[i];
      key = key * 131 + (int32_t)(st.p.wheel_slip[i] * 20.0f);
    }
    if (!ui::changedInt(c_tyres_, key)) return;

    Panel& p = tyres_;
    M5Canvas& c = p.c();
    p.tile("OPONY");

    static const char* kNames[4] = {"PL", "PP", "TL", "TP"};  // przod/tyl lewy/prawy
    const int cellW = (p.w() - 42) / 2;
    const int cellH = (p.h() - 76) / 2;

    for (int i = 0; i < 4; i++) {
      const int col = i & 1, row = i >> 1;
      const int x = 14 + col * (cellW + 14);
      const int y = 40 + row * (cellH + 12);

      c.fillRoundRect(x, y, cellW, cellH, 8, theme::PANEL_HI);
      c.drawRoundRect(x, y, cellW, cellH, 8, theme::LINE);

      // Pasek temperatury po lewej krawedzi kafelka.
      const uint16_t tc = ui::tyreColor(st.p.tyre_temp[i]);
      c.fillRoundRect(x + 6, y + 6, 10, cellH - 12, 5, tc);

      ui::label(c, kNames[i], x + 24, y + 8);

      char buf[16];
      snprintf(buf, sizeof(buf), "%d", (int)lroundf(st.p.tyre_temp[i]));
      c.setFont(&fonts::DejaVu40);
      c.setTextSize(1.0f);
      c.setTextDatum(textdatum_t::top_right);
      c.setTextColor(tc, theme::PANEL_HI);
      c.drawString(buf, x + cellW - 14, y + 26);

      // Poslizg - dla driftu wazniejszy niz cokolwiek innego na tym kafelku.
      const float slip = fminf(1.0f, st.p.wheel_slip[i]);
      ui::bar(c, x + 24, y + cellH - 26, cellW - 40, 12, slip,
              slip > 0.6f ? theme::RED : (slip > 0.3f ? theme::YELLOW : theme::GREEN),
              theme::PANEL);

      snprintf(buf, sizeof(buf), "%.1f psi", st.p.tyre_press[i]);
      c.setFont(&fonts::DejaVu18);
      c.setTextDatum(textdatum_t::top_left);
      c.setTextColor(theme::TEXT_DIM, theme::PANEL_HI);
      c.drawString(buf, x + 24, y + cellH - 48);
    }
    p.push();
  }

  // --- przyczepnosc i pedaly -------------------------------------------------
  void drawGrip(const DashState& st) {
    const float key = st.p.surface_grip + st.p.throttle * 2.0f + st.p.brake * 4.0f +
                      st.p.tyres_out * 8.0f;
    if (!ui::changed(c_grip_, key, 0.01f)) return;

    Panel& p = grip_;
    M5Canvas& c = p.c();
    p.tile(nullptr);

    ui::label(c, "GAZ", 16, 14);
    ui::bar(c, 90, 16, p.w() - 110, 22, st.p.throttle, theme::GREEN);
    ui::label(c, "HAM", 16, 52);
    ui::bar(c, 90, 54, p.w() - 110, 22, st.p.brake, theme::RED);

    ui::label(c, "PRZYCZEPNOSC", 16, 96);
    ui::bar(c, 16, 122, p.w() - 32, 20, st.p.surface_grip, theme::ACCENT);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d%%", (int)lroundf(st.p.surface_grip * 100));
    c.setFont(&fonts::DejaVu24);
    c.setTextDatum(textdatum_t::top_right);
    c.setTextColor(theme::ACCENT, theme::PANEL);
    c.drawString(buf, p.w() - 16, 94);

    // Kola poza torem - przy driftcie latwo zahaczyc o trawe.
    if (st.p.tyres_out > 0) {
      c.fillRoundRect(16, 156, p.w() - 32, 50, 8, theme::ORANGE);
      snprintf(buf, sizeof(buf), "KOLA POZA TOREM: %d", st.p.tyres_out);
      c.setFont(&fonts::DejaVu24);
      c.setTextDatum(textdatum_t::middle_center);
      c.setTextColor(theme::BG, theme::ORANGE);
      c.drawString(buf, p.w() / 2, 181);
    } else {
      snprintf(buf, sizeof(buf), "powietrze %d C   tor %d C", (int)lroundf(st.p.air_temp),
               (int)lroundf(st.p.road_temp));
      c.setFont(&fonts::DejaVu18);
      c.setTextDatum(textdatum_t::middle_center);
      c.setTextColor(theme::TEXT_DIM, theme::PANEL);
      c.drawString(buf, p.w() / 2, 181);
    }
    p.push();
  }

  Panel gauge_, trace_, head_, steer_, yaw_, gball_, tyres_, grip_;
  float c_angle_ = -1e9f, c_yaw_ = -1e9f, c_steer_ = -1e9f, c_steer_angle_ = -1e9f;
  float c_gx_ = -1e9f, c_gy_ = -1e9f, c_speed_ = -1e9f, c_grip_ = -1e9f;
  int32_t c_gear_ = INT32_MIN, c_tyres_ = INT32_MIN;
};

DriftPage g_drift;

}  // namespace

Page& driftPage() { return g_drift; }
