# Test a desktop game

[Docs](../README.md#how-to-guides) · Concept: [Environments and hosts](../explanation/concepts/environments-and-hosts.md)

Use this to check that a desktop game starts, runs and exits, and to check what its gameplay did, with no display. It assumes a game made by `oly new <game> --environment native` ([Your first desktop game](../tutorials/first-desktop-game.md)), where the gameplay is in `native/` and the SDL code is in `desktop/`.

## Run the whole game with no display

`run` takes `RunOptions`. With `frameSeconds` set, a fixed number of frames covers a known amount of game time. Combine it with `SDL_VIDEO_DRIVER=dummy` to run the game, window included:

```cpp
oly::native_host::run(engine, window, {.maxFrames = 600, .frameSeconds = 1.0 / 60});
```

The first frame only records the time, so `maxFrames = n` runs `n - 1` simulated frames. This checks that the game starts, runs and exits cleanly. The template already does it: `oly run --frames 120` is what its test `<target>.headless` runs ([The game template](../reference/game-template.md#native)). The fields are in [Native host](../reference/native-host.md#runoptions).

## Check what the gameplay did

To check what the game did, run the gameplay with no window. Keep the gameplay in functions that take an `Engine`, drive `engine.frame` with a time you choose, then read the `World`. This test presses `D` for one second and checks that the player moved right. Put it in `test/main.cpp`:

```cpp
#include <cstdio>
#include <string>

#include "oly/app/engine.hpp"
#include "oly/app/game.hpp"
#include "oly/ecs/diagnostics.hpp"
#include "oly/ecs/world.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"

int main() {
  oly::ecs::World world;
  oly::app::Engine engine(world);
  oly::app::setUpGame(engine);

  engine.actions().bind(actions::move, {oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
  engine.addSystem(oly::app::Phase::Update, "movePlayer", movePlayer);
  oly::ecs::Entity player = world.spawn(Player{}, Position{0.0f, 0.0f});

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

The test needs the generated schema header but no window. Build it with `oly_add_game` too, without `native_host`, and register it with ctest next to the game in `cmake/game.cmake`:

```cmake
oly_add_game(<target>_test
  ENVIRONMENT native
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/<game>.oly
  SOURCES native/movement.cpp test/main.cpp
  DEPENDS ecs app input)
target_include_directories(<target>_test PRIVATE native)

add_test(NAME <target>.gameplay COMMAND <target>_test)
```

If the game turns on `PLUGINS`, list the same plugins in this target, so its `setUpGame` installs them too.

`checkInvariants` after each step catches corruption at the step that caused it ([Debug the world](debug-the-world.md)).

## Run the tests

```sh
./oly test                                          # builds and runs every test
ctest --test-dir build/cmake -R '^<target>\.'     # only your game, after oly test
```

## See also

- [Native host](../reference/native-host.md): `RunOptions` and `run`.
- [Drive the loop yourself](drive-the-loop-yourself.md): call `frame` and press buttons from a test or a tool.
- [Test a web game](test-a-web-game.md): the same idea in Node, against the WASM.
- [Set up the toolchain](contributor/set-up-the-toolchain.md#5-add-a-test): how to add a test inside the engine.
