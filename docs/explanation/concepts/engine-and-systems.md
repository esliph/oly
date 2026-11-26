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
| `UpdateBefore` | reading input, steering, decisions |
| `Update` | movement, AI, shooting, most gameplay |
| `UpdateAfter` | collision and physics, after everything moved |
| `Render` | drawing; runs once per frame, not per step |

Systems in a phase run in the order their **constraints** give, and in name order where none applies. Systems written in different languages mix in one phase, and a script system is exclusive, so it is ordered against every other system of its phase.

### Order by constraints

A phase is a group that ends with one flush of the deferred changes, and a schedule says when it runs: `FixedStep` in each step, `Frame` once per frame. A plugin declares its own phase (`addPhase<Physics>(FixedStep{}).after<Update>().before<UpdateAfter>()`), and the engine puts it between the two. Systems join a **set** (`inSet<Spatial>()`) and say what they run before or after: a set, a function (every registration of it) or one system by name.

Registration closes at the first `frame`, and the engine builds the order then. The rule is: constraints first, then **name order**. The result never depends on registration order or on the order the plugins were installed, so adding a plugin does not move unrelated systems. A cycle, two phases with no order between them, and a system that needs a missing resource stop the first `frame` with every problem listed ([Errors](../../reference/errors.md#c)).

Reference: [Phases, sets and constraints](../../reference/cpp/engine-and-systems.md#phases-sets-and-constraints).

## Structural changes in systems

Inside a system, `Commands::spawn`, `destroy`, `add` and `remove` are **recorded** and applied at the **end of the phase**. This makes them safe inside `each` and `eachBlock`. The rule for every context: [Direct and deferred changes](world-and-entities.md#direct-and-deferred-changes).

```cpp
void explodeDead(oly::app::Query<const Health, const Position, oly::ecs::With<Enemy>> enemies, oly::app::Commands commands) {
  enemies.each([&](oly::ecs::Entity entity, const Health& health, const Position& position) {
    if (health.value > 0) {
      return;
    }

    commands.destroy(entity);
    commands.spawn(Explosion{}, Position{position.x, position.y});
  });
}
```

What this means for the next systems:

- A system later **in the same phase** still sees the destroyed enemy and does not see the explosion.
- A system in the **next phase** sees the result.
- `commands.spawn` returns the new handle at once. You can `add` to it or store it in another component in the same step, but `isAlive` is `false` until the phase ends.

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
void rememberPosition(oly::app::Query<const Position, PreviousPosition> bodies) {
  bodies.each([](const Position& position, PreviousPosition& previous) {
    previous = PreviousPosition{position.x, position.y};
  });
}

void draw(oly::app::Query<const Position, const PreviousPosition> bodies, const oly::app::StepInfo& step) {
  const float alpha = step.interpolation;

  bodies.each([alpha](const Position& position, const PreviousPosition& previous) {
    const float x = previous.x + (position.x - previous.x) * alpha;
    const float y = previous.y + (position.y - previous.y) * alpha;
    drawCircle(x, y, 8.0f);
  });
}
```

Register `rememberPosition` in `UpdateBefore`, so it records where everything was before any `Update` system moves it, and `draw` in `Render`.

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
void pauseOnPress(oly::app::Res<const oly::input::ActionMap> actionMap, oly::app::Res<oly::app::PauseRequest> pauseRequest) {
  if (actionMap->button(actions::pause).pressed) {
    pauseRequest->pause();
  }
}

void resumeOnPress(oly::app::Res<const oly::input::ActionMap> actionMap, oly::app::Res<oly::app::PauseRequest> pauseRequest) {
  if (actionMap->button(actions::pause).pressed) {
    pauseRequest->resume();
  }
}
```

A `whilePaused` system runs **only** while paused. That is why the two systems above never both fire on the same key press: register `resumeOnPress` with `.whilePaused()` and leave `pauseOnPress` as it is. In Render, read `StepInfo::paused` to draw a "Paused" overlay. The calls: [Pause](../../reference/cpp/engine-and-systems.md#pause).

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
