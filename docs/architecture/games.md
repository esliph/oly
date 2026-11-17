# Games

Every game lives in `games/<game>/` and runs in one environment: `arena` on `web`, `drift` on `native`. Each game's `README.md` says how to run it.

## `games/arena` (web)

Arena is the validation game of the `web` environment: a top-down 2D shooter (move, aim, shoot, dash, enemy waves, coins, menu → playing → game over). It is a client of the engine: if something generic could only be done with a hack outside the engine, the engine would be incomplete.

```
games/arena/
  arena.oly              the game's components, tags, events, actions and states
  native/                per-entity C++ gameplay: control, shooting, chasing, movement, collision
  web/                   the game's npm package, and the page that serves it
    schema.generated.ts  generated from arena.oly by the arena_game target (TYPESCRIPT), which ./spur ts builds (not committed)
    src/game.ts          installGame(engine, { drawer, random }): registers everything, no DOM
    src/render.ts        render through a Drawer interface
    src/main.ts          browser shell: canvas, Drawer over Canvas 2D, runInBrowser, attachBrowserInput
    src/arena.ts         constants shared with native/arena.hpp
    tests/smoke.test.ts  script in node against the real WASM (ts.arena.smoke)
    index.html
  README.md              how to run, manual checklist and the phase list
```

### Split between the languages

- **C++**: everything that runs per entity (systems `steerPlayer`, `fireWeapon`, `chasePlayer`, `integrate`, `confine`, `collide`; handlers `dropCoin`, `destroyDead`).
- **TS**: render, HUD, key bindings, state hooks, waves and score. The boundary is used per system, per block and per batch.

The DOM-free game (`installGame`) is kept separate from the browser shell (`main.ts`). This allows the smoke test in node with a fake clock, input and `Drawer`. The game's `tsconfig.json` has no `lib: ["DOM"]`, so using the DOM outside `main.ts` is a compile error.

### Serving in the browser

`./spur game arena` assembles a static folder in `build/wasm-games-release/web/arena/` (`index.html`, esbuild's `main.js`, `.mjs` and `.wasm`). The `.mjs` does not go into the bundle: it is loaded at run time. `./spur serve arena`, in the `web` service, serves that folder on port 8000 ([build-and-tests.md](build-and-tests.md)).

The manual checklist and the list of what was missing in the engine, what belongs to the game and what was worked around are in `games/arena/README.md`.

# Games: `games/drift`

Drift is the desktop example: the player steers a square, rocks fall, touching one ends the run. It is a C++-only game on the `native` environment (`oly_add_game(... ENVIRONMENT native ...)`) and runs in a window on Linux and Windows.

```
games/drift/
  drift.oly        components, tags, the PlayerHit event, move and restart actions, playing and gameOver states
  native/          the gameplay in C++, with no SDL: systems in three phases, a handler, state hooks, a repeating task
  desktop/         main.cpp (World, Engine, setUpGame, Window, run) and render.cpp (the Render system, SDL drawing)
  README.md        controls and how to run
```

`ENVIRONMENT` says where a game runs, and a game builds only in its own environment: `arena` on `web`, `drift` on `native`. Each game's `CMakeLists.txt` adds its target only there. `./spur desktop drift` builds it and prints the executable's path; `drift.desktop` runs it headless in ctest.

## `games/drift` (native)

Drift is the desktop example: the player steers a square, rocks fall, touching one ends the run. It is a C++ game with no client, on the `native` environment (`oly_add_game(... ENVIRONMENT native ...)`), and runs in a window on Linux and Windows.

```
games/drift/
  drift.oly        components, tags, the PlayerHit event, move and restart actions, playing and gameOver states
  native/          the gameplay in C++, with no SDL: systems in three phases, a handler, state hooks, a repeating task
  desktop/         main.cpp (World, Engine, setUpGame, Window, run) and render.cpp (the Render system, SDL drawing)
  README.md        controls and how to run
```

`ENVIRONMENT` says where a game runs, and a game builds only in its own environment. Each game's `CMakeLists.txt` adds its target only there. `./spur desktop drift` builds it and prints the executable's path; `drift.desktop` runs it headless in ctest. The gameplay never includes SDL, so it stays the same under another shell.
