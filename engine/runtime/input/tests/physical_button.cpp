#include "oly/input/physical_button.hpp"

#include <doctest/doctest.h>

namespace {
  using oly::input::Key;
  using oly::input::MouseButton;
  using oly::input::PhysicalButton;

  struct Sample {
    Key key;
    uint32_t usage;
  };
}

TEST_CASE("HID table samples") {
  const Sample samples[] = {
    {Key::A, 0x0007'0004u},
    {Key::W, 0x0007'001Au},
    {Key::Z, 0x0007'001Du},
    {Key::Digit1, 0x0007'001Eu},
    {Key::Digit0, 0x0007'0027u},
    {Key::Enter, 0x0007'0028u},
    {Key::Escape, 0x0007'0029u},
    {Key::Space, 0x0007'002Cu},
    {Key::F1, 0x0007'003Au},
    {Key::F12, 0x0007'0045u},
    {Key::ArrowRight, 0x0007'004Fu},
    {Key::ArrowLeft, 0x0007'0050u},
    {Key::ArrowDown, 0x0007'0051u},
    {Key::ArrowUp, 0x0007'0052u},
    {Key::ControlLeft, 0x0007'00E0u},
    {Key::ShiftLeft, 0x0007'00E1u},
    {Key::AltLeft, 0x0007'00E2u},
    {Key::MetaLeft, 0x0007'00E3u},
    {Key::ShiftRight, 0x0007'00E5u},
  };

  for (const Sample& sample : samples) {
    CHECK(PhysicalButton(sample.key).usage == sample.usage);
  }

  CHECK(PhysicalButton(MouseButton::Left).usage == 0x0009'0001u);
  CHECK(PhysicalButton(MouseButton::Right).usage == 0x0009'0002u);
  CHECK(PhysicalButton(MouseButton::Middle).usage == 0x0009'0003u);
}

TEST_CASE("equality compares the usage") {
  CHECK(PhysicalButton(Key::Space) == PhysicalButton(0x0007'002Cu));
  CHECK_FALSE(PhysicalButton(Key::Space) == PhysicalButton(Key::Enter));
}

TEST_CASE("isKey accepts every Key and rejects the gaps") {
  CHECK(oly::input::isKey(static_cast<uint32_t>(Key::A)));
  CHECK(oly::input::isKey(static_cast<uint32_t>(Key::Backslash)));
  CHECK(oly::input::isKey(static_cast<uint32_t>(Key::Semicolon)));
  CHECK(oly::input::isKey(static_cast<uint32_t>(Key::ContextMenu)));
  CHECK(oly::input::isKey(static_cast<uint32_t>(Key::ControlLeft)));
  CHECK(oly::input::isKey(static_cast<uint32_t>(Key::MetaRight)));

  CHECK_FALSE(oly::input::isKey(0x0007'0003));
  CHECK_FALSE(oly::input::isKey(0x0007'0032));
  CHECK_FALSE(oly::input::isKey(0x0007'0066));
  CHECK_FALSE(oly::input::isKey(0x0007'00DF));
  CHECK_FALSE(oly::input::isKey(0x0007'00E8));
  CHECK_FALSE(oly::input::isKey(static_cast<uint32_t>(MouseButton::Left)));
}
