#include "window_game.hpp"

namespace enxame {
  namespace {
    constexpr int WindowWidth = 1280;
    constexpr int WindowHeight = 720;

    ScenarioId windowScenario = ScenarioId::S1;
    uint32_t windowSize = 0;
  }

  void setWindowScenario(ScenarioId scenario, uint32_t size) {
    windowScenario = scenario;
    windowSize = size;
  }

  EnxameGame::EnxameGame()
    : Game({"enxame", WindowWidth, WindowHeight}) {}

  void EnxameGame::onStart(oly::app::Engine& engine, [[maybe_unused]] oly::native_host::Window& window) {
    simulation_ = setUpScenario(windowScenario, windowSize, engine);
  }
}
