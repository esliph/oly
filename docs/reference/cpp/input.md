# Input in C++

[Reference](../../README.md#reference) · Concept: [Input](../../explanation/concepts/input.md) · TypeScript: [Input in TypeScript](../typescript/input.md)

Headers: `oly/input/action_map.hpp` for `ActionMap` and the state types, and `oly/input/physical_button.hpp` for `Key`, `MouseButton` and `PhysicalButton`. The three kinds of actions, when `pressed`, `held` and `released` are true and how the axis is normalized: the [concept page](../../explanation/concepts/input.md). The systems read the map with `context.actions()`, and the code that owns the `Engine` with `engine.actions()`.

## Action kinds and ids

The schema declares the actions ([Schema in C++](schema.md)), and `setUpGame` declares them in the `ActionMap`. A game seldom calls `declare`.

### `ActionKind`

```cpp
enum class ActionKind : uint8_t { Button, Axis2d, Pointer };

using ActionId = uint32_t;

struct ButtonAction { ActionId id; };
struct Axis2dAction { ActionId id; };
struct PointerAction { ActionId id; };
```

`ActionKind` is the kind of an action, and `ActionId` is its position in the declaration order. The three structs are typed handles, one per kind: the generated `actions::shoot`, `actions::move` and `actions::aim` are values of them. A call that takes a handle of the wrong kind does not compile; a call that takes an `ActionId` checks the kind at run time.

### `ActionMap::actionCount`

```cpp
uint32_t actionCount() const noexcept;
ActionKind kind(ActionId action) const;
std::string_view name(ActionId action) const;
```

`actionCount()` is the number of declared actions, and the valid ids are `0` up to `actionCount() - 1`. `kind` and `name` return what the schema declared for an id. An id out of range stops with `OLY_ASSERT` (`kind: action <id> out of range (<n> declared)` and `name: action <id> out of range (<n> declared)`).

```cpp
for (oly::input::ActionId action = 0; action < engine.actions().actionCount(); ++action) {
  if (engine.actions().kind(action) == oly::input::ActionKind::Button) {
    std::puts(std::string(engine.actions().name(action)).c_str());
  }
}
```

## Bind buttons

Buttons are `oly::input::Key` and `oly::input::MouseButton` values. Both convert to `PhysicalButton`. The full list of buttons is `oly/input/physical_button.hpp`.

### `ActionMap::bind`

```cpp
void bind(ButtonAction action, PhysicalButton button);
void bind(Axis2dAction action, const Axis2dBinding& binding);
```

Binds a button to a button action, or four buttons to an axis. `Axis2dBinding` holds `up`, `down`, `left` and `right`. Binding the same button again does nothing. The change applies from the next capture. A handle whose action has another kind stops with `OLY_ASSERT` (`bind: action '<name>' has kind <kind>, expected <kind>`). A button with usage `0` stops with `bind: invalid usage 0 (action '<name>')`.

**Where you own the `Engine`** (native):

```cpp
using oly::input::Key;
using oly::input::MouseButton;

engine.actions().bind(actions::shoot, MouseButton::Left);
engine.actions().bind(actions::jump, Key::Space);
engine.actions().bind(actions::jump, Key::W);              // a second button for the same action
engine.actions().bind(actions::pause, Key::Escape);
engine.actions().bind(actions::move, oly::input::Axis2dBinding{Key::W, Key::S, Key::A, Key::D});
engine.actions().bind(actions::move, oly::input::Axis2dBinding{Key::ArrowUp, Key::ArrowDown, Key::ArrowLeft, Key::ArrowRight});
```

**On the web**, the client binds them: [TypeScript](../typescript/input.md#bind-buttons).

### `ActionMap::clearBindings`

```cpp
void clearBindings(ActionId action);
bool isBound(PhysicalButton button) const;
```

`clearBindings` removes every button and axis binding of the action. `isBound` is `true` when any action uses the button, alone or in an axis. An id out of range stops with `OLY_ASSERT` (`clearBindings: action <id> out of range (<n> declared)`). They are the two calls of [Rebind a key](../../how-to/gameplay-recipes.md#rebind-a-key).

## Deliver host input

### `ActionMap::buttonDown`

```cpp
void buttonDown(PhysicalButton button);
void buttonUp(PhysicalButton button);
void pointerMove(float x, float y);
void releaseAll();
```

A host, a test or a replay calls these on `engine.actions()` to tell the engine what the player did. The engine captures the input at the start of the next step, so call them between two `frame` calls. `buttonDown` and `buttonUp` ignore a button that is already in that state. `releaseAll` releases every held button. A button with usage `0` stops with `OLY_ASSERT` (`buttonDown: invalid usage 0`, `buttonUp: invalid usage 0`), and a coordinate that is not finite stops `pointerMove` (`pointerMove: non-finite value (x=<x>, y=<y>)`).

```cpp
engine.actions().buttonDown(oly::input::Key::Space);
engine.frame(1.0 / 60.0);
engine.actions().buttonUp(oly::input::Key::Space);
engine.actions().pointerMove(320.0f, 180.0f);
engine.actions().releaseAll();
```

The native host does this for you from the window events ([Native host](../native-host.md#what-the-host-does-with-the-window)). To step a game yourself: [Drive the loop yourself](../../how-to/drive-the-loop-yourself.md#3-deliver-input).

### `isKey`

```cpp
constexpr bool isKey(uint32_t usage) noexcept;
```

`true` when `usage` is one of the `Key` values. A `PhysicalButton` holds a usage that is either a `Key` or a `MouseButton`, so `isKey(button.usage)` tells which one it is.

## Read actions in systems

### `ActionMap::button`

```cpp
ButtonState button(ButtonAction action) const;
Vector2 axis2d(Axis2dAction action) const;
PointerState pointer(PointerAction action) const;
```

Each call returns the value captured at the start of the step. A handle whose action has another kind stops with `OLY_ASSERT` (`button: action '<name>' has kind <kind>, expected <kind>`, and the same with `axis2d` and `pointer`).

```cpp
struct ButtonState { bool pressed; bool held; bool released; };
struct Vector2 { float x; float y; };
struct PointerState { float x; float y; bool valid; };
```

```cpp
void readInput(oly::app::Res<const oly::input::ActionMap> actionMap) {
  const oly::input::ButtonState jumpButton = actionMap->button(actions::jump);
  const oly::input::PointerState pointer = actionMap->pointer(actions::aim);

  if (jumpButton.pressed) {
    // first step of the press
  }

  if (jumpButton.held) {
    // every step while it is down
  }

  if (jumpButton.released) {
    // the step it went up
  }

  if (pointer.valid) {
    // a pointer position has arrived at least once
  }
}

OLY_SYSTEM(readInput);
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem<&readInput>(oly::app::UpdateBefore{}, "readInput");
```

**On the web**, the client registers it by name: [TypeScript](../typescript/input.md#read-actions-in-systems).

### The 2D axis

```cpp
void steerPlayer(oly::app::Query<Velocity, oly::ecs::With<Player>> players, oly::app::Res<const oly::input::ActionMap> actionMap) {
  auto [player, velocity] = players.single();
  const oly::input::Vector2 direction = actionMap->axis2d(actions::move);

  velocity = Velocity{direction.x * 220.0f, direction.y * 220.0f};
}

OLY_SYSTEM(steerPlayer);
```
