# The game template

[Reference](../README.md#reference) · Concept: [Environments and hosts](../explanation/concepts/environments-and-hosts.md)

`oly new <game> --environment web|native` creates `<game>/` from the game template, `templates/game/` in the engine. It copies `common/` first, then the overlay of the environment (`web/` or `native/`) on top, and renames two files: `gitignore` becomes `.gitignore` and `game.oly` becomes `<game>.oly`. The tutorials walk through the result: [Your first web game](../tutorials/first-web-game.md) and [Your first desktop game](../tutorials/first-desktop-game.md).

## The placeholders

`oly new` replaces these words in every file it copies:

| Placeholder | Becomes |
|---|---|
| `@GAME_NAME@` | the game name, as typed: `[a-z][a-z0-9-]*` |
| `@GAME_TARGET@` | the game name with `-` written as `_`; the CMake target, and on the web the name of the `.mjs` |
| `@OLY_VERSION@` | the engine version |
| `@OLY_ENVIRONMENT@` | `web` or `native` |
| `@OLY_RELEASE_URL@` | the release URL the `@oly` npm tarballs come from |

The pages on this site show the template files with `@GAME_NAME@` written as `<game>` and `@GAME_TARGET@` as `<target>`.

## Common

Both environments get these files.

| Template file | In `<game>/` | What it is |
|---|---|---|
| `common/oly.env` | `oly.env` | the engine version, the environment, the target name and the runner (`OLY_RUNNER=host`); change the version with `oly upgrade <version>`, not by hand ([Upgrade the engine](../how-to/upgrade-the-engine.md)); the keys are [below](#olyenv) |
| `common/CMakeLists.txt` | `CMakeLists.txt` | reads the version from `oly.env`, fetches the engine at that version with `FetchContent` and includes `cmake/game.cmake` |
| `common/game.oly` | `<game>.oly` | the schema: `tag Player` and `action move: axis2d` |
| `common/gitignore` | `.gitignore` | ignores `build/`, `node_modules/`, `web/dist/`, `web/schema.generated.ts` and `*.tsbuildinfo` |
| `common/native/movement.cpp` | `native/movement.cpp` | the C++ system `movePlayer`: moves every `Player` by the `move` action |
| `common/native/systems.hpp` | `native/systems.hpp` | declares `movePlayer` for a shell that adds it with `Engine::addSystem` |
| `common/.github/workflows/ci.yml` | `.github/workflows/ci.yml` | installs `oly` and runs `oly test --docker` on every push and pull request |

`Position` is not in `<game>.oly`: `DIMENSION 2d` in `cmake/game.cmake` turns the `essentials2d` plugin on, which brings it ([Transforms](../explanation/concepts/transforms.md)).

### `oly.env`

`oly.env` is the one file that names the engine version. Each line is `KEY=value`:

| Key | Meaning |
|---|---|
| `OLY_VERSION` | the engine release the game builds with |
| `OLY_ENVIRONMENT` | `web` or `native` |
| `OLY_GAME` | the name of the CMake target |
| `OLY_RUNNER` | `host` (default) or `docker`: where `oly build`, `oly test` and `oly run` run ([Docker](cli.md#docker)) |

A web game names the version a second time: the `@oly/core` and `@oly/web-host` URLs in `package.json` carry it. `oly build` stops when the two differ. Do not edit them by hand: use `oly upgrade`.

Commit everything except what `.gitignore` lists.

## Web

The overlay `templates/game/web/` runs the game as WASM with a TypeScript client.

| Template file | In `<game>/` | What it is |
|---|---|---|
| `web/cmake/game.cmake` | `cmake/game.cmake` | `oly_add_game` for the web, and the ctest test `<target>.smoke` |
| `web/package.json` | `package.json` | the `@oly/core` and `@oly/web-host` tarballs of the engine version, and `esbuild`, `typescript` and `@types/node` |
| `web/tsconfig.json` | `tsconfig.json` | references the three TS projects below |
| `web/web/tsconfig.json` | `web/tsconfig.json` | DOM-free code: the schema and the gameplay; leaves `src/main.ts` and `src/game.ts` out |
| `web/web/tsconfig.browser.json` | `web/tsconfig.browser.json` | only `src/main.ts` and `src/game.ts`, with the DOM types |
| `web/web/tests/tsconfig.json` | `web/tests/tsconfig.json` | the tests; emits them to `web/dist/tests/` |
| `web/web/src/gameplay.ts` | `web/src/gameplay.ts` | `startGameplay(engine, drawer)`: binds the action, spawns the player, registers `movePlayer` and a `draw` system; no DOM |
| `web/web/src/game.ts` | `web/src/game.ts` | `MyGame`, the class that derives `Game`: canvas, module and size options, and `onStart`, which gives `startGameplay` a `Drawer` over the 2D context; the DOM is here |
| `web/web/src/main.ts` | `web/src/main.ts` | one line: `runGame(new MyGame())` |
| `web/web/index.html` | `web/index.html` | the page with the `<canvas id="game">` that `MyGame` finds |
| `web/web/tests/smoke.test.ts` | `web/tests/smoke.test.ts` | holds `KeyD` for one second and checks that the player moved by the player speed |

`web/schema.generated.ts` is written by the build from `<game>.oly`, and is not committed. `oly new` runs `npm install` at the end.

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

`nativeSources` is a variable of the template: the C++ files in `native/`. Add a `.cpp` file there and the next build compiles it. `ENVIRONMENT` is `web` or `native`, `DIMENSION` is `2d` or `3d`, and `TYPESCRIPT` names the TS schema file the build writes. An optional `PLUGINS <plugin>...` turns [plugins](../explanation/concepts/plugins.md) on: their types reach `schema.generated.ts` and their systems run in the WASM module, with no TS code.

## Native

The overlay `templates/game/native/` runs the game in a window, in C++ only.

| Template file | In `<game>/` | What it is |
|---|---|---|
| `native/cmake/game.cmake` | `cmake/game.cmake` | `oly_add_game` for the desktop, and the ctest test `<target>.headless` |
| `native/desktop/main.cpp` | `desktop/main.cpp` | `MyGame` (window, bindings, first entity, systems, drawing) and `main`, which calls `runGame<MyGame>` |

A native game has no registry, no TS and no `.mjs`: the target is a plain executable in `build/cmake/games/`.

### `cmake/game.cmake`

```cmake
file(GLOB gameSources CONFIGURE_DEPENDS native/*.cpp desktop/*.cpp)

oly_add_game(<target>
  ENVIRONMENT native
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/<game>.oly
  SOURCES ${gameSources}
  DEPENDS ecs app input native_host)
target_include_directories(<target> PRIVATE native)

add_test(NAME <target>.headless COMMAND <target> --frames 120)
set_tests_properties(<target>.headless PROPERTIES ENVIRONMENT SDL_VIDEO_DRIVER=dummy)
```

`gameSources` is the variable of this file: the C++ files in `native/` and `desktop/`. `DIMENSION` is required: `2d` or `3d` turns the matching `essentials` plugin on. `PLUGINS <plugin>` is optional. `target_include_directories` lets `desktop/main.cpp` include `systems.hpp` from `native/`. The test runs the game for 120 frames with `SDL_VIDEO_DRIVER=dummy`, so it needs no display.

## See also

- [The `oly` command](cli.md): `oly new` and the other commands.
- [Native host](native-host.md): the window and the loop of `desktop/main.cpp`.
- [Test a web game](../how-to/test-a-web-game.md) and [Test a desktop game](../how-to/test-a-desktop-game.md).
