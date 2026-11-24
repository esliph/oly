#pragma once

#include <array>
#include <cstdint>

#include "workload.hpp"

namespace enxame::workloads {
  // Frozen at C0. Every value is a plan decision, not a measurement. Each vehicle has four turrets, at local (+-6, +-6).
  inline constexpr uint64_t S6Seed = 6;
  inline constexpr std::array<uint32_t, 1> S6Sizes{1000};
  inline constexpr RunLength S6Run{60, 600};
  inline constexpr VehicleParameters S6Vehicles{32, 80.0f, 32.0f, 1.0f, 6.0f};
  inline constexpr uint32_t S6TurretsPerVehicle = 4;
}
