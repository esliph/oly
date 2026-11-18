# Oly architecture

This folder describes Oly as it is today: what each part does, how the parts connect and why the structure is this one. It does not tell the history of the project. The detailed requirements live in the specs ([Reference documents](#reference-documents)).

## Overview

Oly is a game engine written in C++20 for 2D games, with 3D transforms in `essentials3d`, in the browser and on the desktop. It is a study and benchmark project, published for the author's own games ([publishing.md](publishing.md)): every data representation decision has a measured cost.

The pieces:

- **Core**: the C++ engine (`core`, `ecs`, `input`, `app`) and its C API `oly*` (`capi`). The archetype ECS and all engine logic: fixed-step loop, phases, pause, events, input actions, schedule and states. It knows no environment.
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

## Index

| File | Contents |
|---|---|
| [repository.md](repository.md) | folder map and the shape of a module |
| [modules-and-boundaries.md](modules-and-boundaries.md) | dependency graph, how boundaries are enforced, vocabulary rules |
| [core.md](core.md) | base utilities (`OLY_ASSERT` and others) |
| [ecs.md](ecs.md) | the archetype ECS: layers, indices, costs, iteration, `CommandBuffer` |
| [input.md](input.md) | input actions and HID physical buttons |
| [app.md](app.md) | the `Engine`: loop, systems, order bands, pause, events, schedule, states, plugins, recovery |
| [plugins.md](plugins.md) | plugins: parts, install and ownership, order bands, schema merge, build, boundaries |
| [capi.md](capi.md) | the C API and the per-engine script invoker |
| [schema-and-codegen.md](schema-and-codegen.md) | the `.oly` schema, `oly-schema` and each game's WASM module |
| [ts-packages.md](ts-packages.md) | `@oly/core`, `@oly/web-host`, `@oly/testing` |
| [publishing.md](publishing.md) | versioning, the release pipeline, `oly-tools`, the `oly` launcher, local mode, `toolchain.env`, the consumer test |
| [build-and-tests.md](build-and-tests.md) | Docker, presets, tests and code quality |
| [performance.md](performance.md) | targets, current status and boundary costs |
| [decisions.md](decisions.md) | main architectural decisions, with reason and accepted cost |
| [limits.md](limits.md) | known limitations and what is out of scope |
| [glossary.md](glossary.md) | the same verb in C++, C API and TS |

To make a game with the engine: [../guide/README.md](../guide/README.md) (user guide with examples and recipes). How to write and maintain it: [../development/guide-style.md](../development/guide-style.md).

To work on the code: [../development/commands.md](../development/commands.md) (build, tests, benchmarks, games) and [../development/code-style.md](../development/code-style.md).

## Reference documents

The specs are the source of truth. The plans record, phase by phase, how each part was implemented and the decisions the specs left open.

| Document | Contents |
|---|---|
| `docs/superpowers/specs/2026-10-16-ecs-archetype-spec.md` | ECS: requirements `R-*`, invariants `I-1..I-13`, targets `NF-*` |
| `docs/superpowers/specs/2026-11-06-wasm-cost-study-design.md` | native × WASM cost study |
| `docs/superpowers/specs/2026-11-09-game-engine-design.md` | the engine: loop, events, input, schedule, states, validation game |
| `docs/superpowers/specs/2026-11-09-oly-modularization-design.md` | modules, boundaries, names and public surfaces |
| `docs/superpowers/specs/2026-11-13-multi-environment-design.md` | C++ first, environments and clients, `.oly`, native |
| `docs/superpowers/specs/2026-11-13-spur-task-runner-design.md` | the `./spur` task runner |
| `docs/superpowers/specs/2026-11-16-plugins-design.md` | plugins, order bands, schema merge, observers, `essentials` |
| `docs/superpowers/plans/` | implementation plans per phase |
| `CLAUDE.md` | index for Claude Code and rules that always apply |
| `docs/benchmarks/` | results and status of the targets |
