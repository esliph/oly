# Engine, loop and systems in TypeScript

[← Queries](queries.md) · [TypeScript client](README.md) · Concept: [Engine, loop and systems](../../guide/concepts/engine-and-systems.md) · Next: [Events →](events.md)

The loop, the phases and the rules of structural changes are in the [concept page](../../guide/concepts/engine-and-systems.md). This page shows the TS calls.

## Create the engine

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

In the browser, `runInBrowser(engine)` calls `engine.frame` with `requestAnimationFrame` and calls `engine.setHidden` when the tab is hidden ([the loop](../../guide/concepts/engine-and-systems.md#the-loop)).

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

The name appears in error messages; use a clear one.

Registration returns an object with two options. Both can be chained:

```ts
engine.addSystem(Phase.UpdateBefore, "resumeOnPress", resumeOnPress).whilePaused().inState(playing);
```

| Option | Effect |
|---|---|
| `.whilePaused()` | the system runs **only** while the game is paused (not in Render) |
| `.inState(...states)` | the system runs only in those states ([States](states.md)) |

## C++ systems, handlers and hooks

A web game can register the functions it defines in C++ with `OLY_SYSTEM`, `OLY_HANDLER` and `OLY_STATE_HOOK` ([the macros](../../guide/concepts/engine-and-systems.md#systems-in-c-registered-by-name-oly_system-oly_handler-oly_state_hook)). TS looks the name up once, at registration.

| Macro | Register from TS with |
|---|---|
| `OLY_SYSTEM(name)` | `engine.addNativeSystem(phase, "name")` |
| `OLY_HANDLER(name, Event)` | `engine.addNativeHandler(Event, "name")` |
| `OLY_STATE_HOOK(name)` | `engine.addNativeOnEnter(state, "name")` / `addNativeOnExit` |

A wrong name throws `ContractError` right there, not in the middle of the game.

```ts
engine.addNativeSystem(Phase.Update, "integrate").inState(playing);
```

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

## Driving the loop yourself

You can call `frame` from a test, a worker or a server.

```ts
const engine = runtime.createEngine({ fixedHz: 60 });
engine.frame(0);                 // the first frame only records the time

for (let step = 1; step <= 600; ++step) {
  engine.frame(step / 60);       // exactly one step per call: 10 seconds of game
}
```

`frame` refuses a time that goes backwards or is not finite.

## When a system throws

An exception in a TS system goes to your code, or to the browser console with `runInBrowser`, which keeps the loop alive. Until the next `frame` recovers ([what recovery does](../../guide/concepts/engine-and-systems.md#when-a-system-throws)), registering systems, handlers or hooks throws `ContractError`.

## Next

[Events →](events.md): let systems react to what other systems did.
