# Oly architecture

This section describes Oly as it is today: what each part does, how the parts connect and why the structure is this one. It does not tell the history of the project. Every page of the section is listed in [Explanation](../../README.md#explanation). The requirements live in the specs ([Specs and plans](../../superpowers/README.md)).

## Overview

Oly is a game engine written in C++20 for 2D games, with 3D transforms in `essentials3d`, in the browser and on the desktop. It is a study and benchmark project, published for the author's own games ([Publishing](publishing.md)): every data representation decision has a measured cost.

The pieces:

- **Core**: the C++ engine (`core`, `ecs`, `input`, `app`) and its C API `oly*` (`capi`). The archetype ECS and all engine logic: fixed-step loop, phases, pause, events, input actions, schedule and states. It knows no environment. `core` (`engine/runtime/core/`) holds the dependency-free utilities every module uses: `OLY_ASSERT` (`assert.hpp`, with a handler the death tests replace), `align.hpp`, `small_map.hpp` and `type_name.hpp`.
- **Environment**: how the core is loaded and who drives the loop. Each environment has one **host** and zero or more **clients**.
  - `web`: the core compiled to WASM, in a browser or Node. Host in JS (`@oly/web-host`). Client: TypeScript (`@oly/core`).
  - `native`: the core compiled for the machine, in a native process. Host in C++ (`oly::native_host`: window, loop, input). No client: the game is C++.
- **Host**: loads the core, drives `frame`, forwards time, visibility and input. It has no engine rules.
- **Client**: the API of one language over the C API, with that language's idioms. **C++ is not a client**: a game's C++ systems, handlers and hooks are compiled together with the core in every environment. A C++ game and a C++ host use `oly::app::Engine` directly, never the C API.
- **Plugin**: engine-level functionality compiled together with the core, that a game turns on in its build (`oly_add_game(... PLUGINS ...)`). The `Engine` installs and owns it. Plugins depend on the core, never the reverse.

Three rules guide the design:

1. **The engine is complete in C++.** A client has API conveniences, never engine behaviour.
2. **Each client crosses its boundary per block, per batch or per system, never per entity**, on hot paths.
3. **There is a single source for the data layout**: the game's `.oly` file, merged with the `.oly` of each plugin it turns on. `oly-schema`, a C++ generator, produces the C++ and each client's code from it.

```
 web environment                          native environment
 ──────────────────────────────────       ─────────────────────────────
 game: TS systems, C++ systems            game: C++ `main` and systems
 TS client (@oly/core)                    native host (oly::native_host:
 web host (@oly/web-host: rAF,              SDL window, loop, input)
   visibility, keyboard, mouse)
 ─────────────────────────────────────────────────────────────────────────
 core (C++): app: Engine (loop, phases, pause, events, schedule, states)
             input: ActionMap
             capi: C API oly* + per-engine script invoker (web: WASM exports)
 ECS: component → entity → table → archetype → query / command_buffer → world
 base: assert, align, small_map, type_name
```

## See also

- [Repository map](../../reference/contributor/repository.md): the folders and the shape of a module.
- [Modules and boundaries](modules-and-boundaries.md): the dependency graph and how the boundaries are enforced.
- [Main architectural decisions](decisions.md): the reason and the accepted cost of each decision.
- [Known limitations](../../reference/limits.md) and [Glossary](../../reference/glossary.md).
