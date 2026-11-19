# Queries in C++

[Reference](../../README.md#reference) · Concept: [Queries](../../explanation/concepts/queries.md) · TypeScript: [Queries in TypeScript](../typescript/queries.md)

Headers: `oly/ecs/query.hpp` for `Query`, `With` and `Without`, `oly/ecs/world.hpp` for `World::query`, and `oly/app/context.hpp` for the context methods. What you may do while a query iterates: [Rules while iterating](../../explanation/concepts/queries.md#rules-while-iterating). Ready patterns: [Gameplay recipes](../../how-to/gameplay-recipes.md#count-entities).

## The query spec

### `World::query`

```cpp
template <class... Ts, class... Filters>
Query<Ts...>& query(Filters... filters);
```

`EngineContext::query` has the same signature and forwards to the `World`, so `context.query` works in every context. The component types are template parameters: `const T` is read-only and `T` is read and write. A tag in the list is a compile error (`tag in Query<Ts...> is not allowed; use With<Tag>/Without<Tag> (R-QY-2)`), and so is a reference or `volatile` type (`Query<Ts...> accepts only T or const T`).

Queries are **cached**. Asking for the same `Ts` and filters returns the same `Query` object, and the engine keeps its list of matching tables up to date as new archetypes appear. Calling `context.query` in a system every step is cheap. The `Query` is valid while its `World` exists.

```cpp
OLY_SYSTEM(integrate) {
  const float deltaTime = context.fixedDeltaTime();

  context.query<Position, const Velocity>(oly::ecs::With<Enemy>{}, oly::ecs::Without<Frozen>{}).each([deltaTime](Position& position, const Velocity& velocity) {
    position.x += velocity.x * deltaTime;
    position.y += velocity.y * deltaTime;
  });
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "integrate", integrate);
```

**On the web**, the client registers it by name: [TypeScript](../typescript/engine-and-systems.md#c-systems-handlers-and-hooks).

### `With` and `Without`

```cpp
template <class... Us>
struct With {};
template <class... Us>
struct Without {};
```

They filter without giving you data, so they accept tags. `With<Enemy>{}` visits every entity that has `Enemy` and `Without<Frozen>{}` skips the frozen ones, whatever data you read. Both are optional.

| Parameter | Gives you | Accepts tags? |
|---|---|---|
| `const T` in the template list | a read-only reference | no |
| `T` in the template list | a read-write reference | no |
| `With<T...>{}` | only filtering | yes |
| `Without<T...>{}` | only filtering | yes |

## Iterate

Entities with the same components live together in a **table** (one per archetype).

### `Query::each`

```cpp
template <class F>
void each(F&& callback);
```

Calls `callback(Ts&...)` or `callback(Entity, Ts&...)` once per entity. Add `oly::ecs::Entity` as the first parameter when you need the handle. While it runs, the `World` counts as iterating.

```cpp
context.query<const Health>().each([&](oly::ecs::Entity entity, const Health& health) {
  if (health.value <= 0) {
    context.destroy(entity);
  }
});
```

### `Query::eachBlock`

```cpp
template <class F>
void eachBlock(F&& callback);
```

Calls `callback(std::span<Ts>...)` once per table that has rows, with a span per component (`std::span<const T>` for `const T`). Use it for whole-column loops the compiler can vectorize. `each` and `eachBlock` read and write the stored columns directly: there is no copy.

```cpp
context.query<Position, const Velocity>().eachBlock([](std::span<Position> positions, std::span<const Velocity> velocities) {
  for (size_t row = 0; row < positions.size(); ++row) {
    positions[row].x += velocities[row].x;
  }
});
```

## Exactly one: `single` and `trySingle`

Many games have one player, one camera or one boss. `single` reads it without a loop.

| Method | 0 rows | 1 row | 2+ rows |
|---|---|---|---|
| `single()` | contract violation | the row | contract violation |
| `trySingle()` | `std::nullopt` | the row | contract violation |

### `Query::single`

```cpp
std::tuple<Entity, Ts&...> single();
```

Returns the only row as a tuple of the `Entity` and **references**, valid until the next structural change. Stops with `OLY_ASSERT` (`single: <Query type> with <n> rows; expected 1`) unless the query has exactly one row. Copy the value when you only need it for reading.

```cpp
OLY_SYSTEM(steerPlayer) {
  auto [player, velocity] = context.query<Velocity>(oly::ecs::With<Player>{}).single();
  const oly::input::Vector2 direction = context.actions().axis2d(actions::move);

  velocity = Velocity{direction.x * 220.0f, direction.y * 220.0f};
}
```

```cpp
const Position target = std::get<1>(context.query<const Position>(oly::ecs::With<Player>{}).single());
```

### `Query::trySingle`

```cpp
std::optional<std::tuple<Entity, Ts&...>> trySingle();
```

Like `single`, but returns `std::nullopt` when the query has no row. Stops with `OLY_ASSERT` (`trySingle: <Query type> with <n> rows; expected 0 or 1`) when it has more than one. Use it where the entity may not exist yet, for example in a HUD drawn while in the menu.

```cpp
OLY_SYSTEM(drawHealth) {
  auto player = context.query<const Health>(oly::ecs::With<Player>{}).trySingle();
  if (!player) {
    return;
  }

  const int32_t value = std::get<1>(*player).value;
}
```
