#pragma once

#include <cstdint>

#include "oly/app/engine.hpp"
#include "oly/native_host/window.hpp"

namespace oly::native_host {
  struct RunOptions {
    uint64_t maxFrames = 0; // 0 = until the window is closed
    double frameSeconds = 0.0; // greater than 0: fixed time per frame instead of the clock
  };

  // Drives the engine until the window is closed or maxFrames frames ran; returns the process exit code.
  int run(oly::app::Engine& engine, Window& window, RunOptions options = {});
}
