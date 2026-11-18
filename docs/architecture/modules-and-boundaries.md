# Modules and boundaries

## Dependency graph

```mermaid
graph BT
  ecs --> core
  input --> core
  app --> ecs
  app --> input
  capi --> app
  webHost_native["web_host (C++, web only)"] --> capi
  nativeHost["native_host (C++, native only, SDL3)"] --> app
  plugins["plugins (oly_add_plugin)"] --> app
  games["games (oly_add_game)"] --> capi
  games --> webHost_native
  games --> nativeHost
  games --> plugins
  schema_typescript["schema_typescript (C++, native only)"] --> schema["schema (C++, native only)"]
  webHost["@oly/web-host"] --> tscore
  testing["@oly/testing"] --> tscore
```

| Module | CMake target | `DEPENDS` | Type |
|---|---|---|---|
| `core` | `oly::core` | — | library |
| `ecs` | `oly::ecs` | `core` | library |
| `input` | `oly::input` | `core` | library |
| `app` | `oly::app` | `input ecs core` | library |
| `capi` | `oly::capi` | `app input ecs core` | `OBJECT` |
| `web_host` | `oly::web_host` | `capi` | `OBJECT`, built only with Emscripten |
| `native_host` | `oly::native_host` | `app input ecs core` + SDL3 | library, built only with `OLY_BUILD_NATIVE_HOST` |
| `schema` | `oly::schema` | — | `STATIC`, native builds only |
| `schema_typescript` | `oly::schema_typescript` | `schema` | `OBJECT`, native builds only |

A **plugin** (`oly_add_plugin`, [plugins.md](plugins.md)) is a `STATIC` target `oly::<name>` that links the modules of its `DEPENDS` and the plugins of its `PLUGINS`. A game links the plugins it turns on and their dependencies. Plugins depend on the core; nothing in `engine/` depends on a plugin under `plugins/`. Today the only plugins are the engine's test plugins:

| Plugin | CMake target | `DEPENDS` | `PLUGINS` |
|---|---|---|---|
| `counting` | `oly::counting` | `ecs app` | — |
| `doubling` | `oly::doubling` | `ecs app` | `counting` |

`capi` and `web_host` are `OBJECT` because, in a static library, the linker would discard the unreferenced members and take the `EMSCRIPTEN_KEEPALIVE` functions with them.

`native_host` depends on `app`, never on `capi`: a C++ game and a C++ host use `oly::app::Engine` directly. It links SDL3 statically and `PUBLIC`, so a game's drawing code can include `<SDL3/SDL.h>`.

The C API headers, `oly/capi/c_api.h` and `oly/capi/engine_api.h`, are valid C11 and C++20: `capi.foreign_client` is a `.c` file that includes only them.

## How the boundaries are enforced

The boundaries do not hold by convention alone. Three mechanisms enforce them:

1. **The build.** `oly_add_module(NAME <m> DEPENDS <...>)` links only the declared modules. `ecs` does not link `capi`, so an include of `capi` inside the ECS does not compile. `oly_add_plugin` does the same with `DEPENDS` and `PLUGINS`.
2. **The `oly.lint.boundaries` lint** (`tools/check_boundaries.py`, runs in ctest, with a self-test in `oly.lint.boundaries_selftest`). Its manifest lists modules, plugins and games with their dependencies. Module and plugin names may contain digits (`oly/<x>/` with `<x>` in `[a-z0-9_]+`). It rejects:
   - `#include "oly/<x>/..."` in a module, plugin or game that did not declare `<x>` in `DEPENDS` (or `PLUGINS`) (dependencies are `PUBLIC` and transitive, so only the lint catches the undeclared direct include);
   - `oly/<x>/detail/` included outside module or plugin `<x>`'s own directory (its tests and benchmarks may);
   - a file in `engine/` that includes a plugin whose directory is under `plugins/` ("engine cannot depend on plugin"); the test plugins in `engine/testing/fixture/plugins/` are engine code;
   - a game name (a folder name under `games/`, as a whole word) in the contents or the path of any file under `plugins/`;
   - `oly_add_engine_test_game` in a `CMakeLists.txt` outside `engine/`;
   - `@oly/core/internal` imported outside `engine/`;
   - `@oly/core/generated` imported outside `*.generated.ts` and `@oly/core`;
   - `@oly/web-host` imported in `@oly/core`;
   - a relative import that crosses packages;
   - `engine/runtime/` including a module or importing a package of `engine/environments/`;
   - `engine/schema/` including a module or importing a package of `engine/environments/`;
   - one environment including or importing another;
   - `__EMSCRIPTEN__`, `EM_*`, `EMSCRIPTEN_*` or `<emscripten.h>` outside `engine/environments/web/` and `OLY_API` (`c_api.h`); a test under `tests/` or a benchmark under `bench/` may use `__EMSCRIPTEN__` to skip a case on WASM.
3. **The TS surface test** (`ts.core.unit`, `surface.test.ts`). It checks the public exports of `@oly/core` against the versioned list `engine/environments/web/clients/typescript/core/api/public-exports.txt`. Adding something to the public API is an explicit change in that file.

Why `detail/` and a lint, and not pimpl: the hot-path templates (`Query<Ts...>::each`, `World::get<T>`, `World::spawn<Ts...>`) need to see `Table` and `Archetype` to inline. Hiding these types behind pimpl or a virtual interface would put indirection in the iteration loop. The compiler sees the details; the build rejects whoever uses them from outside.

## Vocabulary rules

- Game concepts (enemy, coin, dash) never enter `engine/`. Game names only appear in `engine/testing/fixture/`, never in `plugins/`.
- The same verb holds across the three surfaces (C++, C API, TS): `spawn`, `destroy`, `add`, `remove`, `has`, `get`, `query`, `eachBlock`. The full glossary is in [glossary.md](glossary.md).
- The `oly` prefix appears only in the C API (`oly*`, `OlyWorld`, `OLY_API`) and in what is internal to `capi`. `extern "C"` symbols live in a single global namespace of the WASM module, together with libc and the game's exports; the prefix avoids collisions. The C++ (`oly::ecs::World`) and TS (`World`) APIs do not repeat `oly`.
- The `ById` suffix marks the variant by `ComponentId` at run time: `addById`, `removeById`, `hasById`, `getById`, `tryGetById`, `queryById`.
