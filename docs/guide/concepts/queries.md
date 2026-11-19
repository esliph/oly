# Queries

[← World and entities](../../explanation/concepts/world-and-entities.md) · [Guide index](../README.md) · Next: [Engine, loop and systems →](engine-and-systems.md)

A query visits every entity that has a given set of components. It is the main way to run gameplay over many entities.

## The query spec

The component types are template parameters. `const T` is read-only, `T` is read and write. `With` and `Without` filter without giving you data, so they accept tags.

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

**On the web**, the client registers it by name: [TypeScript](../../reference/typescript/engine-and-systems.md#c-systems-handlers-and-hooks).

| Parameter | Gives you | Accepts tags? |
|---|---|---|
| `const T` in the template list | a read-only reference | no |
| `T` in the template list | a read-write reference | no |
| `With<T...>{}` | only filtering | yes |
| `Without<T...>{}` | only filtering | yes |

Both filters are optional. `With<Enemy>{}` visits every enemy and `Without<Frozen>{}` skips the frozen ones, whatever data you read.

Queries are **cached**. Asking for the same shape returns the same `Query` object, and the engine keeps its list of matching tables up to date as new archetypes appear. Calling `context.query` in a system every step is cheap.

## Iterate

Entities with the same components live together in a **table** (one per archetype). `each` calls your function once per entity. Add `oly::ecs::Entity` as the first parameter when you need the handle:

```cpp
context.query<const Health>().each([&](oly::ecs::Entity entity, const Health& health) {
  if (health.value <= 0) {
    context.destroy(entity);
  }
});
```

`eachBlock` calls your function once per table, with a `std::span` per component. Use it for whole-column loops the compiler can vectorize:

```cpp
context.query<Position, const Velocity>().eachBlock([](std::span<Position> positions, std::span<const Velocity> velocities) {
  for (size_t row = 0; row < positions.size(); ++row) {
    positions[row].x += velocities[row].x;
  }
});
```

Both read and write the stored columns directly: there is no copy.

## Rules while iterating

- References and spans are valid **only inside the callback**. Do not keep them for later.
- A direct structural change on the `World` (`spawn`, `destroy`, `add`, `remove`) while a query iterates is a contract violation (`OLY_ASSERT`). Inside a system, use `context.spawn/destroy/add/remove`: they are deferred to the end of the phase and safe. Outside the loop, collect the entities first and change them after the iteration, or use a `CommandBuffer` ([World and entities](../../reference/cpp/world-and-entities.md#commandbuffer)).

```cpp
OLY_SYSTEM(removeDead) {
  context.query<const Health>().each([&](oly::ecs::Entity entity, const Health& health) {
    if (health.value <= 0) {
      context.destroy(entity);
    }
  });
}
```

## Exactly one: `single` and `trySingle`

Many games have one player, one camera or one boss. `single` reads it without a loop.

```cpp
OLY_SYSTEM(steerPlayer) {
  auto [player, velocity] = context.query<Velocity>(oly::ecs::With<Player>{}).single();
  const oly::input::Vector2 direction = context.actions().axis2d(actions::move);

  velocity = Velocity{direction.x * 220.0f, direction.y * 220.0f};
}
```

| Method | 0 rows | 1 row | 2+ rows |
|---|---|---|---|
| `single()` | contract violation | the row | contract violation |
| `trySingle()` | `std::nullopt` | the row | contract violation |

The row is a tuple `(Entity, Ts&...)` of **references**, valid until the next structural change. Copy the value when you only need it for reading:

```cpp
const Position target = std::get<1>(context.query<const Position>(oly::ecs::With<Player>{}).single());
```

Use `trySingle` where the entity may not exist yet, for example in a HUD drawn while in the menu:

```cpp
OLY_SYSTEM(drawHealth) {
  auto player = context.query<const Health>(oly::ecs::With<Player>{}).trySingle();
  if (!player) {
    return;
  }

  const int32_t value = std::get<1>(*player).value;
}
```

## Common query patterns

### Count entities

Count rows per table, not entity by entity:

```cpp
size_t countEnemies(oly::app::SystemContext& context) {
  size_t total = 0;

  context.query<const Health>(oly::ecs::With<Enemy>{}).eachBlock([&total](std::span<const Health> healths) {
    total += healths.size();
  });

  return total;
}
```

### Find the nearest entity

```cpp
oly::ecs::Entity nearestEnemy(oly::app::SystemContext& context, float x, float y) {
  oly::ecs::Entity nearest = oly::ecs::NullEntity;
  float nearestDistance = std::numeric_limits<float>::infinity();

  context.query<const Position>(oly::ecs::With<Enemy>{}).each([&](oly::ecs::Entity entity, const Position& position) {
    const float deltaX = position.x - x;
    const float deltaY = position.y - y;
    const float distance = deltaX * deltaX + deltaY * deltaY;

    if (distance < nearestDistance) {
      nearestDistance = distance;
      nearest = entity;
    }
  });

  return nearest;
}
```

### Two queries together (pairs)

Read the small set into a local value first, then iterate the large set. Here, enemies chase the player:

```cpp
OLY_SYSTEM(chasePlayer) {
  const Position target = std::get<1>(context.query<const Position>(oly::ecs::With<Player>{}).single());

  context.query<const Position, const Chaser, Velocity>(oly::ecs::With<Enemy>{}).each([target](const Position& position, const Chaser& chaser, Velocity& velocity) {
    const float deltaX = target.x - position.x;
    const float deltaY = target.y - position.y;
    const float length = std::max(std::sqrt(deltaX * deltaX + deltaY * deltaY), 1e-6f);

    velocity = Velocity{deltaX / length * chaser.speed, deltaY / length * chaser.speed};
  });
}
```

For "each against each" (bullets × enemies), take the inner query reference once, then nest the loops. Changes go through the context, so they are safe inside both loops.

```cpp
OLY_SYSTEM(hitEnemies) {
  auto& enemies = context.query<const Position, const Collider, Health>(oly::ecs::With<Enemy>{});

  context.query<const Position, const Collider>(oly::ecs::With<Bullet>{}).each([&](oly::ecs::Entity bullet, const Position& bulletPosition, const Collider& bulletCollider) {
    bool hit = false;

    enemies.each([&](oly::ecs::Entity enemy, const Position& enemyPosition, const Collider& enemyCollider, Health& health) {
      if (hit || !touches(bulletPosition, bulletCollider, enemyPosition, enemyCollider)) {
        return;
      }

      hit = true;
      health.value -= 1;
      context.destroy(bullet);

      if (health.value <= 0) {
        context.emit(EnemyDied{enemy, enemyPosition.x, enemyPosition.y});
      }
    });
  });
}
```

This is a brute-force O(bullets × enemies) check. It is fine for hundreds of entities. For thousands, add a spatial grid.

### Several shapes with one function

Pass the filter as a parameter. One function draws coins, bullets, enemies and the player:

```cpp
template <class... Filters>
void drawCircles(oly::app::SystemContext& context, Drawer& drawer, Color color, Filters... filters) {
  context.query<const Position, const Collider>(filters...).each([&](const Position& position, const Collider& collider) {
    drawer.circle(position.x, position.y, collider.radius, color);
  });
}

drawCircles(context, drawer, red, oly::ecs::With<Enemy>{});
drawCircles(context, drawer, blue, oly::ecs::With<Player>{}, oly::ecs::Without<Invulnerable>{});
```

## Next

[Engine, loop and systems →](engine-and-systems.md): run queries every step inside systems.

In other clients: [TypeScript](../../reference/typescript/queries.md).
