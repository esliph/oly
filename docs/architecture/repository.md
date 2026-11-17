# Repository map

```
CMakeLists.txt  CMakePresets.json  Spurfile  spur   C++ build and task runner
Dockerfile  Dockerfile.wasm  Dockerfile.windows  docker-compose.yaml    toolchains (gcc:14, emsdk 3.1.74, mingw-w64 + Wine)
package.json  tsconfig.base.json  tsconfig.json     npm workspaces and project references
cmake/            build functions: OlyModule, OlyPlugin, OlyGame, OlyOptions, OlyWarnings, OlyWasm, Dependencies; toolchains/mingw-w64.cmake; tests/ (plugin order)
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
plugins/          planned (phase 3 of the plugins spec): one folder per plugin, beside engine/ and games/
games/
  arena/          validation game (arena.oly, C++ in native/, TS package in web/ (src/, tests/))
  drift/          desktop example, C++ only (drift.oly, gameplay in native/, SDL shell in desktop/)
tools/            measurement and lint scripts (Python and shell)
docs/
  architecture/   this documentation
  development/    commands and code style
  benchmarks/     recorded results: ecs/, bridge/, wasm/
  superpowers/    specs (source of truth) and implementation plans per phase
.claude/          Claude Code skills, agents and formatting hook
```

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

A plugin follows the same shape, with an optional `.oly` ([plugins.md](plugins.md)):

```
<plugin>/
  CMakeLists.txt         oly_add_plugin(NAME <p> [SCHEMA <p>.oly] DEPENDS ... [PLUGINS ...])
  <p>.oly                the plugin's components, tags and events (optional)
  include/oly/<p>/       public headers; plugin.hpp declares oly::<p>::Plugin
  src/                   implementation (comes in by glob, at least one file)
```

`plugins/` sits beside `engine/` and `games/`, not inside `engine/`: plugins depend on the core, and a separate root makes a later split into packages a move of folders. Until it exists, the only plugins are the test plugins in `engine/testing/fixture/plugins/`.

See also: [modules-and-boundaries.md](modules-and-boundaries.md).
