#include "oly/native_host/run.hpp"

#include <SDL3/SDL.h>

#include "oly/native_host/detail/input_translation.hpp"

namespace oly::native_host {
  namespace {
    constexpr int HiddenWaitMilliseconds = 100;

    // False when the window was closed.
    bool applyPendingEvents(detail::InputTranslation& translation, oly::app::Engine& engine, SDL_Renderer* renderer) {
      SDL_Event event;

      // A minimized window has no vsync to pace the loop.
      if (translation.hidden() && SDL_WaitEventTimeout(&event, HiddenWaitMilliseconds) && !translation.apply(engine, renderer, event)) {
        return false;
      }

      while (SDL_PollEvent(&event)) {
        if (!translation.apply(engine, renderer, event)) {
          return false;
        }
      }

      return true;
    }
  }

  int run(oly::app::Engine& engine, Window& window, RunOptions options) {
    detail::InputTranslation translation;
    SDL_Renderer* renderer = window.renderer();
    uint64_t frames = 0;

    while (options.maxFrames == 0 || frames < options.maxFrames) {
      if (!applyPendingEvents(translation, engine, renderer)) {
        break;
      }

      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);

      double now = options.frameSeconds > 0.0 ? static_cast<double>(frames) * options.frameSeconds : static_cast<double>(SDL_GetTicksNS()) / 1e9;
      engine.frame(now);

      SDL_RenderPresent(renderer);
      ++frames;
    }

    return 0;
  }
}
