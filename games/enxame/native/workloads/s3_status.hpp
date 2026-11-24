#pragma once

#include <array>
#include <cstdint>

#include "s1_horde.hpp"
#include "workload.hpp"

namespace enxame::workloads {
  // Frozen at C0. Every value is a plan decision, not a measurement. The horde is S1's, so the two scenarios differ
  // only by the status toggles.
  inline constexpr uint64_t S3Seed = 3;
  inline constexpr std::array<uint32_t, 1> S3Sizes{10000};
  inline constexpr RunLength S3Run{60, 600};
  inline constexpr HordeParameters S3Horde = S1Horde;
  inline constexpr float S3ToggleProbability = 0.10f;
}
