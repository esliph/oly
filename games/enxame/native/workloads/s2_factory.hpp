#pragma once

#include <array>
#include <cstdint>

#include "workload.hpp"

namespace enxame::workloads {
  // Frozen at C0. Every value is a plan decision, not a measurement. An item arrives every 0.32 s (the belt length over
  // the speed, divided by 100 items) and the assembler crafts every 0.25 s, so the stock stays bounded and the item
  // count stays near 50 000.
  inline constexpr uint64_t S2Seed = 2;
  inline constexpr std::array<uint32_t, 1> S2Sizes{50000};
  inline constexpr RunLength S2Run{60, 600};
  inline constexpr FactoryParameters S2Factory{500, 64, 8.0f, 16.0f, 0.25f, 4, 16.0f};
}
