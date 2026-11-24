#pragma once

#include <array>
#include <cstdint>

#include "workload.hpp"

namespace enxame::workloads {
  // Frozen at C0. Every value is a plan decision, not a measurement.
  inline constexpr uint64_t S5Seed = 5;
  inline constexpr std::array<uint32_t, 1> S5Sizes{2000};
  inline constexpr RunLength S5Run{0, 10000};
  inline constexpr HordeParameters S5Horde{128, 128, 8.0f, 0.10f, 24.0f, 6.0f, 30.0f};
  inline constexpr CombatParameters S5Combat{0.5f, 64.0f, 160.0f, 1.0f, 4.0f};

  // One click every 200 steps from step 100, on a 10 x 5 grid of screen positions. Some land on an obstacle or on the
  // base and are refused: that is part of the frozen input.
  inline constexpr uint32_t S5ClickCount = 50;

  struct Click {
    uint32_t step;
    float screenX;
    float screenY;
  };

  constexpr Click s5ClickAt(uint32_t index) {
    return {100 + 200 * index, 200.0f + 24.0f * static_cast<float>(index % 10), 120.0f + 30.0f * static_cast<float>(index / 10)};
  }
}
