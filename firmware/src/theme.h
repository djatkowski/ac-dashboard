// Palette and shared layout constants.
#pragma once

#include <M5Unified.h>

namespace theme {

// Dark ground so it does not glare at night, plus strong accents that
// register in peripheral vision.
constexpr uint16_t BG = 0x0861;        // near black
constexpr uint16_t PANEL = 0x18C3;     // tile background
constexpr uint16_t PANEL_HI = 0x2965;  // highlighted tile background
constexpr uint16_t LINE = 0x39C7;      // borders and grid
constexpr uint16_t TEXT = 0xFFFF;
constexpr uint16_t TEXT_DIM = 0x8C71;
constexpr uint16_t ACCENT = 0x05FF;    // cyan - neutral data
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

// Linear interpolation between two RGB565 colours.
uint16_t lerp565(uint16_t a, uint16_t b, float t);

// Shared "cold -> hot" scale for tyres and brakes.
uint16_t heatColor(float t);

}  // namespace theme
