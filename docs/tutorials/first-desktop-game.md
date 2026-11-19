# Your first desktop game

[Docs](../README.md#tutorials) · Previous: [Your first web game](first-web-game.md)

## What you will build

A white square that you move with `W`, `A`, `S` and `D` in a window on Linux or Windows. The game is C++ only, with no TypeScript and no WASM, and it lives in its own repository. You will read every file `oly new` writes, change two numbers and run the game with no display.

## Before you start

You need a computer with these tools. In step 1, `oly doctor` checks `cmake`, `ninja` (Windows only) and `g++`. It does not check the other tools in this list.

- On Linux: `curl`, `tar`, `git`, `make`, CMake, GCC, and the X11 and Wayland development headers that SDL3 needs to open a window.
- On Windows: Git Bash and MSYS2 UCRT64 (`gcc`, `cmake` and `ninja`). Run every command from Git Bash, not from PowerShell or `cmd`.
- Docker is optional. A window cannot open in a container, so `oly run --docker` does not work for a desktop game.

The exact versions and the install steps are in [The `oly` command](../reference/cli.md#install). To make a game for the browser instead, follow [Your first web game](first-web-game.md).

## 1. Install `oly`

Install the launcher with the command in [The `oly` command](../reference/cli.md#install). Open a new terminal, then check your tools:

```sh
oly doctor --environment native
```

You see one line per tool. Each line starts with `ok`, `old`, `untested`, `missing` or `wrong`, then the tool and what to install. Fix every line that does not say `ok`, then run the command again.

## 2. Create the game

```sh
oly new <game> --environment native
cd <game>
```

`<game>` is the name of the folder: lowercase letters, digits and `-`, starting with a letter. `oly new` copies [the game template](../reference/game-template.md#native).

You see `created <game> (native, oly <version>)`, and a folder with `oly.env`, `<game>.oly`, `native/`, `desktop/` and the other files of the template.

## 3. Run the game

```sh
oly run
```

`oly run` builds the game and runs it. You see a window with a white square in the middle. Press `W`, `A`, `S` and `D`: the square moves. Close the window to stop the game.

The arguments of `oly run` go to the game. This command runs 60 frames with a fixed clock and exits:

```sh
oly run --frames 60
```

You see no window stay open: the game starts, runs and ends by itself.

## 4. Read the schema

The schema declares the data of the game. This is `<game>.oly`:

```
tag Player
action move: axis2d
```

`Player` marks the entity that moves. `move` is an input action with two axes. `Position` is not declared here: `DIMENSION 2d` in `cmake/game.cmake` turns the `essentials2d` plugin on, and the plugin brings it ([Transforms](../explanation/concepts/transforms.md)).

The build turns this file into `schema.generated.hpp`, which has the `Position` and `Player` structs and the `actions::move` handle.

You see the result in the build: `schema.generated.hpp` is somewhere under `build/` after `oly build`.

## 5. Read the gameplay

The gameplay lives in `native/` and does not include SDL, so it stays the same if you add another shell. This is `native/movement.cpp`:

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

`OLY_SYSTEM(movePlayer)` defines a function the engine can run every step. The query visits every entity that has `Position` and the tag `Player`. `native/systems.hpp` declares the function, so the shell can add it:

```cpp
#pragma once

#include "oly/app/system.hpp"

// Registered by name through OLY_SYSTEM; declared here for a shell that adds it with Engine::addSystem.
void movePlayer(oly::app::SystemContext& context);
```

Change `PlayerSpeed` to `400.0f` and run `oly run` again. You see the square cross the window twice as fast.

## 6. Read the shell

The shell is `desktop/main.cpp`. It creates the `World` and the `Engine`, opens the window and hands the loop to `run`. This is the whole file:

```cpp
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string_view>

#include "oly/app/engine.hpp"
#include "oly/app/game.hpp"
#include "oly/ecs/query.hpp"
#include "oly/ecs/world.hpp"
#include "oly/native_host/run.hpp"
#include "oly/native_host/window.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"

namespace {
  constexpr int UsageExitCode = 64;
  constexpr int LogicalWidth = 640;
  constexpr int LogicalHeight = 360;
  constexpr float PlayerSize = 16.0f;

  bool parseOptions(int argumentCount, char** arguments, oly::native_host::RunOptions& options) {
    for (int index = 1; index < argumentCount; ++index) {
      const std::string_view argument = arguments[index];
      if (argument != "--frames" || index + 1 >= argumentCount) {
        return false;
      }

      const unsigned long long frames = std::strtoull(arguments[++index], nullptr, 10);
      if (frames == 0) {
        return false;
      }

      // Headless runs: a fixed number of frames over a known amount of game time.
      options.maxFrames = frames;
      options.frameSeconds = 1.0 / 60;
    }

    return true;
  }

  void draw(oly::app::SystemContext& context) {
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(context.userData());

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    context.query<const Position>(oly::ecs::With<Player>{}).each([renderer](const Position& position) {
      const SDL_FRect box{position.x - PlayerSize / 2, position.y - PlayerSize / 2, PlayerSize, PlayerSize};
      SDL_RenderFillRect(renderer, &box);
    });
  }
}

int main(int argumentCount, char** arguments) {
  oly::native_host::RunOptions options;
  if (!parseOptions(argumentCount, arguments, options)) {
    std::fprintf(stderr, "usage: %s [--frames <n>]\n", arguments[0]);
    return UsageExitCode;
  }

  try {
    oly::ecs::World world;
    oly::app::Engine engine(world);
    oly::app::setUpGame(engine);

    engine.actions().bind(actions::move, {oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
    world.spawn(Player{}, Position{LogicalWidth / 2.0f, LogicalHeight / 2.0f});
    engine.addSystem(oly::app::Phase::Update, "movePlayer", movePlayer);

    oly::native_host::Window window({"<game>", LogicalWidth, LogicalHeight});
    engine.addSystem(oly::app::Phase::Render, "draw", draw, window.renderer());

    return oly::native_host::run(engine, window, options);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "<game>: %s\n", error.what());
    return 1;
  }
}
```

In `main`, in order:

- `oly::app::setUpGame(engine)` declares the schema's events, actions and states, then installs the game's plugins. The build generates it. Call it first, on an `Engine` that has declared nothing yet.
- `engine.actions().bind` ties the keys `W`, `S`, `A` and `D` to the two axes of `move`.
- `world.spawn` creates the player in the middle of the logical area.
- `engine.addSystem` adds `movePlayer` to the `Update` phase and `draw` to the `Render` phase. `draw` gets the SDL renderer through `userData`.
- `Window` opens a resizable window. Its size, `640` by `360`, is the logical size the game draws in: the host scales it to the window and adds bars when the shape differs ([Native host](../reference/native-host.md)).
- `run` returns when the window is closed, and `main` returns its result as the exit code. `parseOptions` fills `RunOptions` from `--frames <n>`.

Change `PlayerSize` to `32.0f` and run `oly run` again. You see a square twice as wide.

## 7. Run the tests

```sh
oly test
```

`oly test` builds the game and runs its tests. The test `<target>.headless` runs the game for 120 frames with `SDL_VIDEO_DRIVER=dummy`, so it needs no display ([The game template](../reference/game-template.md#native)).

You see ctest report `<target>.headless` as passed.

## Next steps

- [The game template](../reference/game-template.md#native): every file of the game, one row each.
- [Test a desktop game](../how-to/test-a-desktop-game.md): check what the game did, not only that it ran.
- [Native host](../reference/native-host.md): the window, the loop and what the host does with window events.
- [Environments and hosts](../explanation/concepts/environments-and-hosts.md): how the core and the native host fit together.
- [Concepts](../explanation/concepts/README.md): the rules a game relies on, in reading order.
- [The `oly` command](../reference/cli.md): every command, including `oly build --platform windows` for the Windows `.exe`.
