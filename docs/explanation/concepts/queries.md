# Queries

[← World and entities](world-and-entities.md) · [Concepts](README.md) · Next: [Engine, loop and systems →](engine-and-systems.md)

A query visits every entity that has a given set of components. It is the main way to run gameplay over many entities. This page explains the rules that hold while a query iterates. The spec of a query and the calls that read its rows are in the API pages.

## Rules while iterating

- References and spans are valid **only inside the callback**. Do not keep them for later.
- A direct structural change on the `World` (`spawn`, `destroy`, `add`, `remove`) while a query iterates is a contract violation (`OLY_ASSERT`). The iteration would see moved rows. Inside a system, use `context.spawn/destroy/add/remove`: they are deferred to the end of the phase and safe ([Direct and deferred changes](world-and-entities.md#direct-and-deferred-changes)). Outside the loop, collect the entities first and change them after the iteration, or use a `CommandBuffer` ([World and entities](../../reference/cpp/world-and-entities.md#commandbuffer)).

```cpp
OLY_SYSTEM(removeDead) {
  context.query<const Health>().each([&](oly::ecs::Entity entity, const Health& health) {
    if (health.value <= 0) {
      context.destroy(entity);
    }
  });
}
```

- The row of `single` and `trySingle` is a tuple of references. It stays valid until the next structural change, not only inside a callback, because no user code runs between the call and your use of it.

TypeScript: [Rules while iterating](../../reference/typescript/queries.md#rules-while-iterating) (the client throws `ContractError` instead of stopping on `OLY_ASSERT`).

## API

- C++: [Queries in C++](../../reference/cpp/queries.md)
- TypeScript: [Queries in TypeScript](../../reference/typescript/queries.md)

Ready patterns, such as counting entities, finding the nearest one or checking two sets against each other: [Gameplay recipes](../../how-to/gameplay-recipes.md#count-entities).

## Next

[Engine, loop and systems](engine-and-systems.md): run queries every step inside systems.
