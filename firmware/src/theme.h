// Paleta i wspolne stale ukladu.
#pragma once

#include <M5Unified.h>

namespace theme {

// Ciemne tlo, zeby nie razilo w nocy, plus mocne akcenty czytelne katem oka.
constexpr uint16_t BG = 0x0861;        // prawie czarny
constexpr uint16_t PANEL = 0x18C3;     // tlo kafelka
constexpr uint16_t PANEL_HI = 0x2965;  // tlo kafelka podswietlonego
constexpr uint16_t LINE = 0x39C7;      // ramki i siatka
constexpr uint16_t TEXT = 0xFFFF;
constexpr uint16_t TEXT_DIM = 0x8C71;
constexpr uint16_t ACCENT = 0x05FF;    // cyjan - dane neutralne
constexpr uint16_t GREEN = 0x2FE6;
constexpr uint16_t YELLOW = 0xFE60;
constexpr uint16_t ORANGE = 0xFC60;
constexpr uint16_t RED = 0xF9A6;
constexpr uint16_t BLUE = 0x3D5F;
constexpr uint16_t MAGENTA = 0xF81F;

constexpr int W = 1280;
constexpr int H = 720;
constexpr int TABBAR_H = 52;
constexpr int PAGE_H = H - TABBAR_H;  // 668

// Liniowa interpolacja miedzy dwoma kolorami RGB565.
uint16_t lerp565(uint16_t a, uint16_t b, float t);

// Wspolna skala "zimno -> goraco" dla opon i hamulcow.
uint16_t heatColor(float t);

}  // namespace theme
