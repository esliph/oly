#include <cstdio>
#include <cstdlib>
#include <exception>
#include <random>
#include <string_view>

#include "drift.hpp"
#include "oly/app/engine.hpp"
#include "oly/app/schema.hpp"
#include "oly/ecs/world.hpp"
#include "oly/native_host/run.hpp"
#include "oly/native_host/window.hpp"
#include "render.hpp"

namespace {
  constexpr int UsageExitCode = 64;

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
    oly::app::declareSchema(engine);

    drift::Run run{.random = std::minstd_rand(std::random_device{}())};
    drift::installGameplay(engine, run);

    oly::native_host::Window window({"Drift", drift::LogicalWidth, drift::LogicalHeight});
    drift::Canvas canvas{window.renderer(), &run};
    engine.addSystem(oly::app::Phase::Render, "drawDrift", drift::drawDrift, &canvas);

    return oly::native_host::run(engine, window, options);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "drift: %s\n", error.what());
    return 1;
  }
}
