# Engine, loop and systems in TypeScript

[TypeScript client](README.md) · Concept: [Engine, loop and systems](../../explanation/concepts/engine-and-systems.md) · C++: [Engine, loop and systems in C++](../cpp/engine-and-systems.md)

The loop, the phases and the rules of structural changes are in the [concept page](../../explanation/concepts/engine-and-systems.md). This page shows the TS calls. To call `frame` yourself: [Drive the loop yourself](../../how-to/drive-the-loop-yourself.md).

## Create the engine

`runtime.createEngine(config)` creates an `Engine` with its own world. `config` is an optional `EngineConfig`, and every field is optional:

| Field | Default | Meaning |
|---|---|---|
| `fixedHz` | `60` | gameplay steps per second; a positive number |
| `maxStepsPerFrame` | `5` | catch-up limit per frame; a positive integer |
| `maxEventRounds` | `16` | event round limit per step; a positive integer |

A field that breaks its rule throws `ContractError` (`createEngine fixedHz=<value>: must be a positive number`, and the same for the other two with `a positive integer`). The engine takes the events, actions and states of `runtime.schema` ([Schema](schema.md#the-schema-object)) in schema order.

```ts
const runtime = await loadRuntime(factory, schema);
const engine = runtime.createEngine({
  fixedHz: 60,            // gameplay steps per second (default 60)
  maxStepsPerFrame: 5,    // catch-up limit per frame (default 5)
  maxEventRounds: 16,     // event round limit per step (default 16)
});

// ... register systems, handlers, hooks

engine.dispose();          // also disposes engine.world
```

## The loop

In the browser, `runInBrowser(engine)` calls `engine.frame` with `requestAnimationFrame` and calls `engine.setHidden` when the tab is hidden ([the loop](../../explanation/concepts/engine-and-systems.md#the-loop)).

```ts
import { runInBrowser } from "@oly/web-host";

const loop = runInBrowser(engine);
// loop.stop() to end it
```

## Register systems

```ts
import { Phase } from "@oly/core";

engine.addSystem(Phase.Update, "spin", (context) => {
  context.query({ write: [Rotation] }).eachBlock((block) => {
    const rotation = block.write(Rotation);
    for (let row = 0; row < block.count; ++row) {
      rotation.angle.set(row, rotation.angle.get(row) + Math.PI * context.fixedDeltaTime);
    }
  });
});

engine.addNativeSystem(Phase.Update, "integrate");   // a C++ OLY_SYSTEM(integrate)
```

The name appears in error messages; use a clear one. The system has the type `SystemFunction`, `(context: SystemContext) => void`.

Registration returns a `SystemRegistration`: its `id` is the system id, and it has two options. Both can be chained:

```ts
engine.addSystem(Phase.UpdateBefore, "resumeOnPress", resumeOnPress).whilePaused().inState(playing);
```

| Option | Effect |
|---|---|
| `.whilePaused()` | the system runs **only** while the game is paused (not in Render) |
| `.inState(...states)` | the system runs only in those states ([States](states.md)) |

## C++ systems, handlers and hooks

A web game can register the functions it defines in C++ with `OLY_SYSTEM`, `OLY_HANDLER` and `OLY_STATE_HOOK` ([the macros](../cpp/engine-and-systems.md#c-systems-handlers-and-hooks)). TS looks the name up once, at registration.

| Macro | Register from TS with |
|---|---|
| `OLY_SYSTEM(name)` | `engine.addNativeSystem(phase, "name")` |
| `OLY_HANDLER(name, Event)` | `engine.addNativeHandler(Event, "name")` |
| `OLY_STATE_HOOK(name)` | `engine.addNativeOnEnter(state, "name")` / `addNativeOnExit` |

A wrong name throws `ContractError` right there, not in the middle of the game.

```ts
engine.addNativeSystem(Phase.Update, "integrate").inState(playing);
```

### `Phase`

```ts
const Phase = { UpdateBefore: 0, Update: 1, UpdateAfter: 2, Render: 3 } as const;
```

`addSystem` takes one of `Phase.UpdateBefore`, `Phase.Update`, `Phase.UpdateAfter` and `Phase.Render`. The first three run in each fixed step, in this order, and `Render` runs once per frame ([Phases](../../explanation/concepts/engine-and-systems.md#phases)). A TS system always runs in the `Default` band.

C++ and TS systems mix freely in one phase, in registration order.

## The system context

A system receives a `SystemContext`. It is valid only during that call.

| Member | What it does |
|---|---|
| `time` | game time at the start of this step, in seconds (stops while paused) |
| `fixedDeltaTime` | seconds per step (`1 / fixedHz`) |
| `interpolation` | in Render: how far (0..1) the real time is between the last step and the next |
| `paused`, `pause()`, `resume()` | game pause |
| `query(spec)`, `get`, `set`, `has`, `isAlive` | read and write component values |
| `spawn`, `destroy`, `add`, `remove`, `persist` | **deferred** structural changes |
| `emit(Event, values)` | send an event ([Events](events.md)) |
| `actions` | input ([Input](input.md)) |
| `schedule` | timers ([Schedule](schedule.md)) |
| `state`, `requestState(state)` | game state ([States](states.md)) |
| `phase`, `name` | where the system is registered |

## The contexts

Four contexts reach a game's code. Three change the world at once, and the system context defers the changes to the end of the phase.

| | System | Handler | Task | State hook |
|---|---|---|---|---|
| structural changes | deferred to the end of the phase | direct | direct | direct |
| members only this context has | `phase`, `name`, `interpolation` | `eventType`, `name` (and the batch argument) | `handle`, `lastRun` | `fromState`, `toState` |

Every context has `time`, `fixedDeltaTime`, `paused`, `pause()`, `resume()`, `state`, `requestState()`, `actions`, `schedule`, `query`, `get`, `set`, `has`, `isAlive`, `emit`, `spawn`, `destroy`, `add`, `remove` and `persist`. Each context is valid only during the call it was passed to.

## Structural changes in systems

```ts
engine.addSystem(Phase.Update, "explodeDead", (context) => {
  context.query({ read: [Health, Position], with: [Enemy] }).eachBlock((block) => {
    const health = block.read(Health);
    const position = block.read(Position);
    for (let row = 0; row < block.count; ++row) {
      if (health.value.get(row) > 0) {
        continue;
      }

      context.destroy(block.entity(row));
      context.spawn([Explosion, Position], { Position: { x: position.x.get(row), y: position.y.get(row) } });
    }
  });
});
```

`context.set` is **not** structural: it writes the value immediately.

## Time and interpolation

```ts
// first system of UpdateBefore: remember where everything was
engine.addSystem(Phase.UpdateBefore, "rememberPosition", (context) => {
  context.query({ read: [Position], write: [PreviousPosition] }).eachBlock((block) => {
    const position = block.read(Position);
    const previous = block.write(PreviousPosition);
    for (let row = 0; row < block.count; ++row) {
      previous.x.set(row, position.x.get(row));
      previous.y.set(row, position.y.get(row));
    }
  });
});

engine.addSystem(Phase.Render, "draw", (context) => {
  const alpha = context.interpolation;

  context.query({ read: [Position, PreviousPosition] }).eachBlock((block) => {
    const position = block.read(Position);
    const previous = block.read(PreviousPosition);
    for (let row = 0; row < block.count; ++row) {
      const x = previous.x.get(row) + (position.x.get(row) - previous.x.get(row)) * alpha;
      const y = previous.y.get(row) + (position.y.get(row) - previous.y.get(row)) * alpha;
      drawer.circle(x, y, 8);
    }
  });
});
```

## Pause

```ts
engine.pause();
engine.resume();
engine.paused;        // true or false
```

```ts
function pauseOnPress(context: SystemContext): void {
  if (context.actions.button(pause).pressed) {
    context.pause();
  }
}

function resumeOnPress(context: SystemContext): void {
  if (context.actions.button(pause).pressed) {
    context.resume();
  }
}

engine.addSystem(Phase.UpdateBefore, "pauseOnPress", pauseOnPress);
engine.addSystem(Phase.UpdateBefore, "resumeOnPress", resumeOnPress).whilePaused();
```

In Render, read `context.paused` to draw a "Paused" overlay.

## When a system throws

An exception in a TS system goes to your code, or to the browser console with `runInBrowser`, which keeps the loop alive. Until the next `frame` recovers ([what recovery does](../../explanation/concepts/engine-and-systems.md#when-a-system-throws)), registering systems, handlers or hooks throws `ContractError`.
