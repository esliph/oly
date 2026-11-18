# Oly

Oly is a game engine in C++20 for 2D games, with 3D transforms in `essentials3d`. The core is an archetype-based, single-threaded ECS (*Entity Component System*) with the whole engine on top of it: fixed-step loop, events, input actions, schedule and states. The core runs in **environments**: `web` (compiled to WebAssembly, with a TypeScript **client** and a web host for the browser and Node) and `native` (a desktop host in C++, in a window on Linux and Windows). A C API `oly*` lets a client in another language drive the core.

It is a study project. The goal is to measure how data representation (SoA, archetypes, aligned columns) and architectural decisions affect execution cost. That is why every performance target has a benchmark and a recorded result.

> **Status:** in development. The ECS, the `Engine`, the C API, the TypeScript client and the web and native hosts work and have tests, and there is an example game per environment (`arena` on `web`, `drift` on `native`). Oly does not draw, play audio or do physics: the game does. The packages are not published on npm yet. The API may change.

## What already exists

| Part | Where | What it does |
|---|---|---|
| `oly::core` | `engine/runtime/core` | Base utilities: `OLY_ASSERT`, alignment, `small_map`, type name. |
| `oly::ecs` | `engine/runtime/ecs` | The ECS: `World`, `Entity`, cached queries, `CommandBuffer`, invariant diagnostics. |
| `oly::input` | `engine/runtime/input` | Input actions (`ActionMap`) over physical buttons. |
| `oly::app` | `engine/runtime/app` | The `Engine`: loop, phases, systems, events, schedule and states. |
| `oly::capi` | `engine/runtime/capi` | C API (`oly*`) for clients in other languages. |
| `oly::native_host` | `engine/environments/native/host` | The `native` environment's host: window, input and the run loop. |
| `@oly/core` | `engine/environments/web/clients/typescript/core` | The TypeScript client: schema, `World`, `Query`, `Block`, `Engine`. |
| `@oly/web-host` | `engine/environments/web/host/js` | The `web` environment's host: loads the WASM, drives the loop, forwards input. |
| oly-schema | `engine/schema/codegen` | Generates the C++ and each client's code from a game's `.oly` schema. |

## How the ECS works

- Each combination of components forms an **archetype**. Each archetype stores its entities in an SoA table: one contiguous column per component, aligned to 64 bytes.
- Adding or removing a component moves the entity to another archetype. Transitions are cached as edges of the archetype graph.
- A **query** is cached by the `World` and only visits the archetypes that match it. New archetypes join existing queries incrementally.
- Structural changes during an iteration are a contract violation. That is what the **`CommandBuffer`** is for: it records the changes and `flush` applies a single transition per entity.
- Only the `Entity` (index + generation) is a stable handle. Pointers and views are valid until the next structural change.

## Examples

### C++

```cpp
#include "oly/ecs/world.hpp"

struct Position { float x, y; };
struct Velocity { float x, y; };
struct Frozen {};  // tag: component with no data

oly::ecs::World world;
oly::ecs::Entity player = world.spawn(Position{0, 0}, Velocity{1, 2});

// Iterates only entities with Position and Velocity and without the Frozen tag.
world.query<Position, const Velocity>(oly::ecs::Without<Frozen>{})
    .each([](Position& position, const Velocity& velocity) {
      position.x += velocity.x;
      position.y += velocity.y;
    });

// Structural changes during iteration go through the CommandBuffer.
oly::ecs::CommandBuffer commands = world.commands();
world.query<const Position>().each([&](oly::ecs::Entity entity, const Position& position) {
  if (position.x > 100) {
    commands.destroy(entity);
  }
});
world.flush(commands);
```

### TypeScript

The game declares the schema in a `.oly` file. `oly-schema` generates the C++ and the TS from it.

```
# game.oly
component Velocity { x: f32, y: f32 }
tag Enemy
```

```ts
import { loadRuntime } from "@oly/core";
import { moduleFactoryFromUrl } from "@oly/web-host";

import schema, { Position, Velocity, Enemy } from "./schema.generated.js";

// Throws SchemaMismatchError if the WASM was generated with another schema.
const runtime = await loadRuntime(moduleFactoryFromUrl("/game.mjs"), schema);
const world = runtime.createWorld();

const enemy = world.spawn([Position, Velocity, Enemy], {
  Position: { x: 0, y: 0 },
  Velocity: { x: 1, y: 2 },
});

world.query({ read: [Velocity], write: [Position] }).eachBlock((block) => {
  const velocity = block.read(Velocity);
  const position = block.write(Position);
  for (let row = 0; row < block.count; ++row) {
    position.x.set(row, position.x.get(row) + velocity.x.get(row));
    position.y.set(row, position.y.get(row) + velocity.y.get(row));
  }
});

console.log(world.get(enemy, Position)); // { x: 1, y: 2 }
world.dispose();
```

The views from `block.read` and `block.write` access WASM memory directly, without copying, and are only valid inside the callback. TypeScript validates usage before calling into C++ and throws `ContractError` when usage is invalid.

A complete example, with C++ systems called from TypeScript, is in [`engine/testing/fixture/`](engine/testing/fixture) and in the tests under [`engine/testing/tests/wasm/`](engine/testing/tests/wasm).

## Build and test

The requirements on the host are **Git Bash** and **Docker** with Docker Compose. The images ship GCC, CMake, `perf`, Emscripten and Node, in the versions of `toolchain.env`. Run `./spur images` once; `./spur --list` shows every task.

```sh
# Native C++: builds and runs all tests with ASan + UBSan.
./spur test

# Only one module or one suite (the arguments go to ctest).
./spur test -R '^ecs\.'

# ECS in WebAssembly + TypeScript packages + integration tests.
./spur games-test
```

`-x` and `-n` given on the host show only the dispatch to the container; trace a task with `./spur dev ./spur -x test`.

The build goes to `build/<preset>/`. The presets are in [`CMakePresets.json`](CMakePresets.json). The main ones are `debug-asan` (default), `release` and `wasm-games`.

## Benchmarks

Performance targets are ratios against a baseline measured in the same run, for example: "iterating in the ECS costs at most 1.10× the cost of a hand-written SoA loop".

```sh
PRESET=release ./spur build
./spur dev ./build/release/bench/ecs_bench_iterate
```

The recorded results and the status of each target are in [`docs/benchmarks/README.md`](docs/benchmarks/README.md).

## Documentation

| Where | What it has |
|---|---|
| [`docs/guide/`](docs/guide/README.md) | user guide: how to make a game, step by step, with examples, recipes and a cheat sheet |
| [`docs/architecture/`](docs/architecture/README.md) | architecture: overview, modules and boundaries, ECS, `Engine`, `capi`, TS packages, games, decisions and limitations |
| [`docs/architecture/repository.md`](docs/architecture/repository.md) | repository structure |
| [`docs/development/commands.md`](docs/development/commands.md) | all commands for build, tests, benchmarks and games |
| [`docs/development/code-style.md`](docs/development/code-style.md) | code style |
| [`docs/benchmarks/`](docs/benchmarks/README.md) | benchmark results |
| [`docs/superpowers/specs/`](docs/superpowers/specs) | specs (source of truth for the requirements) |
