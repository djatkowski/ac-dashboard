// Male cegielki UI. Kazdy Panel to sprite w PSRAM, przerysowywany tylko
// wtedy, gdy jego dane sie zmienily - przy 1280x720 przerysowywanie calego
// ekranu co klatke byloby marnotrawstwem pasma do framebufora.
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

  // Tlo kafelka z ramka i opcjonalnym tytulem w lewym gornym rogu.
  void tile(const char* title = nullptr, uint16_t bg = theme::PANEL);

 private:
  M5Canvas spr_{&M5.Display};
  int x_ = 0, y_ = 0, w_ = 0, h_ = 0;
  bool ready_ = false;
};

namespace ui {

// Zwraca true i aktualizuje cache, jesli wartosc zmienila sie istotnie.
bool changed(float& cache, float value, float eps);
bool changedInt(int32_t& cache, int32_t value);

// Poziomy pasek wypelnienia 0..1 z tlem.
void bar(M5Canvas& c, int x, int y, int w, int h, float frac, uint16_t fg,
         uint16_t bg = theme::LINE);

// Pasek dwustronny: 0 w srodku, wartosc -1..1 wychodzi w lewo lub w prawo.
void centerBar(M5Canvas& c, int x, int y, int w, int h, float v, uint16_t fg,
               uint16_t bg = theme::LINE);

// Segment luku. Kat: 0 = gora, rosnie w prawo (zgodnie z ruchem wskazowek).
void arcSegment(M5Canvas& c, int cx, int cy, int r0, int r1, float a0_deg,
                float a1_deg, uint16_t color);

// Tekst wysrodkowany w poziomie wzgledem x.
void centerText(M5Canvas& c, const char* s, int x, int y, const lgfx::IFont* font,
                float size, uint16_t color);

// Mala etykieta w kolorze przygaszonym.
void label(M5Canvas& c, const char* s, int x, int y, uint16_t color = theme::TEXT_DIM);

// Kolor opony wg temperatury rdzenia (okno optimum ok. 80-95 C).
uint16_t tyreColor(float temp_c);

}  // namespace ui
