// RACE screen: rev bar, speed, gear, lap times.
#include <Arduino.h>
#include <math.h>

#include "config.h"
#include "pages.h"

namespace {

class RacePage : public Page {
 public:
  const char* name() const override { return "RACE"; }

  void begin() override {
    rpm_.begin(0, 0, 1280, 76);
    speed_.begin(16, 88, 400, 300);
    gear_.begin(432, 88, 280, 300);
    rpmnum_.begin(728, 88, 280, 140);
    aids_.begin(728, 244, 280, 144);
    fuel_.begin(1024, 88, 240, 300);
    laps_.begin(16, 400, 992, 140);
    pos_.begin(1024, 400, 240, 140);
    pedals_.begin(16, 552, 1248, 100);
  }

  void enter() override {
    M5.Display.fillRect(0, 0, theme::W, theme::PAGE_H, theme::BG);
    invalidate();
  }

  void update(const DashState& st) override {
    drawRpmBar(st);
    drawSpeed(st);
    drawGear(st);
    drawRpmNumber(st);
    drawAids(st);
    drawFuel(st);
    drawLaps(st);
    drawPosition(st);
    drawPedals(st);
  }

 private:
  void invalidate() {
    c_rpm_ = c_speed_ = c_fuel_ = -1e9f;
    c_gear_ = c_rpmnum_ = c_aids_ = c_lap_ = c_pos_ = INT32_MIN;
    c_thr_ = c_brk_ = c_clu_ = c_steer_ = -1e9f;
    c_blink_ = -1;
  }

  // --- rev bar ---------------------------------------------------------------
  void drawRpmBar(const DashState& st) {
    const float frac = st.rpmFraction();
    // Flashing on the limiter: the single most important signal on the
    // whole screen, it has to register in peripheral vision.
    const bool limiter = (st.p.flags & F_ENGINE_LIMITER) && st.game_live;
    const int blink = limiter ? (int)((millis() / 80) % 2) : 0;

    if (!ui::changed(c_rpm_, frac, 0.004f) && blink == c_blink_) return;
    c_blink_ = blink;

    M5Canvas& c = rpm_.c();
    c.fillSprite(theme::BG);

    constexpr int kSegments = 48;
    constexpr int kGap = 4;
    const int segW = (1280 - kGap * (kSegments + 1)) / kSegments;

    for (int i = 0; i < kSegments; i++) {
      const float segFrac = (i + 0.5f) / kSegments;
      uint16_t color;
      if (segFrac > frac) {
        color = theme::PANEL;  // segment off
      } else if (blink) {
        color = theme::TEXT;   // limiter flash
      } else if (segFrac < SHIFT_LIGHT_START) {
        color = theme::lerp565(theme::GREEN, theme::YELLOW, segFrac / SHIFT_LIGHT_START);
      } else if (segFrac < SHIFT_LIGHT_RED) {
        color = theme::ORANGE;
      } else {
        color = theme::RED;
      }
      const int x = kGap + i * (segW + kGap);
      c.fillRoundRect(x, 12, segW, 52, 3, color);
    }
    rpm_.push();
  }

  // --- speed -----------------------------------------------------------------
  void drawSpeed(const DashState& st) {
    if (!ui::changed(c_speed_, st.speed_smooth, 0.4f)) return;
    Panel& p = speed_;
    M5Canvas& c = p.c();
    p.tile("SPEED");

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(fmaxf(0.0f, st.speed_smooth)));
    c.setFont(&fonts::Font7);
    c.setTextSize(3.2f);
    c.setTextDatum(textdatum_t::middle_center);
    c.setTextColor(theme::TEXT, theme::PANEL);
    c.drawString(buf, p.w() / 2, p.h() / 2 + 8);

    ui::label(c, "km/h", p.w() - 62, p.h() - 34);
    p.push();
  }

  // --- gear ------------------------------------------------------------------
  void drawGear(const DashState& st) {
    const int32_t gear = st.game_live ? st.p.gear : -2;
    const float frac = st.rpmFraction();
    const int32_t zone = frac > SHIFT_LIGHT_RED ? 2 : (frac > SHIFT_LIGHT_START ? 1 : 0);
    const int32_t key = gear * 4 + zone;
    if (!ui::changedInt(c_gear_, key)) return;

    Panel& p = gear_;
    M5Canvas& c = p.c();
    // The gear tile changes colour near the shift point, so an upshift is
    // visible without looking straight at it.
    const uint16_t bg = zone == 2 ? theme::RED : (zone == 1 ? theme::ORANGE : theme::PANEL);
    p.tile("GEAR", bg);

    c.setFont(&fonts::DejaVu72);
    c.setTextSize(2.6f);
    c.setTextDatum(textdatum_t::middle_center);
    c.setTextColor(theme::TEXT, bg);
    c.drawString(gear == -2 ? "-" : st.gearLabel(), p.w() / 2, p.h() / 2 + 8);
    p.push();
  }

  // --- revs as a number ------------------------------------------------------
  void drawRpmNumber(const DashState& st) {
    const int32_t rpm = (int32_t)(lroundf(st.rpm_smooth / 10.0f) * 10);
    if (!ui::changedInt(c_rpmnum_, rpm)) return;

    Panel& p = rpmnum_;
    M5Canvas& c = p.c();
    p.tile("RPM");

    char buf[12];
    snprintf(buf, sizeof(buf), "%d", (int)rpm);
    c.setFont(&fonts::DejaVu56);
    c.setTextSize(1.0f);
    c.setTextDatum(textdatum_t::middle_right);
    c.setTextColor(theme::ACCENT, theme::PANEL);
    c.drawString(buf, p.w() - 58, p.h() / 2 + 12);
    ui::label(c, "rpm", p.w() - 48, p.h() / 2 - 2);
    p.push();
  }

  // --- ABS / TC / DRS / PIT --------------------------------------------------
  void drawAids(const DashState& st) {
    const int32_t key = st.game_live ? (int32_t)st.p.flags : -1;
    if (!ui::changedInt(c_aids_, key)) return;

    Panel& p = aids_;
    M5Canvas& c = p.c();
    p.tile(nullptr);

    struct Item {
      const char* text;
      uint8_t flag;
      uint16_t color;
    };
    static const Item kItems[] = {
        {"ABS", F_ABS_ACTIVE, theme::BLUE},
        {"TC", F_TC_ACTIVE, theme::YELLOW},
        {"DRS", F_DRS, theme::GREEN},
        {"PIT", F_PIT_LIMITER, theme::MAGENTA},
    };
    const int n = sizeof(kItems) / sizeof(kItems[0]);
    const int boxW = (p.w() - 28 - (n - 1) * 8) / n;

    for (int i = 0; i < n; i++) {
      const bool on = st.game_live && (st.p.flags & kItems[i].flag);
      const int x = 14 + i * (boxW + 8);
      c.fillRoundRect(x, 34, boxW, 76, 8, on ? kItems[i].color : theme::PANEL_HI);
      c.setFont(&fonts::DejaVu24);
      c.setTextSize(1.0f);
      c.setTextDatum(textdatum_t::middle_center);
      c.setTextColor(on ? theme::BG : theme::TEXT_DIM);
      c.drawString(kItems[i].text, x + boxW / 2, 72);
    }
    p.push();
  }

  // --- fuel ------------------------------------------------------------------
  void drawFuel(const DashState& st) {
    if (!ui::changed(c_fuel_, st.p.fuel_l, 0.05f)) return;

    Panel& p = fuel_;
    M5Canvas& c = p.c();
    p.tile("FUEL");

    const float pct = st.p.fuel_pct / 100.0f;
    const uint16_t color = pct < 0.1f ? theme::RED : (pct < 0.25f ? theme::ORANGE : theme::GREEN);

    // A vertical tank reads faster at a glance than a horizontal bar.
    const int bx = 30, by = 56, bw = 56, bh = 200;
    c.fillRoundRect(bx, by, bw, bh, 8, theme::PANEL_HI);
    const int fill = (int)(bh * pct);
    if (fill > 4) c.fillRoundRect(bx, by + bh - fill, bw, fill, 8, color);
    c.drawRoundRect(bx, by, bw, bh, 8, theme::LINE);

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", st.p.fuel_l);
    c.setFont(&fonts::DejaVu40);
    c.setTextDatum(textdatum_t::middle_left);
    c.setTextColor(theme::TEXT, theme::PANEL);
    c.drawString(buf, bx + bw + 20, by + 60);
    ui::label(c, "liters", bx + bw + 22, by + 82);

    snprintf(buf, sizeof(buf), "%d%%", st.p.fuel_pct);
    c.setFont(&fonts::DejaVu24);
    c.setTextDatum(textdatum_t::middle_left);
    c.setTextColor(color, theme::PANEL);
    c.drawString(buf, bx + bw + 20, by + 150);
    p.push();
  }

  // --- lap times -------------------------------------------------------------
  void drawLaps(const DashState& st) {
    const int32_t key = (int32_t)(st.p.lap_ms / 10) ^ (int32_t)st.p.last_lap_ms ^
                        (int32_t)(st.p.best_lap_ms << 1);
    if (!ui::changedInt(c_lap_, key)) return;

    Panel& p = laps_;
    M5Canvas& c = p.c();
    p.tile(nullptr);

    struct Item {
      const char* title;
      uint32_t ms;
      uint16_t color;
    };
    const Item kItems[] = {
        {"CURRENT", st.p.lap_ms, theme::TEXT},
        {"LAST", st.p.last_lap_ms, theme::ACCENT},
        {"BEST", st.p.best_lap_ms, theme::MAGENTA},
    };
    const int boxW = (p.w() - 28 - 2 * 10) / 3;

    char buf[16];
    for (int i = 0; i < 3; i++) {
      const int x = 14 + i * (boxW + 10);
      c.fillRoundRect(x, 12, boxW, p.h() - 24, 8, theme::PANEL_HI);
      ui::label(c, kItems[i].title, x + 16, 20);
      formatLapTime(st.game_live ? kItems[i].ms : 0, buf, sizeof(buf));
      c.setFont(&fonts::DejaVu56);
      c.setTextSize(1.0f);
      c.setTextDatum(textdatum_t::middle_center);
      c.setTextColor(kItems[i].color, theme::PANEL_HI);
      c.drawString(buf, x + boxW / 2, 88);
    }
    p.push();
  }

  // --- position and lap count ------------------------------------------------
  void drawPosition(const DashState& st) {
    const int32_t key = st.p.position * 10000 + st.p.lap_count;
    if (!ui::changedInt(c_pos_, key)) return;

    Panel& p = pos_;
    M5Canvas& c = p.c();
    p.tile(nullptr);

    char buf[12];
    ui::label(c, "POSITION", 16, 16);
    snprintf(buf, sizeof(buf), "%d", st.game_live ? st.p.position : 0);
    c.setFont(&fonts::DejaVu40);
    c.setTextDatum(textdatum_t::middle_left);
    c.setTextColor(theme::TEXT, theme::PANEL);
    c.drawString(buf, 16, 62);

    ui::label(c, "LAPS", 130, 16);
    snprintf(buf, sizeof(buf), "%d", st.game_live ? st.p.lap_count : 0);
    c.setTextColor(theme::ACCENT, theme::PANEL);
    c.drawString(buf, 130, 62);
    p.push();
  }

  // --- pedals and steering ---------------------------------------------------
  void drawPedals(const DashState& st) {
    const bool dirty = ui::changed(c_thr_, st.p.throttle, 0.01f) |
                       ui::changed(c_brk_, st.p.brake, 0.01f) |
                       ui::changed(c_clu_, st.p.clutch, 0.01f) |
                       ui::changed(c_steer_, st.p.steer, 0.01f);
    if (!dirty) return;

    Panel& p = pedals_;
    M5Canvas& c = p.c();
    p.tile(nullptr);

    // Two columns. The left one ends at x=480 (bars plus their percentages),
    // the right one starts at x=520 - they used to overlap.
    constexpr int kBarH = 26;
    constexpr int kLeftBarX = 90, kLeftBarW = 290;   // 90..380
    constexpr int kPctRight = 480;                   // percentages right-aligned here
    constexpr int kRightLabelX = 520;
    constexpr int kRightBarX = 620;

    // Both rows share one baseline: bars at y=18 and y=58, so their centres
    // are at 31 and 71. Captions and percentages hang off those same numbers
    // instead of being positioned by eye.
    constexpr int kRow1 = 18, kRow2 = 58;
    constexpr int kMid1 = kRow1 + kBarH / 2;   // 31
    constexpr int kMid2 = kRow2 + kBarH / 2;   // 71

    ui::labelMid(c, "THR", 16, kMid1);
    ui::bar(c, kLeftBarX, kRow1, kLeftBarW, kBarH, st.p.throttle, theme::GREEN);
    ui::labelMid(c, "BRK", 16, kMid2);
    ui::bar(c, kLeftBarX, kRow2, kLeftBarW, kBarH, st.p.brake, theme::RED);

    ui::labelMid(c, "CLU", kRightLabelX, kMid1);
    ui::bar(c, kRightBarX, kRow1, 200, kBarH, st.p.clutch, theme::YELLOW);

    ui::labelMid(c, "STEERING", kRightLabelX, kMid2);
    ui::centerBar(c, kRightBarX, kRow2, p.w() - kRightBarX - 18, kBarH, st.p.steer,
                  theme::ACCENT);

    // Throttle/brake as numbers - useful when analysing corner entry.
    char buf[16];
    c.setFont(&fonts::DejaVu24);
    c.setTextSize(1.0f);
    c.setTextDatum(textdatum_t::middle_right);
    snprintf(buf, sizeof(buf), "%d%%", (int)(st.p.throttle * 100));
    c.setTextColor(theme::GREEN, theme::PANEL);
    c.drawString(buf, kPctRight, kMid1);
    snprintf(buf, sizeof(buf), "%d%%", (int)(st.p.brake * 100));
    c.setTextColor(theme::RED, theme::PANEL);
    c.drawString(buf, kPctRight, kMid2);
    p.push();
  }

  Panel rpm_, speed_, gear_, rpmnum_, aids_, fuel_, laps_, pos_, pedals_;
  float c_rpm_ = -1e9f, c_speed_ = -1e9f, c_fuel_ = -1e9f;
  float c_thr_ = -1e9f, c_brk_ = -1e9f, c_clu_ = -1e9f, c_steer_ = -1e9f;
  int32_t c_gear_ = INT32_MIN, c_rpmnum_ = INT32_MIN, c_aids_ = INT32_MIN;
  int32_t c_lap_ = INT32_MIN, c_pos_ = INT32_MIN;
  int c_blink_ = -1;
};

RacePage g_race;

}  // namespace

Page& racePage() { return g_race; }
