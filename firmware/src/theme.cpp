#include "theme.h"

namespace theme {

uint16_t lerp565(uint16_t a, uint16_t b, float t) {
  if (t <= 0.0f) return a;
  if (t >= 1.0f) return b;
  const int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  const int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  const int r = ar + (int)((br - ar) * t);
  const int g = ag + (int)((bg - ag) * t);
  const int bl = ab + (int)((bb - ab) * t);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

uint16_t heatColor(float t) {
  // zimno -> niebieski, optimum -> zielony, goraco -> zolty -> czerwony
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  if (t < 0.33f) return lerp565(BLUE, GREEN, t / 0.33f);
  if (t < 0.66f) return lerp565(GREEN, YELLOW, (t - 0.33f) / 0.33f);
  return lerp565(YELLOW, RED, (t - 0.66f) / 0.34f);
}

}  // namespace theme
