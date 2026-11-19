# Drive the loop yourself

Use this to run the game without a host, from a test, a tool, a worker or a server: you decide when each frame runs and which input the game sees. It assumes an `Engine` with the game's schema declared and its systems registered.

## 1. Create the engine and record the first time

The first `frame` only records the time, so call it once before the steps you count.

**Where you own the `Engine`** (native):

```cpp
oly::ecs::World world;
oly::app::Engine engine(world);
oly::app::setUpGame(engine);

engine.frame(0.0);
```

In TypeScript:

```ts
const engine = runtime.createEngine({ fixedHz: 60 });
engine.frame(0);
```

## 2. Call `frame` once per step

With `fixedHz` of 60, a time that grows by `1 / 60` per call runs exactly one fixed step per call. Ten seconds of game take 600 calls.

```cpp
for (int step = 1; step <= 600; ++step) {
  engine.frame(step / 60.0);
}
```

```ts
for (let step = 1; step <= 600; ++step) {
  engine.frame(step / 60);
}
```

`frame` refuses a time that goes backwards (`OLY_ASSERT` in C++). The TypeScript client also refuses a time that is not finite.

## 3. Deliver input

A test, a replay, a touch overlay or a gamepad poller plays the part of the host: it calls `buttonDown`, `buttonUp`, `pointerMove` and `releaseAll` on `engine.actions()`. The engine captures the input at the start of the next step ([Input](../explanation/concepts/input.md#read-actions)), so call these between two `frame` calls.

```cpp
engine.actions().buttonDown(oly::input::Key::Space);
engine.frame(1.0 / 60.0);   // this step sees pressed and held for the action bound to Space

engine.actions().buttonUp(oly::input::Key::Space);
engine.frame(2.0 / 60.0);   // this step sees released

engine.actions().pointerMove(320.0f, 180.0f);
engine.actions().releaseAll();
```

```ts
engine.actions.buttonDown("Space");
engine.frame(1 / 60);

engine.actions.buttonUp("Space");
engine.frame(2 / 60);

engine.actions.pointerMove(320, 180);
engine.actions.releaseAll();
```

`buttonDown` and `buttonUp` ignore a button that is already in that state, so key auto-repeat gives one `pressed`. `releaseAll` releases every held button. `pointerMove` stops with `OLY_ASSERT` in C++ when a coordinate is not finite (`pointerMove: non-finite value (x=<x>, y=<y>)`), and the TypeScript client throws `ContractError` (`pointerMove x=<x> y=<y>: non-finite value`). A button with usage `0` stops with `OLY_ASSERT` (`buttonDown: invalid usage 0`, `buttonUp: invalid usage 0`).

A virtual joystick for a touch screen maps to the same `move` action by pressing and releasing the four buttons bound to it, or you bind a dedicated set of buttons and press those.

## See also

- [Engine, loop and systems](../explanation/concepts/engine-and-systems.md#the-loop): what one frame runs.
- [Engine::frame](../reference/cpp/engine-and-systems.md#engineframe) in C++ and [The loop](../reference/typescript/engine-and-systems.md#the-loop) in TypeScript.
- [Input](../explanation/concepts/input.md): what the host forwards and when the game reads it. The calls: [Input in C++](../reference/cpp/input.md) and [Input in TypeScript](../reference/typescript/input.md).
- [Rebind a key](gameplay-recipes.md#rebind-a-key): change a binding at run time.
- [Test a web game](test-a-web-game.md): run the real game in Node.
