# Schema in TypeScript

[TypeScript client](README.md) · Concept: [Schema](../../../concepts/schema.md) · Next: [World and entities →](world-and-entities.md)

The `.oly` file, the field types and the naming rules are in the [concept page](../../../concepts/schema.md). This page shows what TS gets from them.

## The five kinds of entries

TS gets every entry from the generated module:

```ts
import schema, { Health, shoot } from "../schema.generated.js";
```

The default export is the schema you pass to `loadRuntime`. You pass the entries to every API call: `world.get(entity, Health)`, `context.emit(EnemyDied, ...)`, `actions.button(shoot)`. There are no string lookups at run time.

## Field types

| Type | TS reads it as |
|---|---|
| `f32` `f64` | `number` |
| `i8` `u8` `i16` `u16` `i32` `u32` | `number` |
| `entity` | `Entity` |

There are no 64-bit integers because they would need `BigInt`. Keep text, sprites and other assets on the TS side, indexed by a number in a component.

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

## Next

[World and entities →](world-and-entities.md): create entities from these types.
