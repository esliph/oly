# Oly

Oly is a game engine in C++20 for 2D games, built on an archetype ECS (*Entity Component System*). A game runs in the browser (WebAssembly with a TypeScript client) or on the desktop (a native host in a window). Oly is a production engine for indie 2D games; 3D is expected later. C++ on `native` is the first-class environment, and `web` stays supported and may lag one phase behind. Every performance target has a benchmark and a recorded result ([Benchmark results](docs/reference/benchmarks/README.md)).

> **Status:** version `0.x`, in development. The API may change. The packages are not published on npm: a release carries them.

## Features

With Oly you can:

- keep thousands of entities in an archetype ECS and iterate over them without copies;
- write gameplay in **C++**, and in **TypeScript** on the web, or mix both in the same phase;
- run a **fixed-step loop** with pause, hidden-window handling and render interpolation;
- send **events** between systems, in batches;
- map the keyboard and mouse to **actions** and rebind them at run time;
- **schedule** work on the game clock that pauses with the game;
- organize the game in **states** (menu, playing, game over) that clean up after themselves;
- **test the game** with a fake clock and fake input, in Node or in a native test.

Oly does not draw, play audio or do physics: the game does ([Known limitations](docs/reference/limits.md)).

## Install

| | Linux | Windows |
|---|---|---|
| Shell | any POSIX shell | Git Bash |
| Tools | `curl`, `tar` | `curl`, `tar` (Git Bash has them) |
| `native` game | `cmake`, `git`, `make`, GCC, and the X11 and Wayland development headers that SDL3 needs to open a window | MSYS2 UCRT64 with GCC, CMake and Ninja |
| `web` game | the same, plus Node.js and emsdk | the same, plus Node.js and emsdk |

`oly doctor` checks `cmake`, `ninja` (Windows only) and, for a `native` game, `g++` or, for a `web` game, `emcc`, `node` and `npm`, and says what is missing. It does not check `curl`, `tar`, `git`, `make` or the X11 and Wayland headers. The versions and the full table are in [The `oly` command](docs/reference/cli.md#install).

## Create a game

```sh
curl -fsSL https://github.com/esliph/oly/releases/latest/download/oly | sh -s -- new my-game --environment web   # or: --environment native
cd my-game
./oly doctor
./oly test
./oly run
```

Nothing is installed first: the game holds its own launcher as `./oly`. The details are in [The `oly` command](docs/reference/cli.md#create-a-game).

## Minimal example

The `<game>.oly` file declares the `Player` tag and the `move` action. The C++ system moves every `Player` by the axis of `move`:

<!-- template: templates/game/common/game.oly -->
```
tag Player
action move: axis2d
```

<!-- template: templates/game/common/native/movement.cpp -->
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

`oly new` writes these files at `<game>/<game>.oly` and `<game>/native/movement.cpp`. The TypeScript system of a web game is in [Read the TypeScript system](docs/tutorials/first-web-game.md#7-read-the-typescript-system).

## Documentation

| Where | What it has |
|---|---|
| [Tutorials](docs/README.md#tutorials) | make your first web game and your first desktop game |
| [How-to guides](docs/README.md#how-to-guides) | steps for one task: test, debug, add a plugin, upgrade |
| [Reference](docs/README.md#reference) | the `oly` command, the C++ and TypeScript APIs, errors, limits and benchmarks |
| [Explanation](docs/README.md#explanation) | the rules a game relies on, and why the engine is built this way |
| [Working on the engine](docs/README.md#working-on-the-engine) | toolchain, commands, repository map and release |

## Contributing

Bug fixes, tests, benchmarks and documentation are welcome as pull requests against `develop`; a new feature or a public API change starts with an issue. Read [CONTRIBUTING.md](CONTRIBUTING.md) and the [code of conduct](CODE_OF_CONDUCT.md). Questions are welcome in an issue: [SUPPORT.md](SUPPORT.md). Security problems are reported privately: [SECURITY.md](SECURITY.md).

## License

[MIT](LICENSE).
