# Your first web game

[Docs](../README.md#tutorials) · Next: [Your first desktop game](first-desktop-game.md)

## What you will build

A white square that you move with `W`, `A`, `S` and `D` in the browser. The game lives in its own repository. Its movement is a C++ system that runs as WASM, and its drawing is a TypeScript system. You will read every file `oly new` writes, change two numbers and see the result.

## Before you start

You need a computer with these tools. In step 1, `oly doctor` checks `cmake`, `ninja` (Windows only), `emcc`, `node` and `npm`. It does not check the other tools in this list.

- On Linux: `curl`, `tar`, `git`, `make`, CMake, GCC, Node.js with npm, and emsdk.
- On Windows: Git Bash, MSYS2 UCRT64 (`gcc`, `cmake` and `ninja`), Node.js with npm, and emsdk. Run every command from Git Bash, not from PowerShell or `cmd`.
- Docker is optional: add `--docker` to a command to build in a container instead.

The exact versions are in [The `oly` command](../reference/cli.md#install).

## 1. Check your tools

```sh
curl -fsSL https://github.com/esliph/oly/releases/latest/download/oly | sh -s -- doctor --environment web
```

There is nothing to install first: the command downloads the launcher of the latest release and runs its `doctor` outside a game. You see one line per tool. Each line starts with `ok`, `old`, `untested`, `missing` or `wrong`, then the tool and what to install. Fix every line that does not say `ok`, then run the command again.

## 2. Create the game

```sh
curl -fsSL https://github.com/esliph/oly/releases/latest/download/oly | sh -s -- new <game> --environment web
cd <game>
```

`<game>` is the name of the folder: lowercase letters, digits and `-`, starting with a letter. The command downloads the launcher, which copies [the game template](../reference/game-template.md) into the folder, installs the npm packages and leaves itself in the game as `./oly`.

You see `created <game> (web, oly <version>)`, and a folder with `oly`, `oly.env`, `<game>.oly`, `native/`, `web/` and the other files of the template. Run `./oly` from inside it: it is the launcher of this game.

## 3. Run the test

```sh
./oly test
```

`oly test` builds the game and runs its tests. The test `<target>.smoke` loads the real WASM in Node, holds `KeyD` for one second and checks that the player moved right.

You see ctest report `<target>.smoke` as passed.

## 4. Run the game

```sh
./oly run
```

`oly run` builds the game and serves `http://localhost:8000`.

> `oly run` needs an interactive terminal: esbuild stops when stdin closes.

Open the address and press `W`, `A`, `S` and `D`. You see a white square in the middle of the page, and it moves with the keys. Press `Ctrl+C` in the terminal to stop the server.

## 5. Read the schema

The schema declares the data of the game. This is `<game>.oly`:

```
tag Player
action move: axis2d
```

`Player` marks the entity that moves. `move` is an input action with two axes. `Position` is not declared here: `DIMENSION 2d` in `cmake/game.cmake` turns the `essentials2d` plugin on, and the plugin brings it ([Transforms](../explanation/concepts/transforms.md)).

The build turns this file into `schema.generated.hpp` for C++ and `web/schema.generated.ts` for TypeScript, so both languages agree on the data. At start-up, `loadRuntime` checks that the WASM and the TS code come from the same schema.

You see the result in `web/schema.generated.ts`: open it and find `Player`, `Position` and `move`.

## 6. Read the C++ system

C++ is the right place for code that touches every entity each step. This is `native/movement.cpp`:

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

`OLY_SYSTEM(movePlayer)` defines the system and registers it by name, so the TS side can add it. The query visits every entity that has `Position` and the tag `Player`. `native/systems.hpp` declares the function:

```cpp
#pragma once

#include "oly/app/system.hpp"

// Registered by name through OLY_SYSTEM; declared here for a shell that adds it with Engine::addSystem.
void movePlayer(oly::app::SystemContext& context);
```

Change `PlayerSpeed` to `400.0f` and run `./oly run` again. You see the square cross the page twice as fast.

## 7. Read the TypeScript system

This is `web/src/gameplay.ts`. It knows nothing about the browser:

```ts
import { Phase } from "@oly/core";
import type { Engine } from "@oly/core";

import { Player, Position, move } from "../schema.generated.js";

const PlayerSize = 16;

export interface Drawer {
  clear(): void;
  square(x: number, y: number, size: number): void;
}

export function startGameplay(engine: Engine, drawer: Drawer): void {
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

`startGameplay` does four things. It binds the keys to `move`. It spawns the player in the middle of the canvas. It adds the C++ system `movePlayer` by name, with `addNativeSystem`. It adds a TS system that draws in the `Render` phase. The system draws through the `Drawer` it receives, so the browser gives it a canvas and a test gives it a recorder ([Test a web game](../how-to/test-a-web-game.md)).

The drawing system reads positions in blocks with `eachBlock`: one call across the WASM boundary for many entities ([Web boundary](../explanation/concepts/web-boundary.md)).

Change `PlayerSize` to `32` and run `./oly run` again. You see a square twice as wide.

## 8. Read the browser shell

Two files touch the page. `web/src/game.ts` holds the game as a class, `MyGame`. `web/src/main.ts` starts it with one call. This is `game.ts`:

```ts
import type { Engine } from "@oly/core";
import { Game } from "@oly/web-host";

import schema from "../schema.generated.js";
import { startGameplay } from "./gameplay.js";

const Width = 640;
const Height = 360;

export class MyGame extends Game {
  constructor() {
    super({ canvas: "game", module: "./<target>.mjs", schema, width: Width, height: Height });
  }

  onStart(engine: Engine, canvas: HTMLCanvasElement): void {
    const context = canvas.getContext("2d");
    if (context === null) {
      throw new Error("the browser did not provide a 2D context for the canvas");
    }

    startGameplay(engine, {
      clear: () => context.clearRect(0, 0, canvas.width, canvas.height),
      square: (x, y, size) => {
        context.fillStyle = "#ffffff";
        context.fillRect(x - size / 2, y - size / 2, size, size);
      },
    });
  }
}
```

And this is `main.ts`:

```ts
import { runGame } from "@oly/web-host";

import { MyGame } from "./game.js";

void runGame(new MyGame());
```

`MyGame` derives `Game` from `@oly/web-host` and gives its options to `super`: the id of the canvas, the `.mjs` the build wrote, the schema and the canvas size. `<target>` is the game name with `-` written as `_`, which is also the name of the `.mjs`. `onStart` gets the 2D context of the canvas and calls `startGameplay` with a `Drawer` over it. Rename `MyGame` to fit your game.

`runGame` does the rest, in order: it finds the canvas and sets its size, loads the WASM, creates the engine, calls `onStart`, attaches the browser input and starts the loop. When a step fails, it writes `Error starting the game: <message>` into the page and rejects with the same error ([Web host](../reference/typescript/web-host.md#rungame)).

Two `tsconfig` files keep the DOM out of the gameplay: `web/tsconfig.json` has no DOM types and leaves out `src/main.ts` and `src/game.ts`, and `web/tsconfig.browser.json` has them for those two files only. You see the rule at work if you write `document` in `gameplay.ts` and run `./oly build`: the build stops with a compile error.

## Next steps

- [The game template](../reference/game-template.md): every file of the game, one row each.
- [Test a web game](../how-to/test-a-web-game.md): write more tests like the smoke test.
- [Schema](../explanation/concepts/schema.md): every kind of data you can declare.
- [Environments and hosts](../explanation/concepts/environments-and-hosts.md): how the core, the web host and the client fit together.
- [Web boundary](../explanation/concepts/web-boundary.md): which work belongs in C++ and which in TypeScript.
- [The `oly` command](../reference/cli.md): every command.
