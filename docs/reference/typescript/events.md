# Events in TypeScript

[TypeScript client](README.md) · Concept: [Events](../../explanation/concepts/events.md) · C++: [Events in C++](../cpp/events.md)

When events are dispatched, in what order, and what rounds are: see the [concept page](../../explanation/concepts/events.md). This page shows the TS calls.

## Declare, emit, handle

```ts
// a system emits
if (health.value.get(row) <= 0) {
  context.emit(EnemyDied, { entity: block.entity(row), x: position.x.get(row), y: position.y.get(row) });
}
```

```ts
// a handler reacts
engine.on(EnemyDied, "countKill", (context, batch) => {
  score.kills += batch.count;
});
```

Missing fields in `emit` are `0`. A handler written in C++ is registered with `engine.addNativeHandler(EnemyDied, "name")` ([C++ systems, handlers and hooks](engine-and-systems.md#c-systems-handlers-and-hooks)).

## Handlers receive batches

```ts
engine.on(EnemyDied, "spawnCorpses", (context, batch) => {
  const events = batch.read();

  for (let index = 0; index < batch.count; ++index) {
    context.spawn([Corpse, Position], { Position: { x: events.x.get(index), y: events.y.get(index) } });
  }
});
```

`batch.read()` returns views over the event queue in WASM memory, with no copy. Like query views, they are valid only during the handler call.

## Handlers change the world directly

The entity named in an event may already be dead ([why](../../explanation/concepts/events.md#handlers-change-the-world-directly)). Check it:

```ts
engine.on(CoinCollected, "collectCoin", (context, batch) => {
  const coins = batch.read().coin;

  for (let index = 0; index < batch.count; ++index) {
    const coin = coins.get(index);
    if (context.isAlive(coin)) {
      context.destroy(coin);
      score.coins += 1;
    }
  }
});
```

## The engine does not remove duplicates

```ts
engine.on(EnemyDied, "countKill", (_context, batch) => {
  const enemies = batch.read().entity;
  const counted = new Set<Entity>();

  for (let index = 0; index < batch.count; ++index) {
    const enemy = enemies.get(index);
    if (!counted.has(enemy)) {
      counted.add(enemy);
      score.kills += 1;
    }
  }
});
```

## Order

### Put the destroying handler last

```ts
engine.addNativeHandler(EnemyDied, "dropCoin");                          // reads the position
engine.on(EnemyDied, "countKill", (_context, batch) => countKill(score, batch));
engine.addNativeHandler(EnemyDied, "destroyDead");                       // destroys: last
```

## Restrict handlers to states

```ts
engine.on(PlayerDied, "endRound", (context) => context.requestState(gameOver)).inState(playing);
```
