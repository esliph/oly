# Getting started on the web

[Guide index](../../README.md)

This page takes you from nothing to your own game running in the browser, in its own repository. The `web` environment runs the C++ engine as WASM, with the [TypeScript client](typescript/README.md) on top. For a game in a desktop window, see [Getting started on the desktop](../native/getting-started.md).

## What you need

On **Linux**: `curl`, `tar`, `git`, `make`, CMake, GCC, Node.js with npm and emsdk. On **Windows**: **Git Bash**, **MSYS2 UCRT64** (`gcc`, `cmake` and `ninja`), Node.js with npm and emsdk. Run every command from Git Bash, not from PowerShell or `cmd`. Docker is optional.

The exact versions, the install steps and the Docker option are in [The `oly` command](../../cli.md#what-you-install). `oly doctor` checks them for you.

## Create and run a game

```sh
curl -fsSL https://github.com/esliph/oly/releases/latest/download/install.sh | sh
```

Open a new terminal, then:

```sh
oly doctor --environment web
oly new <game> --environment web
cd <game>
oly test
oly run
```

`oly run` serves `http://localhost:8000`. Open it and press `W`, `A`, `S` and `D` to move the square.

> `oly run` needs an interactive terminal: esbuild stops when stdin closes.

Every command is described in [The `oly` command](../../cli.md). To build in Docker instead of with the tools of your machine, add `--docker`.

## How a game is built

You write the schema and the gameplay. `oly build` turns them into one WASM module and one JS bundle.

```
<game>.oly ──oly-schema──► schema.generated.hpp (C++) + web/schema.generated.ts (TS)
native/*.cpp  (your C++ systems, optional) ──┐
engine (ecs + app + input + capi + web_host) ┴──emcc──► build/web/<target>.mjs + .wasm
web/src/*.ts  (your TS systems and the shell) ──tsc + esbuild──► build/web/main.js
```

At start-up, `loadRuntime` checks that the WASM was built from the same schema as the TS code. If you change the schema and forget to rebuild, you get a `SchemaMismatchError` instead of corrupted data.

## What `oly new` created

```
<game>/
  oly.env                    the engine version, the environment, the target name and the runner
  CMakeLists.txt             fetches the engine at the version of oly.env
  cmake/game.cmake           oly_add_game and the test
  <game>.oly                 the schema: components, tags, events, actions, states
  native/                    C++ systems
  web/
    tsconfig.json            DOM-free code: schema and game logic
    tsconfig.browser.json    only src/main.ts, with the DOM
    schema.generated.ts      generated from <game>.oly by the build, not committed
    src/game.ts              installGame(engine, drawer): registers everything, no DOM
    src/main.ts              browser shell
    index.html
    tests/                   the smoke test
  package.json, tsconfig.json
  .github/workflows/ci.yml   installs oly and runs oly test --docker
```

`oly.env` and the `@oly` URLs in `package.json` carry the engine version. Change them with `oly upgrade <version>`, not by hand ([The `oly` command](../../cli.md#upgrade-the-engine)).

Keeping `game.ts` free of the DOM is what lets you [test the game in Node](testing.md). The two `tsconfig` files enforce it: using `document` outside `main.ts` is a compile error.

### `<game>.oly`

```
tag Player
action move: axis2d
```

`Position` is not declared here: `DIMENSION 2d` in `cmake/game.cmake` turns the `essentials2d` plugin on, which brings it ([Transforms](../../concepts/transforms.md)).

### `cmake/game.cmake`

```cmake
file(GLOB nativeSources CONFIGURE_DEPENDS native/*.cpp)

oly_add_game(<target>
  ENVIRONMENT web
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/<game>.oly
  SOURCES ${nativeSources}
  DEPENDS ecs app capi
  TYPESCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/web/schema.generated.ts)

add_test(NAME <target>.smoke COMMAND ${OLY_NODE} --test ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/tests/)
set_tests_properties(<target>.smoke PROPERTIES ENVIRONMENT OLY_GAME_MODULE=$<TARGET_FILE:<target>>)
```

`<target>` is the game name with `-` written as `_`; it is also the name of the `.mjs` that `main.ts` loads. `ENVIRONMENT` says where the game runs (`web` or `native`), `DIMENSION` is `2d` or `3d`, and `TYPESCRIPT` names the TS schema file the build writes. An optional `PLUGINS <plugin>...` turns [plugins](../../concepts/plugins.md) on: their types reach `schema.generated.ts` and their systems run in the WASM module, with no TS code.

### `native/movement.cpp`

C++ is the right place for code that touches every entity each step. This system moves every `Player` by the `move` action:

```cpp
#include "oly/app/system.hpp"
#include "oly/ecs/query.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"

namespace {
  // Pixels per second; an initial value, not measured.
  constexpr float PlayerSpeed = 200.0f;
}

OLY_SYSTEM(movePlayer) {
  const oly::input::Vector2 axis = context.actions().axis2d(actions::move);
  const float step = PlayerSpeed * context.fixedDeltaTime();

  context.query<Position>(oly::ecs::With<Player>{}).each([&](Position& position) {
    position.x += axis.x * step;
    position.y += axis.y * step;
  });
}
```

`schema.generated.hpp` is created by the build from `<game>.oly`. It has the `Position` and `Player` structs and the `actions::move` handle.

### `web/src/game.ts`

```ts
import { Phase } from "@oly/core";
import type { Engine } from "@oly/core";

import { Player, Position, move } from "../schema.generated.js";

const PlayerSize = 16;

export interface Drawer {
  clear(): void;
  square(x: number, y: number, size: number): void;
}

export function installGame(engine: Engine, drawer: Drawer): void {
  engine.actions.bind(move, { up: "KeyW", down: "KeyS", left: "KeyA", right: "KeyD" });
  engine.world.spawn([Player, Position], { Position: { x: 320, y: 180 } });

  engine.addNativeSystem(Phase.Update, "movePlayer");

  engine.addSystem(Phase.Render, "draw", (context) => {
    drawer.clear();
    context.query({ read: [Position], with: [Player] }).eachBlock((block) => {
      const position = block.read(Position);
      for (let row = 0; row < block.count; ++row) {
        drawer.square(position.x.get(row), position.y.get(row), PlayerSize);
      }
    });
  });
}
```

`installGame` registers the C++ system by name (`addNativeSystem`) and a TS render system. It draws through a `Drawer` it receives, so `main.ts` gives it a canvas and a test gives it a recorder.

### `web/src/main.ts`

`main.ts` finds the canvas, then loads the WASM, creates the engine, calls `installGame` with a canvas `Drawer`, attaches the browser input and starts the loop:

```ts
const runtime = await loadRuntime(moduleFactoryFromUrl("./<target>.mjs"), schema);
const engine = runtime.createEngine({ fixedHz: 60 });
installGame(engine, { clear, square });   // functions that draw on the canvas
attachBrowserInput(engine, canvas);
runInBrowser(engine);
```

### The test

`oly test` builds the game and runs `web/tests/smoke.test.ts` against the real WASM: it holds `KeyD` for one second and checks that the player moved by the player speed. `cmake/game.cmake` registers it with ctest. [Testing a web game](testing.md) explains how to write more.

## How to split work between C++ and a client

On the web, a game is C++ systems plus a client (TypeScript today). Both can live in the same phase, in the order you register them.

| Put it in C++ | Put it in the client |
|---|---|
| runs for every entity, every step | runs once per step, or rarely |
| movement, collision, AI, weapons | render, HUD, menus |
| tight math | score, waves, game rules that change often |
| | bindings, state hooks that talk to the page |

Why: a call across the WASM boundary has a fixed cost, so per-entity work belongs next to the data ([The WASM↔JS boundary](boundary.md)). A game can also use only TS systems (prototypes, few entities: the `native/` folder can be empty), or only C++ with no client at all: that is a [desktop game](../native/getting-started.md) or a tool, with an `oly::app::Engine` over an `oly::ecs::World`.

### Deterministic randomness

Use `std::minstd_rand` with a fixed seed. Its sequence is the same in every standard library; the `std::*_distribution` classes are not, so use `generator() % 100` or your own mapping.

### Shared constants

Values used by both languages (world size, bullet speed) live in a C++ header (`native/constants.hpp`) and a TS module (`src/constants.ts`). Keep them side by side and say in each file that the other one must change with it.

## Advanced: your own exported C functions

A game can export extra `extern "C"` functions with `OLY_API`. From an `OlyWorld*` (the C API handle), `oly::capi::worldOf(world)` gives the `oly::ecs::World&` behind it, and `oly::capi::engineOf(engine)` gives the `Engine&`. The engine's test fixture (`engine/testing/fixture/systems.cpp`) shows this. Calling such functions from TS needs the module internals (`@oly/core/internal`), which are reserved for the engine; in game code, prefer `OLY_SYSTEM`, `OLY_HANDLER` and `OLY_STATE_HOOK` ([Engine, loop and systems](../../concepts/engine-and-systems.md#systems-in-c-registered-by-name-oly_system-oly_handler-oly_state_hook)).

## Inside the engine repository

This section is for people who change the engine. A game always lives in its own repository; to build it against your checkout instead of a release, use [local mode](../../cli.md#build-with-an-engine-checkout). The engine's toolchain is Docker only, through `./spur`: Git Bash and Docker are all you need ([commands](../../../development/commands.md)).

```sh
./spur images             # once, and again after toolchain.env changes
./spur games-test         # builds the WASM and the TS and runs the web tests of the engine
```

## Next

[TypeScript client](typescript/README.md): the TS API, page by page. [Schema](../../concepts/schema.md): every kind of data you can declare. [Testing a web game](testing.md): run the game in Node.
