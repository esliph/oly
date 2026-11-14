#include "oly/native_host/detail/input_translation.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <optional>

#include "oly/app/engine.hpp"
#include "oly/native_host/keys.hpp"

namespace oly::native_host::detail {
  namespace {
    using oly::input::MouseButton;

    std::optional<MouseButton> mouseButtonOf(uint8_t button) {
      switch (button) {
        case SDL_BUTTON_LEFT:
          return MouseButton::Left;
        case SDL_BUTTON_RIGHT:
          return MouseButton::Right;
        case SDL_BUTTON_MIDDLE:
          return MouseButton::Middle;
        default:
          return std::nullopt;
      }
    }

    bool insideLogicalArea(SDL_Renderer* renderer, float x, float y) {
      int width = 0;
      int height = 0;
      SDL_GetRenderLogicalPresentation(renderer, &width, &height, nullptr);

      return x >= 0.0f && y >= 0.0f && x < static_cast<float>(width) && y < static_cast<float>(height);
    }
  }

  bool InputTranslation::apply(oly::app::Engine& engine, SDL_Renderer* renderer, SDL_Event& event) {
    oly::input::ActionMap& actions = engine.actions();

    switch (event.type) {
      case SDL_EVENT_QUIT:
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        return false;
      case SDL_EVENT_KEY_DOWN:
        if (std::optional<oly::input::PhysicalButton> button = keyButtonOf(event.key.scancode); button && !event.key.repeat) {
          actions.buttonDown(*button);
        }
        break;
      case SDL_EVENT_KEY_UP:
        if (std::optional<oly::input::PhysicalButton> button = keyButtonOf(event.key.scancode)) {
          actions.buttonUp(*button);
        }
        break;
      case SDL_EVENT_MOUSE_MOTION:
        SDL_ConvertEventToRenderCoordinates(renderer, &event);
        actions.pointerMove(event.motion.x, event.motion.y);
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        pressMouse(engine, renderer, event);
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
        releaseMouse(engine, event);
        break;
      case SDL_EVENT_WINDOW_FOCUS_LOST:
        actions.releaseAll();
        pressedMouseButtons_.clear();
        break;
      case SDL_EVENT_WINDOW_MINIMIZED:
      case SDL_EVENT_WINDOW_HIDDEN:
      case SDL_EVENT_WINDOW_OCCLUDED:
        setHidden(engine, true);
        break;
      case SDL_EVENT_WINDOW_RESTORED:
      case SDL_EVENT_WINDOW_SHOWN:
      case SDL_EVENT_WINDOW_EXPOSED:
        setHidden(engine, false);
        break;
      default:
        break;
    }

    return true;
  }

  bool InputTranslation::hidden() const noexcept {
    return hidden_;
  }

  void InputTranslation::setHidden(oly::app::Engine& engine, bool hidden) {
    if (hidden == hidden_) {
      return;
    }

    hidden_ = hidden;
    engine.setHidden(hidden);
  }

  void InputTranslation::pressMouse(oly::app::Engine& engine, SDL_Renderer* renderer, SDL_Event& event) {
    SDL_ConvertEventToRenderCoordinates(renderer, &event);
    engine.actions().pointerMove(event.button.x, event.button.y);

    std::optional<MouseButton> button = mouseButtonOf(event.button.button);
    if (!button || !insideLogicalArea(renderer, event.button.x, event.button.y)) {
      return;
    }

    engine.actions().buttonDown(*button);
    pressedMouseButtons_.push_back(*button);
  }

  void InputTranslation::releaseMouse(oly::app::Engine& engine, SDL_Event& event) {
    std::optional<MouseButton> button = mouseButtonOf(event.button.button);
    if (!button) {
      return;
    }

    auto pressed = std::find(pressedMouseButtons_.begin(), pressedMouseButtons_.end(), *button);
    if (pressed == pressedMouseButtons_.end()) {
      return;
    }

    pressedMouseButtons_.erase(pressed);
    engine.actions().buttonUp(*button);
  }
}
