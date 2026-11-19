# Schedule in C++

[Reference](../../README.md#reference) · Concept: [Schedule](../../explanation/concepts/schedule.md) · TypeScript: [Schedule in TypeScript](../typescript/schedule.md)

Headers: `oly/app/scheduler.hpp` for `Scheduler`, `oly/app/task.hpp` for `TaskHandle`, `TaskFunction` and the payload limits, and `oly/app/task_context.hpp` for `TaskContext`. When tasks run and how pause and states affect them: the [concept page](../../explanation/concepts/schedule.md). The scheduler is `engine.schedule()` and `context.schedule()`.

## `after` and `every`

### `Scheduler::after`

```cpp
TaskHandle after(double delaySeconds, TaskFunction function);
template <class P>
TaskHandle after(double delaySeconds, TaskFunction function, const P& payload, TaskDiscardFunction discard = nullptr);
```

Runs `function` once, `delaySeconds` of game time from now, and returns its `TaskHandle`. `TaskFunction` is `void (*)(TaskContext& context)`. The delay must be finite and not negative: otherwise the call stops with `OLY_ASSERT` (`schedule: delay=<delay> negative or not finite`). The overload with a payload copies `payload` into the task ([Payloads](#payloads)).

```cpp
void spawnEnemy(oly::app::TaskContext& context) {
  context.spawn(Enemy{}, Position{0.0f, 0.0f});
}

OLY_SYSTEM(startRound) {
  context.schedule().after(2.0, spawnEnemy);   // once, 2 seconds from now
}
```

### `Scheduler::every`

```cpp
TaskHandle every(double intervalSeconds, TaskFunction function);
template <class P>
TaskHandle every(double intervalSeconds, TaskFunction function, const P& payload, TaskDiscardFunction discard = nullptr);
```

Runs `function` every `intervalSeconds`, the first run `intervalSeconds` from now. The interval must be positive and finite: otherwise the call stops with `OLY_ASSERT` (`every: interval=<interval> must be positive and finite`).

```cpp
void spawnWave(oly::app::TaskContext& context) {
  // spawn several enemies
}

OLY_SYSTEM(startWaves) {
  context.schedule().every(5.0, spawnWave);    // every 5 seconds; the first run is 5 seconds from now
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "startRound", startRound);
engine.schedule().after(2.0, spawnEnemy);
```

**On the web**, the client schedules from TS: [TypeScript](../typescript/schedule.md#after-and-every).

### `Scheduler::schedule`

```cpp
TaskHandle schedule(double delaySeconds, double intervalSeconds, TaskFunction function, const TaskPayload& payload = {}, TaskDiscardFunction discard = nullptr);
```

The base of `after`, `every` and the C API. An `intervalSeconds` of `0` schedules an `after`; a positive one schedules an `every` whose first run is at `time() + delaySeconds`. The delay and the interval must be finite and not negative, and the function cannot be null: the call stops with `OLY_ASSERT` (`schedule: delay=<delay> negative or not finite`, `schedule: interval=<interval> negative or not finite` or `schedule: null function`). Use it when the delay of the first run differs from the interval, or when you build the `TaskPayload` yourself with `TaskPayload::of(value)`.

```cpp
context.schedule().schedule(1.0, 5.0, spawnWave);   // the first run in 1 second, then every 5 seconds
```

## The task context

A task receives a `TaskContext`, a direct context ([Direct and deferred changes](../../explanation/concepts/world-and-entities.md#direct-and-deferred-changes)). It is valid only during the call.

| Member | What it is |
|---|---|
| `handle()` | the `TaskHandle` of this task |
| `lastRun()` | `true` on the run of an `after` (it will not run again) |
| `payload<P>()` | the payload the task was scheduled with |
| `spawn`, `destroy`, `add`, `remove`, `persist` | direct changes |
| `query`, `get`, `tryGet`, `has`, `isAlive`, `emit`, `actions`, `schedule`, `state`, `requestState`, `time`... | as in systems |

### `TaskContext::payload`

```cpp
template <class P>
const P& payload() const;
```

Returns the payload as `P`. `P` must be the type given when the task was scheduled: otherwise the call stops with `OLY_ASSERT` (`payload: type "<P>" differs from the scheduled "<scheduled>"`; a task scheduled without a payload shows `(none)`).

## Cancel and check

### `Scheduler::cancel`

```cpp
bool cancel(TaskHandle task);
bool isPending(TaskHandle task) const;
bool persist(TaskHandle task);
uint32_t pendingCount() const noexcept;
```

`cancel` removes the task and runs its discard, and returns `true` if the task was pending, `false` if it already ended or was cancelled. `isPending` is `true` while the task may still run. Both return `false` for `TaskHandle{}`, which references no task, and for an `after` that is running or has run. `persist` removes the scope of a pending task, so a state change does not cancel it ([Scope: automatic clean-up](../../explanation/concepts/states.md#scope-automatic-clean-up)); it returns `false` if the task is not pending. A handle with a slot out of range stops with `OLY_ASSERT` (`cancel: slot=<slot> out of range (<n> slots)`, and the same with `isPending` and `persist`).

`pendingCount()` is the number of tasks that may still run: every `every` that is not cancelled and every `after` that has not run. Use it in a test to check that a round left nothing behind.

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

A payload is a struct that is trivially copyable and no larger than `MaxTaskPayloadSize` bytes, with alignment no larger than `MaxTaskPayloadAlignment`. It is copied into the task.

### `MaxTaskPayloadSize`

```cpp
inline constexpr std::size_t MaxTaskPayloadSize = 32;
inline constexpr std::size_t MaxTaskPayloadAlignment = 8;
```

`MaxTaskPayloadSize` is 32 bytes and `MaxTaskPayloadAlignment` is 8. A payload that breaks a limit does not compile (`TaskPayload: payload larger than 32 bytes`, `TaskPayload: payload alignment larger than 8` or `TaskPayload: the payload must be trivially copyable`).

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

If the task refers to an entity, check that it is still alive when the task runs, as `endDash` does.

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "startDash", startDash);
```

**On the web**, the client uses a closure instead: [TypeScript](../typescript/schedule.md#captured-values-closures).

## Discard callbacks

### `TaskDiscardFunction`

```cpp
using TaskDiscardFunction = void (*)(void* payload);
```

The last parameter of `after` and `every` with a payload. It runs once with the payload when the task leaves the queue without its last run: cancelled, cleared by a state change, or the engine destroyed. It never runs after the last run of an `after`: release the resource inside the task when `context.lastRun()` is `true`.

```cpp
void releaseSlot(void* payload) {
  const SlotPayload* slot = static_cast<const SlotPayload*>(payload);
  pool.release(slot->index);
}

context.schedule().after(5.0, useSlot, SlotPayload{index}, releaseSlot);
```
