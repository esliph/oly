# World and entities in TypeScript

[TypeScript client](README.md) · Concept: [World and entities](../../explanation/concepts/world-and-entities.md) · C++: [World and entities in C++](../cpp/world-and-entities.md)

The rules (deferred and direct changes, what a handle is) are in the [concept page](../../explanation/concepts/world-and-entities.md). This page shows the TS calls.

## Where the World comes from

| Situation | How you get a World | Who disposes it |
|---|---|---|
| a game with a loop | `runtime.createEngine()` → `engine.world` | `engine.dispose()` |
| ECS only, no loop (tools, tests, benchmarks) | `runtime.createWorld()` | you: `world.dispose()` |
| inside a system, handler, task or hook | use the `context` methods | — |

```ts
const runtime = await loadRuntime(factory, schema);

const world = runtime.createWorld();
// ... use it
world.dispose();
```

During `frame`, the client refuses `spawn`, `destroy`, `add` and `remove` on `engine.world` with `ContractError` (`direct structural change during frame; use context.spawn/destroy/add/remove`): use the context instead. `get`, `set`, `has`, `isAlive` and `query` stay allowed.

`world.handle` is the number the client uses to name the world in calls to the [C API](../c-api.md) (the address of its `OlyWorld`). A game does not need it.

## Create entities

`spawn` takes the list of types and, optionally, the initial values. Missing fields start at `0`.

```ts
const player = world.spawn([Player, Position, Velocity, Health], {
  Position: { x: 100, y: 50 },
  Health: { value: 3 },
});

const marker = world.spawn([]);                  // an empty entity
const rock = world.spawn([Position]);           // Position = { x: 0, y: 0 }
```

Tags go in the list and take no values.

### `SpawnValues`

```ts
type SpawnValues<Ts extends readonly SchemaType[]> = {
  [C in Extract<Ts[number], ComponentType> as C["name"]]?: Partial<ValueOf<C["fields"]>>;
};
```

The type of the second argument of `spawn`: an optional object with one optional key per component in the list, named after the component, whose value holds some or all of its fields ([`ValueOf`](schema.md#valueof)). A tag has no key.

## Read and write values

```ts
const health = world.get(player, Health);        // a copy: { value: 3 }
world.set(player, Health, { value: health.value - 1 });
world.set(player, Position, { x: 0 });           // partial: y is kept
```

`get` returns a **copy**. Changing it does not change the world. Use `set` to write.

`get` and `set` throw `ContractError` if the entity is dead or does not have the component. When the component may be missing, check first:

```ts
if (world.has(player, Health)) {
  world.set(player, Health, { value: 10 });
}
```

## Add and remove components

```ts
world.add(enemy, Frozen);                        // a tag
world.add(enemy, Slow, { factor: 0.5 });         // a component with a value
world.add(enemy, Slow, { factor: 0.25 });        // already has it: the value is replaced
world.remove(enemy, Frozen);
world.remove(enemy, Frozen);                     // no effect: it does not have it
```

## Destroy and check

```ts
world.destroy(enemy);
world.isAlive(enemy);   // false
world.destroy(enemy);   // ContractError: dead entity
```

## Entity handles

In TS, an `Entity` is a plain `number`. You can store it in components (`entity` fields), in `Map`s and `Set`s, and compare it with `===`.

```ts
import { formatEntity, NullEntity } from "@oly/core";

console.log(formatEntity(player));   // "12:0" (index:generation)

let target = NullEntity;             // "no entity"
```

`entityIndex(entity)` and `entityGeneration(entity)` return the two parts that `formatEntity` prints, as numbers. All four come from `@oly/core`.

### Keep references between entities

```ts
// later, in a system
const leader = follower.leader.get(row);
if (!context.isAlive(leader)) {
  context.destroy(block.entity(row));     // the leader died: remove the follower too
}
```

## What stays valid

`get` returns copies, so there is nothing to invalidate. The TS-only rule is for `raw()`: the arrays it returns go stale when the WASM memory grows, which any spawn or add can cause. Call `raw()` again after one. `ReadField.get`, `WriteField.get` and `WriteField.set` read the heap on every call, so they follow the growth. Use views from queries only inside their callback ([Queries](queries.md)).

## Debugging the world

```ts
const problem = world.checkInvariants();   // null when the world is consistent
if (problem !== null) {
  throw new Error(problem);
}
```

Call `checkInvariants` in tests after each step: [Debug the world](../../how-to/debug-the-world.md).
