#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include "oly/app/engine.hpp"
#include "simulation.hpp"
#include "workload.hpp"

namespace enxame {
  enum class ScenarioId : uint8_t { Playable,
                                    S1,
                                    S2,
                                    S3,
                                    S4,
                                    S5,
                                    S6,
                                    S7,
                                    S8 };

  struct ScenarioInfo {
    std::string_view name; // "S1"; empty for an id with no row
    std::span<const uint32_t> sizes; // the frozen sizes; empty when blocked
    uint64_t seed;
    RunLength run;
    std::string_view blockedReason; // empty when runnable
  };

  std::optional<ScenarioId> parseScenario(std::string_view name); // only names with a row in the table
  const ScenarioInfo& scenarioInfo(ScenarioId scenario);
  std::span<const ScenarioId> listedScenarios(); // S1..S8 with a row, in order

  // Prints the reason to stderr and returns false when the scenario is blocked, or when size is not one of its frozen
  // sizes (checked in that order). A missing size is valid: the caller picks the first one.
  bool checkScenario(ScenarioId scenario, std::optional<uint32_t> size);

  // Binds actions::place to the left mouse button, registers the scenario's systems and spawns its world. The Engine
  // already ran setUpGame. The returned Simulation is the userData of every system: keep it alive as long as the Engine.
  std::unique_ptr<Simulation> setUpScenario(ScenarioId scenario, uint32_t size, oly::app::Engine& engine);
}
