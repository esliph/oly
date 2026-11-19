# Events in C++

[Reference](../../README.md#reference) · Concept: [Events](../../explanation/concepts/events.md) · TypeScript: [Events in TypeScript](../typescript/events.md)

Headers: `oly/app/event.hpp`, `oly/app/handler.hpp`, `oly/app/engine.hpp` and `oly/app/context.hpp`. When events are dispatched, in what order and what rounds are: the [concept page](../../explanation/concepts/events.md).

## Declare, emit, handle

An event is a trivially copyable struct. The schema declares it (`event EnemyDied { entity: entity, x: f32, y: f32 }`) and the build generates the struct ([Schema in C++](schema.md)).

### `Engine::declareEvent`

```cpp
template <class T>
EventTypeId declareEvent(std::string_view name = oly::core::typeName<T>());
```

Declares the event type `T` and returns its `EventTypeId`. The declaration order is the dispatch order of the types. `setUpGame` declares the events of the schema, so a game does not call it. Declaring during `frame` stops with `OLY_ASSERT` (`declareEvent: event "<name>" declared during frame`), and so does declaring the same type twice (`declareEvent: event "<name>" already declared`).

### `context.emit`

```cpp
template <class T>
void emit(const T& event);
```

Emits `event` from a system, handler, task or hook. It is dispatched in the rounds of the current step, as the [dispatch table](../../explanation/concepts/events.md#when-events-are-dispatched) says. An event type that is not declared stops with `OLY_ASSERT` (`emit: event "<T>" not declared`). From the code that owns the `Engine`: [Engine::emit](engine-and-systems.md#engineemit).

```cpp
OLY_SYSTEM(killDead) {
  context.query<const Health, const Position>(oly::ecs::With<Enemy>{}).each([&](oly::ecs::Entity entity, const Health& health, const Position& position) {
    if (health.value <= 0) {
      context.emit(EnemyDied{entity, position.x, position.y});
    }
  });
}
```

### `OLY_HANDLER`

```cpp
OLY_HANDLER(name, EventType)
```

Defines `void name(HandlerContext& context, std::span<const EventType> events)` and registers it by name, so every environment can find it ([C++ systems, handlers and hooks](engine-and-systems.md#c-systems-handlers-and-hooks)).

```cpp
OLY_HANDLER(countKill, EnemyDied) {
  Score& score = std::get<1>(context.query<Score>().single());
  score.kills += static_cast<int32_t>(events.size());
}
```

### `Engine::on`

```cpp
template <class T>
HandlerId on(std::string_view name, HandlerFunction function, void* userData = nullptr);
```

Registers `function` as a handler of the event type `T` and returns its `HandlerId`. `HandlerFunction` is a function of the context alone, so the registration of an `OLY_HANDLER` is a captureless lambda that passes the batch in. The handler reads `userData` with `context.userData()`. Handlers of one type run in registration order, once per round, with the whole batch. Register everything at start-up: a call during `frame` stops with `OLY_ASSERT` (`on: handler "<name>" registered during frame`), and a type that is not declared stops with `on: handler "<name>" for event "<T>" not declared`. A chain of rounds longer than `maxEventRounds` stops with `dispatchEvents: limit of <n> event rounds exceeded; pending: <events>`.

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "killDead", killDead);
engine.on<EnemyDied>("countKill", [](oly::app::HandlerContext& context) { countKill(context, context.events<EnemyDied>()); });
```

**On the web**, the client registers it by name: [TypeScript](../typescript/events.md#declare-emit-handle).

## Handlers receive batches

The batch is a `std::span<const EventType>` over the event queue. It is valid only during the handler call.

```cpp
OLY_HANDLER(spawnCorpses, EnemyDied) {
  for (const EnemyDied& died : events) {
    context.spawn(Corpse{}, Position{died.x, died.y});
  }
}
```

## The handler context

A handler receives a `HandlerContext`, a direct context ([Direct and deferred changes](../../explanation/concepts/world-and-entities.md#direct-and-deferred-changes)). It is valid only during the call.

### `HandlerContext::events`

```cpp
template <class T>
std::span<const T> events() const;
uint32_t eventCount() const noexcept;
const void* eventData() const noexcept;
```

`events<T>()` returns the batch of the handler's type. `T` must be that type: otherwise the call stops with `OLY_ASSERT` (`events: type "<T>" is not the event type of handler "<name>"`). The batch does not change while the handlers of that type run. `eventCount()` is its length and `eventData()` points to its first event, as `const void*`. Use `eventData()` when you do not know `T` at compile time; the layout is the one of the event struct.

### `HandlerContext::eventType`

```cpp
EventTypeId eventType() const noexcept;
HandlerId handlerId() const noexcept;
std::string_view name() const noexcept;
void* userData() const noexcept;
```

`eventType()` is the `EventTypeId` of the batch and `handlerId()` is the id that `on` returned for the running handler. `name()` is the name it was registered with, and `userData()` is the pointer given to `on`. Use them with a handler that serves several registrations, to know which one is running.

## Handlers change the world directly

The entity named in an event may already be dead ([why](../../explanation/concepts/events.md#handlers-change-the-world-directly)). Check it:

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

### Put the destroying handler last

**Where you own the `Engine`** (native):

```cpp
engine.on<EnemyDied>("dropCoin", [](oly::app::HandlerContext& context) { dropCoin(context, context.events<EnemyDied>()); });   // reads the position
engine.on<EnemyDied>("countKill", [](oly::app::HandlerContext& context) { countKill(context, context.events<EnemyDied>()); });
engine.on<EnemyDied>("destroyDead", [](oly::app::HandlerContext& context) { destroyDead(context, context.events<EnemyDied>()); });   // destroys: last
```

**On the web**, the client registers them by name: [TypeScript](../typescript/events.md#put-the-destroying-handler-last).

## Restrict handlers to states

`on` returns a `HandlerId`. A restricted handler is skipped when the active state is not in its list ([Run only in some states](states.md#run-only-in-some-states)).

### `Engine::setHandlerStates`

```cpp
void setHandlerStates(HandlerId handler, std::initializer_list<StateId> states);
```

Restricts the handler to the given states. An empty list removes the restriction. With no active state, a restricted handler does not run. Cannot be called during `frame`: it stops with `OLY_ASSERT` (`setHandlerStates: state restriction during frame`). A `handler` that `on` did not return stops with `setHandlerStates: handler=<id> out of range (<n> handlers)`, and a state that is not declared with `setHandlerStates: state=<id> out of range (<n> states)`.

```cpp
OLY_HANDLER(endRound, PlayerDied) {
  context.requestState(states::gameOver);
}

oly::app::HandlerId endRoundId = engine.on<PlayerDied>("endRound", [](oly::app::HandlerContext& context) { endRound(context, context.events<PlayerDied>()); });

engine.setHandlerStates(endRoundId, {states::playing});
```

**On the web**, the client uses `.inState(...)`: [TypeScript](../typescript/events.md#restrict-handlers-to-states).

### `Engine::setHandlerStateMask`

```cpp
void setHandlerStateMask(HandlerId handler, StateMask mask);
```

The same restriction, with the states as a `StateMask`: bit `n` is the state with `StateId` `n`, and `0` removes the restriction ([`Engine::setSystemStateMask`](engine-and-systems.md#enginesetsystemstatemask)). Stops with `OLY_ASSERT` (`setHandlerStates: mask 0x<mask> with undeclared state (<n> states)`) when the mask sets a bit of a state that is not declared.

```cpp
engine.setHandlerStateMask(endRoundId, (oly::app::StateMask{1} << states::menu) | (oly::app::StateMask{1} << states::playing));
```
