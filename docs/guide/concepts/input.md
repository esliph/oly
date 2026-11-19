# Input

[← Events](../../explanation/concepts/events.md) · [Guide index](../README.md) · Next: [Schedule →](schedule.md)

Gameplay never reads keys directly. It reads **actions** ("shoot", "move", "aim"). You **bind** physical buttons to actions, and you can change the bindings at any time. This gives you key rebinding, several keys per action and fake input in tests for free.

## Three kinds of actions

| Kind | Value | Typical use |
|---|---|---|
| `button` | `{ pressed, held, released }` | shoot, jump, pause, confirm |
| `axis2d` | `{ x, y }`, each in -1..1 | movement with WASD or arrows |
| `pointer` | `{ x, y, valid }` in logical pixels | aim, click targets |

```
# game.oly
action move: axis2d
action aim: pointer
action shoot: button
action jump: button
action pause: button
```

## Bind buttons

Buttons are `oly::input::Key` and `oly::input::MouseButton` values. A key is the **physical position** on the keyboard, so WASD stays under your fingers on AZERTY, Dvorak or any layout.

Bindings are set up where you own the `Engine`:

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

**On the web**, the client binds them: [TypeScript](../../reference/typescript/input.md#bind-buttons).

Binding the same button again does nothing. Pointer actions need no binding: they follow the mouse. The full list of buttons is `oly/input/physical_button.hpp`.

## Read actions in systems

```cpp
OLY_SYSTEM(readInput) {
  const oly::input::ButtonState jumpButton = context.actions().button(actions::jump);
  const oly::input::Vector2 direction = context.actions().axis2d(actions::move);
  const oly::input::PointerState pointer = context.actions().pointer(actions::aim);

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
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::UpdateBefore, "readInput", readInput);
```

**On the web**, the client registers it by name: [TypeScript](../../reference/typescript/input.md#read-actions-in-systems).

| Flag | True in |
|---|---|
| `pressed` | the one step where the action went down |
| `held` | every step while any bound button is down |
| `released` | the one step where the action went up |

Input is captured **once per fixed step**, at the start of the step. All systems in that step see the same values.

A tap shorter than one step still counts: that step shows `pressed` **and** `released`. It may not show `held`, so use `pressed` for one-shot actions (jump, confirm, pause) and `held` for continuous ones (shoot while holding, run).

### The 2D axis

The axis gives `-1`, `0` or `1` on each side, with `y` pointing **down** like the screen. The diagonal is normalized, so moving diagonally is not faster:

| Keys down | Value |
|---|---|
| D | `{ x: 1, y: 0 }` |
| W | `{ x: 0, y: -1 }` |
| W + D | `{ x: 0.707, y: -0.707 }` |
| A + D | `{ x: 0, y: 0 }` |

```cpp
OLY_SYSTEM(steerPlayer) {
  auto [player, velocity] = context.query<Velocity>(oly::ecs::With<Player>{}).single();
  const oly::input::Vector2 direction = context.actions().axis2d(actions::move);

  velocity = Velocity{direction.x * 220.0f, direction.y * 220.0f};
}
```

## Where input comes from

The engine does not care where input comes from. The **host** of each environment forwards what the player does with `buttonDown`, `buttonUp`, `pointerMove` and `releaseAll` (all on `engine.actions()`):

- the desktop host, from the window ([What the host does with the window](../environments/native/getting-started.md#what-the-host-does-with-the-window));
- the web host, from the browser ([Getting started on the web](../environments/web/getting-started.md)).

Key auto-repeat is ignored: holding a key gives one `pressed`. When the window loses focus the host calls `releaseAll`, so nothing stays stuck. When the game is hidden, the engine releases every button too ([Engine, loop and systems](../../explanation/concepts/engine-and-systems.md#hidden)).

## Rebind at run time

Bindings can change at any time, even inside a system. The change applies from the next capture.

```cpp
OLY_SYSTEM(rebindJump) {
  oly::input::ActionMap& map = context.actions();

  map.clearBindings(actions::jump.id);
  map.bind(actions::jump, oly::input::Key::K);
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::UpdateBefore, "rebindJump", rebindJump);
```

**On the web**, the client rebinds: [TypeScript](../../reference/typescript/input.md#rebind-at-run-time).

Use `map.isBound(button)` to warn when a button is already in use.

## Deliver input yourself

Call these on `engine.actions()` from a test, a replay, a touch overlay or a gamepad poller:

**Where you own the `Engine`** (native):

```cpp
engine.actions().buttonDown(oly::input::Key::Space);
engine.actions().buttonUp(oly::input::Key::Space);
engine.actions().pointerMove(320.0f, 180.0f);
engine.actions().releaseAll();
```

**On the web**, the client has the same calls: [TypeScript](../../reference/typescript/input.md#deliver-input-yourself).

A virtual joystick for touch screens can map to the same `move` action by pressing and releasing the four bound keys, or you can bind a dedicated set of keys and press those.

## Next

[Schedule →](schedule.md): run code later, on the game clock.

In other clients: [TypeScript](../../reference/typescript/input.md).
