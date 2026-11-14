# Schedule

[← Input](input.md) · [Guide index](../README.md) · Next: [States →](states.md)

The scheduler runs code **later** or **repeatedly**, on the **game clock**. Game time stops when the game pauses, so timers stop too. You never need to count frames or read the wall clock.

## `after` and `every`

A task is a function of a `TaskContext`. Data goes in a [payload](#payloads).

```cpp
void spawnEnemy(oly::app::TaskContext& context) {
  context.spawn(Enemy{}, Position{0.0f, 0.0f});
}

void spawnWave(oly::app::TaskContext& context) {
  // spawn several enemies
}

OLY_SYSTEM(startRound) {
  context.schedule().after(2.0, spawnEnemy);   // once, 2 seconds from now
  context.schedule().every(5.0, spawnWave);    // every 5 seconds; the first run is 5 seconds from now
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "startRound", startRound);
engine.schedule().after(2.0, spawnEnemy);
```

**On the web**, the client schedules from TS: [TypeScript](../environments/web/typescript/schedule.md#after-and-every).

The same scheduler is `engine.schedule()` and `context.schedule()` in every system, handler, task and hook.

| Call | Runs | Rule |
|---|---|---|
| `after(delay, function)` | once, at `time + delay` | `delay ≥ 0` and finite |
| `every(interval, function)` | at `time + interval`, then every `interval` | `interval > 0` and finite |

Due tasks run at **step 2** of the fixed step (after input capture, before the phases), in due-time order. `every` does not drift: the 10th run is at exactly `10 × interval`, and a task fires at most once per step.

## The task context

The function receives a `TaskContext`. Tasks run outside any iteration, so structural changes are **direct**, like in handlers.

| Member | What it is |
|---|---|
| `handle()` | the `TaskHandle` of this task |
| `lastRun()` | `true` on the run of an `after` (it will not run again) |
| `payload<P>()` | the payload the task was scheduled with |
| `spawn`, `destroy`, `add`, `remove`, `persist` | direct changes |
| `query`, `get`, `tryGet`, `has`, `isAlive`, `emit`, `actions`, `schedule`, `state`, `requestState`, `time`... | as in systems |

## Cancel and check

```cpp
oly::app::TaskHandle handle = context.schedule().every(1.0, tick);

context.schedule().isPending(handle);   // true while it may still run
context.schedule().cancel(handle);      // true if it was pending; false if it already ended or was cancelled
```

A task can cancel itself:

```cpp
void tick(oly::app::TaskContext& context) {
  Countdown& countdown = std::get<1>(context.query<Countdown>().single());
  countdown.remaining -= 1;

  if (countdown.remaining == 0) {
    context.schedule().cancel(context.handle());
  }
}
```

## Payloads

A payload is a trivially copyable struct of up to 32 bytes (alignment up to 8), copied into the task. It is the way a task carries the values it needs.

```cpp
struct PlayerTarget {
  oly::ecs::Entity player;
};

void endDash(oly::app::TaskContext& context) {
  const oly::ecs::Entity player = context.payload<PlayerTarget>().player;

  if (context.isAlive(player)) {
    context.get<Dash>(player).active = 0;
  }
}

OLY_SYSTEM(startDash) {
  auto [player, dash] = context.query<Dash>(oly::ecs::With<Player>{}).single();

  dash.active = 1;
  context.schedule().after(dash.duration, endDash, PlayerTarget{player});
}
```

If the task refers to an entity, check that it is still alive when the task runs, as `endDash` does. `payload<P>()` checks the type: asking for a different type than the one scheduled is a contract violation (`OLY_ASSERT`).

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "startDash", startDash);
```

**On the web**, the client uses a closure instead: [TypeScript](../environments/web/typescript/schedule.md#captured-values-closures).

## Discard callbacks

If the payload owns something (an index into your own pool, for example), pass a **discard** function. It runs once if the task leaves the queue without its last run: cancelled, cleared by a state change, or the engine destroyed.

```cpp
void releaseSlot(void* payload) {
  const SlotPayload* slot = static_cast<const SlotPayload*>(payload);
  pool.release(slot->index);
}

context.schedule().after(5.0, useSlot, SlotPayload{index}, releaseSlot);
```

On the last run of an `after`, the discard does **not** run; release the resource inside the task when `context.lastRun()` is true.

## Pause, states and recovery

- **Pause**: tasks do not run and the clock does not move. A 2-second `after` scheduled before a 10-second pause still fires 2 game seconds later.
- **States**: a task created while a state is active is **scoped** to it. A state change cancels it, running its discard. This is what makes "play again" safe: the old wave timer does not keep spawning in the menu. Use `schedule().persist(handle)` to keep a task across state changes. See [States](states.md#scope-automatic-clean-up).
- **Created during step 2**: a task scheduled by another task runs from the next step on. `after(0.0, ...)` inside a task is not a loop.
- **Recovery**: scheduled tasks survive an interrupted frame ([Engine, loop and systems](engine-and-systems.md#when-a-system-throws)).

## Next

[States →](states.md): menus, rounds and game over with automatic clean-up.

In other clients: [TypeScript](../environments/web/typescript/schedule.md).
