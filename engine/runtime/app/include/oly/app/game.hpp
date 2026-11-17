#pragma once

namespace oly::app {
  class Engine;

  // Defined by the code oly_add_game generates (game_setup.generated.cpp): declares the game's schema (declareSchema),
  // then installs the game's plugins in dependency order. Called once per Engine, before the first frame: by
  // olyEngineCreate, and by a native game's main.
  void setUpGame(Engine& engine);
}
