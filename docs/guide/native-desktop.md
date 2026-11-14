# Desktop games

[Guide index](README.md) · [Getting started](01-getting-started.md) · [C++ gameplay](10-cpp-gameplay.md)

A desktop game is a C++-only game that runs in a window on Linux and Windows. There is no TypeScript, no WASM and no C API: your `main` creates the `World` and the `Engine`, opens a `Window` and hands the loop to `run`.

Choose it when the game does not need a browser. Choose the web (TS + C++) when it does. The engine, the schema and the gameplay code are the same on both; only the shell and the drawing change.

## The smallest game

```
# game.oly
component Position { x: f32, y: f32 }
tag Player
action move: axis2d
```

```cpp
// main.cpp
#include <SDL3/SDL.h>

#include "oly/app/engine.hpp"
#include "oly/app/schema.hpp"
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
  oly::app::declareSchema(engine);

  engine.actions().bind(actions::move, {oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
  world.spawn(Player{}, Position{320.0f, 240.0f});

  oly::native_host::Window window({"Game", 640, 480});
  engine.addSystem(oly::app::Phase::Render, "draw", draw, window.renderer());

  return oly::native_host::run(engine, window);
}
```

- `oly::app::declareSchema(engine)` declares the schema's events, actions and states in schema order, so `actions::move` and the other generated ids are valid. Call it first, on an `Engine` that has declared nothing yet (`OLY_ASSERT`).
- `Window` opens a resizable window and an SDL renderer. Its size is the **logical size** your game draws in: the host scales it to the window and adds bars when the aspect ratio differs. If SDL cannot open the window (no display, no video driver), the constructor throws `std::runtime_error` with SDL's message.
- `run` returns when the window is closed, and its result is the process exit code.

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

## Headless and fixed-time runs

`run` takes `RunOptions`:

| Field | Meaning |
|---|---|
| `maxFrames` | stop after this many frames; `0` (default) runs until the window closes |
| `frameSeconds` | greater than 0: advance the clock by exactly this much per frame instead of reading it |

With `frameSeconds` set, a fixed number of frames covers a known amount of game time, so a test can run a game for ten seconds in a blink. Combine it with `SDL_VIDEO_DRIVER=dummy` to run with no display:

```cpp
oly::native_host::run(engine, window, {.maxFrames = 600, .frameSeconds = 1.0 / 60});
```

The first frame only records the time, so `maxFrames = n` runs `n - 1` simulated frames.

## Build it

Add the game to CMake with `ENVIRONMENT native`:

```cmake
file(GLOB gameSources CONFIGURE_DEPENDS native/*.cpp desktop/*.cpp)

oly_add_game(<game>_game
  ENVIRONMENT native
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/<game>.oly
  SOURCES ${gameSources}
  DEPENDS ecs app input native_host)
```

A game builds only in its own environment: `oly_add_game` stops the configure when `ENVIRONMENT` is not the one of the build (`native` for the `native-games*` and `windows*` presets, `web` for the WASM ones), so wrap the game's `CMakeLists.txt` in `if(OLY_ENVIRONMENT STREQUAL "native")`. A native game has no registry and no `.mjs`: the target is a plain executable in `build/<preset>/games/`.

## Run it

| Where | Command | Result |
|---|---|---|
| Linux | `./spur desktop <game>` | builds `native-games-release` and prints `build/native-games-release/games/<game>_game` |
| Windows | `PRESET=windows-release ./spur desktop <game>` | cross-compiles in the `windows` image and prints `build/windows-release/games/<game>_game.exe` |

On Linux, run the executable on the host: the window needs your display (X11 or Wayland). The Windows `.exe` is static, so a double click runs it on a machine with nothing installed.

Test a desktop game like any other executable:

```cmake
add_test(NAME <game>.desktop COMMAND <game>_game --frames 600)
set_tests_properties(<game>.desktop PROPERTIES ENVIRONMENT SDL_VIDEO_DRIVER=dummy)
```

`./spur native-games-test` runs the Linux tests, and `./spur windows-test` runs the same tests under Wine. Each game under `games/` has its own `README.md`.
