#pragma once

#include <array>
#include <cstdint>

#include "workload.hpp"

namespace enxame::workloads {
  // Frozen at C0. Every value is a plan decision, not a measurement. The size is the number of enemies per wave.
  inline constexpr uint64_t S8Seed = 8;
  inline constexpr std::array<uint32_t, 1> S8Sizes{2000};
  inline constexpr RunLength S8Run{0, 3000};
  inline constexpr WaveParameters S8Waves{60, 0.5f, 1024.0f};
}
