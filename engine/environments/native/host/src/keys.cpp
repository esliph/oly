#include "oly/native_host/keys.hpp"

namespace oly::native_host {
  std::optional<oly::input::PhysicalButton> keyButtonOf(SDL_Scancode scancode) noexcept {
    // SDL3 scancodes are the USB HID usages of the keyboard page, the same numbering Key uses.
    uint32_t usage = 0x0007'0000u | static_cast<uint32_t>(scancode);
    if (!oly::input::isKey(usage)) {
      return std::nullopt;
    }

    return oly::input::PhysicalButton(usage);
  }
}
