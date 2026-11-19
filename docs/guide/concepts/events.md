# Events

[← Engine, loop and systems](../../explanation/concepts/engine-and-systems.md) · [Guide index](../README.md) · Next: [Input →](input.md)

An event is a small message: "this enemy died here", "the player picked up this coin". The system that sees it happen **emits** it. Any number of **handlers** react to it later in the same step. The emitter does not know who listens.

Use events when one thing must trigger several independent reactions: an enemy death drops a coin, adds score, plays a sound and destroys the enemy.

## Declare, emit, handle

```
# game.oly
event EnemyDied { entity: entity, x: f32, y: f32 }
```

A system emits with the generated struct:

```cpp
OLY_SYSTEM(killDead) {
  context.query<const Health, const Position>(oly::ecs::With<Enemy>{}).each([&](oly::ecs::Entity entity, const Health& health, const Position& position) {
    if (health.value <= 0) {
      context.emit(EnemyDied{entity, position.x, position.y});
    }
  });
}
```

A handler reacts. `OLY_HANDLER(name, Event)` defines `void name(HandlerContext& context, std::span<const Event> events)`:

```cpp
OLY_HANDLER(countKill, EnemyDied) {
  Score& score = std::get<1>(context.query<Score>().single());
  score.kills += static_cast<int32_t>(events.size());
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "killDead", killDead);
engine.on<EnemyDied>("countKill", [](oly::app::HandlerContext& context) { countKill(context, context.events<EnemyDied>()); });
```

The macro's function takes the batch as a span, and `on<T>` takes a `HandlerFunction` (a function of the context alone), so the registration is a captureless lambda that passes the batch in.

**On the web**, the client registers it by name: [TypeScript](../../reference/typescript/events.md#declare-emit-handle).

Emitting an event that the schema does not declare is a contract violation (`OLY_ASSERT`).

## Handlers receive batches

A handler is called **once per batch**, not once per event. The batch holds every event of that type emitted since the last round, in emission order.

```cpp
OLY_HANDLER(spawnCorpses, EnemyDied) {
  for (const EnemyDied& died : events) {
    context.spawn(Corpse{}, Position{died.x, died.y});
  }
}
```

The span points into the event queue, with no copy. It is valid only during the handler call.

## Handlers change the world directly

Handlers run **after** the phases, outside any iteration. So their `spawn`, `destroy`, `add` and `remove` happen **at once** (a `HandlerContext` is a direct context).

Because of that, the entity named in an event may already be dead: another handler may have destroyed it, or two bullets may have reported the same death. Always check:

```cpp
OLY_HANDLER(collectCoin, CoinCollected) {
  Score& score = std::get<1>(context.query<Score>().single());

  for (const CoinCollected& collected : events) {
    if (context.isAlive(collected.coin)) {
      context.destroy(collected.coin);
      score.coins += 1;
    }
  }
}
```

## The engine does not remove duplicates

If two bullets hit the same enemy in one step, `EnemyDied` arrives twice. Decide in the handler what a duplicate means:

```cpp
OLY_HANDLER(countKillOnce, EnemyDied) {
  Score& score = std::get<1>(context.query<Score>().single());
  std::unordered_set<oly::ecs::Entity, oly::ecs::EntityHash> counted;

  for (const EnemyDied& died : events) {
    if (counted.insert(died.entity).second) {
      score.kills += 1;
    }
  }
}
```

## Order

- **Between types**: the order of the events in the schema ([Schema](../../explanation/concepts/schema.md#order-matters)). Every round handles the first event type, then the second, and so on.
- **Between handlers of one type**: registration order. Handlers written in different languages mix.

### Put the destroying handler last

Register the handlers that **read** the entity before the one that **destroys** it:

**Where you own the `Engine`** (native):

```cpp
engine.on<EnemyDied>("dropCoin", [](oly::app::HandlerContext& context) { dropCoin(context, context.events<EnemyDied>()); });   // reads the position
engine.on<EnemyDied>("countKill", [](oly::app::HandlerContext& context) { countKill(context, context.events<EnemyDied>()); });
engine.on<EnemyDied>("destroyDead", [](oly::app::HandlerContext& context) { destroyDead(context, context.events<EnemyDied>()); });   // destroys: last
```

**On the web**, the client registers them by name: [TypeScript](../../reference/typescript/events.md#order).

## Rounds: events that cause events

What a handler emits goes to the **next round of the same step**. Rounds repeat until no event is pending.

```cpp
OLY_HANDLER(chainExplosion, EnemyDied) {
  // nearby enemies take damage; some of them die and emit EnemyDied again (next round)
}
```

The number of rounds per step is limited by `maxEventRounds` (default 16, see [Engine, loop and systems](../../reference/cpp/engine-and-systems.md#create-the-engine)). Reaching it means an event loop (A emits B, B emits A): a contract violation. Raise the limit only if your chain is legitimately deep.

## When events are dispatched

| Emitted from | Dispatched |
|---|---|
| a system (any gameplay phase) | at step 4 of the same step, after all phases |
| a handler | next round, same step |
| a task or a state hook | in the rounds of the same step |
| Render or outside `frame` | in the next step |

An event nobody handles is discarded at the end of the step.

## Restrict handlers to states

`on` returns a `HandlerId`. Give it to `setHandlerStates`:

```cpp
OLY_HANDLER(endRound, PlayerDied) {
  context.requestState(states::gameOver);
}
```

**Where you own the `Engine`** (native):

```cpp
oly::app::HandlerId endRoundId = engine.on<PlayerDied>("endRound", [](oly::app::HandlerContext& context) { endRound(context, context.events<PlayerDied>()); });

engine.setHandlerStates(endRoundId, {states::playing});
```

**On the web**, the client registers it by name: [TypeScript](../../reference/typescript/events.md#restrict-handlers-to-states).

With an active state that is not in the list, the handler is skipped.

## Events vs. other tools

| You want | Use |
|---|---|
| several reactions to one fact, now | an event |
| something to happen after N seconds | the [schedule](schedule.md) |
| to keep data on an entity across steps | a component |
| to mark entities for another system to process | a tag (`context.add(entity, Hit{})`), then a query `With<Hit>` |
| to change screens | a [state](states.md) |

## Next

[Input →](input.md): turn keys and mouse into actions.

In other clients: [TypeScript](../../reference/typescript/events.md).
