# Repository map

```
CMakeLists.txt  CMakePresets.json  Spurfile  spur   C++ build and task runner
Dockerfile  Dockerfile.wasm  Dockerfile.windows  docker-compose.yaml    toolchains (versions in toolchain.env; gcc, emsdk, mingw-w64 + Wine)
toolchain.env     the toolchain versions: read by the Dockerfiles, oly doctor and the configure check
package.json  tsconfig.base.json  tsconfig.json     npm workspaces and project references
cmake/            build functions: OlyModule, OlyPlugin, OlyGame, OlyOptions, OlyWarnings, OlyWasm, Dependencies, OlyToolchain (the check against toolchain.env); toolchains/mingw-w64.cmake; tests/ (plugin order, toolchain, consumer)
engine/
  runtime/        C++, one module per folder: <m>/{include/oly/<m>, src, tests, bench}
    core/         dependency-free utilities
    ecs/          the archetype ECS
    input/        input actions (ActionMap)
    app/          the Engine: loop, systems, events, schedule, states
    capi/         C API oly*, OlyWorld, OlyEngine, schema registration
  environments/   one folder per target environment
    web/clients/typescript/core/  @oly/core: the engine on the TS side, no DOM
    web/host/js/                  @oly/web-host: browser layer
    web/host/native/              web_host: C++ linked into every web game's WASM (JS script invoker)
    web/clients/typescript/codegen/  schema_typescript: the TS emitter of oly-schema
    native/host/                  native_host: SDL3 window, input translation and the run loop for desktop games
  schema/         the game schema tooling
    format/       the .oly language and its test corpus
    codegen/      oly-schema: parser, validation, layout, hash, emitters, C++ emitter
  testing/        @oly/testing (private): fixture game, WASM tests, boundary benchmarks
    fixture/plugins/  the engine's test plugins (counting, doubling) and the native plugin fixture
plugins/          one folder per plugin, beside engine/ (essentials2d, essentials3d)
tools/
  oly/            the oly launcher, install.sh, the game Spurfile, docker-compose.yaml for --docker and their shell tests
  release/        the release scripts: version check and set, images, oly-tools archive, stage, staged consumer test
  *.py, *.sh      measurement and lint scripts (Python and shell)
templates/
  game/           what oly new copies: common/, web/ and native/ overlays of a minimal game
docs/
  tutorials/      your first web game and your first desktop game
  how-to/         tasks; contributor/ holds the ones about the engine itself
  reference/      the oly command, the APIs, contributor/ and benchmarks/ (recorded results: ecs/, bridge/, wasm/)
  explanation/    what a game relies on (concepts) and how the engine is built (architecture)
  superpowers/    specs and implementation plans per phase
.github/workflows/  release.yml: the release pipeline (docs/explanation/architecture/publishing.md)
.claude/          Claude Code skills, agents and formatting hook
```

## Targets

The name each part has in CMake and npm:

| Target | Folder |
|---|---|
| `oly::core` | `engine/runtime/core` |
| `oly::ecs` | `engine/runtime/ecs` |
| `oly::input` | `engine/runtime/input` |
| `oly::app` | `engine/runtime/app` |
| `oly::capi` | `engine/runtime/capi` |
| `oly::native_host` | `engine/environments/native/host` |
| `@oly/core` | `engine/environments/web/clients/typescript/core` |
| `@oly/web-host` | `engine/environments/web/host/js` |
| `oly-schema` | `engine/schema/codegen` |

## Shape of a C++ module

Every C++ module follows the same shape:

```
engine/runtime/<m>/
  CMakeLists.txt         oly_add_module(NAME <m> DEPENDS ...)
  include/oly/<m>/       public headers
  include/oly/<m>/detail/  internal types (namespace oly::<m>::detail)
  src/                   implementation (comes in by glob)
  tests/                 doctest suites and death cases
  bench/                 benchmarks (only in ecs)
```

The include path repeats the module name (`#include "oly/ecs/world.hpp"`), so the include already says where the header comes from.

## Shape of a plugin

A plugin follows the same shape, with an optional `.oly` ([plugins.md](../../explanation/architecture/plugins.md)):

```
<plugin>/
  CMakeLists.txt         oly_add_plugin(NAME <p> [SCHEMA <p>.oly] DEPENDS ... [PLUGINS ...])
  <p>.oly                the plugin's components, tags and events (optional)
  include/oly/<p>/       public headers; plugin.hpp declares oly::<p>::Plugin
  src/                   implementation (comes in by glob, at least one file)
```

`plugins/` sits beside `engine/`, not inside `engine/`: plugins depend on the core, and a separate root makes a later split into packages a move of folders. Until it exists, the only plugins are the test plugins in `engine/testing/fixture/plugins/`.

## Build functions

The functions in `cmake/`. A game calls `oly_add_game` and a plugin calls `oly_add_plugin`: [CMake](../cmake.md).

| Function | What it does |
|---|---|
| `oly_add_module` | creates `oly_<m>` (alias `oly::<m>`) and links only the `DEPENDS` |
| `oly_add_test` | registers a doctest suite as an executable and a ctest test |
| `oly_add_contract_death` | registers a module's death cases |
| `oly_add_bench` | registers a benchmark executable |
| `oly_add_plugin` (`OlyPlugin.cmake`) | creates the plugin `oly_<p>` (alias `oly::<p>`) from `src/*.cpp`, links its `DEPENDS` and `PLUGINS` and, with `SCHEMA`, generates `oly/<p>/schema.generated.hpp` ([CMake](../cmake.md#oly_add_plugin)) |
| `oly_plugin_order` (`OlyPlugin.cmake`) | the install order of a plugin list: dependencies first, each plugin once, list order among independent ones |
| `oly_schema_tool` (`OlyPlugin.cmake`) | picks the `oly-schema` a custom command runs: the in-tree target, or `OLY_SCHEMA_TOOL` |
| `oly_add_game` | `ENVIRONMENT web`: runs `oly-schema` and links a game's `.mjs`/`.wasm` module, with `oly::web_host`. `ENVIRONMENT native`: runs `oly-schema` and builds an executable. The environment must be the build's `OLY_ENVIRONMENT`. `DIMENSION 2d|3d` (required): turns `essentials2d` or `essentials3d` on, first in the install order; listing either in `PLUGINS` is an error. `PLUGINS`: merges the plugins' `.oly`, links them and generates `game_setup.generated.cpp` (`setUpGame`). `TYPESCRIPT <file.ts>`: also writes the merged schema as TS, through the target `oly_typescript_schemas` |
| `oly_add_engine_test_game` | the same as `oly_add_game`, for the engine's own test games; the lint rejects it outside `engine/` |
| `oly_warnings`, `oly_wasm_exceptions`, `oly_wasm_node_env` | warnings and Emscripten settings |

See also: [Modules and boundaries](../../explanation/architecture/modules-and-boundaries.md) and [Boundary rules](boundary-rules.md).
