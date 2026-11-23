# Oly — the `Game` class and one call to start a game — design

Date: 2026-11-23. Base: `feat/bootstrap` at `f446bde`.

Marking: **[user decision]** was decided in the conversation; **[author decision]** was my choice during the design,
which the user may review; the rest is a direct consequence of the two. Claims about the code carry **[fact]** (read
in the file named) or **[inference]**.

## 1. Intent

Make the startup code a game writes small, and give the game a class that represents it **[user decision]**. Today
most of a game's `main` is the same in every game: parse the headless option, build the engine, set it up in the right
order, open the window or find the canvas, attach input, run the loop and report errors. Only the content differs: key
bindings, the first entities, the systems and the drawing. And there is no place for the game itself: its
configuration and its own state have no home but `main` and globals.

After this change a game derives one class from `Game`, holds its configuration and state there, writes its content
in `onStart`, and hands the class to one call, `runGame`, in each environment.

This is the first of two steps **[user decision]**. The second, with its own design, gives the parts of a game
(player, enemies, HUD) classes of their own, which the game's `onStart` installs (§3, non-goals).

## 2. Current state

| Where | What the game writes today |
|---|---|
| `templates/game/native/desktop/main.cpp` **[fact]** | About 70 lines: `parseOptions` for `--frames` (exit code 64 on a wrong use), `World`, `Engine`, `oly::app::setUpGame(engine)`, `bind`, `spawn`, `addSystem("movePlayer")`, `Window`, `addSystem("draw", draw, window.renderer())`, `oly::native_host::run`, and a `try/catch` that prints `<game>: <message>` and returns 1. |
| `templates/game/web/web/src/main.ts` **[fact]** | Find the canvas, set its size, get the 2D context, `loadRuntime(moduleFactoryFromUrl("./<game>.mjs"), schema)`, `runtime.createEngine({ fixedHz: 60 })`, `installGame(engine, drawer)`, `attachBrowserInput`, `runInBrowser`, and a `catch` that writes the error into the page. |
| `templates/game/web/web/src/game.ts` **[fact]** | `installGame(engine, drawer)`: the content, with a `Drawer` interface so `tests/smoke.test.ts` can run it in Node. |

Two asymmetries:

- On the web, `olyEngineCreate` already calls `setUpGame` (`engine/runtime/capi/src/engine_api.cpp:127`) **[fact]**,
  so `createEngine` returns a set-up engine. On native, the game must call `setUpGame` itself, first, on an engine that
  has declared nothing (`docs/tutorials/first-desktop-game.md`). An ordering contract every native game must remember.
- The `--frames` option exists so ctest runs the game headless (`templates/game/native/cmake/game.cmake`,
  `add_test(... --frames 120)`) **[fact]**. Every native game copies its parser.

## 3. Goals, non-goals, anti-goals

Goals:

- A `Game` base class and a `runGame` in each host: `oly::native_host` (C++) and `@oly/web-host` (TypeScript).
- Both templates use them; the docs and tutorials follow.

Non-goals:

- **The parts of a game as classes.** Step two, its own design **[user decision]**. Two findings it must settle
  **[fact]**: `oly::app::Plugin` (`install(Engine&)`, owned by the `Engine`) already has the shape, but
  `docs/explanation/concepts/plugins.md` defines a plugin as engine functionality turned on in CMake ("No line of game
  code installs it"); and the TypeScript client has no plugin concept ("A plugin has no TypeScript form of its own").
  Nothing in this step depends on how step two answers them: the game's `onStart` will just install its parts.
- Lifecycle hooks other than `onStart` (`onStop`, …). A later hook with an empty default body breaks no game, so it
  waits for a real case **[author decision]**.
- Registering systems with their phase in `OLY_SYSTEM` (option C, §4). Possible later, separately.
- Generating `main` in the build (option B, §4).
- Changing `Engine`, `World`, `run`, `Window`, `RunOptions`, `runInBrowser`, `attachBrowserInput` or `loadRuntime`.
  Tests, tools and custom `main`s keep using them as today ([Drive the loop yourself](../../how-to/drive-the-loop-yourself.md)).

Anti-goals (a failure even if it works):

- Hiding `main` from the game.
- The engine deciding how the game draws (for example handing a Canvas 2D context to `onStart`).
- A startup whose order depends on link order or static initialization.
- A `Game` that outlives its welcome: the class holds configuration and the game's own state; gameplay data stays in
  the ECS.

## 4. Approaches

| Approach | What it is | Trade-off |
|---|---|---|
| **A. One host call per environment, around a `Game` class** (chosen **[user decision]**) | The host builds and wires everything; the game derives `Game` and overrides `onStart`. | `main` drops to one line and stays the game's. The class gives configuration and state a home, and members reach systems through `userData = this`. The game can still use the pieces instead. |
| A0. The same call around a free function `onStart` | First version of A in the conversation. | No home for the game's state: a native `onStart` would need `std::function` or a `void* userData` to carry it. Superseded by the class **[user decision]**. |
| B. Generated `main` | `oly_add_game` writes `main.cpp`, as it writes `setUpGame`. | Less code still, but `main` is hidden: a new argument or a second window needs a new CMake option, and the tutorial loses the step that shows how a game starts. |
| C. `OLY_SYSTEM(name, phase)` | `setUpGame` adds every registered system. | Registration runs in static-initialization order, which follows the order the `.cpp` files are linked, so system order within a phase would depend on link order **[inference]**: determinism lost. Compatible with A; left out. |

## 5. Design

### 5.1 Names

- `Game` **[user decision]**: the class a game derives. "Game" is an engine concept already (`setUpGame`,
  `oly_add_game`), not a game's own name, so it may live in `engine/` (CLAUDE.md: game names only in
  `engine/testing/fixture/`).
- `onStart` **[user decision]**: the method a game overrides. `install` means installing a plugin in the glossary,
  and `initialize` is too close to `setUpGame`. `onStart` follows the `on…` names the `Engine` uses for functions it
  calls (`on`, `onEnter`, `onExit`) and says when it runs: once, before the first frame.
- `runGame` **[author decision]**: the host call, the same name in both environments. On the web it sits beside
  `runInBrowser`, which stays the loop only.

### 5.2 Native: `oly::native_host::Game` and `runGame`

New header `oly/native_host/game.hpp` **[author decision]**:

```cpp
#pragma once

#include <type_traits>

#include "oly/app/engine.hpp"
#include "oly/native_host/run.hpp"
#include "oly/native_host/window.hpp"

namespace oly::native_host {
  // The game itself: its window, its engine configuration and its own state. runGame constructs it before the
  // Engine and destroys it after, so systems may keep a pointer to it in userData.
  class Game {
   private:
    WindowConfig window_;
    oly::app::EngineConfig engine_;

   public:
    explicit Game(WindowConfig window, oly::app::EngineConfig engine = {});
    virtual ~Game() = default;
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    const WindowConfig& windowConfig() const noexcept;
    const oly::app::EngineConfig& engineConfig() const noexcept;

    // Called once, on a set-up Engine, before the first frame.
    virtual void onStart(oly::app::Engine& engine, Window& window) = 0;
  };

  // Builds the World and the Engine, sets up the game, opens the window, calls onStart and runs the loop.
  int runGame(Game& game, RunOptions options = {});

  // Parses --frames, then constructs G and runs it. Returns 64 on a wrong command line and 1 when an exception escapes,
  // the constructor's included; otherwise what run returns.
  template <class G>
  int runGame(int argumentCount, char** arguments);
}
```

`runGame(Game&, RunOptions)`, in this order: build a `World` and an `Engine` with `game.engineConfig()`, call
`oly::app::setUpGame`, open the `Window` with `game.windowConfig()`, call `game.onStart(engine, window)`, then return
`run(engine, window, options)`. It does not catch: it is the call for a game that writes its own `main`, for example
to parse options of its own **[author decision]**.

`runGame<G>(argumentCount, arguments)`, in this order:

1. `static_assert` that `G` derives from `Game` and is default constructible, with messages like `installPlugin`'s.
2. Parse the command line. The only option is `--frames <n>`, `n > 0`. Anything else prints
   `usage: <argv[0]> [--frames <n>]` to stderr and returns 64, before anything is constructed.
3. In a `try`: construct `G`; with `--frames`, set `maxFrames = n` and `frameSeconds = 1.0 / game.engineConfig().fixedHz`
   **[author decision]** (the template used `1.0 / 60` with the default `fixedHz` of 60; tying it to the config keeps
   one step per simulated frame when a game changes `fixedHz`); return `runGame(game, options)`.
4. A `std::exception` that escapes prints `<title>: <what()>` to stderr and returns 1, where `<title>` is the window
   title, or `argv[0]` when the constructor threw.

The first frame only records the time (`engine/environments/native/host/src/run.cpp`, `docs/reference/native-host.md`)
**[fact]**, so `--frames n` runs `n - 1` steps; any new doc text says so.

Lifetime: `G` is a local of `runGame<G>`, constructed before the `World` and the `Engine` (locals of `runGame(Game&)`),
so it is destroyed after them, and a system that holds `this` in `userData` never sees a destroyed game. A game that
writes its own `main` gets the same guarantee by declaring the game before calling `runGame(game, options)`.

Link: `native_host` is a `STATIC` module (`oly_add_module(NAME native_host ...)` without `OBJECT`,
`cmake/OlyModule.cmake`) **[fact]**, and `setUpGame` is defined only by a game's generated `game_setup.generated.cpp`
**[fact]**. So `runGame(Game&, RunOptions)` and `Game` go in their own source file, `src/game.cpp`
**[author decision]**: the linker then pulls it only into executables that use them, and the existing host tests
(`window`, `input`) still link without a `setUpGame` **[inference]**. The test that calls `runGame` defines
`oly::app::setUpGame` itself, as the C API tests do (`engine/runtime/capi/tests/test_schema_registry.cpp`) **[fact]**.
The parser of `--frames` is a non-template function in the same file, declared in a `detail` header or
namespace, so the template stays a few lines.

The template's `main.cpp` becomes **[author decision]** (the class is named after the game; `@GAME_CLASS@` is a new
placeholder, or the builder picks a fixed name such as `MyGame` if `oly new` has no class-name transform):

```cpp
#include <SDL3/SDL.h>

#include "oly/app/engine.hpp"
#include "oly/ecs/query.hpp"
#include "oly/ecs/world.hpp"
#include "oly/native_host/game.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"

namespace {
  constexpr int LogicalWidth = 640;
  constexpr int LogicalHeight = 360;
  constexpr float PlayerSize = 16.0f;

  void draw(oly::app::SystemContext& context) {
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(context.userData());

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    context.query<const Position>(oly::ecs::With<Player>{}).each([renderer](const Position& position) {
      const SDL_FRect box{position.x - PlayerSize / 2, position.y - PlayerSize / 2, PlayerSize, PlayerSize};
      SDL_RenderFillRect(renderer, &box);
    });
  }

  class @GAME_CLASS@ : public oly::native_host::Game {
   public:
    @GAME_CLASS@() : Game({"@GAME_NAME@", LogicalWidth, LogicalHeight}) {}

    void onStart(oly::app::Engine& engine, oly::native_host::Window& window) override {
      engine.actions().bind(actions::move, {oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
      engine.world().spawn(Player{}, Position{LogicalWidth / 2.0f, LogicalHeight / 2.0f});
      engine.addSystem(oly::app::Phase::Update, "movePlayer", movePlayer);
      engine.addSystem(oly::app::Phase::Render, "draw", draw, window.renderer());
    }
  };
}

int main(int argumentCount, char** arguments) {
  return oly::native_host::runGame<@GAME_CLASS@>(argumentCount, arguments);
}
```

`systems.hpp` and `movement.cpp` do not change.

### 5.3 Web: `Game` and `runGame` in `@oly/web-host`

`@oly/web-host` already depends on `@oly/core` (`engine/environments/web/host/js/package.json`) **[fact]**, so it can
call `loadRuntime` and take `Engine`, `EngineConfig` and `Schema` types.

```ts
export interface GameOptions {
  /** The canvas, or the id of a canvas in the page. */
  readonly canvas: string | HTMLCanvasElement;
  /** URL of the game's WASM module (`<game>.mjs`). */
  readonly module: string | URL;
  readonly schema: Schema;
  /** When given, set the canvas size in pixels; otherwise the canvas keeps its own. */
  readonly width?: number;
  readonly height?: number;
  readonly engine?: EngineConfig;
}

/** The game itself: its options and its own state. */
export abstract class Game {
  readonly options: GameOptions;

  protected constructor(options: GameOptions) {
    this.options = options;
  }

  /** Called once, on a set-up engine, before the first frame. Synchronous: `runGame` does not await it. */
  abstract onStart(engine: Engine, canvas: HTMLCanvasElement): void;
}

export interface BrowserGame {
  readonly engine: Engine;
  /** Stops the loop, detaches the input and disposes the engine; calling it again does nothing. */
  stop(): void;
}

export function runGame(game: Game): Promise<BrowserGame>;
```

`runGame(game)`, in this order:

1. Resolve `game.options.canvas`; an id that finds nothing, or finds something that is not a canvas, is an error
   naming the id. Set `width` and `height` when given.
2. `loadRuntime(moduleFactoryFromUrl(options.module), options.schema)`, then `runtime.createEngine(options.engine)`.
3. `game.onStart(engine, canvas)`.
4. `attachBrowserInput(engine, canvas)`, then `runInBrowser(engine)`.
5. Resolve with `{ engine, stop }`.

`onStart` receives the canvas, not a drawing context **[user decision]**: the engine does not decide whether the game
draws with Canvas 2D, WebGL or WebGPU.

`onStart` is synchronous **[author decision]**: `runGame` does not await it, and `web-host.md` says so. Loading assets
before the first frame is a separate feature; widening the return type to `void | Promise<void>` later breaks no
caller.

`stop` called during a frame (from a system, handler or hook) is a contract error, like `Engine.dispose` in a frame
(`engine/environments/web/clients/typescript/core/src/engine.ts`, `"dispose: Engine in frame; dispose after the
frame"`) **[fact]** **[author decision]**. `stop` checks this before it stops anything, so a refused call leaves the
game running. The message names `stop`; `errors.md` gets its row.

Errors: when any step throws or rejects, `runGame` undoes what it already did (stops the loop, detaches the input,
disposes the engine, in that order, each only if it happened), writes `Error starting the game: <message>` into the
page (`document.body.textContent`, as the template does today) and rejects with the same error **[author decision]**.
The template calls it with `void runGame(...)`; the browser reports the unhandled rejection in the console once, so
`runGame` does not log it itself.

Like `runInBrowser` and `attachBrowserInput`, `runGame` takes an optional last parameter that replaces what it reaches
in the global scope, so its tests run in Node without a browser or a WASM build **[author decision]**. What that
parameter holds (the document, the loader, the two functions it calls) is the builder's call. `web-host.md` does not
document it, as it does not document the environment parameter of the other two (`docs/reference/typescript/web-host.md`)
**[fact]**.

The template **[author decision]**. The smoke test runs the gameplay in Node, where there is no canvas, so the
gameplay stays in a function that takes a `Drawer`, and the class adapts the canvas to it:

`main.ts`:

```ts
import { runGame } from "@oly/web-host";

import { @GAME_CLASS@ } from "./game.js";

void runGame(new @GAME_CLASS@());
```

`game.ts`:

```ts
import { Phase } from "@oly/core";
import type { Engine } from "@oly/core";
import { Game } from "@oly/web-host";

import schema, { Player, Position, move } from "../schema.generated.js";

const Width = 640;
const Height = 360;
const PlayerSize = 16;

export interface Drawer {
  clear(): void;
  square(x: number, y: number, size: number): void;
}

export class @GAME_CLASS@ extends Game {
  constructor() {
    super({ canvas: "game", module: "./@GAME_TARGET@.mjs", schema, width: Width, height: Height });
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

export function startGameplay(engine: Engine, drawer: Drawer): void {
  // the body of today's installGame, unchanged
}
```

`tests/smoke.test.ts` imports `startGameplay` instead of `installGame`. Two things the builder checks before keeping
this shape: that `smoke.test.ts` type-checks with `HTMLCanvasElement` in a signature it imports (the tests' tsconfig
may not load the DOM library), and that importing `@oly/web-host` in Node touches no browser global at load time
(its globals are reached inside default parameters today **[fact]**, `browser_loop.ts`). If either fails, the
gameplay function and `Drawer` move to their own file that the test imports, and `game.ts` keeps only the class.

### 5.4 Docs

Following the table in `docs/reference/contributor/docs-style.md` ("A change to the public surface"):

- `docs/reference/native-host.md`: a `Game` and `runGame` section (the class, both overloads, order, exit codes, the
  `setUpGame` call, the lifetime guarantee); the `Window` example uses `onStart`.
- `docs/reference/typescript/web-host.md`: `Game`, `GameOptions`, `runGame`, `BrowserGame`; the opening that says the
  package "has three functions" and that the template calls all three.
- Template change row: `docs/reference/game-template.md`, `docs/reference/cli.md`, `docs/reference/errors.md`, both
  tutorials and `README.md`.
- `docs/reference/errors.md`: the canvas-not-found message, the 2D-context message, `stop` in a frame and the native
  usage line.
- `docs/reference/glossary.md`: a row "the game: the class a game derives, and the call that runs it" with the C++ and
  TypeScript names; the row of `setUpGame` says that `runGame` calls it on native.
- `docs/explanation/concepts/environments-and-hosts.md`: how a game starts in each host, now through `Game`.
- Every page that shows `installGame` or the old `main` (`test-a-web-game.md`, `typescript/gameplay-recipes.md`,
  `test-a-desktop-game.md`, `engine-and-systems.md`): search the whole `docs/`, except `docs/superpowers/` (history).
- Every place that says a native `main` calls `setUpGame`: search for `setUpGame` too, not only `installGame`. Known
  today: `docs/how-to/use-a-plugin.md`, the comment in `engine/runtime/app/include/oly/app/game.hpp` and the comment
  above `oly_add_game` in `cmake/OlyGame.cmake`.
- `CHANGELOG.md`, `## Unreleased`: `Game` and `runGame` in both hosts. Not breaking: nothing is removed. `### Migration`
  says an existing game may switch to `Game` and that its `main` keeps working as is.

## 6. Verification

Every command below exists in the repository today, and each runs the target named **[fact]** (`Spurfile`,
`CMakePresets.json`, `cmake/OlyOptions.cmake`):

| What | How |
|---|---|
| Native: `runGame<G>` with `--frames 3` constructs `G`, calls `onStart` once after `setUpGame`, returns 0, and destroys `G` after the `Engine`; an unknown argument and `--frames 0` return 64 and construct nothing; an exception in the constructor or in `onStart` returns 1 | a new doctest suite in `engine/environments/native/host/tests/`, registered with `oly_add_test(MODULE native_host ...)` and `SDL_VIDEO_DRIVER=dummy` like `window`; it defines `oly::app::setUpGame`. Write it with the `add-test-case` skill. `./spur native-games-test -R '^native_host\.'` and `./spur windows-test`. The default `./spur test` preset does not build the native host (`OLY_BUILD_NATIVE_HOST` is `OFF` there) |
| Web: order (`onStart` before input and loop), canvas errors reject, undo and write the page, `stop` detaches, stops and disposes, `stop` in a frame throws and leaves the game running | a new test in `engine/environments/web/host/js/tests/`, next to `browser_loop.test.ts`. `./spur games-test` (it runs `./spur ts` first) |
| The templates build and their tests pass | `./spur consumer-test native` and `./spur consumer-test web` |
| No placeholder left in a created game, `@GAME_CLASS@` included | `./spur oly-tools-test` (`tools/oly/tests/new_test.sh`) |
| Docs links and the README example | `./spur test` (`check_links.py`, `check_readme_example.py` run as ctest) |
| Formatting | `./spur format-check` |

## 7. Left to the builder

- The exact wording of the usage and error messages, within §5.2 and §5.3.
- The shape of the web test seam (§5.3).
- `@GAME_CLASS@` as a placeholder that `oly new` fills, or a fixed class name (§5.2).
- Whether the native test checks the stderr text or only the exit code, if capturing stderr in doctest is awkward.
- The order of commits.
