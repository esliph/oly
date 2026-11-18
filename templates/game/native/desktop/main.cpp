#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string_view>

#include "oly/app/engine.hpp"
#include "oly/app/game.hpp"
#include "oly/ecs/query.hpp"
#include "oly/ecs/world.hpp"
#include "oly/native_host/run.hpp"
#include "oly/native_host/window.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"

namespace {
  constexpr int UsageExitCode = 64;
  constexpr int LogicalWidth = 640;
  constexpr int LogicalHeight = 360;
  constexpr float PlayerSize = 16.0f;

  bool parseOptions(int argumentCount, char** arguments, oly::native_host::RunOptions& options) {
    for (int index = 1; index < argumentCount; ++index) {
      const std::string_view argument = arguments[index];
      if (argument != "--frames" || index + 1 >= argumentCount) {
        return false;
      }

      const unsigned long long frames = std::strtoull(arguments[++index], nullptr, 10);
      if (frames == 0) {
        return false;
      }

      // Headless runs: a fixed number of frames over a known amount of game time.
      options.maxFrames = frames;
      options.frameSeconds = 1.0 / 60;
    }

    return true;
  }

  void draw(oly::app::SystemContext& context) {
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(context.userData());

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    context.query<const Position>(oly::ecs::With<Player>{}).each([renderer](const Position& position) {
      const SDL_FRect box{position.x - PlayerSize / 2, position.y - PlayerSize / 2, PlayerSize, PlayerSize};
      SDL_RenderFillRect(renderer, &box);
    });
  }
}

int main(int argumentCount, char** arguments) {
  oly::native_host::RunOptions options;
  if (!parseOptions(argumentCount, arguments, options)) {
    std::fprintf(stderr, "usage: %s [--frames <n>]\n", arguments[0]);
    return UsageExitCode;
  }

  try {
    oly::ecs::World world;
    oly::app::Engine engine(world);
    oly::app::setUpGame(engine);

    engine.actions().bind(actions::move, {oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
    world.spawn(Player{}, Position{LogicalWidth / 2.0f, LogicalHeight / 2.0f});
    engine.addSystem(oly::app::Phase::Update, "movePlayer", movePlayer);

    oly::native_host::Window window({"@GAME_NAME@", LogicalWidth, LogicalHeight});
    engine.addSystem(oly::app::Phase::Render, "draw", draw, window.renderer());

    return oly::native_host::run(engine, window, options);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "@GAME_NAME@: %s\n", error.what());
    return 1;
  }
}
