// Ekrany dashboardu. Kazdy trzyma wlasne sprite'y i sam decyduje,
// ktory kafelek wymaga odrysowania.
#pragma once

#include "telemetry.h"
#include "widgets.h"

class Page {
 public:
  virtual ~Page() {}
  virtual const char* name() const = 0;
  virtual void begin() = 0;                    // alokacja sprite'ow (raz)
  virtual void enter() = 0;                    // wejscie: wymus pelne odrysowanie
  virtual void update(const DashState& st) = 0;
};

Page& racePage();
Page& driftPage();
