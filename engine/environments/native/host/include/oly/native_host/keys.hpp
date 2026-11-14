#pragma once

#include <SDL3/SDL_scancode.h>

#include <optional>

#include "oly/input/physical_button.hpp"

namespace oly::native_host {
  // The engine's button for an SDL scancode, or nullopt if Key does not declare it.
  std::optional<oly::input::PhysicalButton> keyButtonOf(SDL_Scancode scancode) noexcept;
}
