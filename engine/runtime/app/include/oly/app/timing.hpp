#pragma once

#include <cstdint>
#include <string_view>

#include "oly/app/phase.hpp"
#include "oly/app/system.hpp"

namespace oly::app {
  // ran is false when the pause or the system's states skipped it. A system that ran may measure 0 ns: the clock is
  // coarse on MinGW under Wine and on WASM.
  struct SystemTiming {
    SystemId system;
    std::string_view name;
    PhaseId phase;
    std::string_view phaseName;
    bool ran;
    uint64_t nanoseconds;
  };

  struct FlushTiming {
    PhaseId phase;
    std::string_view phaseName;
    uint64_t nanoseconds;
  };
}
