#pragma once

#include <cstdint>

#include "oly/native_host/game.hpp"
#include "scenario.hpp"

struct SDL_Renderer;

namespace enxame {
  // The renderer the drawing systems of the Render phase use; EnxameGame inserts it.
  struct View {
    SDL_Renderer* renderer;
  };

  // runGame<G> needs a default-constructible game, so main hands the choice over through this setter.
  void setWindowScenario(ScenarioId scenario, uint32_t size);

  class EnxameGame : public oly::native_host::Game {
   public:
    EnxameGame();

    void onStart(oly::app::Engine& engine, oly::native_host::Window& window) override;
  };
}
