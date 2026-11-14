#pragma once

#include <vector>

#include "oly/input/physical_button.hpp"

struct SDL_Renderer;
union SDL_Event;

namespace oly::app {
  class Engine;
}

namespace oly::native_host::detail {
  class InputTranslation {
   private:
    std::vector<oly::input::MouseButton> pressedMouseButtons_;
    // Engine::setHidden always restarts the clock, and SDL sends EXPOSED and SHOWN to a visible window too.
    bool hidden_ = false;

   public:
    // Applies one SDL event to the engine; false on SDL_EVENT_QUIT and SDL_EVENT_WINDOW_CLOSE_REQUESTED.
    bool apply(oly::app::Engine& engine, SDL_Renderer* renderer, SDL_Event& event);
    bool hidden() const noexcept;

   private:
    void setHidden(oly::app::Engine& engine, bool hidden);
    void pressMouse(oly::app::Engine& engine, SDL_Renderer* renderer, SDL_Event& event);
    void releaseMouse(oly::app::Engine& engine, SDL_Event& event);
  };
}
