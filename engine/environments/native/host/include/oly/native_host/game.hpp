#pragma once

#include <memory>
#include <type_traits>

#include "oly/app/engine.hpp"
#include "oly/native_host/run.hpp"
#include "oly/native_host/window.hpp"

namespace oly::native_host {
  // The game itself: its window, its engine configuration and its own state. runGame constructs it before the
  // Engine and destroys it after, so a resource may keep a pointer to it.
  class Game {
   private:
    WindowConfig window_;
    oly::app::EngineConfig engine_;

   public:
    explicit Game(WindowConfig window, oly::app::EngineConfig engine = {});
    virtual ~Game() = default;
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    const WindowConfig& windowConfig() const noexcept;
    const oly::app::EngineConfig& engineConfig() const noexcept;

    // Called once, on a set-up Engine, before the first frame.
    virtual void onStart(oly::app::Engine& engine, Window& window) = 0;
  };

  // Builds the World and the Engine, sets up the game, opens the window, calls onStart and runs the loop. Does not
  // catch: whatever the game throws reaches the caller.
  int runGame(Game& game, RunOptions options = {});

  namespace detail {
    int runGameFromCommandLine(int argumentCount, char** arguments, std::unique_ptr<Game> (*construct)());
  }

  // Parses --frames, then constructs G and runs it. Returns 64 on a wrong command line and 1 when a std::exception escapes,
  // the constructor's included; otherwise what run returns. Any other thrown value ends the process.
  template <class G>
  int runGame(int argumentCount, char** arguments) {
    static_assert(std::is_base_of_v<Game, G>, "runGame: the game must derive from oly::native_host::Game");
    static_assert(std::is_default_constructible_v<G>, "runGame: the game must be default constructible");

    return detail::runGameFromCommandLine(argumentCount, arguments, []() -> std::unique_ptr<Game> { return std::make_unique<G>(); });
  }
}
