#pragma once

#include "drift.hpp"
#include "oly/app/system.hpp"

struct SDL_Renderer;

namespace drift {
  struct Canvas {
    SDL_Renderer* renderer;
    const Run* run;
  };

  // A Render system: userData is a Canvas.
  void drawDrift(oly::app::SystemContext& context);
}
