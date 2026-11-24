# Oly — phase 0 of the production roadmap: identity and baseline — design

Date: 2026-11-24. Written on `feature/engine-AAA` at `69c49d5`. Phase 0 of
[the production roadmap](2026-11-24-aaa-roadmap-design.md) (called *the roadmap* below). Its rules (§5 of the roadmap,
"Rules every phase keeps") apply here and are not repeated.

Marking, as in the roadmap: **[user decision]** was decided in the conversation; **[author decision]** was my choice
while writing, which the user may review; **[fact]** can be checked in the repository; **[hypothesis]** needs a
measurement.

## 1. Intent

Phase 0 sets the starting point every later phase is measured against. It does four things:

1. Rewrites the project's identity: Oly is a production engine for indie 2D games, no longer a study project.
2. Creates the Enxame, reduced, in `games/enxame/`: a small game that exists to stress the engine.
3. Gives the engine and its tools what a measurement needs: time per system inside the `Engine`, and a release build for
   a game.
4. Creates the scenarios S1–S8 and records their baseline, plus the noise of each scenario, so that the ratio gates of
   later phases have a reference and a known precision.

Why now: rule 5 of the roadmap ("instruments before gates"). Every later gate is a ratio against the phase 0 commit, so
the instruments and the baseline must exist before any phase changes the engine.

## 2. Goals, non-goals, anti-goals

Goals:

- The identity files say what the roadmap §1 says.
- `games/enxame/` builds with `oly` in local mode, opens a window and plays the reduced game (2 000 enemies).
- Every scenario that the current engine can express (S1, S2, S3, S5, S6, S8) runs headless in a release build and
  writes its result; S4 and S7 are recorded as blocked (rule 5 of the roadmap).
- A comparison script builds two commits side by side and computes the ratios; run on the phase 0 commit against
  itself, it gives each scenario's noise.

Non-goals:

- Parallelism, determinism across threads or environments, change detection: phase 3. S1 runs on one thread; S5 runs on
  `native` only.
- Save, mods, plugins declared by the game: later phases. The game-level plugins of the roadmap (`waves`, `flowfield`,
  `logistics`, `combat`) are plain systems in phase 0.
- The time per system in the C API and the TS client (rule 2 of the roadmap; see §5.8).
- A shipping build for players (asserts off, packaging): phase 7. The release build of phase 0 is for measurement.
- A real render: the window draws squares with `SDL_Renderer`, as the game template does; `render2d` is phase 6.

Anti-goals (from the roadmap §2, restated because phase 0 is where they are easiest to break):

- The engine (CMake, root `Spurfile`, presets, release, lint) names `games/`. The comparison script and every Enxame
  command live inside `games/enxame/`.
- An Enxame mechanic with no scenario or row of the roadmap §6 behind it.
- A gate in phase 0. Phase 0 records; it does not gate.

## 3. Constraints

- **[fact]** `oly build` always configures `CMAKE_BUILD_TYPE=Debug` and `OLY_BUILD_NATIVE_HOST=ON`
  (`tools/oly/Spurfile`, `_build_native`). Measuring assert-heavy code would make the baseline meaningless (risk "Game
  builds are `Debug`", roadmap §8).
- **[fact]** `OLY_ENABLE_ASSERTS` empty resolves to 0 when `CMAKE_BUILD_TYPE` is `Release`, `RelWithDebInfo` or
  `MinSizeRel` (`cmake/OlyOptions.cmake:44-49`), so a `Release` build has no asserts without setting it.
- **[fact]** The input module has no mouse wheel: `ActionMap` binds buttons, `Axis2d` and the pointer
  (`engine/runtime/input/include/oly/input/action_map.hpp`), and the native host ignores `SDL_EVENT_MOUSE_WHEEL`
  (`engine/environments/native/host/src/input_translation.cpp`).
- **[fact]** Git does not work inside the dev container when the checkout is a git worktree: the container mounts only
  the checkout at `/app` (`docker-compose.yaml`), and the worktree's `.git` is a file naming a host path. The author
  works in worktrees.
- **[fact]** For the same reason, `./spur format-check` checks nothing from a worktree and still passes: `git ls-files`
  fails, the pipe has no `pipefail`, and `xargs` without `-r` runs `clang-format --dry-run` once on empty input
  (`Spurfile:200-203`). `./spur format <files>` works, because it skips git; the `PostToolUse` hook uses it.
- **[fact]** `oly.lint.boundaries` reads `games/` to learn the game names it bans in `plugins/`
  (`tools/check_boundaries.py:26`, `:115`). Once `games/enxame/` exists, "enxame" is banned in `plugins/`. The lint
  works with or without `games/`, so it does not depend on the game; it is kept as is **[author decision]**.
- **[fact]** `runGameFromCommandLine` accepts only `--frames` and returns 64 on any other option
  (`engine/environments/native/host/src/game.cpp`). A game that takes its own options parses them before calling it.
- **[fact]** A game's `main` can call the generated `oly::app::setUpGame(engine)` directly, without `runGame`
  (`cmake/tests/consumer/native/main.cpp:15`, header `oly/app/game.hpp`).
- **[fact]** `Engine::frame(nowSeconds)` runs up to `maxStepsPerFrame` fixed steps, then `Render` once; the first frame
  only records the time (`engine/runtime/app/include/oly/app/engine.hpp:194`). Fed `nowSeconds = k / fixedHz` on
  frame `k`, every frame after the first runs exactly one fixed step.
- **[fact]** Systems run in `Engine::runPhase`, one call per entry, then `world_.flush(commands_)` once per phase
  (`engine/runtime/app/src/engine.cpp:251`). Nothing in the engine measures time today.
- **[fact]** `essentials2d` registers `propagateTransforms` in its own `install`, in `OrderBand::Last`
  (`plugins/essentials2d/native/src/plugin.cpp:42`). The game does not register it and cannot wrap it.
- **[fact]** `stats(world)` returns `archetypeCount`, `liveEntities` and per-archetype `columnBytes` and `idleBytes`
  (`docs/reference/cpp/world-and-entities.md`).
- **[fact]** `oly.lint.boundaries` and `oly.lint.links` walk the whole repository root, skipping only `build`,
  `node_modules`, `.git`, `.superpowers`, `dist`, `dist-tests`, `.claude` (boundaries) and `build*`, `node_modules`,
  `.git`, `.superpowers` (links) (`tools/check_boundaries.py:13`, `tools/check_links.py:14`). `games/enxame/` is in
  their reach; its `build/` folder is not.
- **[fact]** `./spur format-check` checks every tracked `.cpp`/`.hpp` (`Spurfile:200`), so it covers the game.
- **[fact]** A game in local mode is built in the dev container the way `consumer-native` does it: `OLY_ENGINE_DIR`
  set to the checkout, then `sh ./oly <task>` inside the game folder (`Spurfile:319`).
- A change to the `oly` commands updates `docs/reference/cli.md`, the messages in `docs/reference/errors.md` and
  `CHANGELOG.md` `## Unreleased`; a new C++ function updates its page in `docs/reference/cpp/`
  (`docs/reference/contributor/docs-style.md`, "Public surface").
- Every result is measured as `docs/reference/benchmarks/README.md` requires: ratios measured in the same session,
  never absolute numbers as criteria. Absolute numbers are recorded as context only (rule 4 of the roadmap).

## 4. Approaches

**What the reduced game is.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| A | A viewer: the window shows a scenario's world; the player moves and zooms the camera and places a turret at the cursor | little code to migrate when phase 1 changes the system signature | **chosen [user decision]** |
| B | A minimal game: belts, turrets, waves, a base with life and a lose condition | finds more API friction, but adds mechanics with no row in the roadmap §6 (anti-goal) | rejected |

**Where the time per system is measured.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| A | In the `Engine`: each system and each phase's flush, switched on and off at run time | covers plugin systems; phase 7's profiler builds on it; a public API change, and phase 1 must carry it into the new scheduler | **chosen [user decision]** |
| B | In the harness: the game wraps its own system functions before registering them | no engine change, but cannot time `propagateTransforms` (S6) nor the flush (S3) | rejected |

**Where the release build comes from.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| A | An `oly` option, `--release`, on the tasks that build | every game gets it; phase 7 adds packaging on top; changes a public command | **chosen [author decision, agreed by the user]** |
| B | A task of the Enxame that calls CMake itself | no engine change, but copies `_build_native` and the local-mode logic, and dies with the game | rejected |

## 5. The design

### 5.1 Order of the work

1. The identity rewrite, in commits of documentation only, before anything else **[author decision]**.
2. The `format-check` fix (§5.9), then time per system in the `Engine` (§5.3) and `oly --release` (§5.4), each with
   its tests and docs.
3. The Enxame (§5.5) and its headless runner (§5.6).
4. The comparison script (§5.7), then the baseline and the noise run.

### 5.2 Identity rewrite

The files named by the roadmap: `CLAUDE.md`, `README.md`, `docs/explanation/architecture/README.md`,
`docs/explanation/architecture/publishing.md`, `docs/reference/limits.md`. They say what the roadmap §1 says:

- Oly is a production engine for indie 2D games; 3D is expected later. "Study and benchmark project" goes.
- C++ on `native` is the first-class environment; `web` stays supported and may lag a phase behind.
- `limits.md` today lists render, physics, assets, threads and more as "out of the engine's scope". Each item the
  roadmap plans moves to "not yet", with the phase that brings it. Networking, a 3D render and an editor beyond the
  inspector stay out of scope (non-goals of the roadmap).

The exact wording is the builder's (roadmap §9).

### 5.3 Time per system in the `Engine`

**Contract** **[author decision]**:

- Off by default. A call switches it on or off at run time, outside `frame`.
- When on, the `Engine` records, for the last fixed step and for the last `Render`:
  - the time of each system it ran, with its `SystemId`, name and `Phase`;
  - the time of each phase's flush (`world_.flush` at the end of `runPhase`).
- Each entry says whether the system ran in that step. A system that did not run (paused, outside its states) has
  `ran = false`. A system that ran may measure 0 ns: the clock is coarse on MinGW under Wine and on WASM, so 0 never means
  "did not run".
- Event handlers and scheduled tasks are not timed in phase 0. The step total, measured by the harness, includes them.
- The clock is `std::chrono::steady_clock`.
- Plugin systems are timed like any other: the record is in `runPhase`, not at registration.

The shape below fixes the contract; the names and the header are the plan's:

```cpp
struct SystemTiming {
  SystemId system;
  std::string_view name;
  Phase phase;
  bool ran;
  uint64_t nanoseconds;
};

struct FlushTiming {
  Phase phase;
  uint64_t nanoseconds;
};

void setTimingEnabled(bool enabled);
std::span<const SystemTiming> systemTimings() const noexcept;
std::span<const FlushTiming> flushTimings() const noexcept;
```

Why the last step only, not an accumulation: the harness feeds one step per frame (§3) and reads after each frame, so
the last step is the whole record; phase 7's timeline decides how to keep a history **[author decision]**.

Cost: when off, one branch per system per step; when on, two clock reads per system and per flush. With ~20 systems,
the estimate is ~1 µs per step against the 8 ms budget **[hypothesis]**. §5.7 measures it.

Docs: the signatures in `docs/reference/cpp/` (the page that documents `Engine`), a paragraph in
`docs/explanation/architecture/app.md` if the loop description needs it, `CHANGELOG.md` `## Unreleased`.

Tests (with the `add-test-case` skill), in `engine/runtime/app/tests/`:

- off: no entry says `ran` after a frame;
- on: each system that ran has an entry with `ran = true`, in dispatch order; no test asserts a non-zero time;
- a paused or state-restricted system has `ran = false` in a step it skipped;
- each gameplay phase and `Render` has its flush entry.

One test checks that a plugin's system is recorded. `oly_add_test` cannot link a fixture plugin
(`cmake/OlyModule.cmake:45`), so that test goes with the engine test game in `engine/testing/fixture/`, the existing
pattern for plugin tests **[author decision]**.

### 5.4 `oly --release`

- `--release` is accepted by `oly build`, `oly run` and `oly test`, read by `_start` like `--docker` and `--platform`
  **[author decision]**.
- It configures `CMAKE_BUILD_TYPE=Release` in its own build directory (`build/cmake-release`, with the same `-windows`
  and `-docker` suffixes `_cmake_directory` adds), so a Debug and a Release build coexist.
- Required for `native`, including `--platform windows`. For `web`, the builder decides: support it if changing the
  build type is enough, or refuse it with exit 64 and a line in `limits.md`.
- A test in `tools/oly/tests/` checks that `--release` configures `Release` in its own directory.
- Docs: `docs/reference/cli.md`, `docs/reference/errors.md` for any new message, `CHANGELOG.md`.

### 5.5 The Enxame, reduced

**Layout** **[user decision]** (roadmap §5, "The Enxame"): `games/enxame/` is a game like one in its own repository,
created from the template (`oly new enxame --environment native`), with `oly.env`, `CMakeLists.txt` and its own build,
driven by `oly` with `OLY_ENGINE_DIR` set to the repository root. `DIMENSION 2d`. No CI builds or runs it, and
`decisions.md` gets no row for it.

**One executable, two modes** **[author decision]**:

- `enxame` with no option opens the window through `native_host` (the arguments it does not know, such as `--frames`,
  go to `runGameFromCommandLine`).
- `enxame --headless ...` runs a scenario with no window and no SDL initialization (§5.6).
- Both modes build the world from the same code: one function sets up a scenario (`S1` … `S8`, or the playable world)
  on an `Engine` and a `World`.

**The window** **[user decision]**: `enxame --scenario S1` shows that scenario's world; with no `--scenario`, the
playable world of 2 000 enemies. WASD move the camera and Q/E zoom it; a click places a turret at the cursor. The zoom
uses keys, not the mouse wheel, because the engine has no wheel input (§3) and adding it would be a third engine change
with no scenario behind it **[author decision]**. The click is the input that changes the simulation, and the one S5 records and replays. Drawing is squares with
`SDL_Renderer` in the `Render` phase.

**Mechanics** **[author decision]**, each kept to what a scenario measures:

| Mechanic | What it does | Scenarios |
|---|---|---|
| map | a tile grid generated from the seed, with obstacles, all in memory | all |
| flow field | a direction per tile toward the base, from a breadth-first search over the grid; rebuilt when a turret occupies a tile | S1, S3, S5, S8 |
| horde | an enemy moves along the flow field and is pushed apart from its neighbours, found through a spatial hash rebuilt every step | S1, S3, S5, S8 |
| turrets and projectiles | a turret shoots the nearest enemy in range; a projectile is an entity with a lifetime that destroys the enemy it hits | S5 |
| belts and assemblers | an item moves along belt tiles; an assembler consumes items and produces one at a fixed rate | S2 |
| status | `Burning`, `Slowed`, `Stunned` as tag components | S3 |
| vehicles | an entity with 4 turret children through `Parent` (from `essentials2d`), moving every step | S6 |
| wave modifiers | 8 modifier tags; each enemy of a wave gets a random subset | S8 |

- Enemies, turrets, projectiles, items and vehicles use `essentials2d`'s `Position` **[author decision]**. So
  `propagateTransforms` copies every root's `WorldTransform` each step, and S1 at 100 000 includes that pass in its
  frozen workload. That is the work every 2D game on Oly pays, and `physics2d` (phase 1) will integrate the same
  `Position`, so its takeover keeps the workload unchanged (rule 7 of the roadmap). A game-only position type would make
  phase 1 change the workload.
- Without resources (phase 5), the flow field, the spatial hash and the wave clock live in the `userData` of the system
  that owns them **[author decision]**.
- Systems, by phase, matching the roadmap's step list with today's phases: `UpdateBefore` (wave clock, flow field),
  `Update` (horde, belts, assemblers, vehicles), `UpdateAfter` (turrets, projectiles, status), `Render` (drawing, window
  mode only). The separation and the movement stay game systems until phase 1 moves them to `physics2d` (rule 7 of the
  roadmap).
- Random numbers come from one generator seeded by the scenario, owned by the game, never `rand()` **[author decision]**:
  S5 must give the same `World` hash on every run of the same build.

### 5.6 The headless runner

Command **[author decision]**:

```sh
enxame --headless --scenario <S1|S2|S3|S5|S6|S8> --size <N> --commit <sha> --out <file.json>
```

- `--commit` is the engine commit the build came from, written into the output. The comparison script passes it,
  because git does not work inside the container (§3).

- `--size` picks one of the scenario's frozen sizes (S1 has three); the other parameters come from the scenario's file
  (§5.7, "Workloads").
- `--headless` with `--scenario S4` or `S7` exits 64 with the reason "blocked: needs stable ids" or "blocked: needs
  assets".
- The loop: build the scenario, switch timing on, call `frame(k / fixedHz)` for `k = 0 … warmup + steps`, and after each
  frame read the step's wall time (two `steady_clock` reads around `frame`) and the `Engine`'s timings. Warmup steps are
  not recorded.
- The output, one JSON file per run: the engine commit, the scenario, its size and seed, the step times (p50, p99, max),
  the median time per system (over the steps where it ran) and per flush, and the scenario's own measures:

| Scenario | Its own measures |
|---|---|
| S2 | allocations per step, counted by a global `operator new` replacement in the runner |
| S3 | entities that changed archetype per step (counted from the status toggles the system records) and the flush time of its phase |
| S5 | the `World` hash at the end |
| S6 | `propagateTransforms` time divided by the entities it visits (1 000 roots and 4 000 children); S6's world holds only the vehicles and their turrets |
| S8 | `stats(world).archetypeCount`, total `columnBytes` and the process's resident memory, after each wave |

- The `World` hash (S5) **[author decision]**: an FNV-1a over the bytes of the game's components, visited in increasing
  `Entity` order, so the order of archetypes and tables does not change it. It is the game's, not the engine's.
- Resident memory (S8): read from `/proc/self/statm`; the runner runs in the Linux container.

### 5.7 Workloads, comparison and noise

**Workloads** **[author decision]**. Each scenario has one file in the game that holds its parameters: sizes, seed,
warmup, steps, map size and the rates of its mechanics. These files are frozen at the phase 0 commit (rule 7 of the
roadmap): a later phase that must change one re-measures the phase 0 commit with the new file first.

| Scenario | Workload | Measures |
|---|---|---|
| S1 Horde | N = 1 000, 10 000, 100 000 enemies on the flow field toward a fixed base; no turrets | step p50, p99; time per system |
| S2 Factory | 50 000 belt items, 500 assemblers | step time; allocations per step |
| S3 Status | 10 000 enemies of S1; each step, 10% of them (chosen by the seed) toggle one status | archetype transitions per step; flush time |
| S4 Save | blocked: no stable ids; baseline in phase 2 | — |
| S5 Replay | the playable world (2 000 enemies) and 10 000 steps of recorded clicks, from a file in the game | `World` hash; equal on two runs of the same build |
| S6 Hierarchy | 1 000 vehicles with 4 turrets each | propagation per visited entity, recorded next to the 26–36 ns the review reported (context only: the review measured children alone, so the numbers are not comparable) |
| S7 Streaming | blocked: no assets; baseline in phase 5 | — |
| S8 Waves | 50 waves of 2 000 enemies, each with a random subset of the 8 modifiers; each wave is destroyed before the next | live archetypes and memory after each wave |

Steps per run and the map size of each scenario are the plan's, written in the workload files before the baseline.

**Comparison script** **[author decision]**, a shell script in `games/enxame/`:

- Runs on the host, in Git Bash, because git does not work inside the container from a worktree (§3). Git runs on the
  host; builds and runs go through `./spur dev`.
- Takes two commits, A and B.
- Creates a git worktree for each under `games/enxame/build/` (ignored by both lints), builds each Enxame with its own
  engine (`OLY_ENGINE_DIR` = that worktree) and `oly build --release`, through `./spur dev` in that worktree.
- Runs every runnable scenario and size alternating A, B, A, B, 10 repetitions each, one process at a time, passing
  `--commit` to the runner.
- Writes, per scenario: the median of each measure over the repetitions, for A and for B; the ratio B / A of the step
  p50 and p99; and the 10 per-pair ratios B_i / A_i with their minimum, maximum and interquartile range.
- Removes both worktrees at the end, also on failure.

**Noise run (A/A)**: the script run with A = B = the phase 0 commit. A scenario's precision is the spread of its 10
per-pair ratios: minimum, maximum and interquartile range, for p50 and for p99 **[author decision]**. A later gate on a
ratio closer to 1 than that spread cannot be trusted. Recorded with the baseline.

**The phase 0 commit** **[author decision]**: the code-complete commit that was measured, called C0. A commit cannot
hold its own results, so the results go in a following commit of documentation only, which names C0. Later phases
compare against C0, and the workload files are frozen at C0.

**Timing overhead**: S1 at N = 10 000 run with timing on and off, alternating, 10 repetitions each. The ratio confirms or
rejects the §5.3 hypothesis and is recorded.

**Where the results live** **[author decision]**: `docs/reference/benchmarks/enxame/`, one JSON per scenario and run,
named with the engine commit, and a `README.md` there with the baseline table, the noise run and the overhead. The page
names the game by path in plain text, never as a link: `oly.lint.links` would break when the game is removed. The
results outlive the game; the phase 0 commit keeps the code that produced them.

### 5.8 Environments, C API and TS

- `native` only. The Enxame is not built for `web` in phase 0; S5 on `web` comes with phase 3 **[author decision]**.
- The engine's timing code is C++ in `app`, so it compiles for WASM too; the web keeps building and passing its tests
  (rule 2 of the roadmap).
- The C API and the TS client do not expose the timings in phase 0. `limits.md` says so.

### 5.9 `format-check` that cannot pass silently

**[user decision]**: a small commit of its own, in the root `Spurfile`.

- With no arguments, `format-check` fails with a clear message when `git ls-files` fails, instead of checking empty
  input. It also fails when git lists no file.
- With arguments, it checks the files given, as `format` already does. From a worktree, git runs on the host and the
  list goes to the task: `./spur format-check $(git ls-files '*.cpp' '*.hpp')`.
- `./spur format` gets the same guard for its no-argument path, so it cannot report success after formatting nothing.
- `docs/reference/contributor/commands.md` and `docs/explanation/architecture/build-and-tests.md`, which both name
  `format-check`, say how to run the check from a worktree. A `./spur` task is not a game's public surface, so `CHANGELOG.md` does not change.

## 6. Verification

Each command below is a `./spur` task that reaches the code it checks **[fact]**, except where marked.

| What | Command |
|---|---|
| engine tests, with the new `app` tests, `essentials2d`/`essentials3d` tests and the lints (`oly.lint.boundaries`, `oly.lint.links`) with `games/enxame/` present | `./spur test` (preset `debug-asan`, `Spurfile:125`) |
| formatting, the game included | `./spur format-check $(git ls-files '*.cpp' '*.hpp')` from a worktree, or `./spur format-check` from a main checkout (§5.9); run once from a worktree with no arguments, it must fail |
| `oly --release` | `./spur oly-tools-test` (`tools/oly/tests/`) |
| the web still builds and passes | `./spur games-test` (preset `wasm-games`) |
| the native host still works | `./spur native-games-test` |
| the Enxame builds and its window starts (dummy video driver, `--frames 120`, as the template's `headless` test) | `cd games/enxame && OLY_ENGINE_DIR=<checkout> sh ./oly test`, in the dev container as `consumer-native` does (`Spurfile:319`); the plan's first game task confirms the exact line and writes it here **[hypothesis until then]** |
| the baseline, the noise run and the overhead | the comparison script, on the host (§5.7) |

The engine changes of §5.3 touch `app`, not `table`, `archetype`, `query*`, `command_buffer` or `world`, so the
`checkInvariants` and reviewer-agent rule of `CLAUDE.md` does not apply. A change that reaches them does.

## 7. Gate

Phase 0 closes when **[user decision, from the roadmap]**:

- the baseline of S1, S2, S3, S5, S6, S8 is recorded in `docs/reference/benchmarks/enxame/`, with S4 and S7 recorded as
  blocked;
- the noise run and the timing overhead are recorded next to it;
- the commands of §6 pass.

No ratio is a criterion in phase 0. C0 (§5.7) is the reference every later ratio uses.

## 8. Left to the builder

- The exact names and header of the timing API, within the §5.3 contract.
- The wording of the identity rewrite.
- Steps per run, warmup and map size of each scenario, and the rates of each mechanic, written in the workload files
  before the baseline.
- The format of the recorded input file of S5 and of the workload files.
- `oly --release` on `web`: support or refuse (§5.4).
