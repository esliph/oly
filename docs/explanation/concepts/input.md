# Input

[← Events](events.md) · [Concepts](README.md) · Next: [Schedule →](../../guide/concepts/schedule.md)

Gameplay never reads keys directly. It reads **actions** ("shoot", "move", "aim"). You **bind** physical buttons to actions, and you can change the bindings at any time. This gives you key rebinding, several keys per action and fake input in tests for free.

## Three kinds of actions

| Kind | Value | Typical use |
|---|---|---|
| `button` | `{ pressed, held, released }` | shoot, jump, pause, confirm |
| `axis2d` | `{ x, y }`, each in -1..1 | movement with WASD or arrows |
| `pointer` | `{ x, y, valid }` in logical pixels | aim, click targets |

The schema declares the actions ([Schema](schema.md)):

```
# game.oly
action move: axis2d
action aim: pointer
action shoot: button
```

Each action has an `ActionId`, its position in the declaration order, and a kind. An action is typed by its kind: reading an `axis2d` action as a button is a contract violation. The types: [Action kinds and ids](../../reference/cpp/input.md#action-kinds-and-ids).

TypeScript: [Action kinds and ids](../../reference/typescript/input.md#action-kinds-and-ids).

## Bind buttons

A binding connects a physical button to an action. A key is the **physical position** on the keyboard, so WASD stays under your fingers on AZERTY, Dvorak or any layout. Several buttons can serve one action. Binding the same button to the same action again does nothing. Pointer actions need no binding: they follow the mouse.

Bindings change at any time, even inside a system, and apply from the next capture. To let the player choose keys: [Rebind a key](../../how-to/gameplay-recipes.md#rebind-a-key).

The calls: [Bind buttons](../../reference/cpp/input.md#bind-buttons).

TypeScript: [Bind buttons](../../reference/typescript/input.md#bind-buttons).

## Read actions

Input is captured **once per fixed step**, at the start of the step. All systems in that step see the same values.

| Flag | True in |
|---|---|
| `pressed` | the one step where the action went down |
| `held` | every step while any bound button is down |
| `released` | the one step where the action went up |

A tap shorter than one step still counts: that step shows `pressed` **and** `released`. It may not show `held`, so use `pressed` for one-shot actions (jump, confirm, pause) and `held` for continuous ones (shoot while holding, run).

`pointer.valid` is `false` until a pointer position has arrived at least once.

The calls: [Read actions in systems](../../reference/cpp/input.md#read-actions-in-systems).

TypeScript: [Read actions in systems](../../reference/typescript/input.md#read-actions-in-systems).

### The 2D axis

The axis gives `-1`, `0` or `1` on each side, with `y` pointing **down** like the screen. The diagonal is normalized, so moving diagonally is not faster:

| Keys down | Value |
|---|---|
| D | `{ x: 1, y: 0 }` |
| W | `{ x: 0, y: -1 }` |
| W + D | `{ x: 0.707, y: -0.707 }` |
| A + D | `{ x: 0, y: 0 }` |

The calls: [The 2D axis](../../reference/cpp/input.md#the-2d-axis).

TypeScript: [The 2D axis](../../reference/typescript/input.md#the-2d-axis).

## Where input comes from

The engine does not care where input comes from. The **host** of each environment forwards what the player does with `buttonDown`, `buttonUp`, `pointerMove` and `releaseAll` (all on `engine.actions()`):

- the desktop host, from the window ([What the host does with the window](../../guide/environments/native/getting-started.md#what-the-host-does-with-the-window));
- the web host, from the browser ([Getting started on the web](../../guide/environments/web/getting-started.md)).

Key auto-repeat is ignored: holding a key gives one `pressed`. When the window loses focus the host calls `releaseAll`, so nothing stays stuck. When the game is hidden, the engine releases every button too ([Engine, loop and systems](engine-and-systems.md#hidden)).

A test, a replay, a touch overlay or a gamepad poller calls the same functions: [Drive the loop yourself](../../how-to/drive-the-loop-yourself.md#3-deliver-input).

## API

- C++: [Input in C++](../../reference/cpp/input.md)
- TypeScript: [Input in TypeScript](../../reference/typescript/input.md)

## Next

[Schedule](../../guide/concepts/schedule.md): run code later, on the game clock.
