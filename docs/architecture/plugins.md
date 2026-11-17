# Plugins

Code in `engine/runtime/app/include/oly/app/{plugin,game}.hpp`, `cmake/OlyPlugin.cmake`, `cmake/OlyGame.cmake` and `engine/schema/codegen/src/merge.cpp`. Spec: `docs/superpowers/specs/2026-11-16-plugins-design.md`. How a game turns a plugin on and how to write one: [the guide](../guide/concepts/plugins.md).

A **plugin** is a unit of engine-level functionality, compiled together with the core, that a game turns on in its build. It brings its own data (an optional `.oly`), its systems, handlers, hooks and observers, and its own state. A game uses a plugin through data: it adds and removes the plugin's components.

Plugins depend on the core; the core never depends on a plugin. The core stays complete without them.

The mechanism exists today. The only plugins are the engine's test plugins `counting` and `doubling` ([Test plugins](#test-plugins)). The first real plugin, `essentials` (transforms and a parent-child hierarchy), and the top-level `plugins/` folder arrive in phase 3 of the spec.

## Parts

A plugin has up to three parts. Only the C++ part is required.

| Part | Contents | Exists today |
|---|---|---|
| C++ | data (`.oly`), systems, handlers, hooks, observers, public C++ functions | yes |
| C API | functions exported to clients | no: shape only |
| TS package `@oly/<plugin>` | wrappers of the C API part and pure-TS helpers | no: shape only |

The C API and TS parts get build support with the first plugin that needs them. Their shape is fixed now, by rule 2 of the architecture ([README.md](README.md#overview)):

- A C++ game calls a plugin's public C++ functions directly, and the compiler inlines them.
- A client call crosses the WASM boundary at ~40–50 ns ([performance.md](performance.md)), while a helper such as `lerp` costs ~1 ns. So **a plugin's C API part exposes only functions that work per batch or per system** (`raycastBatch`), never per entity.
- Pure helpers (math) are rewritten in the plugin's TS package, not called through the boundary.

## Install and ownership

| Name | What it does |
|---|---|
| `oly::app::Plugin` (`oly/app/plugin.hpp`) | base class: a virtual destructor and `virtual void install(Engine&) = 0` |
| `Engine::installPlugin<P>()` | constructs `P`, keeps it, runs its `install` once and returns it |
| `Engine::plugin<P>()` | returns the installed `P`, so a game can configure it before the first `frame` |
| `Engine::~Engine()` | destroys the plugins in reverse install order |

- `P` derives from `Plugin` and is default constructible (`static_assert`).
- A plugin installs once per `Engine`, before the first `frame` call. Installing twice, or after the first `frame` call, is an `OLY_ASSERT`; so is `plugin<P>()` for a plugin that is not installed. Each message names the type.
- `install` registers the plugin's systems, handlers, hooks and observers. A plugin installed earlier is reachable from `install` through `plugin<Q>()`.
- **The `Engine` owns the plugin instances.** A plugin's state lives in its instance and reaches its systems through `userData`.
- Each plugin type gets a dense per-process key (`pluginKey<P>()`), the same pattern as `eventKey<T>()`. The lookup walks the installed list; it is a cold path.
- **Destructor contract.** Systems, handlers and hooks die with the `Engine`. Observers belong to the `World` ([ecs.md](ecs.md)), and the `World` outlives the `Engine` (`OlyEngine` and a native `main` declare it first). So a plugin's destructor removes the observers its `install` registered (`World::removeObserver`); otherwise the `World` would call into a destroyed plugin.

No game code installs a plugin: the generated `setUpGame` does it, in dependency order ([Build](#build)).

## Order bands

Plugins install before the game registers its systems, so registration order alone would run every plugin system before the game's systems of the same phase. A plugin that must see the game's changes in the same phase asks for the `Last` band.

- `OrderBand::Default` and `OrderBand::Last` (`oly/app/phase.hpp`). The band is the last parameter of `addSystem` (default `Default`) and is fixed at registration.
- Inside a phase, the `Default` band runs before the `Last` band; inside a band, systems run in registration order.
- The phase applies its `CommandBuffer` once, after the `Last` band.
- **Bands are storage, not a branch.** The `Engine` keeps one vector of systems per phase and band; `runPhase` walks the bands in order. There is no extra check per system.
- The C API and the TS client register in `Default` only.

Details of the loop: [app.md](app.md).

## Schema merge

`oly-schema` takes the plugins' `.oly` files with `--plugin <name>=<file.oly>`, one per plugin with a schema, in install order, plus the game's `.oly` ([schema-and-codegen.md](schema-and-codegen.md)).

- **Order.** The merged schema holds the plugins' declarations in install order, then the game's. That order gives the `ComponentId` order and the event dispatch order.
- **Flat names.** There are no namespaces. A name repeated across the merged files is an error that names both locations: `<later file>:<line>:<column>: repeated name "X"; first declared at <earlier file>:<line>:<column>`.
- **A plugin `.oly` declares only components, tags and events.** An action or a state in a plugin file is an error (`a plugin declares only components, tags and events; action "x" is not allowed`). The reason: action and state ids are `constexpr` values in the generated header (`actions::`, `states::`), starting at 0, and a plugin's header is generated once, while its ids would depend on each game's install order. Events are keyed by C++ type, so they work. The cpp-plugin emitter checks this too, so the plugin's own build fails first.
- **One hash.** The hash covers the merged canonical form. Adding or removing a plugin changes it, so `SchemaMismatchError` catches a TS client built with another plugin set. Without `--plugin`, the output and the hash are the same as before plugins existed.
- **C++ types are defined once (ODR).** The `cpp-plugin` emitter writes the plugin's own header, `oly/<name>/schema.generated.hpp`, with its types only: no declaration and no registry. The game's header includes each plugin header in install order and defines only the game's own types (`Type::origin` is empty). The declaration (`declareSchema`), the registry and the TS output cover the whole merged set.

## Build

### `oly_add_plugin`

`oly_add_plugin(NAME <p> [SCHEMA <file.oly>] DEPENDS <modules...> [PLUGINS <plugins...>])`, in `cmake/OlyPlugin.cmake`, defines a plugin. Every plugin follows one convention:

| Item | Value |
|---|---|
| target / alias | `oly_<name>` (`STATIC`) / `oly::<name>` |
| namespace | `oly::<name>` |
| install class | `oly::<name>::Plugin`, in `include/oly/<name>/plugin.hpp` |
| sources | `src/*.cpp`, by glob, like `oly_add_module`; at least one is required |
| generated header (with `SCHEMA`) | `oly/<name>/schema.generated.hpp`, written by `oly-schema --emit cpp-plugin=<dir>` |
| links | `oly::<m>` for each `DEPENDS`, `oly::<p>` for each `PLUGINS` |

It records the plugin's `.oly` and its plugin dependencies (global properties `OLY_PLUGIN_SCHEMA_<name>` and `OLY_PLUGIN_DEPENDENCIES_<name>`), so `oly_add_game` can find them, and registers the plugin in the boundary manifest. A plugin with no `install` (a pure function library) is not supported yet ([limits.md](limits.md)).

### Install order

`oly_plugin_order(<outputVariable> <plugins...>)` is a depth-first walk over the dependencies: each plugin comes after its dependencies, each plugin appears once, and independent plugins keep the order of the list. A game that lists only `doubling` gets `counting` too, first. An unknown plugin stops the configure with `unknown plugin <p>`; a cycle with `plugin cycle: a -> b -> a`.

### The game: `oly_add_game` and `setUpGame`

`oly_add_game(<target> ENVIRONMENT web|native SCHEMA <file.oly> SOURCES ... DEPENDS ... [PLUGINS <plugins...>] [TYPESCRIPT <file.ts>])`:

1. sorts `PLUGINS` with `oly_plugin_order`;
2. runs `oly-schema` with one `--plugin` per plugin that has a schema;
3. links the plugin targets;
4. writes `game_setup.generated.cpp` (with `file(GENERATE)`), which defines `oly::app::setUpGame`.

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

- `setUpGame` (`oly/app/game.hpp`) is the game's entry point: it declares the schema, then installs the plugins. `olyEngineCreate` calls it ([capi.md](capi.md)); a native game's `main` calls it right after creating the `Engine`. A TS game writes nothing.
- CMake writes `game_setup.generated.cpp`, not `oly-schema`: the plugins' C++ types are a build fact, not schema data.
- Both schema commands depend on `game_setup.generated.cpp`. Make does not rerun a custom command whose command line changed, so a new `PLUGINS` list would leave the schemas stale. `file(GENERATE)` rewrites the file only when the plugin list changes, so it works as the stamp.
- **TS emission.** With `TYPESCRIPT <file.ts>`, the game's target also writes the merged schema as TS to that file. The custom target `oly_typescript_schemas` depends on every game's TS file; `./spur ts` configures a `wasm-games*` preset and builds it before `tsc`. The plugin list lives only in CMake, and the C++ registry and the TS file always hash the same merged schema.
- `oly_schema_tool(<toolVariable> <dependencyVariable>)` picks the `oly-schema` that both `oly_add_plugin` and `oly_add_game` run.

### Engine test games

`oly_add_engine_test_game(...)` takes the same arguments as `oly_add_game` and shares its body. It is only for `engine/` (the lint rejects it elsewhere), and its games turn on only the test plugins of `engine/testing/fixture/plugins/`, because `engine/` never depends on `plugins/`. Today the two functions build the same thing; phase 3 adds `DIMENSION` and `essentials` to `oly_add_game` alone.

## Boundaries

The rules that keep publishing cheap later hold from day one:

- A plugin includes only public headers of the modules and plugins it declares (no `detail/` of another).
- A plugin is its own CMake target, with no path relative to the repository root.
- A plugin's `.oly` reaches `oly-schema` as an argument, never through a fixed path.

The `oly.lint.boundaries` lint checks plugins like modules, and adds rules of its own ([modules-and-boundaries.md](modules-and-boundaries.md#how-the-boundaries-are-enforced)):

- nothing in `engine/` includes a plugin under `plugins/` (`engine cannot depend on plugin <p>`); the test plugins in `engine/testing/fixture/plugins/` are engine code;
- a plugin includes only what it declares in `DEPENDS` or `PLUGINS`, and no `detail/` of another module or plugin;
- game names never appear in `plugins/`, in file contents or paths;
- `oly_add_engine_test_game` appears only under `engine/`.

## Test plugins

Two test plugins in `engine/testing/fixture/plugins/` prove the mechanism on `native` and `web`:

| Plugin | `.oly` | Install |
|---|---|---|
| `counting` | `component Counter { value: u32 }`, `event Counted { value: u32 }` | a `Default`-band `Update` system that adds 1 to every `Counter` |
| `doubling` (`PLUGINS counting`) | `component Doubled { value: u32 }` | a `Last`-band `Update` system that writes `Doubled.value = 2 * Counter.value` |

- `native/`: `plugin_fixture_game`, an engine test game with `PLUGINS doubling`, runs the doctest suite `fixture.plugins` (install order, plugin events declared first, the `Last` band seeing the step's `Default` changes).
- `web`: the `fixture_game` WASM module turns on `PLUGINS doubling` and writes `fixture.generated.ts`; `ts.testing.wasm` checks that the plugins' components come first and that their systems run.
- The fixtures list only `doubling`; `counting` comes in through the dependency.
- The test-only schemas of `engine/testing/fixture/schemas/` that model a variant of the fixture (all but `foreign_*`) start with `Counter` and `Doubled` (and `Counted` before the events), written by hand, as in the merged schema. A change to a test plugin's `.oly` must be copied there.
