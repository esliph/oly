# Plugins

Code in `engine/runtime/app/include/oly/app/{plugin,game}.hpp`, `cmake/OlyPlugin.cmake`, `cmake/OlyGame.cmake` and `engine/schema/codegen/src/merge.cpp`. Spec: `docs/superpowers/specs/2026-11-16-plugins-design.md`. The rules a game relies on are in the [concept page](../concepts/plugins.md). How a game turns a plugin on: [Use a plugin](../../how-to/use-a-plugin.md). How to write one: [Write a plugin](../../how-to/write-a-plugin.md). The CMake calls: [CMake](../../reference/cmake.md).

A **plugin** is a unit of engine-level functionality, compiled together with the core, that a game turns on in its build. It brings its own data (an optional `.oly`), its systems, handlers, hooks and observers, and its own state. A game uses a plugin through data: it adds and removes the plugin's components.

Plugins depend on the core; the core never depends on a plugin. The core stays complete without them.

The mechanism serves two kinds of plugin: `essentials2d` and `essentials3d`, under `plugins/` ([essentials](#essentials)), and the engine's test plugins `counting` and `doubling` ([Test plugins](#test-plugins)).

## Parts

A plugin has up to three parts. Only the C++ part is required.

| Part | Contents | Exists today |
|---|---|---|
| C++ | data (`.oly`), systems, handlers, hooks, observers, public C++ functions | yes |
| C API | functions exported to clients | no: shape only |
| TS package `@oly/<plugin>` | wrappers of the C API part and pure-TS helpers | no: shape only |

The C API and TS parts get build support with the first plugin that needs them. Their shape is fixed now, by rule 2 of the architecture ([Architecture overview](README.md#overview)):

- A C++ game calls a plugin's public C++ functions directly, and the compiler inlines them.
- A client call crosses the WASM boundary at ~40–50 ns ([Benchmark results](../../reference/benchmarks/README.md#boundary-costs)), while a helper such as `lerp` costs ~1 ns. So **a plugin's C API part exposes only functions that work per batch or per system** (`raycastBatch`), never per entity.
- Pure helpers (math) are rewritten in the plugin's TS package, not called through the boundary.

## Install and ownership

| Name | What it does |
|---|---|
| `oly::app::Plugin` (`oly/app/plugin.hpp`) | base class: a virtual destructor and `virtual void install(Engine&) = 0` |
| `Engine::installPlugin<P>()` | constructs `P`, keeps it, runs its `install` once and returns it |
| `Engine::plugin<P>()` | returns the installed `P`, so a game can configure it before the first `frame` |
| `Engine::~Engine()` | discards the pending tasks (their discard may reach a plugin through the payload), then destroys the plugins in reverse install order |

The rules of lifetime are in the [concept page](../concepts/plugins.md#lifetime-and-ownership). The reasons behind them:

- `P` derives from `Plugin` and is default constructible (`static_assert`), so `installPlugin` can build it with no arguments.
- `install` registers the plugin's systems, handlers, hooks and observers. A plugin installed earlier is reachable from `install` through `plugin<Q>()`, which is why the install order follows the dependencies.
- **The `Engine` owns the plugin instances.** A plugin's state lives in its instance and reaches its systems through `userData`.
- Each plugin type gets a dense per-process key (`pluginKey<P>()`), the same pattern as `eventKey<T>()`. The lookup walks the installed list; it is a cold path.
- **Destructor contract.** Systems, handlers and hooks die with the `Engine`. Observers belong to the `World` ([ecs.md](ecs.md)), and the `World` outlives the `Engine` (`OlyEngine` and a native `main` declare it first). So a plugin's destructor removes the observers its `install` registered (`World::removeObserver`); otherwise the `World` would call into a destroyed plugin.

No game code installs a plugin: the generated `setUpGame` does it, in dependency order ([The build](#the-build)).

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

## The build

What the CMake calls take and do is in [CMake](../../reference/cmake.md). This section keeps the reasons.

- **Install order is a depth-first walk.** `oly_plugin_order` puts each plugin after its dependencies and keeps the order of the list among independent plugins. A game that lists only `doubling` gets `counting` too, first. The walk is deterministic, so the install order, the `ComponentId` order and the schema hash are the same on every build.
- **`DIMENSION` is not a plugin name in the list.** `oly_add_game` maps `2d` to `essentials2d` and `3d` to `essentials3d` with `oly_dimension_plugin` and puts the result first. Listing the plugin in `PLUGINS` is an error, so a game cannot name the dimension twice.
- **`setUpGame` is the game's entry point.** It declares the schema, then installs the plugins. `olyEngineCreate` calls it ([capi.md](capi.md)); on native, `runGame` calls it right after creating the `Engine` (a `main` that drives the loop itself calls it: [Drive the loop yourself](../../how-to/drive-the-loop-yourself.md)). A TS game writes nothing.
- **CMake writes `game_setup.generated.cpp`, not `oly-schema`.** The plugins' C++ types are a build fact, not schema data.
- **`game_setup.generated.cpp` is the stamp of the plugin list.** Both schema commands depend on it. Make does not rerun a custom command whose command line changed, so a new `PLUGINS` list would leave the schemas stale. `file(GENERATE)` rewrites the file only when the plugin list changes, so it works as the stamp.
- **One merged schema in two languages.** With `TYPESCRIPT`, the game's target writes the merged schema as TS. The plugin list lives only in CMake, and the C++ registry and the TS file always hash the same merged schema.
- **Test games use another function.** `oly_add_engine_test_game` shares the body of `oly_add_game`. It is only for `engine/`, and its games turn on only the test plugins of `engine/testing/fixture/plugins/`, because `engine/` never depends on `plugins/`. `DIMENSION` and the `essentials` plugins belong to `oly_add_game` alone.

## essentials

`plugins/essentials2d/` and `plugins/essentials3d/` give a game, by its `DIMENSION`, `Position`, `Rotation`, `Scale`, `Parent` and `WorldTransform`, a parent-child hierarchy with cascade destroy, and the derivation of `WorldTransform` ([the user view](../concepts/transforms.md)). Each is a plugin with its own `.oly` and a C++ part (`oly::essentials2d::Plugin`, `oly::essentials3d::Plugin`). `essentials2d` has no quaternion code; the 3D one keeps `multiply` and `rotate` in `include/oly/essentials3d/detail/quaternion.hpp`. The engine's test games get no `essentials`, and `fixture.oly` keeps its own `Position`.

At install the plugin:

- registers `imply<Position, WorldTransform>()` on the `World`; the implication cannot be undone, so it outlives the plugin's destructor;
- registers four observers (`onAdd<Parent>`, `onRemove<Parent>`, `onAdd<WorldTransform>`, `onRemove<detail::HasChildren>`), which its destructor removes;
- inserts the `detail::Hierarchy` resource (the children index, the slots and the propagation order), which the observers receive as `userData`;
- declares the `Propagation` phase in the fixed step, after `Update` and before `UpdateAfter`, and registers `propagateTransforms` in it.

| Part | What it does |
|---|---|
| `detail::Hierarchy` children index | `unordered_map` keyed by `(index << 32) \| generation` of the parent, with the list of its children, plus a slot per child so it leaves in O(1); they change only in observers |
| `detail::HasChildren` | a tag, C++ only, recorded on a parent with its first child and removed with its last; parents and non-parents live in different archetypes |
| `onAdd<Parent>` | asserts the parent is alive, not the entity and not a descendant; adds the child to the index; records the tag on the parent; recomputes the child's `WorldTransform` |
| `onRemove<Parent>` | removes the child from the index in O(1), through the slot it keeps for each child; skips a child the index does not list (a `Parent` written through `get`); removes the tag from a parent left with no children |
| `onRemove<HasChildren>` | if the index still lists children, the parent is being destroyed: records `destroy` for each. When the plugin removes the tag itself, the list is already empty |
| `onAdd<WorldTransform>` | computes the value at once from the locals and the parent's `WorldTransform`, so a child spawned by a command buffer is right in `UpdateAfter` |
| `propagateTransforms` | roots: three block passes over entities without `Parent` (`Position`, then `Rotation`, then `Scale`, each writing its part of `WorldTransform`); children: a cached vector in parent-before-child order, rebuilt by a breadth-first walk of the index when a `Parent` was added or removed |

A parent without `WorldTransform` counts as the identity: a child's chain stops at it. A child without `Position` has no `WorldTransform` and propagation skips it. The propagation runs after the flush of `Update`, so an entity spawned in `Update` already has its `WorldTransform` in `UpdateAfter` of the same step.

`checkHierarchy(world, &why)` on the plugin compares the index and the tags with the `Parent` components of the world; the tests call it after each operation. Writing `Parent::entity` through `get` is not observed and `checkHierarchy` reports it.

### Known costs

Measured with `essentials2d_bench_propagation` (release, Docker, 8 CPUs at 3.1 GHz, 2026-11-17, medians of 10 repetitions, one fixed step with only this system):

| Benchmark | N = 1000 | N = 100000 |
|---|---|---|
| `BM_PropagateRoots` (`Position`, `Rotation`, `Scale`) | 1.4 ns per entity | 3.1 ns per entity |
| `BM_PropagateChildren` (8 children per parent) | 26.6 ns per entity | 36.2 ns per entity |

There is no target. What the perf review found and left as is: each child pays six component lookups by entity and a `sincosf` of its parent's angle (2D); the three root queries are looked up each step; any `Parent` change rebuilds the whole child order the next step.

## physics2d

`plugins/physics2d/` has only C++ and no schema. Its `Plugin` declares the `Physics` phase in the fixed step, after `Update` and before `essentials2d::Propagation`, and has no system in phase 1: the order is the same whichever plugin installs first. Taking over the game's integration and separation needs a velocity column, which is new work that the roadmap's rule 7 re-measures first.

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

## See also

- [Modules and boundaries](modules-and-boundaries.md#plugin-boundaries): the rules that keep plugins publishable.
- [Known limitations](../../reference/limits.md): what plugins do not do yet.
