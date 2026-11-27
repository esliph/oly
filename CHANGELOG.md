# Changelog

Each version of the engine has one section. While Oly is `0.x`, a minor version can break compatibility: read `### Breaking` and `### Migration` before you upgrade ([Upgrade the engine](docs/how-to/upgrade-the-engine.md)).

## Unreleased

### Breaking

- `SystemContext`, the `userData` of a system, `OrderBand`, the `Phase` enum and `addSystem(Phase, name, function, userData, band)` are removed. A system declares its access through its parameters and is registered with `addSystem<&f>(phase, name)`.
- The order of systems of one phase is the constraints `before` / `after` and then the system name, never the registration order or the plugin install order. Two systems that conflict (one writes what the other touches) with no constraint between them stop the first `frame`.
- Registration closes at the first `frame`: `addSystem`, `addPhase`, `set` and a constraint after it stop with `OLY_ASSERT`.
- A constraint against a set or a function no longer orders a system against itself: `after<&f>()` on a registration of `f` orders it after the other registrations instead of reporting a cycle. A system ordered against its own name or `SystemId` is still a cycle ([Phases, sets and constraints](docs/reference/cpp/engine-and-systems.md#phases-sets-and-constraints)).
- With asserts off, a second `World::insertResource` of a type keeps the first resource and destroys the new value, instead of destroying an object twice ([Resources](docs/reference/cpp/world-and-entities.md#resources)).

### Migration

- `Engine::setTimingEnabled`, `systemTimings` and `flushTimings` record the time of each system and flush of the last step ([Time per system](docs/reference/cpp/engine-and-systems.md#time-per-system)). A game keeps working without them.
- `World::insertResource`, `resource` and `tryResource` keep one object per type, owned by the `World` ([Resources](docs/reference/cpp/world-and-entities.md#resources)). A game keeps working without them.
- `physics2d` is a new plugin that declares the `Physics` phase, after `Update` and before `essentials2d::Propagation` ([physics2d](docs/explanation/architecture/plugins.md#physics2d)). It has no system yet. A game keeps working without it.
- `essentials2d` and `essentials3d` propagate `WorldTransform` in their own `Propagation` phase, after `Update` and its flush, instead of the `Last` band of `Update` ([When `WorldTransform` is up to date](docs/explanation/concepts/transforms.md#when-worldtransform-is-up-to-date)). An entity spawned in `Update` now has its `WorldTransform` in `UpdateAfter` of the same step.
- `olyEngineOrderSystems(engine, first, second)` and `SystemRegistration.before` / `after` in TS order two systems ([C API](docs/reference/c-api.md#olyengineordersystems), [Registration options](docs/reference/typescript/engine-and-systems.md#register-systems)). A script system is exclusive: order it against every other system of its phase.
- A game that registers systems moves to declared access, in this order ([System parameters](docs/reference/cpp/engine-and-systems.md#system-parameters), [Phases, sets and constraints](docs/reference/cpp/engine-and-systems.md#phases-sets-and-constraints)):
  1. A system takes its parameters instead of a `SystemContext`: `Query<...>` for `context.query`, `Lookup<T>` for `get` / `tryGet` / `has`, `Commands` for `spawn` / `destroy` / `add` / `remove` / `persist` / `isAlive`, `Events<E>` for `emit`, `Tasks` for `schedule`, `const StepInfo&` for the time, the state and the pause, `Res<const input::ActionMap>` for `actions`, `Res<StateRequest>` for `requestState`, `Res<PauseRequest>` for `pause` and `resume`, `const SystemInfo&` for the id and the name. A system that changes structure directly takes `ecs::World&` and is exclusive.
  2. State that a system read through `userData` becomes a resource: `world.insertResource(T{...})` at start-up, `Res<T>` in the system. A plugin does the same in `install` instead of keeping the state in its instance.
  3. `addSystem(Phase::X, "name", f, userData, band)` becomes `addSystem<&f>(X{}, "name")`, and `setWhilePaused` and `setSystemStates` become `.whilePaused()` and `.inStates({...})` on the result.
  4. Where two systems of one phase touch one component or resource and one writes it, order them: `.before("name")`, `.after<&f>()`, or a `set<S>()`. Registration closes at the first `frame`, and an unordered conflict stops it with the systems listed. Without a constraint the order is by name, not by registration.
  5. `OLY_SYSTEM(name) { ... }` becomes the function followed by `OLY_SYSTEM(name);`.
  6. A plugin that used `OrderBand::Last` declares a phase with `addPhase<P>(FixedStep{}).after<Update>()` and registers its system there.
  7. A native host game whose `Game` kept `this` in `userData` inserts that state as a resource in `onStart`.
- `oly build`, `oly test` and `oly run` accept `--release`, a `Release` build in `build/cmake-release` for measuring ([The `oly` command](docs/reference/cli.md#oly-build-oly-test-and-oly-run)). A game keeps working without it.

## 0.2.0

### Breaking

- The launcher is committed in each game as `./oly`: `oly new` writes it and `oly upgrade` replaces it ([The `oly` command](docs/reference/cli.md#create-a-game)). `install.sh` and `oly self-update` are gone, and nothing is installed on the machine or added to the `PATH`. A game made with 0.1.0 has no `./oly`, and its CI workflow, which runs the `install.sh` of the latest release, fails once 0.2.0 is out.

### Migration

- Launcher, in a game made with 0.1.0, from its root:
  1. Run `curl -fsSL https://github.com/esliph/oly/releases/download/v0.2.0/oly | sh -s -- upgrade 0.2.0`. It writes the 0.2.0 launcher as `./oly` and moves `oly.env` and `package.json` to 0.2.0 ([Upgrade the engine](docs/how-to/upgrade-the-engine.md)). A failed `curl` changes nothing and the pipe still exits 0: check that `./oly` exists.
  2. Add `.gitattributes` with the line `oly text eol=lf`, so a checkout on Windows does not break the script ([The game template](docs/reference/game-template.md#common)).
  3. In `.github/workflows/ci.yml`, delete the `Install oly` step and replace `~/.oly/bin/oly test --docker` with `sh ./oly test --docker` ([Set up the CI of a game](docs/how-to/set-up-game-ci.md)).
  4. Commit `oly`, `.gitattributes`, `oly.env`, `package.json` and the workflow, and run `./oly` from then on. On Windows, set the executable bit with `git update-index --chmod=+x oly` or run `sh ./oly` ([The `oly` command](docs/reference/cli.md#create-a-game)).
  5. Optional: delete `~/.oly/bin` and the `export PATH="$HOME/.oly/bin:$PATH"` line `install.sh` added to `~/.bashrc` or `~/.zshrc`. Keep `~/.oly/versions`: the launcher still caches the releases there.
- Native: a game may derive `oly::native_host::Game`, write its content in `onStart` and start with `runGame<MyGame>(argumentCount, arguments)` in `main`. `runGame` calls `setUpGame`, reads `--frames <n>` and turns a `std::exception` into exit code 1 ([Native host](docs/reference/native-host.md#the-game)). A game that keeps its own `main` works as before.
- Web: a game may derive `Game` from `@oly/web-host` and start with `runGame(new MyGame())`. `runGame` finds the canvas, loads the runtime, calls `onStart`, attaches the input and starts the loop ([Web host](docs/reference/typescript/web-host.md#game)). A game that calls `loadRuntime`, `attachBrowserInput` and `runInBrowser` itself works as before.

## 0.1.0

First release.

### Breaking

None.

### Migration

None.
