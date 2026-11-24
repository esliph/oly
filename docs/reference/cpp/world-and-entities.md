# World and entities in C++

[Reference](../../README.md#reference) · Concept: [World and entities](../../explanation/concepts/world-and-entities.md) · TypeScript: [World and entities in TypeScript](../typescript/world-and-entities.md)

Headers: `oly/ecs/world.hpp`, `oly/ecs/entity.hpp`, `oly/ecs/command_buffer.hpp`, `oly/ecs/observer.hpp`, `oly/ecs/diagnostics.hpp`, and `oly/app/context.hpp` and `oly/app/system.hpp` for the contexts. Which calls are direct and which are deferred: [Direct and deferred changes](../../explanation/concepts/world-and-entities.md#direct-and-deferred-changes).

## Where the World comes from

### `World`

```cpp
oly::ecs::World world;
```

A `World` is not copyable. Declare it before the `Engine` that uses it, so it outlives the `Engine`. In the game's `main`, a test or a tool, you create it. Inside a system, handler, task or hook you use the context methods instead. Who owns it: [Where the World comes from](../../explanation/concepts/world-and-entities.md#where-the-world-comes-from).

A tool or a benchmark can use the ECS alone, with no `Engine`:

```cpp
#include "oly/ecs/world.hpp"

struct Position { float x, y; };
struct Velocity { float x, y; };
struct Frozen {};  // tag: component with no data

oly::ecs::World world;
oly::ecs::Entity player = world.spawn(Position{0, 0}, Velocity{1, 2});

// Iterates only entities with Position and Velocity and without the Frozen tag.
world.query<Position, const Velocity>(oly::ecs::Without<Frozen>{})
    .each([](Position& position, const Velocity& velocity) {
      position.x += velocity.x;
      position.y += velocity.y;
    });

// Structural changes during iteration go through the CommandBuffer.
oly::ecs::CommandBuffer commands = world.commands();
world.query<const Position>().each([&](oly::ecs::Entity entity, const Position& position) {
  if (position.x > 100) {
    commands.destroy(entity);
  }
});
world.flush(commands);
```

### `Engine::world`

```cpp
explicit Engine(ecs::World& world, EngineConfig config = {});
ecs::World& world() noexcept;
```

The `Engine` keeps a reference to the `World` you pass, and `world()` returns it.

```cpp
oly::ecs::World world;
oly::app::Engine engine(world);
```

## Create entities

### `World::spawn`

```cpp
Entity spawn();
template <class... Ts>
Entity spawn(Ts&&... components);
```

`spawn()` creates an entity with no components. `spawn(components...)` creates it directly in its final archetype, with no intermediate moves. Lvalues are copied. A component you do not list is not on the entity; one you list with `{}` starts with every field at `0`. Tags go in the list as empty structs. Stops with `OLY_ASSERT` (`spawn during iteration=<n>`) while a query iterates, and a repeated type is a compile error.

```cpp
oly::ecs::Entity player = engine.world().spawn(Player{}, Position{100.0f, 50.0f}, Velocity{0.0f, 0.0f}, Health{3});
```

### `DirectContext::spawn`

```cpp
ecs::Entity spawn();
template <class... Ts>
ecs::Entity spawn(Ts&&... components);
```

The `spawn` of `HandlerContext`, `TaskContext` and `StateContext`. It calls `World::spawn`; with an active state the entity is born scoped (it disappears on a state change) until it receives `persist`.

### `SystemContext::spawn`

```cpp
ecs::Entity spawn();
template <class... Ts>
ecs::Entity spawn(Ts&&... components);
```

Records the `spawn` and one `add` per component in the phase's `CommandBuffer`. The handle is valid at once; the entity is alive when the phase ends. With an active state the entity is born scoped until it receives `persist`.

```cpp
OLY_SYSTEM(spawnWave) {
  oly::ecs::Entity rock = context.spawn(Position{});
  oly::ecs::Entity marker = context.spawn();
  oly::ecs::Entity enemy = context.spawn(Enemy{}, Position{100.0f, 50.0f}, Health{3});
}
```

### `CommandBuffer::spawn`

```cpp
Entity spawn();
```

Reserves an index immediately and returns the handle. The entity becomes alive on `World::flush`.

## Read and write values

### `get`

```cpp
template <class T>
T& World::get(Entity entity);
template <class T>
T& EngineContext::get(ecs::Entity entity);
```

Returns a reference to the stored component. Write through it. It is a contract violation if the entity is dead or lacks `T`: `OLY_ASSERT` with the message `get e=<index>:<generation> type=<T>`. `T` cannot be a tag. The reference is invalid after any structural change ([What stays valid](../../explanation/concepts/world-and-entities.md#what-stays-valid)).

```cpp
OLY_SYSTEM(hurtPlayer) {
  auto [player, health] = context.query<Health>(oly::ecs::With<Player>{}).single();
  health.value -= 1;
}
```

### `tryGet`

```cpp
template <class T>
T* World::tryGet(Entity entity) noexcept;
template <class T>
T* EngineContext::tryGet(ecs::Entity entity) noexcept;
```

Returns a pointer, or `nullptr` if the entity is dead or lacks `T`. `T` cannot be a tag.

```cpp
if (Shield* shield = context.tryGet<Shield>(player)) {
  shield->charge = 0.0f;
}
```

### `has`

```cpp
template <class T>
bool World::has(Entity entity) const noexcept;
template <class T>
bool EngineContext::has(ecs::Entity entity) const noexcept;
```

Checks without reading. It works on tags and returns `false` for a dead entity.

### `getById`, `tryGetById`, `hasById`

```cpp
void* World::getById(Entity entity, ComponentId componentId);
void* World::tryGetById(Entity entity, ComponentId componentId) noexcept;
bool World::hasById(Entity entity, ComponentId componentId) const noexcept;
```

The same three operations by `ComponentId`, for code that does not know the type at compile time. `getById` stops with `OLY_ASSERT` (`getById e=<index>:<generation> type=<name>`) if the entity is dead or the component is absent; a tag is also an error.

## Add and remove components

### `World::add`

```cpp
template <class T>
void add(Entity entity, T&& value);
```

Adds `T` with `value`, or assigns it if the entity already has `T`. The entity moves to another archetype. Stops with `OLY_ASSERT` (`add e=<index>:<generation> type=<T>`) if the entity is dead or a query iterates. A tag takes an empty struct.

### `World::remove`

```cpp
template <class T>
void remove(Entity entity);
```

Removes `T`; no effect if the entity does not have it. Stops with `OLY_ASSERT` (`remove e=<index>:<generation> type=<T>`) if the entity is dead or a query iterates. Removing an implied component while its implier stays stops with `remove e=<index>:<generation> type=<T> implied by <Implier>`.

### `DirectContext::add`, `DirectContext::remove`

```cpp
template <class T>
void add(ecs::Entity entity, T&& value);
template <class T>
void remove(ecs::Entity entity);
```

Direct, like the `World` calls they forward to.

### `SystemContext::add`, `SystemContext::remove`

```cpp
template <class T>
void add(ecs::Entity entity, T&& value);
template <class T>
void remove(ecs::Entity entity);
```

Deferred to the end of the phase.

```cpp
OLY_SYSTEM(freezeEnemy) {
  oly::ecs::Entity enemy = std::get<0>(context.query<const Health>(oly::ecs::With<Enemy>{}).single());

  context.add(enemy, Frozen{});
  context.add(enemy, Slow{0.5f});
  context.add(enemy, Slow{0.25f});
  context.remove<Frozen>(enemy);
}
```

`Slow{0.25f}` replaces the value of the previous `Slow`, because the entity already has it.

### `addById`, `removeById`

```cpp
void World::addById(Entity entity, ComponentId componentId, void* value);
void World::removeById(Entity entity, ComponentId componentId);
```

The same operations by `ComponentId`. `addById` moves from `value` (the caller still owns the moved-from object) and ignores it for a tag. `DirectContext`, `SystemContext` and `CommandBuffer` have the same two methods.

## Destroy and check

### `World::destroy`

```cpp
void destroy(Entity entity);
```

Destroys the entity. Stops with `OLY_ASSERT` (`destroy e=<index>:<generation> during iteration=<n>`) if the entity is dead or a query iterates. `DirectContext::destroy` forwards to it; `SystemContext::destroy` defers it to the end of the phase, and `CommandBuffer::destroy` to the flush.

```cpp
context.destroy(enemy);
```

### `World::isAlive`

```cpp
bool isAlive(Entity entity) const noexcept;
bool EngineContext::isAlive(ecs::Entity entity) const noexcept;
```

`true` while the entity is alive. A handle from an entity that died is `false`, even if its index was reused. After `destroy` in a `SystemContext` the answer stays `true` until the end of the phase.

### `World::isIterating`

```cpp
bool isIterating() const noexcept;
```

`true` while a query iterates or an observer runs. While it is `true`, direct structural changes, registering observers and `flush` stop on `OLY_ASSERT`.

## Entity handles

### `Entity`

```cpp
struct Entity {
  uint32_t index;
  uint32_t generation;

  friend bool operator==(Entity, Entity) = default;
};
```

`entity.index` is the slot of the entity. `entity.generation` grows each time the slot is reused, so an old handle differs from the new one. Compare handles with `==`; `oly::ecs::EntityHash` hashes them for `std::unordered_map`. Store a handle in a component with an `entity` field in the schema and check it each time you use it:

```
# game.oly
component Follower { leader: entity, distance: f32 }
```

```cpp
OLY_SYSTEM(followLeader) {
  context.query<const Follower>().each([&](oly::ecs::Entity entity, const Follower& follower) {
    if (!context.isAlive(follower.leader)) {
      context.destroy(entity);
    }
  });
}
```

### `NullEntity`

```cpp
inline constexpr Entity NullEntity{UINT32_MAX, 0};
```

"No entity". It is never alive.

## CommandBuffer

### `World::commands`

```cpp
CommandBuffer commands();
```

Creates a `CommandBuffer` for this `World`. The buffer can only be flushed into this `World` and must not outlive it.

### `World::flush`

```cpp
void flush(CommandBuffer& commandBuffer);
```

Applies the commands with one transition per entity and leaves the buffer empty. Commands for stale handles are ignored. Stops with `OLY_ASSERT` with `flush during iteration=<n>` while a query iterates and with `flush of a buffer created by another World` for a buffer of another `World`. How the commands combine: [How deferred changes combine](../../explanation/concepts/world-and-entities.md#how-deferred-changes-combine).

```cpp
oly::ecs::CommandBuffer commands = world.commands();

world.query<const Health>().each([&](oly::ecs::Entity entity, const Health& health) {
  if (health.value <= 0) {
    commands.destroy(entity);
  }
});

world.flush(commands);
```

### `CommandBuffer::add`, `CommandBuffer::remove`, `CommandBuffer::destroy`

```cpp
template <class T>
void add(Entity entity, T&& value);
template <class T>
void remove(Entity entity);
void destroy(Entity entity);
```

Record a command; nothing changes until the flush.

```cpp
oly::ecs::Entity bullet = commands.spawn();
commands.add(bullet, Position{10.0f, 10.0f});
commands.add(bullet, Bullet{});
```

### `CommandBuffer::empty`, `CommandBuffer::commandCount`

```cpp
bool empty() const noexcept;
size_t commandCount() const noexcept;
```

`commandCount()` is the number of recorded commands, and `empty()` is `commandCount() == 0`. A flushed buffer is empty.

### `CommandBuffer::clear`

```cpp
void clear() noexcept;
```

Drops every recorded command, destroys their payloads and forgets the spawned handles. Use it to discard a buffer without flushing it.

## React to changes: observers

When each observer runs and what it may do: [When observers run](../../explanation/concepts/world-and-entities.md#when-observers-run).

### `ObserverFunction`

```cpp
using ObserverFunction = void (*)(ObserverContext& context);
```

A plain function pointer, so it captures nothing. Pass data through `userData`.

```cpp
void destroyFollower(oly::ecs::ObserverContext& context) {
  const Leader& leader = context.world().get<Leader>(context.entity());

  if (context.world().isAlive(leader.follower)) {
    context.commands().destroy(leader.follower);
  }
}
```

### `World::onAdd`, `World::onRemove`

```cpp
template <class T>
ObserverId onAdd(ObserverFunction function, void* userData = nullptr);
template <class T>
ObserverId onRemove(ObserverFunction function, void* userData = nullptr);
```

Register `function` to run when an entity gains (`onAdd`) or loses (`onRemove`) `T`. Observers of one component and kind run in registration order. Stops with `OLY_ASSERT` (`onAdd type=<T> during iteration=<n>`, `onRemove type=<T> during iteration=<n>`) when called during an iteration or inside an observer.

```cpp
engine.world().onRemove<Leader>(destroyFollower);
```

### `World::observeById`

```cpp
ObserverId observeById(ObserverKind kind, ComponentId componentId, ObserverFunction function, void* userData = nullptr);
```

The registration by `ComponentId`. Stops with `observeById type=<name> during iteration=<n>` in the same cases as `onAdd`.

### `World::removeObserver`

```cpp
void removeObserver(ObserverId observer);
```

Stops the observer. Stops with `OLY_ASSERT` (`removeObserver id=<n> during iteration=<n>`) during an iteration or inside an observer, and with `removeObserver id=<n>: unknown or already removed` for an id that was removed or never returned by this `World`.

### `ObserverId`, `ObserverKind`

```cpp
using ObserverId = uint32_t;
enum class ObserverKind : uint8_t { OnAdd, OnRemove };
```

An `ObserverId` is valid until `removeObserver`; ids are never reused. `ObserverKind::OnAdd` runs after the component is in place, and `ObserverKind::OnRemove` runs before it leaves.

### `ObserverContext`

```cpp
World& world() const noexcept;
Entity entity() const noexcept;
ComponentId componentId() const noexcept;
void* userData() const noexcept;
CommandBuffer& commands() const noexcept;
```

Valid only during the observer call. `userData()` is the second argument of `onAdd`, `onRemove` or `observeById`. `commands()` is the only way to change structure from an observer; the `World` applies it before the call that triggered the observer returns.

## Implied components

### `World::imply`

```cpp
template <class T, class Implied>
void imply();
```

Adding `T` also adds `Implied`, value-initialized, in the same change. Removing `T` removes `Implied` once no component left implies it. `T` and `Implied` must differ, and `Implied` must be default constructible without throwing (compile errors). Stops with `OLY_ASSERT` (`imply type=<T> during iteration=<n>`) during an iteration, and with `imply type=<T> after an archetype contains it` when an entity already has `T`. The rules: [Implied components](../../explanation/concepts/world-and-entities.md#implied-components).

```cpp
world.imply<Position, WorldTransform>();
```

The `essentials2d` and `essentials3d` plugins do this for you.

## Debugging the world

How to use these calls in tests: [Debug the world](../../how-to/debug-the-world.md).

### `checkInvariants`

```cpp
#include "oly/ecs/diagnostics.hpp"

bool checkInvariants(const World& world, std::string* why = nullptr);
```

Checks the structural invariants of the `World` (I-1 and I-3 to I-13) and returns `false` on the first one that fails. With `why`, it writes the invariant and the object to it, for example `I-3 e=4:1`.

### `stats`

```cpp
WorldStats stats(const World& world);
```

Returns `archetypeCount`, `liveEntities`, one `ArchetypeStats` per archetype (`count`, `capacity`, `columnBytes`, `idleBytes` and more), a `histogram` of archetype sizes and `signatureLookups`.
