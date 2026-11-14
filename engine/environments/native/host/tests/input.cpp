#include <SDL3/SDL.h>
#include <doctest/doctest.h>

#include <optional>

#include "oly/app/engine.hpp"
#include "oly/ecs/world.hpp"
#include "oly/input/physical_button.hpp"
#include "oly/native_host/detail/input_translation.hpp"
#include "oly/native_host/keys.hpp"
#include "oly/native_host/window.hpp"

using oly::app::Engine;
using oly::input::ButtonAction;
using oly::input::Key;
using oly::input::MouseButton;
using oly::input::PhysicalButton;
using oly::native_host::keyButtonOf;
using oly::native_host::Window;
using oly::native_host::detail::InputTranslation;

namespace {
  struct KeyPair {
    SDL_Scancode scancode;
    Key key;
  };

  struct Session {
    oly::ecs::World world;
    Engine engine{world};
    Window window{{"test", 320, 200}};
    InputTranslation translation;

    bool send(SDL_Event& event) {
      return translation.apply(engine, window.renderer(), event);
    }

    bool key(SDL_Scancode scancode, bool down, bool repeat = false) {
      SDL_Event event{};
      event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
      event.key.windowID = SDL_GetWindowID(window.window());
      event.key.scancode = scancode;
      event.key.down = down;
      event.key.repeat = repeat;
      return send(event);
    }

    bool motion(float x, float y) {
      SDL_Event event{};
      event.type = SDL_EVENT_MOUSE_MOTION;
      event.motion.windowID = SDL_GetWindowID(window.window());
      event.motion.x = x;
      event.motion.y = y;
      return send(event);
    }

    bool mouseButton(uint8_t button, bool down, float x, float y) {
      SDL_Event event{};
      event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
      event.button.windowID = SDL_GetWindowID(window.window());
      event.button.button = button;
      event.button.down = down;
      event.button.x = x;
      event.button.y = y;
      return send(event);
    }

    bool windowEvent(SDL_EventType type) {
      SDL_Event event{};
      event.type = type;
      event.window.windowID = SDL_GetWindowID(window.window());
      return send(event);
    }

    oly::input::ButtonState state(ButtonAction action) {
      engine.actions().capture();
      return engine.actions().button(action);
    }

    oly::input::PointerState pointer(oly::input::PointerAction action) {
      engine.actions().capture();
      return engine.actions().pointer(action);
    }
  };
}

TEST_CASE("every Key is the SDL scancode on the keyboard page") {
  const KeyPair pairs[] = {
    {SDL_SCANCODE_A, Key::A},
    {SDL_SCANCODE_B, Key::B},
    {SDL_SCANCODE_C, Key::C},
    {SDL_SCANCODE_D, Key::D},
    {SDL_SCANCODE_E, Key::E},
    {SDL_SCANCODE_F, Key::F},
    {SDL_SCANCODE_G, Key::G},
    {SDL_SCANCODE_H, Key::H},
    {SDL_SCANCODE_I, Key::I},
    {SDL_SCANCODE_J, Key::J},
    {SDL_SCANCODE_K, Key::K},
    {SDL_SCANCODE_L, Key::L},
    {SDL_SCANCODE_M, Key::M},
    {SDL_SCANCODE_N, Key::N},
    {SDL_SCANCODE_O, Key::O},
    {SDL_SCANCODE_P, Key::P},
    {SDL_SCANCODE_Q, Key::Q},
    {SDL_SCANCODE_R, Key::R},
    {SDL_SCANCODE_S, Key::S},
    {SDL_SCANCODE_T, Key::T},
    {SDL_SCANCODE_U, Key::U},
    {SDL_SCANCODE_V, Key::V},
    {SDL_SCANCODE_W, Key::W},
    {SDL_SCANCODE_X, Key::X},
    {SDL_SCANCODE_Y, Key::Y},
    {SDL_SCANCODE_Z, Key::Z},
    {SDL_SCANCODE_1, Key::Digit1},
    {SDL_SCANCODE_2, Key::Digit2},
    {SDL_SCANCODE_3, Key::Digit3},
    {SDL_SCANCODE_4, Key::Digit4},
    {SDL_SCANCODE_5, Key::Digit5},
    {SDL_SCANCODE_6, Key::Digit6},
    {SDL_SCANCODE_7, Key::Digit7},
    {SDL_SCANCODE_8, Key::Digit8},
    {SDL_SCANCODE_9, Key::Digit9},
    {SDL_SCANCODE_0, Key::Digit0},
    {SDL_SCANCODE_RETURN, Key::Enter},
    {SDL_SCANCODE_ESCAPE, Key::Escape},
    {SDL_SCANCODE_BACKSPACE, Key::Backspace},
    {SDL_SCANCODE_TAB, Key::Tab},
    {SDL_SCANCODE_SPACE, Key::Space},
    {SDL_SCANCODE_MINUS, Key::Minus},
    {SDL_SCANCODE_EQUALS, Key::Equal},
    {SDL_SCANCODE_LEFTBRACKET, Key::BracketLeft},
    {SDL_SCANCODE_RIGHTBRACKET, Key::BracketRight},
    {SDL_SCANCODE_BACKSLASH, Key::Backslash},
    {SDL_SCANCODE_SEMICOLON, Key::Semicolon},
    {SDL_SCANCODE_APOSTROPHE, Key::Quote},
    {SDL_SCANCODE_GRAVE, Key::Backquote},
    {SDL_SCANCODE_COMMA, Key::Comma},
    {SDL_SCANCODE_PERIOD, Key::Period},
    {SDL_SCANCODE_SLASH, Key::Slash},
    {SDL_SCANCODE_CAPSLOCK, Key::CapsLock},
    {SDL_SCANCODE_F1, Key::F1},
    {SDL_SCANCODE_F2, Key::F2},
    {SDL_SCANCODE_F3, Key::F3},
    {SDL_SCANCODE_F4, Key::F4},
    {SDL_SCANCODE_F5, Key::F5},
    {SDL_SCANCODE_F6, Key::F6},
    {SDL_SCANCODE_F7, Key::F7},
    {SDL_SCANCODE_F8, Key::F8},
    {SDL_SCANCODE_F9, Key::F9},
    {SDL_SCANCODE_F10, Key::F10},
    {SDL_SCANCODE_F11, Key::F11},
    {SDL_SCANCODE_F12, Key::F12},
    {SDL_SCANCODE_PRINTSCREEN, Key::PrintScreen},
    {SDL_SCANCODE_SCROLLLOCK, Key::ScrollLock},
    {SDL_SCANCODE_PAUSE, Key::Pause},
    {SDL_SCANCODE_INSERT, Key::Insert},
    {SDL_SCANCODE_HOME, Key::Home},
    {SDL_SCANCODE_PAGEUP, Key::PageUp},
    {SDL_SCANCODE_DELETE, Key::Delete},
    {SDL_SCANCODE_END, Key::End},
    {SDL_SCANCODE_PAGEDOWN, Key::PageDown},
    {SDL_SCANCODE_RIGHT, Key::ArrowRight},
    {SDL_SCANCODE_LEFT, Key::ArrowLeft},
    {SDL_SCANCODE_DOWN, Key::ArrowDown},
    {SDL_SCANCODE_UP, Key::ArrowUp},
    {SDL_SCANCODE_NUMLOCKCLEAR, Key::NumLock},
    {SDL_SCANCODE_KP_DIVIDE, Key::NumpadDivide},
    {SDL_SCANCODE_KP_MULTIPLY, Key::NumpadMultiply},
    {SDL_SCANCODE_KP_MINUS, Key::NumpadSubtract},
    {SDL_SCANCODE_KP_PLUS, Key::NumpadAdd},
    {SDL_SCANCODE_KP_ENTER, Key::NumpadEnter},
    {SDL_SCANCODE_KP_1, Key::Numpad1},
    {SDL_SCANCODE_KP_2, Key::Numpad2},
    {SDL_SCANCODE_KP_3, Key::Numpad3},
    {SDL_SCANCODE_KP_4, Key::Numpad4},
    {SDL_SCANCODE_KP_5, Key::Numpad5},
    {SDL_SCANCODE_KP_6, Key::Numpad6},
    {SDL_SCANCODE_KP_7, Key::Numpad7},
    {SDL_SCANCODE_KP_8, Key::Numpad8},
    {SDL_SCANCODE_KP_9, Key::Numpad9},
    {SDL_SCANCODE_KP_0, Key::Numpad0},
    {SDL_SCANCODE_KP_PERIOD, Key::NumpadDecimal},
    {SDL_SCANCODE_NONUSBACKSLASH, Key::IntlBackslash},
    {SDL_SCANCODE_APPLICATION, Key::ContextMenu},
    {SDL_SCANCODE_LCTRL, Key::ControlLeft},
    {SDL_SCANCODE_LSHIFT, Key::ShiftLeft},
    {SDL_SCANCODE_LALT, Key::AltLeft},
    {SDL_SCANCODE_LGUI, Key::MetaLeft},
    {SDL_SCANCODE_RCTRL, Key::ControlRight},
    {SDL_SCANCODE_RSHIFT, Key::ShiftRight},
    {SDL_SCANCODE_RALT, Key::AltRight},
    {SDL_SCANCODE_RGUI, Key::MetaRight},
  };

  for (const KeyPair& pair : pairs) {
    CHECK(keyButtonOf(pair.scancode) == std::optional<PhysicalButton>(PhysicalButton(pair.key)));
  }

  CHECK_FALSE(keyButtonOf(SDL_SCANCODE_NONUSHASH).has_value());
  CHECK_FALSE(keyButtonOf(SDL_SCANCODE_POWER).has_value());
}

TEST_CASE("a key press and release reach the bound button") {
  Session session;
  ButtonAction jump = session.engine.actions().declareButton("jump");
  session.engine.actions().bind(jump, Key::W);

  session.key(SDL_SCANCODE_W, true);

  CHECK(session.state(jump).held);

  session.key(SDL_SCANCODE_W, false);

  CHECK_FALSE(session.state(jump).held);
}

TEST_CASE("a key repeat changes nothing") {
  Session session;
  ButtonAction jump = session.engine.actions().declareButton("jump");
  session.engine.actions().bind(jump, Key::W);
  session.key(SDL_SCANCODE_W, true);
  session.state(jump);

  session.key(SDL_SCANCODE_W, true, true);

  CHECK_FALSE(session.state(jump).pressed);
  CHECK(session.state(jump).held);
}

TEST_CASE("losing focus releases keys and mouse buttons") {
  Session session;
  ButtonAction jump = session.engine.actions().declareButton("jump");
  ButtonAction fire = session.engine.actions().declareButton("fire");
  session.engine.actions().bind(jump, Key::W);
  session.engine.actions().bind(fire, MouseButton::Left);
  session.key(SDL_SCANCODE_W, true);
  session.mouseButton(SDL_BUTTON_LEFT, true, 10, 10);
  REQUIRE(session.state(jump).held);
  REQUIRE(session.state(fire).held);

  session.windowEvent(SDL_EVENT_WINDOW_FOCUS_LOST);

  CHECK_FALSE(session.state(jump).held);
  CHECK_FALSE(session.state(fire).held);

  session.mouseButton(SDL_BUTTON_LEFT, false, 10, 10);

  CHECK_FALSE(session.state(fire).held);
}

TEST_CASE("a press counts only inside the logical area") {
  Session session;
  oly::input::PointerAction aim = session.engine.actions().declarePointer("aim");
  ButtonAction fire = session.engine.actions().declareButton("fire");
  session.engine.actions().bind(fire, MouseButton::Left);
  SDL_SetRenderLogicalPresentation(session.window.renderer(), 160, 200, SDL_LOGICAL_PRESENTATION_LETTERBOX);

  session.motion(100, 50);
  oly::input::PointerState inside = session.pointer(aim);

  CHECK(inside.valid);
  CHECK(inside.x == doctest::Approx(20.0f));
  CHECK(inside.y == doctest::Approx(50.0f));

  session.mouseButton(SDL_BUTTON_LEFT, true, 10, 50);
  oly::input::PointerState bar = session.pointer(aim);

  CHECK(bar.x == doctest::Approx(-70.0f));
  CHECK(bar.y == doctest::Approx(50.0f));
  CHECK_FALSE(session.state(fire).held);

  session.mouseButton(SDL_BUTTON_LEFT, true, 100, 50);

  CHECK(session.state(fire).held);

  session.mouseButton(SDL_BUTTON_LEFT, false, 10, 50);

  CHECK_FALSE(session.state(fire).held);
}

TEST_CASE("visibility changes hide and show the engine once") {
  Session session;
  session.engine.frame(0.0);
  session.engine.frame(1.0 / 60);
  REQUIRE(session.engine.time() == doctest::Approx(1.0 / 60));

  session.windowEvent(SDL_EVENT_WINDOW_EXPOSED);
  session.windowEvent(SDL_EVENT_WINDOW_SHOWN);
  session.engine.frame(2.0 / 60);

  CHECK(session.engine.time() == doctest::Approx(2.0 / 60));

  session.windowEvent(SDL_EVENT_WINDOW_MINIMIZED);
  session.engine.frame(3.0 / 60);
  session.engine.frame(4.0 / 60);

  CHECK(session.translation.hidden());
  CHECK(session.engine.time() == doctest::Approx(2.0 / 60));

  session.windowEvent(SDL_EVENT_WINDOW_RESTORED);

  CHECK_FALSE(session.translation.hidden());
}

TEST_CASE("occluded and hidden windows hide, shown and exposed windows show") {
  Session session;

  session.windowEvent(SDL_EVENT_WINDOW_OCCLUDED);
  CHECK(session.translation.hidden());
  session.windowEvent(SDL_EVENT_WINDOW_EXPOSED);
  CHECK_FALSE(session.translation.hidden());

  session.windowEvent(SDL_EVENT_WINDOW_HIDDEN);
  CHECK(session.translation.hidden());
  session.windowEvent(SDL_EVENT_WINDOW_SHOWN);
  CHECK_FALSE(session.translation.hidden());
}

TEST_CASE("quit and close requests end the loop") {
  Session session;
  SDL_Event quit{};
  quit.type = SDL_EVENT_QUIT;

  CHECK_FALSE(session.send(quit));
  CHECK_FALSE(session.windowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED));
  CHECK(session.key(SDL_SCANCODE_W, true));
}
