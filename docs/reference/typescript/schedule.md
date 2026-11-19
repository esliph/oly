# Schedule in TypeScript

[TypeScript client](README.md) · Concept: [Schedule](../../explanation/concepts/schedule.md) · C++: [Schedule in C++](../cpp/schedule.md)

When tasks run, how pause and states affect them and the rules of `after` and `every` are in the [concept page](../../explanation/concepts/schedule.md). This page shows the TS calls.

## `after` and `every`

```ts
// once, 2 seconds from now
engine.schedule.after(2, (task) => {
  task.spawn([Enemy, Position], { Position: { x: 0, y: 0 } });
});

// every 5 seconds; the first run is 5 seconds from now
engine.schedule.every(5, (task) => {
  spawnWave(task);
});
```

The same scheduler is available as `engine.schedule` and as `context.schedule` in every system, handler, task and hook.

## The task context

| Member | What it is |
|---|---|
| `handle` | the `TaskHandle` of this task |
| `lastRun` | `true` on the run of an `after` |
| `spawn`, `destroy`, `add`, `remove`, `persist` | direct changes |
| `query`, `get`, `set`, `has`, `isAlive`, `emit`, `actions`, `schedule`, `state`, `requestState`, `time`... | as in systems |

## Cancel and check

```ts
const handle = engine.schedule.every(1, (task) => {
  countdown -= 1;
});

engine.schedule.isPending(handle);   // true while it may still run
engine.schedule.cancel(handle);      // true if it was pending; false if it already ended or was cancelled
```

A task can cancel itself:

```ts
let remaining = 3;

engine.schedule.every(1, (task) => {
  remaining -= 1;
  if (remaining === 0) {
    task.schedule.cancel(task.handle);
  }
});
```

## Captured values: closures

In TS, the callback is a closure. Capture what the task needs, with no payload and no size limit:

```ts
function scheduleRespawn(context: HandlerContext, x: number, y: number): void {
  context.schedule.after(3, (task) => {
    task.spawn([Enemy, Position, Health], { Position: { x, y }, Health: { value: 1 } });
  });
}
```

If the task refers to an entity, check that it is still alive when the task runs:

```ts
function stunFor(context: SystemContext, enemy: Entity, seconds: number): void {
  context.add(enemy, Stunned);
  context.schedule.after(seconds, (task) => {
    if (task.isAlive(enemy)) {
      task.remove(enemy, Stunned);
    }
  });
}
```
