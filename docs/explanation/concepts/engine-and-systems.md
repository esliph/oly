# Engine, loop and systems

[← Queries](queries.md) · [Concepts](README.md) · Next: [Events →](events.md)

The `Engine` runs your game: a fixed-step clock, phases of systems, pause, events, input, schedule and states. It runs on top of one `World` that you give it ([Where the World comes from](world-and-entities.md#where-the-world-comes-from)). This page explains how a frame runs, in which order systems run and what the engine does when the game pauses, hides or fails.

## The loop

Something outside the engine calls `engine.frame(nowSeconds)` once per display frame. That something is the **host** of each environment:

- on the desktop, `oly::native_host::run(engine, window)`, which `runGame` calls ([Native host](../../reference/native-host.md));
- on the web, `runInBrowser`, which `runGame` calls ([Your first web game](../../tutorials/first-web-game.md)).

Each `frame` runs as many **fixed steps** as the elapsed time allows (at most `maxStepsPerFrame`), then **one render**:

```
frame(now)
├── fixed step (repeated)
│   1. input capture
│   2. due scheduled tasks              (skipped while paused)
│   3. UpdateBefore → Update → UpdateAfter   (deferred changes applied after each phase)
│   4. event dispatch, in rounds
│   5. pending state change
└── Render (once)
```

Why a fixed step: gameplay runs with the same `deltaTime` every time, at any display rate (60 Hz, 144 Hz, a slow laptop). Results are reproducible and tests can step the game exactly.

If the game falls far behind (a debugger pause, a slow device), the steps above `maxStepsPerFrame` are dropped instead of piling up. This prevents the "spiral of death".

TypeScript: [The loop](../../reference/typescript/engine-and-systems.md#the-loop). To call `frame` yourself: [Drive the loop yourself](../../how-to/drive-the-loop-yourself.md).

## Phases

| Phase | Use it for |
|---|---|
| `Phase::UpdateBefore` | reading input, steering, decisions |
| `Phase::Update` | movement, AI, shooting, most gameplay |
| `Phase::UpdateAfter` | collision and physics, after everything moved |
| `Phase::Render` | drawing; runs once per frame, not per step |

Inside a phase, systems run in **registration order**. There is no dependency graph: the order you write is the order that runs. Systems written in different languages mix freely in one phase.

### Order bands

A system can ask to run at the **end** of its phase, after every system of the default band, whatever the registration order.

| Band | Runs |
|---|---|
| `OrderBand::Default` | first, in registration order (the default) |
| `OrderBand::Last` | after the whole `Default` band of the phase, in registration order |

The band is fixed at registration. The phase still applies its deferred changes once, after the `Last` band, so a `Last` system sees the direct writes of the `Default` band but not its spawns, adds or removes. `setWhilePaused` and `setSystemStates` work the same in both bands. Plugins use `Last` for work that must follow the game's systems; the clients register in `Default`. The call: [Engine::addSystem](../../reference/cpp/engine-and-systems.md#engineaddsystem).

## Structural changes in systems

Inside a system, `spawn`, `destroy`, `add` and `remove` are **recorded** and applied at the **end of the phase**. This makes them safe inside `each` and `eachBlock`. The rule for every context: [Direct and deferred changes](world-and-entities.md#direct-and-deferred-changes).

```cpp
OLY_SYSTEM(explodeDead) {
  context.query<const Health, const Position>(oly::ecs::With<Enemy>{}).each([&](oly::ecs::Entity entity, const Health& health, const Position& position) {
    if (health.value > 0) {
      return;
    }

    context.destroy(entity);
    context.spawn(Explosion{}, Position{position.x, position.y});
  });
}
```

What this means for the next systems:

- A system later **in the same phase** still sees the destroyed enemy and does not see the explosion.
- A system in the **next phase** sees the result.
- `context.spawn` returns the new handle at once. You can `add` to it or store it in another component in the same step, but `isAlive` is `false` until the phase ends.

Writing through a reference from `get` or a query is **not** structural: it changes the value immediately.

Use the phase boundary on purpose. For example, destroy out-of-bounds bullets in `Update`; a `collide` system in `UpdateAfter` then never sees them.

TypeScript: [Structural changes in systems](../../reference/typescript/engine-and-systems.md#structural-changes-in-systems).

## Time and interpolation

`time` counts only completed, unpaused steps: `steps × fixedDeltaTime`. There is no accumulated floating-point sum, so it does not drift.

Render runs between steps. To draw smooth motion at high refresh rates, blend the previous and current positions with `interpolation`:

```
# game.oly
component PreviousPosition { x: f32, y: f32 }
```

```cpp
OLY_SYSTEM(rememberPosition) {
  context.query<const Position, PreviousPosition>().each([](const Position& position, PreviousPosition& previous) {
    previous = PreviousPosition{position.x, position.y};
  });
}

OLY_SYSTEM(draw) {
  const float alpha = context.interpolation();

  context.query<const Position, const PreviousPosition>().each([alpha](const Position& position, const PreviousPosition& previous) {
    const float x = previous.x + (position.x - previous.x) * alpha;
    const float y = previous.y + (position.y - previous.y) * alpha;
    drawCircle(x, y, 8.0f);
  });
}
```

Register `rememberPosition` as the first system of `UpdateBefore`, so it records where everything was before any system moves it, and `draw` in `Render`.

TypeScript: [Time and interpolation](../../reference/typescript/engine-and-systems.md#time-and-interpolation).

## Pause

The pause takes effect from the **next** step. While paused:

| Keeps running | Stops |
|---|---|
| input capture | the clock (`time`) |
| systems marked `whilePaused` | all other gameplay systems |
| event dispatch, state changes | scheduled tasks |
| Render | |

A paused game still needs a way to resume. Register a system that runs only while paused:

```cpp
OLY_SYSTEM(pauseOnPress) {
  if (context.actions().button(actions::pause).pressed) {
    context.pause();
  }
}

OLY_SYSTEM(resumeOnPress) {
  if (context.actions().button(actions::pause).pressed) {
    context.resume();
  }
}
```

A `whilePaused` system runs **only** while paused. That is why the two systems above never both fire on the same key press: register `resumeOnPress` with `setWhilePaused(id, true)` and leave `pauseOnPress` as it is. In Render, read `context.paused()` to draw a "Paused" overlay. The calls: [Pause](../../reference/cpp/engine-and-systems.md#pause).

TypeScript: [Pause](../../reference/typescript/engine-and-systems.md#pause).

## Hidden

`engine.setHidden(true)` stops the game: no step runs, every held button is released and, on `setHidden(false)`, the lost time is discarded. The game continues where it stopped, with no jump. This is independent of the game pause.

Each host decides when to call it: the desktop host when the window is minimized or hidden ([What the host does with the window](../../reference/native-host.md#what-the-host-does-with-the-window)), `runInBrowser` when the tab is hidden ([Environments and hosts](environments-and-hosts.md)). The call: [Engine::setHidden](../../reference/cpp/engine-and-systems.md#enginesethidden).

## When a system throws

An exception in a system leaves `frame` and goes to the code that called it. The next `frame` recovers: it discards the pending deferred changes, events and state request of the interrupted step and restarts the clock. Everything applied before the exception stays, and scheduled tasks stay.

TypeScript: [When a system throws](../../reference/typescript/engine-and-systems.md#when-a-system-throws).

## API

- C++: [Engine, loop and systems in C++](../../reference/cpp/engine-and-systems.md)
- TypeScript: [Engine, loop and systems in TypeScript](../../reference/typescript/engine-and-systems.md)

Call `frame` from a test or a tool: [Drive the loop yourself](../../how-to/drive-the-loop-yourself.md).

## Next

[Events](events.md): let systems react to what other systems did.
