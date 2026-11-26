# Schedule

[← Input](input.md) · [Concepts](README.md) · Next: [States →](states.md)

The scheduler runs code **later** or **repeatedly**, on the **game clock**. Game time stops when the game pauses, so timers stop too. You never need to count frames or read the wall clock.

The same scheduler is `engine.schedule()`, `context.schedule()` in a handler, task or hook, and the `Tasks` parameter in a system.

## `after` and `every`

A task is a function of a `TaskContext`. Data goes in a [payload](#payloads-and-captured-values).

| Call | Runs | Rule |
|---|---|---|
| `after(delay, function)` | once, at `time + delay` | `delay ≥ 0` and finite |
| `every(interval, function)` | at `time + interval`, then every `interval` | `interval > 0` and finite |

The due time counts from the game clock: inside a step, `time` is the start of the step; outside `frame`, it is the start of the next one.

Due tasks run at **step 2** of the fixed step (after input capture, before the phases), in due-time order. Tasks with the same due time run in creation order. `every` does not drift: the 10th run is at exactly `10 × interval`, and a task fires at most once per step.

The calls: [`after` and `every`](../../reference/cpp/schedule.md#after-and-every).

TypeScript: [`after` and `every`](../../reference/typescript/schedule.md#after-and-every).

## The task context

The function receives a `TaskContext`. Tasks run outside any iteration, so structural changes are **direct**, like in handlers ([Direct and deferred changes](world-and-entities.md#direct-and-deferred-changes)). The task also reads the world, emits events and requests a state as a system does. It has its own `TaskHandle` and knows whether this is the last run.

The members: [The task context](../../reference/cpp/schedule.md#the-task-context).

TypeScript: [The task context](../../reference/typescript/schedule.md#the-task-context).

## Cancel and check

A task has a handle. The handle tells whether the task may still run and cancels it. A task can cancel itself. Cancelling an `every` is how it stops.

The calls: [Cancel and check](../../reference/cpp/schedule.md#cancel-and-check).

TypeScript: [Cancel and check](../../reference/typescript/schedule.md#cancel-and-check).

## Payloads and captured values

A task needs the values it works on. In C++ they travel in a **payload**: a small trivially copyable struct, copied into the task when you schedule it. The task reads it back with the same type. If the task refers to an entity, it checks that the entity is still alive when it runs: the entity may have died while the task waited.

If the payload owns something (an index into your own pool, for example), a **discard** function releases it. The discard runs once if the task leaves the queue without its last run: cancelled, cleared by a state change, or the engine destroyed. On the last run of an `after`, the discard does **not** run: the task releases the resource itself when `lastRun` is true.

The limits of a payload and the calls: [Payloads](../../reference/cpp/schedule.md#payloads) and [Discard callbacks](../../reference/cpp/schedule.md#discard-callbacks).

TypeScript: the callback is a closure, so there is no payload and no discard ([Captured values: closures](../../reference/typescript/schedule.md#captured-values-closures)).

## Pause, states and recovery

- **Pause**: tasks do not run and the clock does not move. A 2-second `after` scheduled before a 10-second pause still fires 2 game seconds later.
- **States**: a task created while a state is active is **scoped** to it. A state change cancels it, running its discard. This is what makes "play again" safe: the old wave timer does not keep spawning in the menu. `persist` keeps a task across state changes ([Scope: automatic clean-up](states.md#scope-automatic-clean-up)).
- **Created during step 2**: a task scheduled by another task runs from the next step on. `after(0.0, ...)` inside a task is not a loop.
- **Recovery**: scheduled tasks survive an interrupted frame ([Engine, loop and systems](engine-and-systems.md#when-a-system-throws)).

## API

- C++: [Schedule in C++](../../reference/cpp/schedule.md)
- TypeScript: [Schedule in TypeScript](../../reference/typescript/schedule.md)

## Next

[States](states.md): menus, rounds and game over with automatic clean-up.
