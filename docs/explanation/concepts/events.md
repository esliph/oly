# Events

[← Engine, loop and systems](engine-and-systems.md) · [Concepts](README.md) · Next: [Input →](input.md)

An event is a small message: "this enemy died here", "the player picked up this coin". The system that sees it happen **emits** it. Any number of **handlers** react to it later in the same step. The emitter does not know who listens.

Use events when one thing must trigger several independent reactions: an enemy death drops a coin, adds score, plays a sound and destroys the enemy. An event type is declared in the schema (`event EnemyDied { entity: entity, x: f32, y: f32 }`, see [Schema](schema.md)). How to emit and handle it: [Declare, emit, handle](../../reference/cpp/events.md#declare-emit-handle).

TypeScript: [Declare, emit, handle](../../reference/typescript/events.md#declare-emit-handle).

## Handlers receive batches

A handler is called **once per batch**, not once per event. The batch holds every event of that type emitted since the last round, in emission order. The batch points into the event queue, with no copy, and is valid only during the handler call.

The calls: [Handlers receive batches](../../reference/cpp/events.md#handlers-receive-batches).

TypeScript: [Handlers receive batches](../../reference/typescript/events.md#handlers-receive-batches).

## Handlers change the world directly

Handlers run **after** the phases, outside any iteration. So their `spawn`, `destroy`, `add` and `remove` happen **at once**: a `HandlerContext` is a direct context ([Direct and deferred changes](world-and-entities.md#direct-and-deferred-changes)).

Because of that, the entity named in an event may already be dead: another handler may have destroyed it, or two bullets may have reported the same death. A handler checks `isAlive` before it uses the entity.

The calls: [Handlers change the world directly](../../reference/cpp/events.md#handlers-change-the-world-directly).

TypeScript: [Handlers change the world directly](../../reference/typescript/events.md#handlers-change-the-world-directly).

## The engine does not remove duplicates

If two bullets hit the same enemy in one step, `EnemyDied` arrives twice. The engine delivers both. The handler decides what a duplicate means: count it once, or count it twice.

The calls: [The engine does not remove duplicates](../../reference/cpp/events.md#the-engine-does-not-remove-duplicates).

TypeScript: [The engine does not remove duplicates](../../reference/typescript/events.md#the-engine-does-not-remove-duplicates).

## Order

- **Between types**: the order of the events in the schema ([Order matters](schema.md#order-matters)). Every round handles the first event type, then the second, and so on.
- **Between handlers of one type**: registration order. Handlers written in different languages mix.

### Put the destroying handler last

Register the handlers that **read** the entity before the one that **destroys** it. The handlers of one type see the same batch, so the one that destroys the entity makes it dead for every handler after it.

The calls: [Put the destroying handler last](../../reference/cpp/events.md#put-the-destroying-handler-last).

TypeScript: [Put the destroying handler last](../../reference/typescript/events.md#put-the-destroying-handler-last).

## Rounds: events that cause events

What a handler emits goes to the **next round of the same step**. Rounds repeat until no event is pending.

The number of rounds per step is limited by `maxEventRounds` (default 16, see [Engine](../../reference/cpp/engine-and-systems.md#create-the-engine)). Reaching it means an event loop (A emits B, B emits A): a contract violation. Raise the limit only if your chain is legitimately deep.

## When events are dispatched

| Emitted from | Dispatched |
|---|---|
| a system (any gameplay phase) | at step 4 of the same step, after all phases |
| a handler | next round, same step |
| a task or a state hook | in the rounds of the same step |
| Render or outside `frame` | in the next step |

An event nobody handles is discarded at the end of the step.

## Handlers and states

A handler can be restricted to some [states](../../guide/concepts/states.md): with an active state that is not in its list, the handler is skipped. The calls: [Restrict handlers to states](../../reference/cpp/events.md#restrict-handlers-to-states).

TypeScript: [Restrict handlers to states](../../reference/typescript/events.md#restrict-handlers-to-states).

## Events and other tools

| You want | Use |
|---|---|
| several reactions to one fact, now | an event |
| something to happen after N seconds | the [schedule](schedule.md) |
| to keep data on an entity across steps | a component |
| to mark entities for another system to process | a tag (`context.add(entity, Hit{})`), then a query `With<Hit>` |
| to change screens | a [state](../../guide/concepts/states.md) |

## API

- C++: [Events in C++](../../reference/cpp/events.md)
- TypeScript: [Events in TypeScript](../../reference/typescript/events.md)

## Next

[Input](input.md): turn keys and mouse into actions.
