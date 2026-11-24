#pragma once

#include <cstdint>
#include <memory>

#include "oly/native_host/game.hpp"
#include "scenario.hpp"
#include "simulation.hpp"

namespace enxame {
  // runGame<G> needs a default-constructible game, so main hands the choice over through this setter.
  void setWindowScenario(ScenarioId scenario, uint32_t size);

  class EnxameGame : public oly::native_host::Game {
   private:
    std::unique_ptr<Simulation> simulation_;

   public:
    EnxameGame();

    void onStart(oly::app::Engine& engine, oly::native_host::Window& window) override;
  };
}
