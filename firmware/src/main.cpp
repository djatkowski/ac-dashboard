// M5Stack Tab5 - Assetto Corsa dashboard.
// Data arrives over USB CDC from the PC bridge (see the pc-app directory).
//
// Switching screens: tap a tab at the bottom, or swipe left/right anywhere
// on the screen.
#include <Arduino.h>
#include <M5Unified.h>

#include "config.h"
#include "pages.h"
#include "telemetry.h"
#include "theme.h"
#include "widgets.h"

namespace {

TelemetryLink g_link;
DashState g_state;

Page* g_pages[2];
int g_page = DEFAULT_PAGE;

Panel g_tabbar;
int32_t g_tab_key = INT32_MIN;

// --- gesture tracking --------------------------------------------------------
bool g_touching = false;
int g_touch_x0 = 0, g_touch_y0 = 0;

constexpr int kSwipeMinPx = 180;   // horizontal travel that counts as a swipe
constexpr int kSwipeMaxDy = 120;   // ...as long as the finger stayed roughly level

void switchPage(int index) {
  if (index < 0) index = 0;
  if (index > 1) index = 1;
  if (index == g_page) return;
  g_page = index;
  g_pages[g_page]->enter();
  g_tab_key = INT32_MIN;  // force the tabs to repaint
}

void handleTouch() {
  const auto count = M5.Touch.getCount();
  if (count == 0) {
    g_touching = false;
    return;
  }
  const auto t = M5.Touch.getDetail(0);

  if (!g_touching && t.wasPressed()) {
    g_touching = true;
    g_touch_x0 = t.x;
    g_touch_y0 = t.y;
    return;
  }

  if (g_touching && t.wasReleased()) {
    g_touching = false;
    const int dx = t.x - g_touch_x0;
    const int dy = t.y - g_touch_y0;

    // 1. Swipe - change screen either way.
    if (abs(dx) >= kSwipeMinPx && abs(dy) <= kSwipeMaxDy) {
      switchPage(dx < 0 ? g_page + 1 : g_page - 1);
      return;
    }

    // 2. Tap on a tab in the bottom bar.
    if (g_touch_y0 >= theme::PAGE_H && abs(dx) < 40) {
      const int tabW = 220;
      if (t.x < tabW) {
        switchPage(0);
      } else if (t.x < tabW * 2) {
        switchPage(1);
      }
    }
  }
}

// --- bottom bar --------------------------------------------------------------
void drawTabBar(float fps) {
  // Repaint key: page, link state and rounded counters.
  const int32_t key = g_page * 1000003 + (g_state.linked ? 7 : 0) +
                      (g_state.game_live ? 13 : 0) + (int32_t)g_state.rx_hz * 31 +
                      (int32_t)fps * 131;
  if (!ui::changedInt(g_tab_key, key)) return;

  M5Canvas& c = g_tabbar.c();
  c.fillSprite(theme::PANEL);
  c.drawFastHLine(0, 0, theme::W, theme::LINE);

  const int tabW = 220;
  for (int i = 0; i < 2; i++) {
    const bool active = (i == g_page);
    const int x = i * tabW;
    c.fillRect(x + 4, 6, tabW - 8, theme::TABBAR_H - 12, active ? theme::ACCENT : theme::PANEL_HI);
    c.setFont(&fonts::DejaVu24);
    c.setTextSize(1.0f);
    c.setTextDatum(textdatum_t::middle_center);
    c.setTextColor(active ? theme::BG : theme::TEXT_DIM);
    c.drawString(g_pages[i]->name(), x + tabW / 2, theme::TABBAR_H / 2);
  }

  // Link state: dot plus a word. Three different situations, three messages.
  const char* status;
  uint16_t color;
  if (!g_state.linked) {
    status = "NO USB";
    color = theme::RED;
  } else if (!g_state.game_live) {
    status = "WAITING FOR GAME";
    color = theme::YELLOW;
  } else {
    status = "CONNECTED";
    color = theme::GREEN;
  }

  const int sx = tabW * 2 + 30;
  c.fillCircle(sx, theme::TABBAR_H / 2, 8, color);
  c.setFont(&fonts::DejaVu24);
  c.setTextDatum(textdatum_t::middle_left);
  c.setTextColor(color, theme::PANEL);
  c.drawString(status, sx + 20, theme::TABBAR_H / 2);

  // Diagnostics on the right - handy when something is not working.
  char buf[64];
  snprintf(buf, sizeof(buf), "%.23s   %.0f Hz   %.0f fps   lost %lu",
           g_state.p.car_name[0] ? g_state.p.car_name : "-", g_state.rx_hz, fps,
           (unsigned long)g_state.dropped);
  c.setFont(&fonts::DejaVu18);
  c.setTextDatum(textdatum_t::middle_right);
  c.setTextColor(theme::TEXT_DIM, theme::PANEL);
  c.drawString(buf, theme::W - 16, theme::TABBAR_H / 2);

  g_tabbar.push();
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(1);  // 1280x720, landscape
  M5.Display.setBrightness(SCREEN_BRIGHTNESS);
  M5.Display.fillScreen(theme::BG);

  g_link.begin();

  g_pages[0] = &racePage();
  g_pages[1] = &driftPage();
  g_pages[0]->begin();
  g_pages[1]->begin();

  g_tabbar.begin(0, theme::PAGE_H, theme::W, theme::TABBAR_H);
  g_pages[g_page]->enter();
}

void loop() {
  static uint32_t next_frame = 0;
  static float fps = 0.0f;
  static uint32_t fps_window = 0;
  static uint32_t fps_count = 0;

  M5.update();
  handleTouch();

  // We collect telemetry on every loop pass, not only on drawing frames, so
  // the USB buffer does not overflow when rendering briefly slows down.
  g_link.update(g_state);

  const uint32_t now = millis();
  if ((int32_t)(now - next_frame) < 0) return;
  next_frame = now + (1000 / TARGET_FPS);

  // A change of link/game state invalidates everything: otherwise the tiles
  // would keep showing the last values from before the disconnect.
  static int8_t last_link_state = -1;
  const int8_t link_state = (g_state.linked ? 1 : 0) + (g_state.game_live ? 2 : 0);
  if (link_state != last_link_state) {
    last_link_state = link_state;
    g_pages[g_page]->enter();
  }

  g_pages[g_page]->update(g_state);

  fps_count++;
  if (now - fps_window >= 1000) {
    fps = fps_count * 1000.0f / (float)(now - fps_window);
    fps_window = now;
    fps_count = 0;
  }
  drawTabBar(fps);
}
