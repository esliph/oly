# World and entities

[← Schema](schema.md) · [Guide index](../README.md) · Next: [Queries →](queries.md)

The `World` holds all entities and their components. An **entity** is only an id. What it *is* comes from the components it has.

## Where the World comes from

The `Engine` does not own the `World`: you give it one. Inside a system, handler, task or hook you never touch the `World` directly; the context gives you the same operations.

| Situation | How you get a World | Who owns it |
|---|---|---|
| inside a system, handler, task or hook | the `context` methods | the code that created the `Engine` |
| where you own the `Engine` (the game's `main`, a test) | `oly::ecs::World world;` declared **before** the `Engine`, then `engine.world()` | you |
| ECS only, no loop (tools, benchmarks) | `oly::ecs::World world;` | you |

**Where you own the `Engine`** (native):

```cpp
oly::ecs::World world;
oly::app::Engine engine(world);
```

On the web the host creates the `World` and the `Engine` for you ([TypeScript](../environments/web/typescript/world-and-entities.md#where-the-world-comes-from)).

## Create entities

`spawn` takes the initial components. A component you do not list is not on the entity; one you list with `{}` starts with every field at `0`.

```cpp
OLY_SYSTEM(spawnWave) {
  oly::ecs::Entity rock = context.spawn(Position{});
  oly::ecs::Entity marker = context.spawn();
  oly::ecs::Entity enemy = context.spawn(Enemy{}, Position{100.0f, 50.0f}, Health{3});
}
```

Tags go in the list as empty structs. The entity is born directly in its final archetype, with no intermediate moves.

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "spawnWave", spawnWave);

oly::ecs::Entity player = engine.world().spawn(Player{}, Position{100.0f, 50.0f}, Velocity{0.0f, 0.0f}, Health{3});
```

**On the web**, the client registers it by name: [TypeScript](../environments/web/typescript/engine-and-systems.md#c-systems-handlers-and-hooks).

## Read and write values

`get` returns a reference to the stored component. Write through it:

```cpp
OLY_SYSTEM(hurtPlayer) {
  auto [player, health] = context.query<Health>(oly::ecs::With<Player>{}).single();
  health.value -= 1;

  if (Shield* shield = context.tryGet<Shield>(player)) {
    shield->charge = 0.0f;
  }
}
```

- `get<T>(entity)` is a reference; it is a contract violation (`OLY_ASSERT`) if the entity is dead or lacks `T`.
- `tryGet<T>(entity)` is a pointer, `nullptr` if the entity is dead or lacks `T`.
- `has<T>(entity)` checks without reading. `get` and `tryGet` are not allowed on tags.

## Add and remove components

Adding or removing a component **moves** the entity to another archetype. This is a structural change.

```cpp
OLY_SYSTEM(freezeEnemy) {
  oly::ecs::Entity enemy = std::get<0>(context.query<const Health>(oly::ecs::With<Enemy>{}).single());

  context.add(enemy, Frozen{});
  context.add(enemy, Slow{0.5f});
  context.add(enemy, Slow{0.25f});   // already has it: the value is replaced
  context.remove<Frozen>(enemy);
  context.remove<Frozen>(enemy);     // no effect: it does not have it
}
```

## Destroy and check

```cpp
context.destroy(enemy);
context.isAlive(enemy);   // false
```

Destroying a dead entity is a contract violation.

## Entity handles

An `Entity` is an index plus a **generation**. When an entity dies, its index is reused later with a new generation. An old handle never points to the new entity: `isAlive` returns `false` for it. `oly::ecs::NullEntity` means "no entity".

### Keep references between entities

Store the other entity's handle in a component and check it each time you use it.

```
# game.oly
component Follower { leader: entity, distance: f32 }
```

```cpp
OLY_SYSTEM(followLeader) {
  context.query<const Follower>().each([&](oly::ecs::Entity entity, const Follower& follower) {
    if (!context.isAlive(follower.leader)) {
      context.destroy(entity);   // the leader died: remove the follower too
    }
  });
}
```

`context.tryGet<Position>(follower.leader)` answers two questions at once: it is `nullptr` if the leader is dead or has lost the component.

## What stays valid

Only the `Entity` handle is stable. **Any** structural change (spawn, destroy, add, remove) can move rows in memory, so references and pointers from `get`, `tryGet` and queries become invalid after a structural change. Copy the value if you need it afterwards.

## Structural changes and the loop

Inside the loop the rules depend on **where** your code runs:

| Code | `spawn`/`destroy`/`add`/`remove` | Why |
|---|---|---|
| a system (`SystemContext`) | **deferred** to the end of the phase | a system may be iterating a query |
| a handler, task or state hook | **direct** | they run outside any iteration |
| any code inside `each` or `eachBlock` | direct changes **refused** (`OLY_ASSERT`) | the iteration would see moved rows |

Details in [Engine, loop and systems](engine-and-systems.md#structural-changes-in-systems).

## Without the engine: `CommandBuffer`

With a bare `World`, record changes during an iteration and apply them after it:

```cpp
oly::ecs::CommandBuffer commands = world.commands();

world.query<const Health>().each([&](oly::ecs::Entity entity, const Health& health) {
  if (health.value <= 0) {
    commands.destroy(entity);
  }
});

world.flush(commands);   // one transition per entity, buffer left empty
```

The `CommandBuffer` merges commands per entity: `add` then `remove` of the same type cancel out, a repeated `add` keeps the last value, and `destroy` drops the earlier commands of that entity. `spawn()` returns the handle at once, so you can add components to it in the same buffer:

```cpp
oly::ecs::Entity bullet = commands.spawn();
commands.add(bullet, Position{10.0f, 10.0f});
commands.add(bullet, Bullet{});
```

The engine uses one `CommandBuffer` per phase for you, so in game code you rarely need your own.

## Debugging the world

```cpp
#include "oly/ecs/diagnostics.hpp"

std::string why;
if (!oly::ecs::checkInvariants(world, &why)) {
  // why explains the broken invariant
}

oly::ecs::WorldStats statistics = oly::ecs::stats(world);   // archetypes, live entities, memory per archetype
```

Call `checkInvariants` in tests after each step. It catches corruption at the step that caused it ([Testing a desktop game](../environments/native/getting-started.md#test-your-game)).

## Next

[Queries →](queries.md): visit many entities at once.

In other clients: [TypeScript](../environments/web/typescript/world-and-entities.md).
