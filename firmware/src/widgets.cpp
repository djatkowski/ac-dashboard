#include "widgets.h"

#include <math.h>

bool Panel::begin(int x, int y, int w, int h) {
  x_ = x;
  y_ = y;
  w_ = w;
  h_ = h;
  spr_.setPsram(true);  // 1280x720 will not fit in SRAM
  spr_.setColorDepth(16);
  ready_ = spr_.createSprite(w, h) != nullptr;
  if (ready_) {
    spr_.fillSprite(theme::BG);
    spr_.setTextWrap(false);
  }
  return ready_;
}

void Panel::push() {
  if (ready_) spr_.pushSprite(x_, y_);
}

void Panel::tile(const char* title, uint16_t bg) {
  spr_.fillSprite(theme::BG);
  spr_.fillRoundRect(0, 0, w_, h_, 10, bg);
  spr_.drawRoundRect(0, 0, w_, h_, 10, theme::LINE);
  if (title) {
    spr_.setFont(&fonts::DejaVu18);
    spr_.setTextSize(1.0f);
    spr_.setTextDatum(textdatum_t::top_left);
    spr_.setTextColor(theme::TEXT_DIM, bg);
    spr_.drawString(title, 14, 10);
  }
}

namespace ui {

bool changed(float& cache, float value, float eps) {
  if (fabsf(cache - value) < eps) return false;
  cache = value;
  return true;
}

bool changedInt(int32_t& cache, int32_t value) {
  if (cache == value) return false;
  cache = value;
  return true;
}

void bar(M5Canvas& c, int x, int y, int w, int h, float frac, uint16_t fg, uint16_t bg) {
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;
  c.fillRoundRect(x, y, w, h, h / 2, bg);
  const int fill = (int)(w * frac);
  if (fill > 2) c.fillRoundRect(x, y, fill, h, h / 2, fg);
}

void centerBar(M5Canvas& c, int x, int y, int w, int h, float v, uint16_t fg, uint16_t bg) {
  if (v < -1.0f) v = -1.0f;
  if (v > 1.0f) v = 1.0f;
  c.fillRoundRect(x, y, w, h, h / 2, bg);
  const int mid = x + w / 2;
  const int len = (int)((w / 2) * fabsf(v));
  if (len > 2) {
    if (v >= 0.0f) {
      c.fillRect(mid, y, len, h, fg);
    } else {
      c.fillRect(mid - len, y, len, h, fg);
    }
  }
  c.drawFastVLine(mid, y - 2, h + 4, theme::TEXT_DIM);  // zero marker
}

void arcSegment(M5Canvas& c, int cx, int cy, int r0, int r1, float a0_deg, float a1_deg,
                uint16_t color) {
  // We do our own trigonometry instead of fillArc so the angle convention is
  // identical for the arc and for everything drawn on it (ticks, needle).
  const float a0 = a0_deg * (float)M_PI / 180.0f;
  const float a1 = a1_deg * (float)M_PI / 180.0f;
  const float s0 = sinf(a0), c0 = cosf(a0);
  const float s1 = sinf(a1), c1 = cosf(a1);

  const int x0o = cx + (int)(r1 * s0), y0o = cy - (int)(r1 * c0);
  const int x1o = cx + (int)(r1 * s1), y1o = cy - (int)(r1 * c1);
  const int x0i = cx + (int)(r0 * s0), y0i = cy - (int)(r0 * c0);
  const int x1i = cx + (int)(r0 * s1), y1i = cy - (int)(r0 * c1);

  c.fillTriangle(x0o, y0o, x1o, y1o, x0i, y0i, color);
  c.fillTriangle(x1o, y1o, x1i, y1i, x0i, y0i, color);
}

void centerText(M5Canvas& c, const char* s, int x, int y, const lgfx::IFont* font, float size,
                uint16_t color) {
  c.setFont(font);
  c.setTextSize(size);
  c.setTextDatum(textdatum_t::top_center);
  c.setTextColor(color);
  c.drawString(s, x, y);
}

void label(M5Canvas& c, const char* s, int x, int y, uint16_t color) {
  c.setFont(&fonts::DejaVu18);
  c.setTextSize(1.0f);
  c.setTextDatum(textdatum_t::top_left);
  c.setTextColor(color);
  c.drawString(s, x, y);
}

uint16_t tyreColor(float temp_c) {
  // Working window of a typical road / semi-slick tyre: ~80-95 C.
  // Below 60 it is cold, above 110 it is overheating.
  const float t = (temp_c - 55.0f) / 60.0f;
  return theme::heatColor(t);
}

}  // namespace ui
