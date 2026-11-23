# Test a web game

[Docs](../README.md#how-to-guides) · Concept: [Web boundary](../explanation/concepts/web-boundary.md)

Use this to run the real game, with the real WASM, in Node: you choose the time, you press the keys, you read the world. Tests are exact and fast, with no browser and no waiting. It assumes a web game made by `oly new <game> --environment web` ([Your first web game](../tutorials/first-web-game.md)), whose `web/tests/smoke.test.ts` is the first test.

## What makes a game testable

1. **`startGameplay(engine, drawer)` has no DOM.** It registers systems, handlers and hooks only. `gameplay.ts` has no DOM; the class in `game.ts` is the only code that touches the canvas.
2. **Everything the outside world provides is injected.** The template injects the drawer. Anything else that would read the clock or a random source is a parameter of the same kind.

This is the signature of `web/src/gameplay.ts` (an excerpt):

```ts
export interface Drawer {
  clear(): void;
  square(x: number, y: number, size: number): void;
}

export function startGameplay(engine: Engine, drawer: Drawer): void {
```

In the browser, `MyGame.onStart` in `game.ts` passes a canvas drawer. In a test, you pass a recorder.

## A test harness

The smoke test builds everything it needs in one file. A small helper keeps further tests short. Put it in `web/tests/harness.ts`:

```ts
import { pathToFileURL } from "node:url";

import { loadRuntime } from "@oly/core";
import type { ButtonName, Engine, OlyModuleFactory, Runtime, SchemaType } from "@oly/core";

import schema from "../schema.generated.js";
import { startGameplay } from "../src/gameplay.js";
import type { Drawer } from "../src/gameplay.js";

const StepsPerSecond = 60;

interface Square {
  readonly x: number;
  readonly y: number;
  readonly size: number;
}

/** Keeps the calls made since the last `clear`, which is what one frame drew. */
export class RecordingDrawer implements Drawer {
  readonly squares: Square[] = [];

  clear(): void {
    this.squares.length = 0;
  }

  square(x: number, y: number, size: number): void {
    this.squares.push({ x, y, size });
  }
}

function gameFactory(): OlyModuleFactory {
  const modulePath = process.env.OLY_GAME_MODULE;
  if (!modulePath) {
    throw new Error("set OLY_GAME_MODULE to the path of the game's .mjs (ctest already does this)");
  }

  return async () => {
    const loaded: { default: OlyModuleFactory } = await import(pathToFileURL(modulePath).href);

    return loaded.default();
  };
}

export function loadGame(): Promise<Runtime> {
  return loadRuntime(gameFactory(), schema);
}

export interface GameTest {
  readonly engine: Engine;
  readonly drawer: RecordingDrawer;
  step(count?: number): void;
  tap(button: ButtonName): void;
  count(...types: SchemaType[]): number;
}

export function withGame(runtime: Runtime, body: (game: GameTest) => void): void {
  const engine = runtime.createEngine({ fixedHz: StepsPerSecond });
  const drawer = new RecordingDrawer();
  startGameplay(engine, drawer);
  engine.frame(0);
  let stepNumber = 0;

  const game: GameTest = {
    engine,
    drawer,
    step(count = 1) {
      for (let index = 0; index < count; ++index) {
        ++stepNumber;
        engine.frame(stepNumber / StepsPerSecond);   // exactly one fixed step

        const broken = engine.world.checkInvariants();
        if (broken !== null) {
          throw new Error(`invariant broken at step ${stepNumber}: ${broken}`);
        }
      }
    },
    tap(button) {
      engine.actions.buttonDown(button);
      engine.actions.buttonUp(button);
      game.step();
    },
    count(...types) {
      let total = 0;
      engine.world.query({ with: types }).eachBlock((block) => {
        total += block.count;
      });

      return total;
    },
  };

  try {
    body(game);
  } finally {
    engine.dispose();
  }
}
```

Key ideas:

- `engine.frame(stepNumber / 60)` with `fixedHz: 60` runs **exactly one** step per call.
- `checkInvariants()` after every step catches ECS corruption at the step that caused it.
- `dispose()` in `finally` frees the WASM memory even when an assertion fails.

## Write tests

Tests use `node:test` and `node:assert`, like the engine's own tests.

```ts
import assert from "node:assert/strict";
import { test } from "node:test";

import { Player, Position } from "../schema.generated.js";
import { loadGame, withGame } from "./harness.js";

const PlayerSpeed = 200;
const Tolerance = 1e-3;

const runtime = await loadGame();

test("the game starts with one player and draws one square", () => {
  withGame(runtime, (game) => {
    game.step();

    assert.equal(game.count(Player), 1);
    assert.equal(game.drawer.squares.length, 1);
  });
});

test("holding KeyD for one second moves the player right by the player speed", () => {
  withGame(runtime, (game) => {
    game.engine.actions.buttonDown("KeyD");

    game.step(60);

    const player = game.engine.world.query({ with: [Player] }).single().entity;
    const position = game.engine.world.get(player, Position);
    assert.ok(Math.abs(position.x - (320 + PlayerSpeed)) <= Tolerance, `x is ${position.x}`);
  });
});
```

### Set up a scene directly

Between frames, `engine.world` accepts direct changes. Use it to place entities exactly where the test needs them:

```ts
test("a second player is drawn too", () => {
  withGame(runtime, (game) => {
    game.engine.world.spawn([Player, Position], { Position: { x: 100, y: 100 } });

    game.step();

    assert.equal(game.drawer.squares.length, 2);
  });
});
```

Entities created with `engine.world.spawn` are never scoped to a state, so they survive a state change. Destroy them yourself if a later part of the test changes state.

### Simulate input

| You want | Do |
|---|---|
| a short press | `buttonDown` + `buttonUp`, then one step: the step sees `pressed` and `released` |
| holding | `buttonDown`, step N times, `buttonUp` |
| aim | `engine.actions.pointerMove(x, y)` before the step |
| alt-tab | `engine.actions.releaseAll()` or `engine.setHidden(true)` |

```ts
game.engine.actions.buttonDown("KeyD");
game.step(30);                                  // half a second of movement
game.engine.actions.buttonUp("KeyD");
```

### Check what was drawn

`RecordingDrawer` keeps the calls of the last frame. `squares` has one entry per `square` call, with its `x`, `y` and `size`:

```ts
assert.equal(game.drawer.squares[0]?.size, 16);
```

### Deterministic randomness

When the game uses randomness, inject it like the drawer: take a `random: () => number` parameter and pass `Math.random` in `game.ts`. In a test, pass a function that cycles over fixed values:

```ts
function sequence(values: readonly number[]): () => number {
  let index = 0;

  return () => values[index++ % values.length]!;
}
```

## Run the tests

A game made with `oly new` already registers its tests: `cmake/game.cmake` has the ctest test that runs `node --test web/dist/tests/` with `OLY_GAME_MODULE` set, and `web/tests/tsconfig.json` emits the compiled tests to `web/dist/tests/`. Put a new `*.test.ts` file in `web/tests/` and run:

```sh
oly test
```

`oly test` builds the game, compiles the TS tests and runs ctest. To run only the tests of the game by their ctest name, use `ctest` on the build `oly test` left:

```sh
ctest --test-dir build/cmake -R '^<target>\.'
```

## Test C++ systems alone

A C++ system with no WASM and no TS is tested on the desktop side: see [Test a desktop game](test-a-desktop-game.md).

## See also

- [Keep the hot path fast](keep-the-hot-path-fast.md#typescript): the techniques for the TypeScript side of the WASM boundary.
- [Errors](../reference/errors.md): what each error means.
- [The game template](../reference/game-template.md#web): the files of the smoke test.
