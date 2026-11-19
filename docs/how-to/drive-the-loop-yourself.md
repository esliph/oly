# Drive the loop yourself

Use this to run the game without a host, from a test, a tool, a worker or a server: you decide when each frame runs. It assumes an `Engine` with the game's schema declared and its systems registered.

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

## See also

- [Engine, loop and systems](../explanation/concepts/engine-and-systems.md#the-loop): what one frame runs.
- [Engine::frame](../reference/cpp/engine-and-systems.md#engineframe) in C++ and [The loop](../reference/typescript/engine-and-systems.md#the-loop) in TypeScript.
- [Test a web game](test-a-web-game.md): run the real game in Node.
