# World and entities

[← Schema](schema.md) · [Concepts](README.md) · Next: [Queries →](queries.md)

The `World` holds all entities and their components. An **entity** is only an id. What it *is* comes from the components it has. This page explains the rules a game relies on: who owns the `World`, what a handle is, what stays valid, which changes are direct and which are deferred, and when observers and implied components act.

## Where the World comes from

The `Engine` does not own the `World`: you give it one. Inside a system, handler, task or hook you never touch the `World` directly; the context gives you the same operations.

| Situation | How you get a World | Who owns it |
|---|---|---|
| inside a system, handler, task or hook | the `context` methods | the code that created the `Engine` |
| where you own the `Engine` (the game's `main`, a test) | `oly::ecs::World world;` declared **before** the `Engine`, then `engine.world()` | you |
| ECS only, no loop (tools, benchmarks) | `oly::ecs::World world;` | you |

On the web the host creates the `World` and the `Engine` for you. TypeScript: [Where the World comes from](../../reference/typescript/world-and-entities.md#where-the-world-comes-from).

## What stays valid

Only the `Entity` handle is stable. **Any** structural change (spawn, destroy, add, remove) can move rows in memory, so references and pointers from `get`, `tryGet` and queries become invalid after a structural change. Copy the value if you need it afterwards.

Adding or removing a component **moves** the entity to another archetype. This is a structural change.

## Entity handles

An `Entity` is an index plus a **generation**. When an entity dies, its index is reused later with a new generation. An old handle never points to the new entity: `isAlive` returns `false` for it. `oly::ecs::NullEntity` means "no entity".

To keep a reference between entities, store the other entity's handle in a component and check it each time you use it. `tryGet<T>(handle)` answers two questions at once: it is `nullptr` if the entity is dead or has lost the component.

C++ example: [Entity](../../reference/cpp/world-and-entities.md#entity). TypeScript: [Entity handles](../../reference/typescript/world-and-entities.md#entity-handles).

## Direct and deferred changes

A change is **direct** when it takes effect inside the call. It is **deferred** when the call records a command and a later `flush` applies it. Which one you get depends on where your code runs:

| Where | Change | When it takes effect | `isAlive` right after `destroy` | A dead or stale handle |
|---|---|---|---|---|
| `World` | direct | in the call | `false` | `OLY_ASSERT` |
| `HandlerContext`, `TaskContext`, `StateContext` (they run outside any iteration) | direct | in the call | `false` | `OLY_ASSERT` |
| `Commands` in a system (a system may be iterating a query) | deferred to the phase's `CommandBuffer` | at the end of the phase | `true` until the end of the phase | ignored at the flush |
| a `CommandBuffer` you own | deferred | at `world.flush(commandBuffer)` | `true` until the flush | ignored at the flush |
| an observer, through `ObserverContext::commands()` | deferred to a `CommandBuffer` of the `World` | before the call that triggered the observer returns | `true` until then | ignored at the flush |

The `World` and the direct contexts also stop on `OLY_ASSERT` when `destroy`, `add` or `remove` gets a dead entity, and on every direct structural change (`spawn`, `destroy`, `add`, `remove`) while a query iterates (`each`, `eachBlock`): the iteration would see moved rows. Inside a system, use the context: it is deferred, so it is safe in an iteration.

```cpp
void removeDeadEnemies(oly::app::Query<const Health, oly::ecs::With<Enemy>> enemies, oly::app::Commands commands) {
  enemies.each([&](oly::ecs::Entity entity, const Health& health) {
    if (health.value <= 0) {
      commands.destroy(entity);
    }
  });
}

OLY_SYSTEM(removeDeadEnemies);
```

In this system `commands.isAlive(entity)` is still `true` after `commands.destroy(entity)`. The entity dies when the phase ends. More on phases: [Engine, loop and systems](engine-and-systems.md#structural-changes-in-systems).

TypeScript: [Where the World comes from](../../reference/typescript/world-and-entities.md#where-the-world-comes-from) (the client refuses `spawn`, `destroy`, `add` and `remove` on `engine.world` during `frame`).

### How deferred changes combine

A `CommandBuffer` merges the commands of each entity when it is flushed, and the entity makes one transition:

- `add` then `remove` of the same type cancel out.
- A repeated `add` keeps the last value.
- `destroy` drops the earlier commands of that entity.
- `spawn()` returns the handle at once, so you can add components to it in the same buffer.
- A command for an entity after its `destroy` in the same buffer is a contract violation (`OLY_ASSERT` in a buffer you own); the buffers of observers ignore it.

The engine uses one `CommandBuffer` per phase for you, so game code rarely needs its own. The calls: [CommandBuffer](../../reference/cpp/world-and-entities.md#commandbuffer).

## When observers run

An observer runs your code when an entity gains (**on add**) or loses (**on remove**) a component, in every path that changes the `World`: direct calls, the flush of a system's changes and the state scope cleanup. Use it to keep two things in step, for example to destroy the followers of a leader that dies.

| Change | What runs |
|---|---|
| `add` of a component the entity lacked, `spawn(components...)` | on add, after the value is in place: read the new value with `get` |
| `remove` | on remove, before the component leaves: read the old value with `get` |
| `destroy` | on remove for every observed component, before the entity dies |
| `add` of a component the entity already has | on remove with the old value, then on add with the new one (a tag runs nothing) |
| a write through `get`, `tryGet` or a query | nothing: field writes are not observed |

Inside a flush (the end of a phase, or `world.flush`) observers see the **net change** of each entity: an `add` followed by a `remove` runs nothing, and repeated `add`s run one on add with the last value. On remove runs before each entity's change; on add runs after every entity of the flush changed, so an on add observer sees the world as the whole flush left it.

Rules:

- An observer changes structure **only** through `context.commands()`. The `World` applies those commands right after the change that triggered the observer, before that call returns, until nothing is pending. Two observers may record the same `destroy`, or a change for an entity another observer destroyed: the extra commands are ignored. A direct `spawn`, `destroy`, `add` or `remove` inside an observer stops on `OLY_ASSERT`, the same as inside `each`. If an observer throws, the change stops where it was and the `World` drops the commands its observers recorded, so nothing from that change runs later.
- Observers that always record more changes never stop: avoid a cycle where each observer triggers the other.
- Register and remove observers at start-up, never inside an observer or a query loop (`OLY_ASSERT`).
- Observers of the same component and kind run in registration order.

The calls: [Observers](../../reference/cpp/world-and-entities.md#react-to-changes-observers).

## Implied components

A component can **imply** others: adding it adds them in the same change, so the entity never exists without them.

- The implied component starts value-initialized (the default member values of the struct) and runs its on add observers like any other, so an observer can fill it at once. In a direct `add`, the observers of the component you added run first.
- If the entity already has the implied component, it keeps its value.
- Removing `Position` removes `WorldTransform` too, unless another component that implies it stays. An implied component leaves with its last implier, even if you added it yourself.
- Implications are transitive, and a cycle is valid: both components always come together.
- Inside a flush the net change decides here too: an implied component the entity has before and after the flush keeps its value and runs no observer, and a component the flush adds stays even if the same flush removes its implier.
- Register an implication **before** any entity has the implying component, and never remove an implied component while its implier stays: both stop on `OLY_ASSERT`. `Implied` must be default constructible without throwing.

The call: [Implied components](../../reference/cpp/world-and-entities.md#implied-components).

## API

- C++: [World and entities in C++](../../reference/cpp/world-and-entities.md)
- TypeScript: [World and entities in TypeScript](../../reference/typescript/world-and-entities.md)

Check a `World` while you build a game: [Debug the world](../../how-to/debug-the-world.md).

## Next

[Queries](queries.md): visit many entities at once.
