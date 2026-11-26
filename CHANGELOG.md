# Changelog

Each version of the engine has one section. While Oly is `0.x`, a minor version can break compatibility: read `### Breaking` and `### Migration` before you upgrade ([Upgrade the engine](docs/how-to/upgrade-the-engine.md)).

## Unreleased

### Breaking

None.

### Migration

- `Engine::setTimingEnabled`, `systemTimings` and `flushTimings` record the time of each system and flush of the last step ([Time per system](docs/reference/cpp/engine-and-systems.md#time-per-system)). A game keeps working without them.
- `World::insertResource`, `resource` and `tryResource` keep one object per type, owned by the `World` ([Resources](docs/reference/cpp/world-and-entities.md#resources)). A game keeps working without them.
- A system function can declare what it reads and writes through its parameters (`Query`, `Lookup`, `Res`, `Commands`, `Events`, `Tasks`, `StepInfo`, `SystemInfo`, `ecs::World&`) ([System parameters](docs/reference/cpp/engine-and-systems.md#system-parameters)). A game keeps working without them.
- Systems can be ordered by phases, sets and constraints: `addSystem<&f>(Update{}, "name")`, `addPhase<P>(FixedStep{})`, `set<S>()` ([Phases, sets and constraints](docs/reference/cpp/engine-and-systems.md#phases-sets-and-constraints)). The order is constraints first, then name; registration closes at the first `frame`, and `systemTimings` and `flushTimings` carry `PhaseId` and `phaseName`. A game keeps working without them: `addSystem(Phase, ...)` stays until a later phase removes it.
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
