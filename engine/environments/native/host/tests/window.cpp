#include "oly/native_host/window.hpp"

#include <SDL3/SDL.h>
#include <doctest/doctest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

using oly::native_host::Window;

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
