#pragma once

#include <array>
#include <cstdint>

#include "workload.hpp"

namespace enxame::workloads {
  // Frozen at C0. Every value is a plan decision, not a measurement. The hash cell is the tile size: the separation
  // radius (6) fits in one cell, so a 3 x 3 cell query finds every neighbour.
  inline constexpr uint64_t S1Seed = 1;
  inline constexpr std::array<uint32_t, 3> S1Sizes{1000, 10000, 100000};
  inline constexpr RunLength S1Run{60, 600};
  inline constexpr HordeParameters S1Horde{512, 512, 8.0f, 0.10f, 24.0f, 6.0f, 30.0f};
}
