// Small UI building blocks. Every Panel is a PSRAM sprite redrawn only when
// its data actually changed - at 1280x720, repainting the whole screen every
// frame would waste framebuffer bandwidth on pixels that did not move.
#pragma once

#include <M5Unified.h>

#include "theme.h"

class Panel {
 public:
  bool begin(int x, int y, int w, int h);
  M5Canvas& c() { return spr_; }
  void push();

  int w() const { return w_; }
  int h() const { return h_; }
  bool ready() const { return ready_; }

  // Tile background with a border and an optional title in the top-left.
  void tile(const char* title = nullptr, uint16_t bg = theme::PANEL);

 private:
  M5Canvas spr_{&M5.Display};
  int x_ = 0, y_ = 0, w_ = 0, h_ = 0;
  bool ready_ = false;
};

namespace ui {

// Returns true and updates the cache if the value changed meaningfully.
bool changed(float& cache, float value, float eps);
bool changedInt(int32_t& cache, int32_t value);

// Horizontal fill bar, 0..1, with a track behind it.
void bar(M5Canvas& c, int x, int y, int w, int h, float frac, uint16_t fg,
         uint16_t bg = theme::LINE);

// Bidirectional bar: zero in the middle, -1..1 extends left or right.
void centerBar(M5Canvas& c, int x, int y, int w, int h, float v, uint16_t fg,
               uint16_t bg = theme::LINE);

// One arc segment. Angles: 0 = up, increasing clockwise.
void arcSegment(M5Canvas& c, int cx, int cy, int r0, int r1, float a0_deg,
                float a1_deg, uint16_t color);

// Text horizontally centred on x.
void centerText(M5Canvas& c, const char* s, int x, int y, const lgfx::IFont* font,
                float size, uint16_t color);

// Small dimmed caption (y = top edge).
void label(M5Canvas& c, const char* s, int x, int y, uint16_t color = theme::TEXT_DIM);

// Same, but vertically centred on y - use it to line a caption up with a bar.
void labelMid(M5Canvas& c, const char* s, int x, int y_center,
              uint16_t color = theme::TEXT_DIM);

// Tyre colour from core temperature (optimal window is roughly 80-95 C).
uint16_t tyreColor(float temp_c);

}  // namespace ui
