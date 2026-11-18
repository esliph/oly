# Getting started on the desktop

[Guide index](../../README.md)

A desktop game is a C++ game that runs in a window on Linux and Windows, in the `native` environment. There is no TypeScript, no WASM and no C API: your `main` creates the `World` and the `Engine`, opens a `Window` and hands the loop to `run`. It lives in its own repository.

Choose it when the game does not need a browser. Choose [the web](../web/getting-started.md) (C++ with a TS client) when it does. The engine, the schema and the gameplay code are the same on both; only the shell and the drawing change.

## What you need

On **Linux**: `curl`, `tar`, CMake and GCC. On **Windows**: **Git Bash** and **MSYS2 UCRT64** (`gcc`, `cmake` and `ninja`). Run every command from Git Bash, not from PowerShell or `cmd`. Docker is optional.

The exact versions, the install steps and the Docker option are in [The `oly` command](../../cli.md#what-you-install). `oly doctor` checks them for you.

## Create and run a game

```sh
curl -fsSL https://github.com/esliph/oly/releases/latest/download/install.sh | sh
```

Open a new terminal, then:

```sh
oly doctor --environment native
oly new <game> --environment native
cd <game>
oly test
oly run
```

`oly run` opens a window. Press `W`, `A`, `S` and `D` to move the square. The arguments go to the game: `oly run --frames 60` runs 60 frames with a fixed clock and exits.

To build the Windows `.exe`, run `oly build --platform windows`. On Linux this cross-compiles with MinGW, so it needs `mingw-w64`. Every command is described in [The `oly` command](../../cli.md).

## What `oly new` created

```
<game>/
  oly.env                    the engine version, the environment, the target name and the runner
  CMakeLists.txt             fetches the engine at the version of oly.env
  cmake/game.cmake           oly_add_game and the test
  <game>.oly                 the schema
  native/                    gameplay: movement.cpp, systems.hpp
  desktop/main.cpp           the SDL shell: window, drawing, options
  .github/workflows/ci.yml   installs oly and runs oly test --docker
```

Keep the gameplay in `native/` and the SDL code in `desktop/`: the gameplay stays the same if you add another shell. `cmake/game.cmake` holds the build:

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

`<target>` is the game name with `-` written as `_`. `DIMENSION` is required: `2d` or `3d` turns the matching `essentials` plugin on, which brings `Position` and the other [transform](../../concepts/transforms.md) components, so the game's `.oly` does not declare them. `PLUGINS <plugin>` is optional: it lists the [plugins](../../concepts/plugins.md) the game turns on. The test target of [Test your game](#test-your-game) lists the same plugins, so its `setUpGame` installs them too.

A native game has no registry and no `.mjs`: the target is a plain executable in `build/cmake/games/`. `oly.env` carries the engine version; change it with `oly upgrade <version>` ([The `oly` command](../../cli.md#upgrade-the-engine)).

The `desktop/main.cpp` of the template is the example below with a `--frames <n>` option and an error exit.

## The smallest game

```
# game.oly
tag Player
action move: axis2d
```

```cpp
// main.cpp
#include <SDL3/SDL.h>

#include "oly/app/engine.hpp"
#include "oly/app/game.hpp"
#include "oly/ecs/query.hpp"
#include "oly/ecs/world.hpp"
#include "oly/native_host/run.hpp"
#include "oly/native_host/window.hpp"
#include "schema.generated.hpp"

namespace {
  void draw(oly::app::SystemContext& context) {
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(context.userData());

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    context.query<const Position>(oly::ecs::With<Player>{}).each([renderer](const Position& position) {
      const SDL_FRect box{position.x - 8.0f, position.y - 8.0f, 16.0f, 16.0f};
      SDL_RenderFillRect(renderer, &box);
    });
  }
}

int main() {
  oly::ecs::World world;
  oly::app::Engine engine(world);
  oly::app::setUpGame(engine);

  engine.actions().bind(actions::move, {oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
  world.spawn(Player{}, Position{320.0f, 240.0f});

  oly::native_host::Window window({"Game", 640, 480});
  engine.addSystem(oly::app::Phase::Render, "draw", draw, window.renderer());

  return oly::native_host::run(engine, window);
}
```

- `oly::app::setUpGame(engine)` declares the schema's events, actions and states in schema order, so `actions::move` and the other generated ids are valid, then installs the game's [plugins](../../concepts/plugins.md). The build generates it. Call it first, on an `Engine` that has declared nothing yet (`OLY_ASSERT`).
- `Window` opens a resizable window and an SDL renderer. Its size is the **logical size** your game draws in: the host scales it to the window and adds bars when the aspect ratio differs. If SDL cannot open the window (no display, no video driver), the constructor throws `std::runtime_error` with SDL's message.
- `run` returns when the window is closed, and its result is the process exit code.

## Pass data to systems and restrict them to states

A system is a function pointer with an optional `userData` pointer, and `addSystem` returns an id you can restrict to states. Handlers take a `userData` pointer too, and read the batch with `context.events<T>()`:

```cpp
struct Game {
  uint32_t hits = 0;
};

void steer(oly::app::SystemContext& context) {
  Game& game = *static_cast<Game*>(context.userData());
  // read context.actions(), use game
}

void countHits(oly::app::HandlerContext& context) {
  static_cast<Game*>(context.userData())->hits += context.eventCount();
}

Game game;
oly::app::SystemId steerId = engine.addSystem(oly::app::Phase::UpdateBefore, "steer", steer, &game);

engine.setSystemStates(steerId, {states::playing});
engine.on<Hit>("countHits", countHits, &game);
engine.requestState(states::playing);
```

The macros (`OLY_SYSTEM`, `OLY_HANDLER`, `OLY_STATE_HOOK`) are only needed when a client registers by name ([Engine, loop and systems](../../concepts/engine-and-systems.md#systems-in-c-registered-by-name-oly_system-oly_handler-oly_state_hook)). A desktop game with no client can use plain functions, as above, or the macros so that the same code also runs on the web.

## Drawing

There is no draw hook. Draw in a `Render` system, as on the web: the host clears the renderer to black before the frame and presents it after the `Render` phase. Pass the renderer to the system through `userData` and include `<SDL3/SDL.h>` where you draw. The gameplay code does not need SDL, so keep it in separate files: it stays the same if you add another shell.

## What the host does with the window

| Window event | What the engine sees |
|---|---|
| key press and release | the matching `oly::input::Key` button; key repeat is ignored; keys that `Key` does not declare are ignored |
| mouse move | the pointer, in logical coordinates |
| mouse button press | the pointer moves, and `MouseButton::Left`, `Right` or `Middle` goes down only if the click is **inside** the logical area (not in the bars) |
| mouse button release | the button goes up, wherever the pointer is, if it was pressed in the window |
| minimized, hidden or covered | `setHidden(true)`: the game stops and the clock restarts on return |
| restored or shown again | `setHidden(false)` |
| focus lost | every key and mouse button is released; the game keeps running |
| close request | `run` returns |

While the window is hidden, `run` waits for events instead of spinning.

## Test your game

`run` takes `RunOptions`:

| Field | Meaning |
|---|---|
| `maxFrames` | stop after this many frames; `0` (default) runs until the window closes |
| `frameSeconds` | greater than 0: advance the clock by exactly this much per frame instead of reading it |

With `frameSeconds` set, a fixed number of frames covers a known amount of game time. Combine it with `SDL_VIDEO_DRIVER=dummy` to run the whole game, window included, with no display:

```cpp
oly::native_host::run(engine, window, {.maxFrames = 600, .frameSeconds = 1.0 / 60});
```

The first frame only records the time, so `maxFrames = n` runs `n - 1` simulated frames. This checks that the game starts, runs and exits cleanly.

To check what the game *did*, run the gameplay with no window at all. Keep the gameplay in functions that take an `Engine` (as `drift` does with `installGameplay`) and drive `engine.frame` with a time you choose, then read the `World`:

```cpp
// test/main.cpp
#include <cstdio>
#include <string>

#include "gameplay.hpp"
#include "oly/app/engine.hpp"
#include "oly/app/game.hpp"
#include "oly/ecs/diagnostics.hpp"
#include "oly/ecs/world.hpp"
#include "schema.generated.hpp"

int main() {
  oly::ecs::World world;
  oly::app::Engine engine(world);
  oly::app::setUpGame(engine);
  installGameplay(engine);

  engine.actions().bind(actions::move, oly::input::Axis2dBinding{oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
  oly::ecs::Entity player = world.spawn(Player{}, Position{0.0f, 0.0f}, Velocity{0.0f, 0.0f});

  engine.actions().buttonDown(oly::input::Key::D);
  engine.frame(0.0);

  for (int step = 1; step <= 60; ++step) {
    engine.frame(step / 60.0);
  }

  std::string why;
  if (!oly::ecs::checkInvariants(world, &why)) {
    std::fprintf(stderr, "invariants: %s\n", why.c_str());
    return 1;
  }

  if (world.get<Position>(player).x <= 0.0f) {
    std::fprintf(stderr, "the player did not move right\n");
    return 1;
  }

  return 0;
}
```

The test needs the generated schema header but no window, so build it with `oly_add_game` too, without `native_host`, and register it with ctest next to the game in `cmake/game.cmake`:

```cmake
oly_add_game(<target>_test
  ENVIRONMENT native
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/<game>.oly
  SOURCES native/gameplay.cpp test/main.cpp
  DEPENDS ecs app input)
target_include_directories(<target>_test PRIVATE native)

add_test(NAME <target>.gameplay COMMAND <target>_test)
```

Run it:

```sh
oly test                                          # builds and runs every test
ctest --test-dir build/cmake -R '^<target>\.'     # only your game, after oly test
```

`checkInvariants` after each step catches corruption at the step that caused it ([World and entities](../../concepts/world-and-entities.md#debugging-the-world)). The engine's own C++ tests use doctest with a bare `World` and `Engine` (`engine/runtime/app/tests/`); to add one inside the engine, follow the `add-test-case` skill described in [build and tests](../../../architecture/build-and-tests.md).

## Inside the engine repository

This section is for people who change the engine. A game can also live in the `games/` folder of the engine repository, which builds it together with the engine instead of fetching a release. Add it to CMake with `ENVIRONMENT native`:

```cmake
file(GLOB gameSources CONFIGURE_DEPENDS native/*.cpp desktop/*.cpp)

oly_add_game(<game>_game
  ENVIRONMENT native
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/<game>.oly
  SOURCES ${gameSources}
  DEPENDS ecs app input native_host
  PLUGINS <plugin>)
```

A game builds only in its own environment: `oly_add_game` stops the configure when `ENVIRONMENT` is not the one of the build (`native` for the `native-games*` and `windows*` presets, `web` for the WASM ones), so wrap the game's `CMakeLists.txt` in `if(OLY_ENVIRONMENT STREQUAL "native")`. The target is an executable in `build/<preset>/games/`. The toolchain there is Docker only, through `./spur`:

| Where | Command | Result |
|---|---|---|
| Linux | `./spur desktop <game>` | builds `native-games-release` and prints `build/native-games-release/games/<game>_game` |
| Windows | `PRESET=windows-release ./spur desktop <game>` | cross-compiles in the `windows` image and prints `build/windows-release/games/<game>_game.exe` |

On Linux, run the executable on the host: the window needs your display (X11 or Wayland). The Windows `.exe` is static, so a double click runs it on a machine with nothing installed. `./spur native-games-test` runs the Linux tests of every game and `./spur windows-test` runs the same tests under Wine. Each game under `games/` has its own `README.md`. To build a game that lives in its own repository against your checkout, use [local mode](../../cli.md#build-with-an-engine-checkout).

## Next

[Schema](../../concepts/schema.md): declare the data of your game. [Engine, loop and systems](../../concepts/engine-and-systems.md): what `run` calls every frame.
