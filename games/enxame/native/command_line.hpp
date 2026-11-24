#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "scenario.hpp"

namespace enxame {
  struct LaunchOptions {
    bool headless = false;
    bool list = false;
    std::optional<ScenarioId> scenario;
    std::optional<uint32_t> size;
    std::optional<uint32_t> steps; // tests only: warmup 0 and this many steps
    bool timing = true;
    std::string commit;
    std::string out;
    std::vector<char*> passThrough; // argv[0] and the arguments Enxame does not know, for runGame
  };

  // std::nullopt after printing the reason and the usage to stderr (exit 64).
  std::optional<LaunchOptions> parseCommandLine(int argumentCount, char** arguments);
}
