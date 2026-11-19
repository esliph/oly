# States

[← Schedule](../../explanation/concepts/schedule.md) · [Guide index](../README.md) · Next: [Performance →](performance.md)

States split the game into screens or modes: `menu`, `playing`, `gameOver`. Each state can turn systems and handlers on or off, run code on entry and exit, and — the best part — **clean up after itself**.

## Declare and start

```
# game.oly
state menu
state playing
state gameOver
```

The engine starts with **no state** (`engine.state() == oly::app::NoState`). Request the first one before the first frame:

**Where you own the `Engine`** (native):

```cpp
engine.requestState(states::menu);
```

**On the web**, the client requests it: [TypeScript](../../reference/typescript/states.md#declare-and-start).

States are flat: one active state at a time, no nesting, no stack. At most 32.

## Change state

```cpp
OLY_SYSTEM(startOnConfirm) {
  if (context.actions().button(actions::confirm).pressed) {
    context.requestState(states::playing);   // from a system, handler, task or hook
  }
}
```

From outside the loop, call `engine.requestState(states::gameOver)`.

A request does not change the state at once. The change happens at **step 5** of the fixed step, after the phases and events. If several requests arrive in one step, the **last one wins**. State changes happen even while paused.

Requesting the state that is already active is a full change too: it runs `onExit`, the clean-up and `onEnter` again. Use it to restart a round.

Read the current state with `context.state()` or `engine.state()` and compare it with the generated id:

```cpp
if (context.state() == states::playing) {
  // ...
}
```

## Run only in some states

**Where you own the `Engine`** (native):

```cpp
engine.setSystemStates(engine.addSystem(oly::app::Phase::UpdateBefore, "startOnConfirm", startOnConfirm), {states::menu, states::gameOver});
engine.setSystemStates(engine.addSystem(oly::app::Phase::Update, "chasePlayer", chasePlayer), {states::playing});
engine.setHandlerStates(engine.on<PlayerDied>("endRound", [](oly::app::HandlerContext& context) { endRound(context, context.events<PlayerDied>()); }), {states::playing});
```

**On the web**, the client uses `.inState(...)`: [TypeScript](../../reference/typescript/states.md#run-only-in-some-states).

- No restriction: runs in every state, and also with no state.
- With a restriction: runs only in the listed states. With no active state, it does not run.
- It works in every phase, Render included.

## Enter and exit hooks

```cpp
OLY_STATE_HOOK(startRound) {
  context.spawn(Player{}, Position{480.0f, 270.0f}, Health{3});
  context.schedule().every(5.0, spawnWave);
}

OLY_STATE_HOOK(saveBest) {
  Score& score = std::get<1>(context.query<Score>().single());
  score.best = std::max(score.best, score.score);
}
```

**Where you own the `Engine`** (native):

```cpp
engine.onEnter(states::playing, "startRound", startRound);
engine.onExit(states::playing, "saveBest", saveBest);
```

**On the web**, the client registers hooks by name or as closures: [TypeScript](../../reference/typescript/states.md#enter-and-exit-hooks).

A hook receives a `StateContext`, with direct structural changes and these extra members:

| Member | Value |
|---|---|
| `fromState()` | the state being left (`oly::app::NoState` on the first change) |
| `toState()` | the state being entered |
| `entering()` | `true` in an `onEnter`; a hook registered for both moments uses it to know which side it is on |

The change runs in this order:

1. `onExit` hooks of the old state, in registration order;
2. **scope clean-up** (below);
3. the state becomes the new one;
4. `onEnter` hooks of the new state;
5. events emitted by the hooks are dispatched.

A `requestState` made inside a hook takes effect in the next step. A hook can emit events, schedule tasks and make direct structural changes, but cannot register anything.

## Scope: automatic clean-up

While a state is active, **everything created through a context is scoped to that state**:

- entities created with `context.spawn` (in systems, handlers, tasks and hooks);
- tasks created with `schedule().after` / `schedule().every`.

When the state changes, the engine destroys the scoped entities and cancels the scoped tasks. So "play again" needs **no reset code**: leaving `playing` removes the player, the enemies, the bullets and the wave timer.

| Created by | Scoped? |
|---|---|
| `context.spawn(...)` with an active state | yes |
| `schedule().after/every` with an active state | yes |
| `world.spawn(...)` (direct on the world) | **never** |
| anything with no active state | no |

What `onExit` creates is cleaned in the same change. What `onEnter` creates belongs to the new state.

### Keep something across states: `persist`

```cpp
OLY_STATE_HOOK(startRound) {
  oly::ecs::Entity camera = context.spawn(Camera{}, Position{});
  context.persist(camera);                     // survives every state change

  oly::app::TaskHandle music = context.schedule().every(30.0, changeTrack);
  context.schedule().persist(music);
}
```

In a system, `persist` is deferred to the end of the phase, like `remove`. In hooks, handlers and tasks it is direct.

For things that exist from the start (a camera, a background), create them with `engine.world().spawn` before the first frame: they are never scoped.

## Complete pattern: menu → playing → game over

The bodies, reduced to the state logic:

```cpp
OLY_SYSTEM(startOnConfirm) {
  if (context.actions().button(actions::confirm).pressed) {
    context.requestState(states::playing);
  }
}

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

OLY_HANDLER(endRound, PlayerDied) {
  context.requestState(states::gameOver);
}

OLY_STATE_HOOK(startRound) {
  Score& score = std::get<1>(context.query<Score>().single());
  score.kills = 0;

  context.spawn(Player{}, Position{480.0f, 270.0f}, Health{3});
  context.schedule().every(5.0, spawnWave);
}

OLY_STATE_HOOK(resumeOnExit) {
  context.resume();
}
```

**Where you own the `Engine`** (native):

```cpp
engine.actions().bind(actions::confirm, oly::input::Key::Enter);
engine.actions().bind(actions::pause, oly::input::Key::Escape);

engine.setSystemStates(engine.addSystem(oly::app::Phase::UpdateBefore, "startOnConfirm", startOnConfirm), {states::menu, states::gameOver});

engine.setSystemStates(engine.addSystem(oly::app::Phase::UpdateBefore, "pauseOnPress", pauseOnPress), {states::playing});

oly::app::SystemId resumeId = engine.addSystem(oly::app::Phase::UpdateBefore, "resumeOnPress", resumeOnPress);
engine.setWhilePaused(resumeId, true);
engine.setSystemStates(resumeId, {states::playing});

engine.on<PlayerDied>("endRound", [](oly::app::HandlerContext& context) { endRound(context, context.events<PlayerDied>()); });

engine.onEnter(states::playing, "startRound", startRound);
// The pause belongs to the whole engine and only resumeOnPress (limited to playing) lifts it:
// leaving playing while paused would freeze the next state.
engine.onExit(states::playing, "resumeOnExit", resumeOnExit);

engine.requestState(states::menu);
```

**On the web**, the client registers the same pieces: [TypeScript](../../reference/typescript/states.md#complete-pattern-menu--playing--game-over).

Notice:

- No code destroys the enemies or cancels the wave timer: the scope does it.
- The pause is global. `resumeOnExit` makes sure a pause never leaks into the next state.
- The render system has no restriction: it draws every state and picks the overlay with `context.state()`.

## Next

[Performance →](performance.md): keep the hot path fast.

In other clients: [TypeScript](../../reference/typescript/states.md).
