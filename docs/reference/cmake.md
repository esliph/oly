# CMake

[Reference](../README.md#reference) · Concept: [Plugins](../explanation/concepts/plugins.md) · Template: [The game template](game-template.md)

The CMake functions and options a game sets. A game calls `oly_add_game` once per target; a plugin calls `oly_add_plugin`. The engine's own build options are in [Commands](contributor/commands.md#engine-options).

## `oly_add_game`

```cmake
oly_add_game(<target>
  ENVIRONMENT web|native
  DIMENSION 2d|3d
  SCHEMA <file.oly>
  SOURCES <cpp...>
  DEPENDS <modules...>
  [PLUGINS <plugins...>]
  [TYPESCRIPT <file.ts>])
```

Defines the game target `<target>`, an executable written to `build/games/` of the build directory.

| Argument | Meaning |
|---|---|
| `ENVIRONMENT` | `web` or `native`. It must be the environment of the build: `web` under Emscripten, `native` otherwise |
| `DIMENSION` | `2d` or `3d`. It turns on `essentials2d` or `essentials3d` ([Transforms](../explanation/concepts/transforms.md)) |
| `SCHEMA` | the game's `.oly` file |
| `SOURCES` | the game's `.cpp` files |
| `DEPENDS` | the modules the game links, such as `ecs app capi` for a web game or `ecs app input native_host` for a desktop game |
| `PLUGINS` | the plugins to turn on, besides the one `DIMENSION` brings |
| `TYPESCRIPT` | the file where the build writes the merged schema as TypeScript. Only a web game that has TypeScript passes it |

What the call does:

1. Puts the plugin of `DIMENSION` first in the plugin list.
2. Sorts the list: each plugin comes after the plugins it depends on, each plugin appears once, and independent plugins keep the order of the list. A game that lists only `doubling` also gets `counting`, first.
3. Runs `oly-schema` on the game's `.oly` and the `.oly` of each plugin that has one, in that order, and writes the C++ schema.
4. Writes `game_setup.generated.cpp`, which defines `oly::app::setUpGame`: it declares the schema, then installs the plugins in the sorted order.
5. Links the modules of `DEPENDS` and the plugins.
6. With `ENVIRONMENT web`, also writes the schema registry, links `oly::web_host` and makes the target a WASM module with the suffix `.mjs`. With `ENVIRONMENT native`, `SOURCES` bring their own `main`, which calls `runGame<G>` (it calls `setUpGame`), or drives the loop itself and calls `setUpGame` first.
7. With `TYPESCRIPT`, writes the same merged schema as TypeScript to that file, so the TS client and the WASM module hash the same schema.

For `PLUGINS doubling`, the generated function is:

```cpp
namespace oly::app {
  void setUpGame(Engine& engine) {
    declareSchema(engine);
    engine.installPlugin<oly::counting::Plugin>();
    engine.installPlugin<oly::doubling::Plugin>();
  }
}
```

Configure stops with a message in these cases:

| Message | Cause |
|---|---|
| `<target>: oly_add_game needs DIMENSION 2d\|3d` | `DIMENSION` is missing |
| `<value>: DIMENSION is 2d or 3d` | `DIMENSION` has another value |
| `<target>: <plugin> comes from DIMENSION; do not list it in PLUGINS` | `PLUGINS` lists `essentials2d` or `essentials3d` |
| `<target>: oly_add_game needs ENVIRONMENT web\|native` | `ENVIRONMENT` is missing |
| `<target> is a <environment> game, but this build is <environment>` | `ENVIRONMENT` is not the environment of the build |
| `unknown plugin <plugin>` | a name in `PLUGINS` has no `oly_add_plugin` |
| `plugin cycle: a -> b -> a` | plugins depend on each other in a loop |

The same messages are in [Errors](errors.md#at-build-time). How a game turns a plugin on: [Use a plugin](../how-to/use-a-plugin.md).

## `oly_add_plugin`

```cmake
oly_add_plugin(NAME <name>
  [SCHEMA <file.oly>]
  DEPENDS <modules...>
  [PLUGINS <plugins...>])
```

Defines a plugin as a static library built from `src/*.cpp`. The folder needs at least one source file; otherwise configure stops with `plugin <name>: no src/*.cpp; a plugin defines oly::<name>::Plugin`.

| Argument | Meaning |
|---|---|
| `NAME` | the name of the plugin |
| `SCHEMA` | the plugin's `.oly` file, relative to the plugin's folder. The plugin declares only components, tags and events |
| `DEPENDS` | the modules the plugin links |
| `PLUGINS` | the plugins it depends on. A game that turns this plugin on gets them too, installed first |

Every plugin follows one convention:

| Item | Value |
|---|---|
| target and alias | `oly_<name>` and `oly::<name>` |
| namespace | `oly::<name>` |
| install class | `oly::<name>::Plugin`, in `include/oly/<name>/plugin.hpp` |
| sources | `src/*.cpp` |
| generated header, with `SCHEMA` | `oly/<name>/schema.generated.hpp`, with the plugin's own types only |

How to write a plugin: [Write a plugin](../how-to/write-a-plugin.md). The rules a plugin follows: [Boundary rules](contributor/boundary-rules.md).

## Options

A game sets these options when it configures. `oly build` and the other game tasks pass the ones they need.

| Option | Default | What it does |
|---|---|---|
| `OLY_BUILD_NATIVE_HOST` | `OFF` | Builds `oly::native_host` and fetches SDL3. A desktop game needs it. It stops the configure on Emscripten, which has no native host |
| `OLY_BUILD_TESTS` | `ON` when the engine is the top-level project, `OFF` when a game fetches the engine | Builds the engine's own tests |
| `OLY_ENABLE_ASSERTS` | empty | `0` or `1`. Empty means `0` in the `Release`, `RelWithDebInfo` and `MinSizeRel` build types, and `1` otherwise. With `0`, `OLY_ASSERT` checks nothing ([Errors](errors.md#c)) |
| `OLY_ALLOW_UNTESTED_TOOLCHAIN` | `OFF` | Turns the error for an Emscripten other than the `EMSDK_VERSION` of `toolchain.env` into a warning. It does not relax the minimum versions of GCC, CMake and Ninja ([Errors](errors.md#at-build-time)) |
| `OLY_SCHEMA_TOOL` | `<project source dir>/build/schema-tool/bin/oly-schema` | The `oly-schema` executable that `oly_add_game` and `oly_add_plugin` run when the build has no `oly-schema` target of its own, as in a web game that fetches the engine. It must be built for the machine, because WASM cannot run the one it builds. The configure stops when the file does not exist. the web build of `oly` passes it |
