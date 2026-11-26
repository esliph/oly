# Your first desktop game

[Docs](../README.md#tutorials) · Previous: [Your first web game](first-web-game.md)

## What you will build

A white square that you move with `W`, `A`, `S` and `D` in a window on Linux or Windows. The game is C++ only, with no TypeScript and no WASM, and it lives in its own repository. You will read every file `oly new` writes, change two numbers and run the game with no display.

## Before you start

You need a computer with these tools. In step 1, `oly doctor` checks `cmake`, `ninja` (Windows only) and `g++`. It does not check the other tools in this list.

- On Linux: `curl`, `tar`, `git`, `make`, CMake, GCC, and the X11 and Wayland development headers that SDL3 needs to open a window.
- On Windows: Git Bash and MSYS2 UCRT64 (`gcc`, `cmake` and `ninja`). Run every command from Git Bash, not from PowerShell or `cmd`.
- Docker is optional. A window cannot open in a container, so `oly run --docker` does not work for a desktop game.

The exact versions are in [The `oly` command](../reference/cli.md#install). To make a game for the browser instead, follow [Your first web game](first-web-game.md).

## 1. Check your tools

```sh
curl -fsSL https://github.com/esliph/oly/releases/latest/download/oly | sh -s -- doctor --environment native
```

There is nothing to install first: the command downloads the launcher of the latest release and runs its `doctor` outside a game. You see one line per tool. Each line starts with `ok`, `old`, `untested`, `missing` or `wrong`, then the tool and what to install. Fix every line that does not say `ok`, then run the command again.

## 2. Create the game

```sh
curl -fsSL https://github.com/esliph/oly/releases/latest/download/oly | sh -s -- new <game> --environment native
cd <game>
```

`<game>` is the name of the folder: lowercase letters, digits and `-`, starting with a letter. The command downloads the launcher, which copies [the game template](../reference/game-template.md#native) into the folder and leaves itself in the game as `./oly`.

You see `created <game> (native, oly <version>)`, and a folder with `oly`, `oly.env`, `<game>.oly`, `native/`, `desktop/` and the other files of the template. Run `./oly` from inside it: it is the launcher of this game.

## 3. Run the game

```sh
./oly run
```

`oly run` builds the game and runs it. You see a window with a white square in the middle. Press `W`, `A`, `S` and `D`: the square moves. Close the window to stop the game.

The arguments of `oly run` go to the game. This command runs 60 frames with a fixed clock and exits:

```sh
./oly run --frames 60
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
#include "oly/app/system_descriptor.hpp"
#include "systems.hpp"

namespace {
  // Pixels per second; an initial value, not measured.
  constexpr float PlayerSpeed = 200.0f;
}

void movePlayer(oly::app::Query<Position, oly::ecs::With<Player>> players, oly::app::Res<const oly::input::ActionMap> actions, const oly::app::StepInfo& step) {
  const oly::input::Vector2 axis = actions->axis2d(actions::move);
  const float distance = PlayerSpeed * step.fixedDeltaTime;

  players.each([&](Position& position) {
    position.x += axis.x * distance;
    position.y += axis.y * distance;
  });
}

OLY_SYSTEM(movePlayer);
```

`movePlayer` is a function the engine can run every step; its parameters declare what it reads and writes, and `OLY_SYSTEM(movePlayer);` registers it by name. The query visits every entity that has `Position` and the tag `Player`. `native/systems.hpp` declares the function, so the shell can add it:

```cpp
#pragma once

#include "oly/app/system_parameters.hpp"
#include "oly/ecs/query.hpp"
#include "oly/input/action_map.hpp"
#include "schema.generated.hpp"

// Registered by name through OLY_SYSTEM; declared here for a shell that adds it with Engine::addSystem.
void movePlayer(oly::app::Query<Position, oly::ecs::With<Player>> players, oly::app::Res<const oly::input::ActionMap> actions, const oly::app::StepInfo& step);
```

Change `PlayerSpeed` to `400.0f` and run `./oly run` again. You see the square cross the window twice as fast.

## 6. Read the shell

The shell is `desktop/main.cpp`. It holds the game as a class, `MyGame`, and starts it with one call, `runGame`. This is the whole file:

```cpp
#include <SDL3/SDL.h>

#include "oly/app/engine.hpp"
#include "oly/app/system_descriptor.hpp"
#include "oly/ecs/query.hpp"
#include "oly/ecs/world.hpp"
#include "oly/native_host/game.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"

namespace {
  constexpr int LogicalWidth = 640;
  constexpr int LogicalHeight = 360;
  constexpr float PlayerSize = 16.0f;

  struct Renderer {
    SDL_Renderer* renderer;
  };

  void draw(oly::app::Query<const Position, oly::ecs::With<Player>> players, oly::app::Res<const Renderer> target) {
    SDL_Renderer* renderer = target->renderer;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    players.each([renderer](const Position& position) {
      const SDL_FRect box{position.x - PlayerSize / 2, position.y - PlayerSize / 2, PlayerSize, PlayerSize};
      SDL_RenderFillRect(renderer, &box);
    });
  }

  class MyGame : public oly::native_host::Game {
   public:
    MyGame()
      : Game({"<game>", LogicalWidth, LogicalHeight}) {}

    void onStart(oly::app::Engine& engine, oly::native_host::Window& window) override {
      engine.actions().bind(actions::move, {oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
      engine.world().spawn(Player{}, Position{LogicalWidth / 2.0f, LogicalHeight / 2.0f});
      engine.world().insertResource(Renderer{window.renderer()});
      engine.addSystem<&movePlayer>(oly::app::Update{}, "movePlayer");
      engine.addSystem<&draw>(oly::app::Render{}, "draw");
    }
  };
}

int main(int argumentCount, char** arguments) {
  return oly::native_host::runGame<MyGame>(argumentCount, arguments);
}
```

In the file, in order:

- `Renderer` is a resource the shell inserts in `onStart`; `draw` is a `Render` system that reads it through `Res<const Renderer>`.
- `MyGame` derives `oly::native_host::Game`. Its constructor gives the window configuration. The size, `640` by `360`, is the logical size the game draws in: the host scales it to the window and adds bars when the shape differs ([Native host](../reference/native-host.md)).
- `onStart` is the content of the game. `runGame` calls it once, on an `Engine` that it already set up with `setUpGame`, with the window open. `engine.actions().bind` ties the keys `W`, `S`, `A` and `D` to the two axes of `move`. `engine.world().spawn` creates the player in the middle of the logical area. `engine.world().insertResource` stores the renderer, and `addSystem<&movePlayer>` and `addSystem<&draw>` add the two systems to the `Update` and `Render` phases.
- `runGame<MyGame>` reads `--frames <n>`, constructs the game, opens the window and runs the loop. It returns the exit code: `0` when the window closes, `64` for a wrong command line and `1` when an exception escapes, after it prints `<title>: <message>`.

Rename `MyGame` to fit your game. Change `PlayerSize` to `32.0f` and run `./oly run` again. You see a square twice as wide.

## 7. Run the tests

```sh
./oly test
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
