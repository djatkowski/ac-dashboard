// Dashboard screens. Each owns its sprites and decides for itself which
// tiles need repainting.
#pragma once

#include "telemetry.h"
#include "widgets.h"

class Page {
 public:
  virtual ~Page() {}
  virtual const char* name() const = 0;
  virtual void begin() = 0;                    // allocate sprites (once)
  virtual void enter() = 0;                    // on entry: force a full repaint
  virtual void update(const DashState& st) = 0;
};

Page& racePage();
Page& driftPage();
