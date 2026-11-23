#include "oly/native_host/game.hpp"

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "oly/app/game.hpp"
#include "oly/ecs/world.hpp"

namespace oly::native_host {
  namespace {
    constexpr int UsageExitCode = 64;
    constexpr int ExceptionExitCode = 1;
    constexpr std::string_view FramesOption = "--frames";
    constexpr const char* FallbackProgramName = "game";

    // std::nullopt is a wrong command line; 0 is no --frames, the meaning RunOptions::maxFrames gives it.
    std::optional<uint64_t> parseFrames(int argumentCount, char** arguments) {
      uint64_t frames = 0;

      for (int index = 1; index < argumentCount; index += 2) {
        if (std::string_view(arguments[index]) != FramesOption || index + 1 >= argumentCount) {
          return std::nullopt;
        }

        std::string_view text(arguments[index + 1]);
        uint64_t value = 0;
        auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size() || value == 0) {
          return std::nullopt;
        }

        frames = value;
      }

      return frames;
    }
  }

  Game::Game(WindowConfig window, oly::app::EngineConfig engine)
    : window_(std::move(window)),
      engine_(engine) {}

  const WindowConfig& Game::windowConfig() const noexcept {
    return window_;
  }

  const oly::app::EngineConfig& Game::engineConfig() const noexcept {
    return engine_;
  }

  int runGame(Game& game, RunOptions options) {
    oly::ecs::World world;
    oly::app::Engine engine(world, game.engineConfig());
    oly::app::setUpGame(engine);
    Window window(game.windowConfig());
    game.onStart(engine, window);

    return run(engine, window, options);
  }

  namespace detail {
    int runGameFromCommandLine(int argumentCount, char** arguments, std::unique_ptr<Game> (*construct)()) {
      std::string name = argumentCount > 0 && arguments[0] != nullptr ? arguments[0] : FallbackProgramName;

      std::optional<uint64_t> frames = parseFrames(argumentCount, arguments);
      if (!frames) {
        std::fprintf(stderr, "usage: %s [--frames <n>]\n", name.c_str());
        return UsageExitCode;
      }

      try {
        std::unique_ptr<Game> game = construct();
        name = game->windowConfig().title;

        RunOptions options;
        if (*frames > 0) {
          options.maxFrames = *frames;
          options.frameSeconds = 1.0 / game->engineConfig().fixedHz;
        }

        return runGame(*game, options);
      } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", name.c_str(), error.what());
        return ExceptionExitCode;
      }
    }
  }
}
