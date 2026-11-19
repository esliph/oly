# Schema in TypeScript

[TypeScript client](README.md) · Concept: [Schema](../../explanation/concepts/schema.md) · C++: [Schema in C++](../cpp/schema.md)

The `.oly` file is explained in the [concept page](../../explanation/concepts/schema.md); its field types and naming rules are in [The `.oly` schema language](../../../engine/schema/format/README.md). This page shows what TS gets from them.

## The five kinds of entries

TS gets every entry from the generated module:

```ts
import schema, { Health, shoot } from "../schema.generated.js";
```

The default export is the schema you pass to `loadRuntime`. You pass the entries to every API call: `world.get(entity, Health)`, `context.emit(EnemyDied, ...)`, `actions.button(shoot)`. There are no string lookups at run time.

### The schema object

The default export is a `Schema`. The runtime keeps it as `runtime.schema`.

| Member | What it holds |
|---|---|
| `types` | the components and tags, in declaration order. The index of a type is its `ComponentId` in the WASM |
| `events` | the events, in declaration order, which is the dispatch order |
| `actions` | the input actions, in declaration order. The index is the `ActionId` |
| `states` | the states, in declaration order. The index is the `StateId` |
| `hash` | the hash of the merged schema, which `loadRuntime` compares with the WASM's ([the schema check](#the-schema-check-at-start-up)) |

Each entry also has `schemaIndex`, its position in its own list.

## Field types

| Type | TS reads it as |
|---|---|
| `f32` `f64` | `number` |
| `i8` `u8` `i16` `u16` `i32` `u32` | `number` |
| `entity` | `Entity` |

The schema has no 64-bit integers because they would need `BigInt`.

### `ValueOf`

```ts
type ValueOf<F extends Fields> = { [K in keyof F]: number };
```

The type of the values of a component's fields: one `number` per field name. `Partial<ValueOf<C["fields"]>>` is what `spawn`, `add` and `emit` accept for a component or an event `C` ([`SpawnValues`](world-and-entities.md#spawnvalues)).

## What the build generates

`schema.generated.ts` has one constant per entry and the schema as the default export:

```ts
export const Health = componentType("Health", { value: "i32" }, 1, { size: 4, alignment: 4, offsets: { value: 0 } });
// ...
export default schemaOf([Position, Health, Weapon, Target, Enemy, Frozen, EnemyDied, move, aim, shoot, menu, playing], 0x1a2b3c4d);
```

Import `@oly/core/generated` only in `*.generated.ts`. The file is generated, never committed and never edited.

## The schema check at start-up

```ts
import { loadRuntime, SchemaMismatchError } from "@oly/core";

try {
  const runtime = await loadRuntime(factory, schema);
} catch (error) {
  if (error instanceof SchemaMismatchError) {
    console.error("rebuild the WASM:", error.message);
  }
  throw error;
}
```

`loadRuntime` compares the hash, the number of entries and the size and alignment of each type. The message names the first type that differs. An entry from another generated module is refused by `loadRuntime` and by the API calls, because the runtime only accepts the entries of its own schema.
