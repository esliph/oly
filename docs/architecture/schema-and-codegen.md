# Schema, oly-schema and each game's WASM module

## The `.oly` schema

The game declares everything in a `<game>.oly` file, the single source of the data layout for every environment and language (together with the `.oly` of its plugins):

```
# game.oly
component Health { value: i32 }
tag Enemy
event EnemyDied { entity: entity, x: f32, y: f32 }
action move: axis2d
state playing
```

- Scalar types: `f32 f64 i8 u8 i16 u16 i32 u32 entity`. There are no 64-bit integers (they would require `BigInt`).
- Fields in declaration order, natural alignment. A component becomes a trivial struct; the column stores structs (SoA across components, AoS inside the component).
- The grammar, the naming rules, the limits and the test corpus are in `engine/schema/format/README.md`.
- A plugin may bring its own `.oly`, with components, tags and events only. `oly-schema` merges it with the game's ([Plugin schemas](#plugin-schemas)).

## `oly-schema`

`oly-schema` (`engine/schema/codegen/`) is a C++ executable:

```
usage: oly-schema <file.oly> [--plugin <name>=<file.oly> ...] --emit <emitter>=<target> [--emit <emitter>=<target> ...]
```

For example, `oly-schema game.oly --plugin counting=counting.oly --emit cpp=<dir> --emit ts=<file>`. The `--plugin` and `--emit` arguments may come in any order; the order of the `--plugin` arguments is the install order. A malformed `--plugin` prints the usage.

- It parses, validates, lays out and hashes the schema once. Every client gets the same layout and the same hash, because no client computes them.
- An emitter turns the schema into the code of one language. The C++ emitter is part of the core (C++ is the engine language, not a client). A client's emitter lives in its environment folder and registers itself into `oly-schema` when linked; the TS emitter is `engine/environments/web/clients/typescript/codegen/`. Adding a language is one folder and one `add_subdirectory`; the generator core does not change.
- Errors report `<file>:<line>:<column>: <message>` and the build fails without writing any file. `oly-schema` reads every input file before writing anything.

## Plugin schemas

Each plugin with a schema reaches `oly-schema` as a `--plugin <name>=<file.oly>` argument; CMake passes them in install order ([plugins.md](plugins.md#schema-merge)).

- `mergeSchemas` parses each file with its own validation, then merges them: the plugins' declarations in install order, then the game's. That order gives the `ComponentId` order and the event dispatch order. Each merged type keeps its `origin` (the plugin's name; empty for the game's types), and the schema lists its `plugins` in order.
- Names are flat. A name repeated across files is an error at the later one: `<file>:<line>:<column>: repeated name "X"; first declared at <file>:<line>:<column>`.
- A plugin file declares only components, tags and events; an action or a state there is an error. Action and state ids are `constexpr` in the generated header, and a plugin's header is generated once, while its ids would depend on each game's install order.
- The hash covers the merged canonical form, so a client built with another plugin set fails the hash check. With no `--plugin`, the output and the hash are the same as before plugins existed.
- **The `cpp-plugin` emitter** writes `<dir>/schema.generated.hpp` with the plugin's own types: no declaration and no registry. `oly_add_plugin` passes `<generated>/oly/<name>`, so the header is `oly/<name>/schema.generated.hpp`.
- The game's `cpp` output includes each plugin header in install order and defines only the game's own types, so no type is defined twice (ODR). The declaration, the registry and the `ts` output cover the whole merged set.
- Corpus: `engine/schema/format/corpus/merge/`, one folder per case, checked by the `schema.merge` suite.

## The build flow

```
plugins' .oly ─┐ (--plugin, in install order)
<game>.oly ────┴► oly-schema ─┬─ cpp ──► schema.generated.hpp + schema_registry.generated.cpp
                              │            │
                              │   game_setup.generated.cpp (written by CMake: setUpGame) ──┤
                              │   the game's C++ systems (OLY_SYSTEM, OLY_HANDLER) ──┤
                              │   the plugins + oly::capi + app + input + ecs + core + oly::web_host ┴──► emcc ──► <game>_game.mjs + .wasm
                              └─ ts ───► web/schema.generated.ts (imports @oly/core/generated)
```

- `oly_add_game` (`cmake/OlyGame.cmake`) runs `oly-schema` on the game's `.oly` and the `.oly` of each plugin it turns on (`PLUGINS`), writes `game_setup.generated.cpp` (which defines `setUpGame`: declare the schema, then install the plugins) and puts it all together. The WASM is compiled **per game**.
- The generated C++ has `static_assert`s on `sizeof`, `alignof` and `offsetof` ("diverges from oly-schema"), and `actions::` and `states::` namespaces with the ids. A wrong name in a native system is a compile error.
- **Schema hash**: 32-bit FNV-1a of a canonical form (components, tags, events, actions and states) of the merged schema, plugins included. The form did not change when the schema moved to `.oly`, so the hash of an existing schema is the same. `loadRuntime` checks the hash and the sizes and throws `SchemaMismatchError` if the WASM was generated with another schema.
- **Where `oly-schema` is built**: native presets build it with the tree. A WASM build cannot run a tool it compiles to WASM, so the `wasm` image builds it for the machine with the `schema-tool` preset (`./spur schema-tool`, which `./spur ts` runs first). There, `oly_add_game` and `oly_add_plugin` run the executable named by the cache variable `OLY_SCHEMA_TOOL` (default `build/schema-tool/bin/oly-schema`); `oly_schema_tool` makes that choice for both.
- **The TS schema comes from the game's CMake target**, the only place that knows the game's plugins. With `oly_add_game(... TYPESCRIPT <file.ts>)`, a custom command writes the merged schema as TS to that file (`web/schema.generated.ts` in a game made by `oly new`), and the custom target `oly_typescript_schemas` depends on every such file. `./spur ts` builds `oly-schema`, configures a `wasm-games*` preset (default `wasm-games`), builds `oly_typescript_schemas`, generates the test-only schemas of `engine/testing/fixture/schemas/` with `oly-schema` directly, then runs `tsc`. `*.generated.ts` is never committed and never edited.
- Link: `-sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=node,web`, growable memory. The output goes to `build/<preset>/games/<game>_game.{mjs,wasm}`.

See also: [capi.md](capi.md) (how the `OlyEngine` declares the schema on creation) and [ts-packages.md](ts-packages.md) (the TS side: `@oly/core/generated`).
