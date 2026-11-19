# Input in TypeScript

[TypeScript client](README.md) · Concept: [Input](../../explanation/concepts/input.md) · C++: [Input in C++](../cpp/input.md)

The three kinds of actions, when `pressed`, `held` and `released` are true and how the axis is normalized are in the [concept page](../../explanation/concepts/input.md). This page shows the TS calls. To rebind keys: [Rebind a key](../../how-to/typescript/gameplay-recipes.md#rebind-a-key). To press buttons yourself: [Drive the loop yourself](../../how-to/drive-the-loop-yourself.md#3-deliver-input).

## Action kinds and ids

```ts
type ActionKind = "button" | "axis2d" | "pointer";
```

An action declared in the schema is an `ActionType`: its `actionKind` is one of the three kinds, and its `schemaIndex` is its position in `schema.actions`, which is also its `ActionId`. The types `ButtonAction`, `Axis2dAction` and `PointerAction` are an `ActionType` of one kind each: a call that takes a handle of the wrong kind does not compile, and one that gets it anyway throws `ContractError` (`<operation> action=<name>: action of kind <kind>, expected <kind>`).

## Bind buttons

Buttons use the W3C `KeyboardEvent.code` names.

```ts
engine.actions.bind(shoot, "MouseLeft");
engine.actions.bind(jump, "Space");
engine.actions.bind(jump, "KeyW");              // a second button for the same action
engine.actions.bind(pause, "Escape");
engine.actions.bind(move, { up: "KeyW", down: "KeyS", left: "KeyA", right: "KeyD" });
engine.actions.bind(move, { up: "ArrowUp", down: "ArrowDown", left: "ArrowLeft", right: "ArrowRight" });
```

Available names: `KeyA`..`KeyZ`, `Digit0`..`Digit9`, `F1`..`F12`, `Enter`, `Escape`, `Space`, `Tab`, `Backspace`, arrows (`ArrowUp`...), modifiers (`ShiftLeft`, `ControlLeft`, `AltLeft`, `MetaLeft` and the `Right` versions), punctuation (`Minus`, `Comma`, `Slash`...), the numpad (`Numpad0`..`Numpad9`, `NumpadAdd`...) and the mouse (`MouseLeft`, `MouseRight`, `MouseMiddle`). The full list is the `ButtonName` type; `isButtonName(text)` checks a string at run time.

`engine.actions.clearBindings(action)` removes every binding of the action. `engine.actions.isBound(button)` is `true` when any action uses the button. A button name outside the table throws `ContractError` (`<operation> button=<name>: button outside the W3C table`).

## Read actions in systems

```ts
engine.addSystem(Phase.UpdateBefore, "readInput", (context) => {
  const jumpButton = context.actions.button(jump);
  const direction = context.actions.axis2d(move);
  const pointer = context.actions.pointer(aim);

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
});
```

### The 2D axis

```ts
const direction = context.actions.axis2d(move);
velocity.x.set(row, direction.x * speed);
velocity.y.set(row, direction.y * speed);
```

## Connect the browser

`attachBrowserInput` sends keyboard and mouse events to the engine:

```ts
import { attachBrowserInput } from "@oly/web-host";

const input = attachBrowserInput(engine, canvas);
// input.detach() removes all listeners
```

What it does:

- keys from `window`, by `code`;
- mouse presses only **on the canvas**, releases anywhere in the window;
- pointer position converted to **canvas pixels** (it handles CSS scaling);
- on `blur` (the window loses focus), releases every button so nothing stays stuck;
- calls `preventDefault` only for buttons bound to an action: Space does not scroll the page and Tab does not leave the game, but unbound keys keep their browser behaviour.

The right-click menu is prevented only when `MouseRight` is bound. If you do not bind it but still want no menu:

```ts
canvas.addEventListener("contextmenu", (event) => event.preventDefault());
```
