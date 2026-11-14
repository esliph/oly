# Engine, loop and systems

[← Queries](queries.md) · [Guide index](../README.md) · Next: [Events →](events.md)

The `Engine` runs your game: a fixed-step clock, phases of systems, pause, events, input, schedule and states. It runs on top of one `World` that you give it.

## Create the engine

**Where you own the `Engine`** (native):

```cpp
oly::ecs::World world;
oly::app::Engine engine(world, oly::app::EngineConfig{
  .fixedHz = 60.0,          // gameplay steps per second (default 60)
  .maxStepsPerFrame = 5,    // catch-up limit per frame (default 5)
  .maxEventRounds = 16,     // event round limit per step (default 16)
});

oly::app::declareSchema(engine);
```

All three values must be positive (`OLY_ASSERT`). Register systems, handlers and hooks next, before the first `frame`.

**On the web**, the host creates the engine: [TypeScript](../environments/web/typescript/engine-and-systems.md#create-the-engine).

## The loop

Something outside the engine calls `engine.frame(nowSeconds)` once per display frame. That something is the **host** of each environment:

- on the desktop, `oly::native_host::run(engine, window)` ([Getting started on the desktop](../environments/native/getting-started.md));
- on the web, `runInBrowser(engine)` ([Getting started on the web](../environments/web/getting-started.md)).

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

## Phases

| Phase | Use it for |
|---|---|
| `Phase::UpdateBefore` | reading input, steering, decisions |
| `Phase::Update` | movement, AI, shooting, most gameplay |
| `Phase::UpdateAfter` | collision and physics, after everything moved |
| `Phase::Render` | drawing; runs once per frame, not per step |

Inside a phase, systems run in **registration order**. There is no dependency graph: the order you write is the order that runs. Systems written in different languages mix freely in one phase.

## Register systems

A system is a function of the context. Write it with `OLY_SYSTEM`:

```cpp
OLY_SYSTEM(spin) {
  const float step = 3.14159f * context.fixedDeltaTime();

  context.query<Rotation>().each([step](Rotation& rotation) {
    rotation.angle += step;
  });
}
```

**Where you own the `Engine`** (native):

```cpp
oly::app::SystemId spinId = engine.addSystem(oly::app::Phase::Update, "spin", spin);

engine.setSystemStates(spinId, {states::playing});
engine.setWhilePaused(engine.addSystem(oly::app::Phase::UpdateBefore, "resumeOnPress", resumeOnPress), true);
```

**On the web**, the client registers it by name: [TypeScript](../environments/web/typescript/engine-and-systems.md#register-systems).

`addSystem` takes a phase, a name for the error messages, the function and an optional `userData` pointer that the system reads with `context.userData()`. It returns a `SystemId`:

| Call | Effect |
|---|---|
| `setWhilePaused(id, true)` | the system runs **only** while the game is paused (not in Render) |
| `setSystemStates(id, {states::playing})` | the system runs only in those states ([States](states.md)) |

Systems cannot be registered during `frame` (from inside another system). Register everything at start-up.

## Systems in C++, registered by name: `OLY_SYSTEM`, `OLY_HANDLER`, `OLY_STATE_HOOK`

Three macros define the functions a game registers. Each one registers its function **by name** when the module loads, so an environment can find it from a string. That is what the C API's `olyEngineAddNative*` functions use, for every client.

| Macro | Defines | Native registration |
|---|---|---|
| `OLY_SYSTEM(name)` | `void name(oly::app::SystemContext& context)` | `engine.addSystem(phase, "name", name)` |
| `OLY_HANDLER(name, Event)` | `void name(HandlerContext& context, std::span<const Event> events)` | `engine.on<Event>("name", lambda)` ([Events](events.md#declare-emit-handle)) |
| `OLY_STATE_HOOK(name)` | `void name(oly::app::StateContext& context)` | `engine.onEnter(state, "name", name)` / `onExit` |

A system or a state hook is already the plain function that `addSystem`, `onEnter` and `onExit` take. A handler takes its batch as a span, so its native registration is a lambda ([Events](events.md#declare-emit-handle)).

Include the engine headers you use and the generated schema:

```cpp
#include "oly/app/handler.hpp"        // OLY_HANDLER
#include "oly/app/state_context.hpp"  // OLY_STATE_HOOK (StateContext)
#include "oly/app/system.hpp"         // OLY_SYSTEM
#include "oly/app/task_context.hpp"   // TaskContext, payloads
#include "oly/ecs/query.hpp"          // With, Without
#include "schema.generated.hpp"       // your components, events, actions:: and states::
```

## The system context

A system receives a `SystemContext`. It is valid only during that call.

| Need | Call |
|---|---|
| query | `context.query<Position, const Velocity>(oly::ecs::With<Enemy>{})` |
| read / write one component | `context.get<Health>(entity)` (reference), `context.tryGet<Health>(entity)` (pointer or `nullptr`) |
| check | `context.has<Frozen>(entity)`, `context.isAlive(entity)` |
| create | `context.spawn(Bullet{}, Position{x, y}, Velocity{vx, vy})` |
| change structure | `context.add(entity, Frozen{})`, `context.remove<Frozen>(entity)`, `context.destroy(entity)`, `context.persist(entity)` |
| events | `context.emit(EnemyDied{entity, x, y})` ([Events](events.md)) |
| input | `context.actions().button(actions::shoot).held` ([Input](input.md)) |
| time | `context.fixedDeltaTime()`, `context.time()`, `context.interpolation()` |
| schedule | `context.schedule().after(seconds, function, payload)` ([Schedule](schedule.md)) |
| state | `context.state() == states::playing`, `context.requestState(states::gameOver)` ([States](states.md)) |
| pause | `context.pause()`, `context.resume()`, `context.paused()` |
| where it runs | `context.phase()`, `context.name()`, `context.userData()` |

## Structural changes in systems

Inside a system, `spawn`, `destroy`, `add` and `remove` are **recorded** and applied at the **end of the phase**. This makes them safe inside `each` and `eachBlock`.

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

## Time and interpolation

`time` counts only completed, unpaused steps: `steps × fixedDeltaTime`. There is no accumulated floating-point sum, so it does not drift.

Render runs between steps. To draw smooth motion at high refresh rates, blend the previous and current positions with `interpolation`:

```
# game.oly
component PreviousPosition { x: f32, y: f32 }
```

```cpp
// first system of UpdateBefore: remember where everything was
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

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::UpdateBefore, "rememberPosition", rememberPosition);
engine.addSystem(oly::app::Phase::Render, "draw", draw);
```

**On the web**, the client registers it by name: [TypeScript](../environments/web/typescript/engine-and-systems.md#time-and-interpolation).

## Pause

**Where you own the `Engine`** (native):

```cpp
engine.pause();    // or context.pause() in a system
engine.resume();
engine.paused();   // true or false
```

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

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::UpdateBefore, "pauseOnPress", pauseOnPress);
engine.setWhilePaused(engine.addSystem(oly::app::Phase::UpdateBefore, "resumeOnPress", resumeOnPress), true);
```

**On the web**, the client registers it by name: [TypeScript](../environments/web/typescript/engine-and-systems.md#pause).

A `whilePaused` system runs **only** while paused. That is why the two systems above never both fire on the same key press.

In Render, read `context.paused()` to draw a "Paused" overlay.

## Hidden

`engine.setHidden(true)` stops the game: no step runs, every held button is released and, on `setHidden(false)`, the lost time is discarded. The game continues where it stopped, with no jump. This is independent of the game pause.

Each host decides when to call it: the desktop host when the window is minimized or hidden ([What the host does with the window](../environments/native/getting-started.md#what-the-host-does-with-the-window)), `runInBrowser` when the tab is hidden ([Getting started on the web](../environments/web/getting-started.md)).

## Driving the loop yourself

You can call `frame` from anything: a test or a tool.

**Where you own the `Engine`** (native):

```cpp
oly::ecs::World world;
oly::app::Engine engine(world);
oly::app::declareSchema(engine);

engine.frame(0.0);                    // the first frame only records the time

for (int step = 1; step <= 600; ++step) {
  engine.frame(step / 60.0);          // exactly one step per call: 10 seconds of game
}
```

`frame` refuses a time that goes backwards (`OLY_ASSERT`).

## When a system throws

An exception in a system leaves `frame` and goes to the code that called it. The next `frame` recovers: it discards the pending deferred changes, events and state request of the interrupted step and restarts the clock. Everything applied before the exception stays, and scheduled tasks stay.

## Next

[Events →](events.md): let systems react to what other systems did.

In other clients: [TypeScript](../environments/web/typescript/engine-and-systems.md).
