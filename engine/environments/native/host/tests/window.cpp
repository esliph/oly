#include "oly/native_host/window.hpp"

#include <SDL3/SDL.h>
#include <doctest/doctest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "oly/app/engine.hpp"
#include "oly/app/system_parameters.hpp"
#include "oly/ecs/world.hpp"
#include "oly/input/action_map.hpp"
#include "oly/input/physical_button.hpp"
#include "oly/native_host/run.hpp"

using oly::app::Engine;
using oly::app::Render;
using oly::app::Update;
using oly::native_host::run;
using oly::native_host::RunOptions;
using oly::native_host::Window;

namespace {
  struct Draws {
    uint32_t count = 0;
  };

  void countCall(oly::app::Res<Draws> draws) {
    ++draws->count;
  }

  struct JumpProbe {
    oly::input::ButtonAction jump;
    bool sawPressed = false;
  };

  void recordJump(oly::app::Res<JumpProbe> probe, oly::app::Res<const oly::input::ActionMap> actions) {
    probe->sawPressed = probe->sawPressed || actions->button(probe->jump).pressed;
  }
}

TEST_CASE("a window gives a renderer with the configured logical size") {
  Window window({"test", 320, 200});

  int width = 0;
  int height = 0;
  SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
  SDL_GetRenderLogicalPresentation(window.renderer(), &width, &height, &mode);

  CHECK(window.window() != nullptr);
  CHECK(window.renderer() != nullptr);
  CHECK(width == 320);
  CHECK(height == 200);
  CHECK(mode == SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

TEST_CASE("the video drivers of a playable build are compiled in") {
  std::vector<std::string> drivers;

  for (int index = 0; index < SDL_GetNumVideoDrivers(); ++index) {
    drivers.push_back(SDL_GetVideoDriver(index));
  }

  auto has = [&](const char* name) { return std::find(drivers.begin(), drivers.end(), name) != drivers.end(); };

  CHECK(has("dummy"));
#if defined(__linux__)
  CHECK(has("x11"));
  CHECK(has("wayland"));
#endif
#if defined(_WIN32)
  CHECK(has("windows"));
#endif
}

TEST_CASE("a window that SDL cannot open throws with SDL's message") {
  SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "nonexistent", SDL_HINT_OVERRIDE);

  CHECK_THROWS_AS(Window({"test", 320, 200}), std::runtime_error);

  SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
}

TEST_CASE("run drives frames and draws after each one") {
  oly::ecs::World world;
  Engine engine(world);
  Window window({"test", 320, 200});
  Draws& draws = engine.world().insertResource(Draws{});
  engine.addSystem<&countCall>(Render{}, "draw");

  int exitCode = run(engine, window, {.maxFrames = 11, .frameSeconds = 1.0 / 60});

  CHECK(exitCode == 0);
  CHECK(engine.time() == doctest::Approx(10.0 / 60));
  CHECK(draws.count == 10);
}

TEST_CASE("a quit event ends run before the next frame") {
  oly::ecs::World world;
  Engine engine(world);
  Window window({"test", 320, 200});
  Draws& draws = engine.world().insertResource(Draws{});
  engine.addSystem<&countCall>(Render{}, "draw");
  SDL_Event quit{};
  quit.type = SDL_EVENT_QUIT;
  SDL_PushEvent(&quit);

  int exitCode = run(engine, window, {.maxFrames = 5, .frameSeconds = 1.0 / 60});

  CHECK(exitCode == 0);
  CHECK(draws.count == 0);
}

TEST_CASE("run forwards pending input before the frame") {
  oly::ecs::World world;
  Engine engine(world);
  Window window({"test", 320, 200});
  JumpProbe& probe = engine.world().insertResource(JumpProbe{engine.actions().declareButton("jump")});
  engine.actions().bind(probe.jump, oly::input::Key::W);
  engine.addSystem<&recordJump>(Update{}, "record");
  SDL_Event press{};
  press.type = SDL_EVENT_KEY_DOWN;
  press.key.windowID = SDL_GetWindowID(window.window());
  press.key.scancode = SDL_SCANCODE_W;
  press.key.down = true;
  SDL_PushEvent(&press);

  run(engine, window, {.maxFrames = 2, .frameSeconds = 1.0 / 60});

  CHECK(probe.sawPressed);
}
