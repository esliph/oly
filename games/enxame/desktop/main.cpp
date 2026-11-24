#include <cstdio>

#include "command_line.hpp"
#include "headless.hpp"
#include "oly/native_host/game.hpp"
#include "scenario.hpp"
#include "window_game.hpp"

int main(int argumentCount, char** arguments) {
  std::optional<enxame::LaunchOptions> options = enxame::parseCommandLine(argumentCount, arguments);

  if (!options) {
    return 64;
  }

  if (options->list) {
    for (const enxame::ScenarioId scenario : enxame::listedScenarios()) {
      const enxame::ScenarioInfo& info = enxame::scenarioInfo(scenario);

      for (const uint32_t size : info.sizes) {
        std::printf("%.*s %u\n", static_cast<int>(info.name.size()), info.name.data(), size);
      }
    }

    return 0;
  }

  if (options->headless) {
    return enxame::runHeadless(*options);
  }

  const enxame::ScenarioId scenario = options->scenario.value_or(enxame::ScenarioId::Playable);

  if (!enxame::checkScenario(scenario, options->size)) {
    return 64;
  }

  const uint32_t size = options->size.value_or(enxame::scenarioInfo(scenario).sizes.front());
  enxame::setWindowScenario(scenario, size);

  return oly::native_host::runGame<enxame::EnxameGame>(static_cast<int>(options->passThrough.size()), options->passThrough.data());
}
