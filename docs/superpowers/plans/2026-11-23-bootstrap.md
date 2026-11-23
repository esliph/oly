# The `Game` class and `runGame` — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A game derives one class from `Game`, writes its content in `onStart`, and starts with one `runGame` call, in the native host (C++) and in `@oly/web-host` (TypeScript); both templates and the docs follow.

**Architecture:** Native: `oly/native_host/game.hpp` declares `Game`, `runGame(Game&, RunOptions)` and a three-line `runGame<G>(argc, argv)` template that forwards to a non-template `detail::runGameFromCommandLine` in `src/game.cpp` (own file, so executables that never call it need no `setUpGame`). Web: `src/game.ts` in `@oly/web-host` declares `Game`, `runGame` and a test seam `GameEnvironment`; `runGame` wraps the engine in its own frame target so `stop` can refuse a call made during a frame without any change to `Engine`. The templates move their content into a `MyGame` class.

**Tech Stack:** C++20, SDL3, doctest, CMake (`oly_add_test`); TypeScript 5.6 strict, `node --test`; Docker through `./spur`.

**Spec:** `docs/superpowers/specs/2026-11-23-bootstrap-design.md`

## Global Constraints

- Every toolchain command runs through `./spur <task>` from Git Bash; there is no compiler on the host.
- C++: `.clang-format` decides layout (`./spur format <files>`; a hook formats after each edit); braces on every `if`/`for`/`while`/`else`; full names; types `PascalCase`, functions and variables `camelCase`, private members end in `_`; in a `class`, a `private:` section with data first, then `public:`, then private helpers; one `namespace` block per file; standard headers before project headers (quotes).
- `-Wall -Wextra -Wpedantic -Werror` on every target; TS in `strict`.
- Comments and messages in English. Comments only for a reason or a non-obvious contract.
- Names from the spec, exactly: `Game`, `onStart`, `runGame`, `GameOptions`, `BrowserGame`, `windowConfig`, `engineConfig`.
- Exit codes: `64` for a wrong command line, `1` when a `std::exception` escapes.
- Usage line: `usage: <argv[0]> [--frames <n>]`. Error line: `<title>: <what()>`, with `argv[0]` when the constructor threw.
- Web page text on a failed start: `Error starting the game: <message>`.
- Do not change `Engine`, `World`, `run`, `Window`, `RunOptions`, `runInBrowser`, `attachBrowserInput` or `loadRuntime`.
- Game names never in `engine/`. The template class is `MyGame` (decision below).
- Every public API change updates `docs/` in the same branch, following `docs/reference/contributor/docs-style.md` ("A change to the public surface").
- New tests follow the `add-test-case` skill. A mutation made to see a test fail is undone with the editor, never with `git checkout` (it would drop uncommitted work in the same file).
- This plan creates no worktree. If the executor creates one, it removes it right after its last use (`git worktree remove`).

**Decision on the class name (spec §7):** the templates use a fixed `MyGame`, not a new `@GAME_CLASS@` placeholder. `oly new` has no class-name transform, and a name derived from the game collides for valid game names: `player` → `Player` and `position` → `Position` (the template's schema types), `game` → `Game` (the base class). `oly new` and `new_test.sh` stay as they are; the existing `no_placeholder_left` check still covers the templates.

**Decision on the web template files (spec §5.3 fallback):** `web/web/tsconfig.json` and `web/web/tests/tsconfig.json` load `lib: ["ES2022"]` without `DOM` (checked), so a file with `HTMLCanvasElement` in a signature cannot be in the project the smoke test imports. The gameplay goes to `src/gameplay.ts` (DOM-free); `src/game.ts` keeps only the class and moves to the browser project.

## Review Focus

1. `--frames` with a value that is not a whole positive number: `5x`, `-1`, `18446744073709551616`. The old parser used `strtoull` and accepted `5x` as 5 and `-1` as 2^64-1. Expected: 64, nothing constructed. Test in Task 1.
2. `argc == 0` (a legal `execve`) with a constructor that throws. Expected: return 1 and print with a fallback name, no null `%s`. Test in Task 1.
3. A frame that throws (a TS system error) and then `stop()`. Expected: `stop` works; the in-frame flag resets in a `finally`. Test in Task 2.
4. A value thrown during start that is not an `Error` (`throw "text"`). Expected: the page shows `Error starting the game: text` and `runGame` rejects with that same value. Test in Task 2.
5. A canvas id that finds an element that is not a canvas, or nothing. Expected: rejects naming the id, and the runtime is never loaded. Test in Task 2.

---

### Task 1: Native `Game` and `runGame`

**Files:**
- Create: `engine/environments/native/host/include/oly/native_host/game.hpp`
- Create: `engine/environments/native/host/src/game.cpp` (picked up by the `src/*.cpp` glob in `cmake/OlyModule.cmake`)
- Create: `engine/environments/native/host/tests/game.cpp`
- Modify: `engine/environments/native/host/tests/CMakeLists.txt`
- Modify: `docs/reference/native-host.md`, `docs/reference/errors.md` (section `### At run time` under `## C++`)

**Interfaces:**
- Produces (used by Task 3 and the docs):

```cpp
#pragma once

#include <memory>
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

  // Builds the World and the Engine, sets up the game, opens the window, calls onStart and runs the loop. Does not
  // catch: exceptions reach the caller.
  int runGame(Game& game, RunOptions options = {});

  namespace detail {
    int runGameFromCommandLine(int argumentCount, char** arguments, std::unique_ptr<Game> (*construct)());
  }

  // Parses --frames, then constructs G and runs it. Returns 64 on a wrong command line and 1 when an exception escapes,
  // the constructor's included; otherwise what run returns.
  template <class G>
  int runGame(int argumentCount, char** arguments) {
    static_assert(std::is_base_of_v<Game, G>, "runGame: the game must derive from oly::native_host::Game");
    static_assert(std::is_default_constructible_v<G>, "runGame: the game must be default constructible");

    return detail::runGameFromCommandLine(argumentCount, arguments, []() -> std::unique_ptr<Game> { return std::make_unique<G>(); });
  }
}
```

The heap allocation is one per process; it lets the template stay three lines while the parser, the `try` and the messages live in `game.cpp`, where the test reaches them.

**Behaviour of `src/game.cpp`:**

- `runGame(Game&, RunOptions)`, in this order, all locals: `oly::ecs::World world;` `oly::app::Engine engine(world, game.engineConfig());` `oly::app::setUpGame(engine);` `Window window(game.windowConfig());` `game.onStart(engine, window);` `return run(engine, window, options);`.
- `runGameFromCommandLine`:
  1. Program name: `arguments[0]` when `argumentCount > 0` and it is not null, else `"game"`.
  2. Parse in an anonymous-namespace function `std::optional<uint64_t> parseFrames(int argumentCount, char** arguments)`: `std::nullopt` = wrong usage; `0` = no `--frames` (the `RunOptions::maxFrames` meaning). Accept only `--frames <n>` pairs; `n` read with `std::from_chars` into `uint64_t`, which must consume the whole argument and give `n > 0` (rejects `5x`, `-1`, overflow, empty). A repeated `--frames`: the last wins. On `nullopt`, print the usage line to `stderr` and return 64 before constructing anything.
  3. In a `try`: `std::unique_ptr<Game> game = construct();` once the game exists, the name for errors becomes `game->windowConfig().title`; with frames > 0, `options.maxFrames = frames` and `options.frameSeconds = 1.0 / game->engineConfig().fixedHz`; `return runGame(*game, options);`.
  4. `catch (const std::exception& error)`: print `<name>: <error.what()>\n` to `stderr`, return 1.

**Teardown order** (the contract the docs state; every piece of user code that runs during teardown): when `runGame(Game&)` returns or throws, its locals go in reverse: the `Window` closes first; then `~Engine` runs the `discard` of each pending task, then destroys the plugins in reverse install order; then the `World`. Only after that does `runGameFromCommandLine` destroy the game. So task discards and plugin destructors run while the game is alive but after the window is gone: they must not use the renderer.

- [ ] **Step 1: Write the failing test** `engine/environments/native/host/tests/game.cpp` and register it after the `input` lines:

```cmake
oly_add_test(MODULE native_host NAME game SOURCES game.cpp)
set_tests_properties(native_host.game PROPERTIES ENVIRONMENT SDL_VIDEO_DRIVER=dummy)
```

The file defines `oly::app::setUpGame(Engine&)` itself (it counts calls), as `engine/runtime/capi/tests/test_schema_registry.cpp` does. A `struct Probe` with static counters, reset at the start of each case: `constructed`, `setUpCalls`, `setUpCallsAtStart` (copied in `onStart`), `startCalls`, `updateSteps`, `titleAtStart` (from `SDL_GetWindowTitle(window.window())`), `engineDestroyed` (set by a `ProbePlugin` destructor), `engineDestroyedWhenGameDestroyed` (copied in the game's destructor). A helper `int runWith(std::vector<std::string> arguments)` builds a `char*` array and calls `runGame<ProbeGame>`. `ProbeGame` uses `{"probe", 64, 64}` and `{.fixedHz = 30.0}`, and in `onStart` installs `ProbePlugin` and adds an `Update` system that increments `Probe::updateSteps`.

```cpp
TEST_CASE("runGame with --frames 3 starts the game once after setUpGame and steps at the game's fixedHz") {
  CHECK(runWith({"probe", "--frames", "3"}) == 0);
  CHECK(Probe::constructed == 1);
  CHECK(Probe::setUpCallsAtStart == 1);
  CHECK(Probe::startCalls == 1);
  CHECK(Probe::titleAtStart == "probe");
  CHECK(Probe::updateSteps == 2); // 3 frames, the first only records the time; 1.0 / 60 would give 1 step at 30 Hz
}

TEST_CASE("runGame destroys the game after the Engine") {
  runWith({"probe", "--frames", "1"});
  CHECK(Probe::engineDestroyedWhenGameDestroyed);
}

TEST_CASE("a wrong command line returns 64 and constructs nothing") {
  // one SUBCASE per line: {"--bogus"}, {"--frames"}, {"--frames", "0"}, {"--frames", "-1"}, {"--frames", "5x"},
  // {"--frames", "18446744073709551616"}, {"--frames", "3", "extra"}; each prefixed with "probe"
  CHECK(runWith(arguments) == 64);
  CHECK(Probe::constructed == 0);
}

TEST_CASE("an exception from the game's constructor returns 1") {
  CHECK(runGame<ThrowingConstructorGame>(argumentCount, arguments) == 1); // with {"probe", "--frames", "1"}
}

TEST_CASE("an exception from the game's constructor with no arguments at all returns 1") {
  CHECK(oly::native_host::runGame<ThrowingConstructorGame>(0, nullptr) == 1);
}

TEST_CASE("an exception from onStart returns 1 and still destroys the game") {
  CHECK(runGame<ThrowingStartGame>(...) == 1); // with {"probe", "--frames", "1"}; its destructor sets a flag
  CHECK(Probe::gameDestroyed);
}
```

Checking the `stderr` text is optional (spec §7); the exit codes are required.

- [ ] **Step 2: Run the suite and see it fail to build** (`game.hpp` does not exist)

Run: `./spur native-games-test -R '^native_host\.game'`
Expected: build error naming `oly/native_host/game.hpp`.

- [ ] **Step 3: Write `game.hpp` (above) and `src/game.cpp`** with the behaviour listed above.

- [ ] **Step 4: Run the native host suites**

Run: `./spur native-games-test -R '^native_host\.'`
Expected: `native_host.window`, `native_host.input` and `native_host.game` pass; the `game` output shows 6 test cases, not 0. `window` and `input` still link without a `setUpGame`, which proves `game.cpp` is pulled only on use.

- [ ] **Step 5: See the frames test fail for the right reason.** In `game.cpp`, change `1.0 / game->engineConfig().fixedHz` to `1.0 / 60`, run Step 4's command, see only the first case fail on `updateSteps == 2`, then restore the line with the editor.

- [ ] **Step 6: Run under Wine**

Run: `./spur windows-test -R '^native_host\.'`
Expected: the same three suites pass.

- [ ] **Step 7: Docs.** `docs/reference/native-host.md`: a section `## The game` before `## The window`, with `Game` (constructor, `windowConfig`, `engineConfig`, `onStart`), both `runGame` overloads, the order of `runGame(Game&)`, that it calls `setUpGame` (so a game never does), the exit codes and the two stderr lines, that `--frames n` sets `frameSeconds = 1 / fixedHz` and runs `n - 1` steps, and the teardown order above, plus how a game with its own `main` gets the same lifetime (declare the game before calling `runGame(game, options)`). The `Window` example (the `draw` system with `window.renderer()`) moves into an `onStart`. `docs/reference/errors.md`, `## C++` → `### At run time`: a row for `usage: <program> [--frames <n>]` (cause: an unknown argument or a `--frames` value that is not a whole number above 0; exit 64).

- [ ] **Step 8: Format and commit**

Run: `./spur format-check`
Expected: no file listed.

```bash
git add engine/environments/native/host docs/reference/native-host.md docs/reference/errors.md
git commit -m "feat(native-host): add Game and runGame"
```

---

### Task 2: Web `Game` and `runGame`

**Files:**
- Create: `engine/environments/web/host/js/src/game.ts`
- Modify: `engine/environments/web/host/js/src/index.ts`
- Create: `engine/environments/web/host/js/tests/game.test.ts`
- Modify: `docs/reference/typescript/web-host.md`, `docs/reference/errors.md` (`## TypeScript`)

**Interfaces:**
- Consumes: `loadRuntime`, `ContractError`, types `Engine`, `EngineConfig`, `Schema` from `@oly/core`; `moduleFactoryFromUrl`, `runInBrowser`, `attachBrowserInput`, types `FrameTarget`, `BrowserLoop`, `BrowserInput` from this package.
- Produces (used by Task 4): `GameOptions`, `Game`, `BrowserGame`, `runGame` exactly as spec §5.3, plus the seam:

```ts
/** What runGame reaches in the browser; tests pass a fake. Not documented in web-host.md, like BrowserEnvironment. */
export interface GameEnvironment {
  /** The canvas with this id, or null when the id finds nothing or finds an element that is not a canvas. */
  canvasById(id: string): HTMLCanvasElement | null;
  showError(message: string): void;
  loadRuntime(module: string | URL, schema: Schema): Promise<{ createEngine(config?: EngineConfig): Engine }>;
  attachBrowserInput(engine: Engine, canvas: HTMLCanvasElement): BrowserInput;
  runInBrowser(target: FrameTarget): BrowserLoop;
}

export function runGame(game: Game, environment: GameEnvironment = globalGameEnvironment()): Promise<BrowserGame>;
```

`globalGameEnvironment()` touches `globalThis.document` and `HTMLCanvasElement` only when called, never at module load (importing the package in Node must stay safe). `canvasById` uses `instanceof HTMLCanvasElement`; `showError` sets `document.body.textContent`; `loadRuntime` is `loadRuntime(moduleFactoryFromUrl(module), schema)`.

`index.ts` adds: `export { Game, runGame } from "./game.js";` and `export type { BrowserGame, GameEnvironment, GameOptions } from "./game.js";`.

**Algorithm of `runGame`** (an `async` function; the order is the contract):

1. Canvas: `typeof options.canvas === "string"` → `environment.canvasById(id)`; `null` throws `Error('canvas with id "<id>" not found on the page')`. Then set `canvas.width` / `canvas.height` for each one given.
2. `const runtime = await environment.loadRuntime(options.module, options.schema)`; `engine = runtime.createEngine(options.engine)`.
3. `game.onStart(engine, canvas)` (not awaited).
4. `input = environment.attachBrowserInput(engine, canvas)`.
5. `loop = environment.runInBrowser(target)`, where `target` is not the engine but `{ frame(now) { inFrame = true; try { engine.frame(now); } finally { inFrame = false; } }, setHidden: (hidden) => engine.setHidden(hidden) }`. This flag is how `stop` knows it runs inside a frame (systems, handlers, hooks and tasks all run inside `engine.frame`) without touching `Engine`.
6. Resolve with `{ engine, stop }`.

On any throw or rejection in 1–5: `loop?.stop()`, `input?.detach()`, `engine?.dispose()` in that order, each only if it exists; `environment.showError("Error starting the game: " + (error instanceof Error ? error.message : String(error)))`; rethrow the same value. No `console.error` (the browser reports the unhandled rejection).

`stop()`: if `inFrame`, throw `new ContractError("stop: game in frame; stop after the frame")` before changing anything; if already stopped, return; else mark stopped, `loop.stop()`, `input.detach()`, `engine.dispose()`.

- [ ] **Step 1: Write the failing test** `tests/game.test.ts`, importing from `../dist/index.js` like `browser_loop.test.ts`. Fakes: a `FakeEnvironment implements GameEnvironment` that records every call in a shared `log: string[]`, holds `canvases: Map<string, HTMLCanvasElement | null>`, `pageText`, `loadCalls`, a `FakeLoop` that keeps the `FrameTarget` and has `tick(now)`; a fake engine `{ frame, setHidden, dispose }` cast `as unknown as Engine`, whose `frame` calls an optional `onFrame` callback; a fake canvas `{ width: 0, height: 0 } as unknown as HTMLCanvasElement`; a `TestGame extends Game` whose `onStart` is a settable callback.

```ts
test("runGame starts the game before it attaches the input and starts the loop", ...);
// assert.deepEqual(log, ["loadRuntime", "createEngine", "onStart", "attachBrowserInput", "runInBrowser"])

test("runGame finds the canvas by id and sets the size given in the options", ...);
// options { canvas: "game", width: 640, height: 360 } → canvas.width === 640, canvas.height === 360

test("runGame uses a canvas given as an element without looking it up", ...);
// canvasById never called; onStart receives that same object

test("an id that finds no canvas rejects naming the id, writes the page and loads nothing", ...);
// canvases.set("game", null); await assert.rejects(runGame(...), /canvas with id "game" not found on the page/);
// pageText === 'Error starting the game: canvas with id "game" not found on the page'; loadCalls === 0

test("an onStart that throws disposes the engine, writes the page and rejects with the same error", ...);
// const error = new Error("boom"); await assert.rejects(runGame(...), (thrown) => thrown === error);
// log ends with "dispose"; no "attachBrowserInput"; pageText === "Error starting the game: boom"

test("a thrown value that is not an Error reaches the page through String", ...);
// onStart throws "text" → pageText === "Error starting the game: text"; rejects with "text"

test("a runInBrowser that throws detaches the input, then disposes the engine", ...);
// log after the failure: [..., "runInBrowser", "detach", "dispose"]

test("stop stops the loop, detaches the input and disposes the engine, once", ...);
// game.stop(); game.stop(); → log tail ["loopStop", "detach", "dispose"] exactly once

test("stop during a frame throws ContractError and leaves the game running", ...);
// onFrame = () => game.stop(); assert.throws(() => loop.tick(1000), (e) => e instanceof ContractError && /^stop:/.test(e.message));
// then: no "loopStop", "detach" or "dispose" in log

test("stop works after a frame that threw", ...);
// onFrame throws Error("system"); assert.throws(() => loop.tick(1000), /system/); game.stop() → log tail ["loopStop", "detach", "dispose"]
```

- [ ] **Step 2: Run it and see it fail**

Run: `./spur games-test -R '^ts\.web_host'`
Expected: the TS build fails: `runGame` and `Game` are not exported.

- [ ] **Step 3: Write `src/game.ts` and the `index.ts` exports** as above.

- [ ] **Step 4: Run the web host tests**

Run: `./spur games-test -R '^ts\.web_host'`
Expected: `ts.web_host.unit` passes, and its output lists the ten new tests.

- [ ] **Step 5: See the in-frame guard fail for the right reason.** Remove the `inFrame` check from `stop`, run Step 4, see only the two `stop` frame tests fail, restore it with the editor.

- [ ] **Step 6: Docs.** `docs/reference/typescript/web-host.md`: the opening no longer says "three functions" nor that the template calls all three — the template calls `runGame`, and the three functions stay for a game that drives the loop itself; new sections `## Game`, `## GameOptions`, `## runGame` (the order above, that `onStart` is synchronous and not awaited, the error path and the page text), `## BrowserGame` (`stop`, idempotent, an error during a frame). `docs/reference/errors.md` `## TypeScript`: rows for `canvas with id "<id>" not found on the page` and `stop: game in frame; stop after the frame`.

- [ ] **Step 7: Commit**

```bash
git add engine/environments/web/host/js docs/reference/typescript/web-host.md docs/reference/errors.md
git commit -m "feat(web-host): add Game and runGame"
```

---

### Task 3: Native template on `MyGame`

**Files:**
- Modify: `templates/game/native/desktop/main.cpp`
- Modify: `docs/tutorials/first-desktop-game.md` (section `## 6. Read the shell`), `docs/reference/game-template.md` (the `native/desktop/main.cpp` row)

**Interfaces:**
- Consumes: `oly::native_host::Game`, `runGame<G>(int, char**)` from Task 1.

- [ ] **Step 1: Replace `main.cpp`** with this file (spec §5.2, with the fixed name):

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

  class MyGame : public oly::native_host::Game {
   public:
    MyGame() : Game({"@GAME_NAME@", LogicalWidth, LogicalHeight}) {}

    void onStart(oly::app::Engine& engine, oly::native_host::Window& window) override {
      engine.actions().bind(actions::move, {oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
      engine.world().spawn(Player{}, Position{LogicalWidth / 2.0f, LogicalHeight / 2.0f});
      engine.addSystem(oly::app::Phase::Update, "movePlayer", movePlayer);
      engine.addSystem(oly::app::Phase::Render, "draw", draw, window.renderer());
    }
  };
}

int main(int argumentCount, char** arguments) {
  return oly::native_host::runGame<MyGame>(argumentCount, arguments);
}
```

- [ ] **Step 2: Build and test a created game**

Run: `./spur consumer-test native`
Expected: the game builds and `consumer_game.headless` (120 frames) passes.

- [ ] **Step 3: Docs.** `first-desktop-game.md` §6 quotes the new file and explains it: `MyGame` holds the window configuration; `onStart` binds, spawns and adds the systems on an engine that `runGame` already set up; `runGame<MyGame>` parses `--frames`, opens the window, runs the loop and turns an exception into exit code 1; rename the class to fit the game. Drop the bullets about `parseOptions` and about calling `setUpGame` first. `game-template.md`: the `desktop/main.cpp` row becomes "`MyGame` (window, bindings, first entity, systems, drawing) and `main`, which calls `runGame<MyGame>`".

- [ ] **Step 4: Commit**

```bash
git add templates/game/native docs/tutorials/first-desktop-game.md docs/reference/game-template.md
git commit -m "feat(template): start the native game with runGame"
```

---

### Task 4: Web template on `MyGame`

**Files:**
- Create: `templates/game/web/web/src/gameplay.ts` (with `git mv` from `src/game.ts`, then a new `src/game.ts`)
- Modify: `templates/game/web/web/src/game.ts`, `templates/game/web/web/src/main.ts`, `templates/game/web/web/tsconfig.json`, `templates/game/web/web/tsconfig.browser.json`, `templates/game/web/web/tests/smoke.test.ts`
- Modify: `docs/tutorials/first-web-game.md` (§7 and §8, headings unchanged: the README links `#7-read-the-typescript-system`), `docs/how-to/test-a-web-game.md`, `docs/how-to/typescript/gameplay-recipes.md` (the `installGame` example near "Game rules that are not per entity"), `docs/reference/game-template.md` (the `web/web/...` rows), `docs/reference/errors.md` (the 2D-context row)

**Interfaces:**
- Consumes: `Game`, `runGame` from `@oly/web-host` (Task 2).
- Produces: `startGameplay(engine: Engine, drawer: Drawer): void` and `interface Drawer` in `src/gameplay.ts`; `class MyGame extends Game` in `src/game.ts`.

- [ ] **Step 1: Point the smoke test at the new name and see it fail.** `git mv src/game.ts src/gameplay.ts`; in `tests/smoke.test.ts`, import `startGameplay` and `Drawer` from `../src/gameplay.js` and call `startGameplay(engine, drawer)`.

Run: `./spur consumer-test web`
Expected: `tsc -b` fails: `gameplay.ts` has no export `startGameplay`, and `main.ts` still imports the moved `./game.js`.

- [ ] **Step 2: Write the files.**
  - `gameplay.ts`: rename `installGame` to `startGameplay`; nothing else changes.
  - `game.ts`: the class of spec §5.3 named `MyGame`, with `super({ canvas: "game", module: "./@GAME_TARGET@.mjs", schema, width: Width, height: Height })` (`Width = 640`, `Height = 360`), and `onStart` that gets the 2D context (`null` → `Error("the browser did not provide a 2D context for the canvas")`) and calls `startGameplay` with a `Drawer` over it. It imports `schema` from `../schema.generated.js` and `startGameplay` from `./gameplay.js`.
  - `main.ts`: `import { runGame } from "@oly/web-host";`, `import { MyGame } from "./game.js";`, `void runGame(new MyGame());`.
  - `tsconfig.json`: `"exclude": ["src/main.ts", "src/game.ts"]`. `tsconfig.browser.json`: `"include": ["src/main.ts", "src/game.ts"]`.

- [ ] **Step 3: Build and test a created game**

Run: `./spur consumer-test web`
Expected: `tsc -b` and the esbuild bundle succeed, and `consumer_game.smoke` passes.

- [ ] **Step 4: Docs.** `first-web-game.md` §7 shows `startGameplay` from `web/src/gameplay.ts` (same explanation, new name and file); §8 shows `web/src/game.ts` and the one-line `main.ts`, and says what `runGame` does and that a failed start writes the message into the page. `test-a-web-game.md` and `gameplay-recipes.md`: `installGame` → `startGameplay`, `src/game.ts` → `src/gameplay.ts`; point 1 of `test-a-web-game.md` says `gameplay.ts` has no DOM and the class in `game.ts` is the only code that touches the canvas. `game-template.md`: rows for `gameplay.ts`, `game.ts` (the class, DOM) and `main.ts` (`runGame(new MyGame())`); the two tsconfig rows list the new include and exclude. `errors.md` `## TypeScript`: a row for `the browser did not provide a 2D context for the canvas` (origin: the template's `MyGame.onStart`).

- [ ] **Step 5: Commit**

```bash
git add templates/game/web docs/tutorials/first-web-game.md docs/how-to docs/reference/game-template.md docs/reference/errors.md
git commit -m "feat(template): start the web game with runGame"
```

---

### Task 5: Cross-cutting docs and changelog

**Files:**
- Modify: `docs/reference/glossary.md`, `docs/explanation/concepts/environments-and-hosts.md`, `docs/explanation/concepts/engine-and-systems.md`, `docs/explanation/architecture/app.md`, `docs/explanation/architecture/plugins.md`, `docs/how-to/use-a-plugin.md`, `docs/reference/cmake.md`, `engine/runtime/app/include/oly/app/game.hpp` (comment), `cmake/OlyGame.cmake` (comment above `oly_add_game`), `CHANGELOG.md`

- [ ] **Step 1: Find every page left behind.**

Run: `grep -rn "installGame\|native game's .\?main\|native .\?main.\? calls\|own .\?main.\?, which calls\|your .main. creates\|calls all three\|three functions" docs README.md cmake engine/runtime/app/include --include=*.md --include=*.hpp --include=*.cmake | grep -v docs/superpowers`
Expected after Tasks 1–4 (run on this base while writing the plan, minus the `installGame` and `web-host.md` lines those tasks remove): `docs/explanation/architecture/app.md`, `docs/explanation/architecture/plugins.md`, `docs/explanation/concepts/environments-and-hosts.md`, `docs/how-to/use-a-plugin.md`, `docs/reference/cmake.md`, `cmake/OlyGame.cmake`, `engine/runtime/app/include/oly/app/game.hpp`. After Step 2: only lines that also name `runGame` (a `main` that drives the loop itself). An `installGame` line left there means Task 3 or 4 missed an `installGame`.

- [ ] **Step 2: Edit.**
  - Wherever a native `main` is said to call `setUpGame`: `runGame` calls it; a `main` that drives the loop itself calls it ([Drive the loop yourself](docs/how-to/drive-the-loop-yourself.md)).
  - `glossary.md`: a row "the game: the class a game derives, and the call that runs it" — C++ `oly::native_host::Game`, `runGame`; C API none; TS `Game`, `runGame` (`@oly/web-host`). The `setUpGame` row says that `runGame` calls it on native.
  - `environments-and-hosts.md`: the table's web row and the desktop paragraph say how a game starts in each host, through `Game` and `runGame`. `engine-and-systems.md` line 12 area: the web loop starts through `runGame`, which calls `runInBrowser`.
  - `docs/how-to/test-a-desktop-game.md` and `cmake/tests/consumer/native/main.cpp` keep their own `main`: they drive the loop themselves, which the spec keeps (§3).
  - `README.md` stays: its "Minimal example" quotes only `game.oly` and `movement.cpp` (checked).
  - `CHANGELOG.md`, `## Unreleased`, following `docs-style.md` ("The changelog": only `### Breaking` and `### Migration` exist, and a new call that a game may adopt goes under Migration): `### Breaking` stays `None.`; `### Migration` replaces `None.` with one entry for each host: a game may derive `Game` and start with `runGame` (links to `native-host.md` and `typescript/web-host.md`); a game that keeps its own `main` keeps working without it.

- [ ] **Step 3: Check links, the README example and formatting**

Run: `./spur test -R '^oly\.lint\.'`
Expected: `oly.lint.links`, `oly.lint.readme_example` and their self-tests pass.

Run: `./spur format-check`
Expected: no file listed.

- [ ] **Step 4: Run everything the change touches once more**

Run: `./spur native-games-test -R '^native_host\.'`, `./spur games-test -R '^ts\.'`, `./spur consumer-test native`, `./spur consumer-test web`, `./spur oly-tools-test`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add docs CHANGELOG.md engine/runtime/app/include/oly/app/game.hpp cmake/OlyGame.cmake
git commit -m "docs: start games through Game and runGame"
```
