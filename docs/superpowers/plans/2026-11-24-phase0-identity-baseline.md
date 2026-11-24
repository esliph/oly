# Phase 0 — identity and baseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite Oly's identity, give the engine time per system and a release build for games, build the reduced
Enxame with headless scenarios S1–S8, and record the phase 0 baseline with its noise.

**Architecture:** Two engine changes, both public API: `Engine::setTimingEnabled` records the time of each system and
each phase flush of the last fixed step and the last `Render` (`app` module), and `oly build|test|run --release`
configures a `Release` game build in its own directory (`tools/oly/Spurfile`). The Enxame lives in `games/enxame/` as a
game in its own repository: a schema-free library (`core/`: random numbers, tile map, flow field, spatial hash,
statistics) with unit tests, game systems in `native/`, one executable with a window mode and a `--headless` runner,
and a comparison script that builds two commits in git worktrees and alternates their runs.

**Tech Stack:** C++20, CMake, doctest (engine tests), POSIX sh (Spurfile, scripts), Python 3 (summary), SDL3
(`SDL_Renderer` squares), Docker through `./spur`.

**Spec:** `docs/superpowers/specs/2026-11-24-phase0-identity-baseline-design.md` (called *the spec*), which follows
`docs/superpowers/specs/2026-11-24-aaa-roadmap-design.md` (called *the roadmap*). Read both before any task.

## Global Constraints

- Toolchain only in Docker through `./spur` from Git Bash; no C++ compiler on the host. Git runs **on the host only**:
  this checkout is a git worktree, and git fails inside the container (spec §3).
- Engine code follows `docs/reference/contributor/code-style.md` and the project copy of the style skill in
  `.claude/skills/code-style/` (in this repository a blank line separates a block's preparation from the block). The
  Enxame follows the same rules.
- The engine (CMake, root `Spurfile`, presets, release, lint) never names `games/`. Every Enxame command and script lives
  in `games/enxame/`. The word `enxame` never appears in `plugins/`.
- The Enxame includes only public engine headers (never an `oly/<module>/detail/` header): `oly.lint.boundaries` checks it.
- Every Enxame mechanic points to a scenario (spec §5.5 table). No mechanic beyond that table.
- Random numbers in the Enxame come only from `enxame::Random` (SplitMix64). No `rand()`, no `std::*_distribution`
  (their output differs between standard libraries).
- New engine tests use the `add-test-case` skill; every `app` test ends with `CHECK_INVARIANTS(fixture.world)` after the
  last frame, as the `app` suites do.
- A public API change updates `docs/` and `CHANGELOG.md` `## Unreleased` in the same commit. A new call a game may adopt
  goes under `### Migration`, with a line saying a game keeps working without it (`docs/reference/contributor/docs-style.md`,
  "The changelog"); replace the `None.` placeholder.
- Phase 0 records, it does not gate. Results are ratios measured in the same session; absolute numbers are context only.
- From Task 2 on, the format check from this worktree is
  `./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')` (`-co` also lists new files not yet added).
- The Enxame is built and tested in the dev container, with `OLY_IN_DOCKER=1` so its build folders get the `-docker`
  suffix and never clash with a build made on the Windows host:
  `./spur dev sh -c 'cd games/enxame && OLY_ENGINE_DIR=/app OLY_IN_DOCKER=1 sh ./oly test'`
  (called *the game test command* below). Expected end: `100% tests passed`.
- Commits: English, conventional prefix, short subject, no spec or plan citation. Each message ends with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Commit on the host.

## Review Focus

1. A system added while timing is on: the `name` views of the record point into `std::string`s that move when the
   system vectors grow. Expected: names stay right. Test: Task 3, "systems added while timing is on are recorded with
   their names" (under ASan a dangling view fails).
2. `--release` combined with `--platform windows` and `--docker`, in any order. Expected: one build directory
   `build/cmake-release[-windows][-docker]`, and the container receives `--release`. Tests: Task 4, `docker_test.sh`.
3. A wrong headless command line: a `--size` that is not frozen, a missing `--out` or `--commit`, an unknown scenario,
   a commit with characters outside `[0-9A-Za-z._-]`. Expected: exit 64 and a message, no file written. Tests: Task 6,
   `enxame.usage.*`.
4. `--out` in a folder that does not exist. Expected: exit 1 with `cannot write <path>`. Test: Task 6,
   `enxame.usage.unwritable_out`.
5. The comparison script failing halfway (a commit that does not exist, a build error). Expected: both worktrees
   removed, `git worktree list` back to what it was. Test: Task 11, Step 6.

## File map

| Path | Task | Responsibility |
|---|---|---|
| `CLAUDE.md`, `README.md`, `CONTRIBUTING.md`, `.claude/agents/perf-reviewer.md`, `.claude/agents/spec-conformance-reviewer.md`, `docs/explanation/architecture/README.md`, `docs/explanation/architecture/publishing.md`, `docs/reference/limits.md` | 1 | identity |
| `Spurfile` (`format`, `format-check`), `docs/reference/contributor/commands.md`, `docs/explanation/architecture/build-and-tests.md` | 2 | format check that cannot pass silently |
| `engine/runtime/app/include/oly/app/timing.hpp` (new), `.../engine.hpp`, `engine/runtime/app/src/engine.cpp`, `engine/runtime/app/tests/timing.cpp` (new), `.../tests/CMakeLists.txt`, `.../tests/contract_death.cpp`, `engine/testing/fixture/plugins/native/plugins_test.cpp` | 3 | time per system |
| `tools/oly/Spurfile`, `tools/oly/tests/configure_test.sh`, `tools/oly/tests/docker_test.sh` | 4 | `--release` |
| `games/enxame/` (created by `oly new`), `games/enxame/core/*`, `games/enxame/tests/unit.cpp` | 5 | game skeleton and pure algorithms |
| `games/enxame/native/{simulation,scenario,workload,command_line,headless}.*`, `native/horde.cpp`, `native/systems.hpp`, `native/workloads/s1_horde.hpp`, `desktop/main.cpp`, `tests/*.cmake` | 6 | horde, S1, runner |
| `games/enxame/desktop/window_game.*` | 7 | window: camera and squares |
| `native/combat.cpp`, `native/world_hash.*`, `native/workloads/s5_replay.hpp` | 8 | turrets, projectiles, click, playable world, S5 |
| `native/factory.cpp`, `native/allocation_counter.*`, `native/workloads/s2_factory.hpp` | 9 | S2 |
| `native/status.cpp`, `native/vehicles.cpp`, `native/waves.cpp`, `native/workloads/s{3,6,8}_*.hpp` | 10 | S3, S6, S8 |
| `games/enxame/compare/{compare.sh,inside.sh,summarize.py,test_summarize.py}` | 11 | comparison and noise |
| `docs/reference/benchmarks/enxame/` (new), `docs/reference/benchmarks/README.md`, `docs/README.md` | 12 | baseline, noise, overhead |

---

### Task 1: Identity rewrite (documentation only)

**Files:**
- Modify: `CLAUDE.md:3`, `README.md:3`, `CONTRIBUTING.md:3`, `docs/explanation/architecture/README.md:7`,
  `docs/explanation/architecture/publishing.md:3`, `docs/reference/limits.md:1-3`
- Modify: `.claude/agents/perf-reviewer.md:7`, `.claude/agents/spec-conformance-reviewer.md:7`

The spec names five files; a grep for "study and benchmark" found the same claim in `CONTRIBUTING.md` and in the two
reviewer agents, so they change too. The wording is the builder's; each file must state these facts (roadmap §1):

- Oly is a production engine for indie 2D games; 3D is expected later. "Study project", "study and benchmark project"
  and "published for the author's own games" go. Keep "every performance target has a benchmark" where the file has it:
  it is still true.
- C++ on `native` is the first-class environment; `web` stays supported and may lag one phase behind.
- `CLAUDE.md` also names the roadmap: `docs/superpowers/specs/2026-11-24-aaa-roadmap-design.md`. It keeps "on top of a
  single-threaded archetype ECS": still true until phase 3.
- The two agents keep "single-threaded" and drop "study and benchmark project".
- `publishing.md`, first paragraph: one sentence that releases serve indie 2D games made outside this repository.
- `limits.md`: the title becomes "Known limitations, not yet and out of scope". The first bullet splits in three:
  - **Not yet**, with the phase of the roadmap (link the roadmap file): physics (`physics2d`, phase 1), save and load
    with stable ids and the in-game inspector (phase 2), camera (`camera2d`, phase 2), threads (phase 3), assets
    (phase 5), render (`render2d`, phase 6).
  - **Not planned yet**: audio, UI, gamepad — no phase of the roadmap plans them.
  - **Out of scope**: networking, a 3D render, an editor beyond the in-game inspector.
  - "Render and collision belong to the game" stays, ending with "until their phase".

- [ ] **Step 1: Confirm the targets still read as above**

Run: `grep -rn -i "study and benchmark\|study project\|author's own games\|Out of the engine's scope" --include=*.md . | grep -v docs/superpowers/ | grep -v /build`
Expected: seven lines — the files under **Files** except `publishing.md`, which carries no such claim and only gains
the sentence below.

- [ ] **Step 2: Rewrite each file as stated above**

Touch only those sentences. `README.md` holds the template blocks that `oly.lint.readme_example` checks: leave every
code block as it is.

- [ ] **Step 3: Check the claim is gone and the links still resolve**

Run the grep of Step 1 again. Expected: no line.
Run: `./spur test -R '^oly\.lint\.(links|readme_example)$'`
Expected: `100% tests passed, 0 tests failed out of 2`.

- [ ] **Step 4: Commit**

```bash
git add CLAUDE.md README.md CONTRIBUTING.md .claude/agents/perf-reviewer.md .claude/agents/spec-conformance-reviewer.md docs/explanation/architecture/README.md docs/explanation/architecture/publishing.md docs/reference/limits.md
git commit  # subject: "docs: describe Oly as a production engine for indie 2D games"
```

---

### Task 2: `format` and `format-check` that cannot pass silently

**Files:**
- Modify: `Spurfile:186-203` (the `format` and `format-check` tasks)
- Modify: `docs/reference/contributor/commands.md:30`, `:95`; `docs/explanation/architecture/build-and-tests.md:61`

**Interfaces:**
- Produces: `./spur format-check <files...>` checks the given files; with no file it fails when git cannot list the
  tracked files or lists none. All later tasks use the form in Global Constraints.

- [ ] **Step 1: See the bug**

Run from this worktree: `./spur format-check; echo "exit=$?"`
Expected: `exit=0` with no file checked (git fails inside the container and `xargs` runs `clang-format` on empty input).

- [ ] **Step 2: Implement the guard in the root `Spurfile`**

Add a helper next to the other `_` helpers, and use it in both tasks. With arguments, `format-check` runs
`exec clang-format --dry-run --Werror "$@"`, as `format` already does with `-i`.

```sh
# _list_tracked_sources <file>: writes the tracked .cpp and .hpp files to <file>, NUL-separated. Fails when git cannot
# list them (a git worktree: the container mounts only the checkout) or lists none, so a check never passes on nothing.
_list_tracked_sources() {
  # The mounted checkout belongs to another user than the container's, and git refuses to read it without this.
  git -c safe.directory='*' ls-files -z '*.cpp' '*.hpp' > "$1" || _fail 1 "git cannot list the tracked files here: pass them, as in ./spur $SPUR_TASK \$(git ls-files '*.cpp' '*.hpp')"
  [ -s "$1" ] || _fail 1 "git lists no tracked .cpp or .hpp file"
}
```

The no-argument path of each task becomes: a `mktemp` file, `_list_tracked_sources "$list"`, then
`xargs -0 clang-format <flags> < "$list"`, and the file removed (`trap 'rm -f "$list"' EXIT`). Move the existing
`safe.directory` comment into the helper, as above; it no longer sits in `format`.

- [ ] **Step 3: See it fail without arguments and pass with them**

Run: `./spur format-check; echo "exit=$?"`
Expected: `spur format-check: git cannot list the tracked files here: ...` and `exit=1`.
Run: `./spur format; echo "exit=$?"`
Expected: the same message with `format`, `exit=1`.
Run: `./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp'); echo "exit=$?"`
Expected: `exit=0`.

- [ ] **Step 4: Document the worktree form**

`commands.md` line 30 (table) and the example at line 95: say that with no file the task lists the tracked files with
git, and that from a git worktree you pass them: `./spur format-check $(git ls-files '*.cpp' '*.hpp')`.
`build-and-tests.md:61`: one sentence with the same form. `CHANGELOG.md` does not change: a `./spur` task is not a
game's public surface.

- [ ] **Step 5: Commit**

```bash
git add Spurfile docs/reference/contributor/commands.md docs/explanation/architecture/build-and-tests.md
git commit  # subject: "build: fail format and format-check when git lists no file"
```

---

### Task 3: Time per system in the `Engine`

**Files:**
- Create: `engine/runtime/app/include/oly/app/timing.hpp`
- Modify: `engine/runtime/app/include/oly/app/engine.hpp`, `engine/runtime/app/src/engine.cpp`
- Create: `engine/runtime/app/tests/timing.cpp`; Modify: `engine/runtime/app/tests/CMakeLists.txt`
- Modify: `engine/runtime/app/tests/contract_death.cpp`
- Modify: `engine/testing/fixture/plugins/native/plugins_test.cpp`
- Docs: `docs/reference/cpp/engine-and-systems.md` (new section `## Time per system` at the end), `docs/reference/errors.md`
  (row), `docs/reference/limits.md` (line), `CHANGELOG.md`

**Interfaces:**
- Produces (namespace `oly::app`, header `oly/app/timing.hpp`, included by `oly/app/engine.hpp`):

```cpp
// ran is false when the pause or the system's states skipped it. A system that ran may measure 0 ns: the clock is
// coarse on MinGW under Wine and on WASM.
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
```

- Produces (public members of `Engine`):

```cpp
void setTimingEnabled(bool enabled);
bool timingEnabled() const noexcept;
std::span<const SystemTiming> systemTimings() const noexcept;
std::span<const FlushTiming> flushTimings() const noexcept;
```

Contract (goes into the header comments and the reference page):
- Off by default. `setTimingEnabled` during `frame` stops with `OLY_ASSERT` (`setTimingEnabled: called during frame`).
- Off: both spans are empty. On: `systemTimings()` has one entry per registered system, in dispatch order (phase, then
  band, then registration), each with `ran = false` and 0 ns until its phase runs; `flushTimings()` has four entries, in
  `Phase` order. Each `runPhase` overwrites the entries of its phase. So after a frame, the gameplay entries hold the last
  fixed step and the `Render` entries the last `Render`; a frame with no fixed step keeps the last step's entries.
- The spans, and the names in them, stay valid until the next `addSystem` or `setTimingEnabled` call.
- Event handlers and scheduled tasks are not timed. The clock is `std::chrono::steady_clock`.

- [ ] **Step 1: Write the failing suite `engine/runtime/app/tests/timing.cpp`**

```cpp
#include <doctest/doctest.h>

#include <span>
#include <string>
#include <vector>

#include "components.hpp"
#include "log.hpp"
#include "oly/app/engine.hpp"

using oly::app::Engine;
using oly::app::FlushTiming;
using oly::app::OrderBand;
using oly::app::Phase;
using oly::app::SystemTiming;

namespace {
  struct Fixture {
    oly::ecs::World world;
    Engine engine{world};
    Log log;
    double now = 0.0;

    void step() {
      now += 1.0 / 60;
      engine.frame(now);
    }
  };

  std::vector<std::string> namesOf(std::span<const SystemTiming> timings) {
    std::vector<std::string> names;

    for (const SystemTiming& timing : timings) {
      names.emplace_back(timing.name);
    }

    return names;
  }

  // Short names fit in std::string's inline buffer, so they move with the entries when the system vectors grow.
  std::vector<std::string> addNumberedSystems(Fixture& fixture, int count) {
    std::vector<std::string> names;

    for (int index = 0; index < count; ++index) {
      names.push_back("s" + std::to_string(index));
      fixture.engine.addSystem(Phase::Update, names.back(), recordName, &fixture.log);
    }

    return names;
  }
}

TEST_CASE("timing off: a frame records no system and no flush") {
  Fixture fixture;
  fixture.engine.addSystem(Phase::Update, "update", recordName, &fixture.log);
  fixture.engine.frame(0.0);

  fixture.step();

  CHECK(fixture.engine.systemTimings().empty());
  CHECK(fixture.engine.flushTimings().empty());
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("timing on: one entry per system, in dispatch order, with its id and phase") {
  Fixture fixture;
  const auto last = fixture.engine.addSystem(Phase::Update, "last", recordName, &fixture.log, OrderBand::Last);
  const auto update = fixture.engine.addSystem(Phase::Update, "update", recordName, &fixture.log);
  const auto before = fixture.engine.addSystem(Phase::UpdateBefore, "before", recordName, &fixture.log);
  const auto render = fixture.engine.addSystem(Phase::Render, "render", recordName, &fixture.log);
  fixture.engine.setTimingEnabled(true);
  fixture.engine.frame(0.0);

  fixture.step();

  const std::span<const SystemTiming> timings = fixture.engine.systemTimings();
  REQUIRE(timings.size() == 4);
  CHECK(namesOf(timings) == std::vector<std::string>{"before", "update", "last", "render"});
  CHECK(timings[0].system == before);
  CHECK(timings[0].phase == Phase::UpdateBefore);
  CHECK(timings[1].system == update);
  CHECK(timings[2].system == last);
  CHECK(timings[2].phase == Phase::Update);
  CHECK(timings[3].system == render);
  CHECK(timings[3].phase == Phase::Render);
  CHECK(timings[0].ran);
  CHECK(timings[1].ran);
  CHECK(timings[2].ran);
  CHECK(timings[3].ran);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a system skipped by the pause or by its states has ran false and 0 ns") {
  Fixture fixture;
  const auto playing = fixture.engine.declareState("playing");
  fixture.engine.addSystem(Phase::Update, "gameplay", recordName, &fixture.log);
  const auto paused = fixture.engine.addSystem(Phase::Update, "paused", recordName, &fixture.log);
  fixture.engine.setWhilePaused(paused, true);
  const auto restricted = fixture.engine.addSystem(Phase::Update, "restricted", recordName, &fixture.log);
  fixture.engine.setWhilePaused(restricted, true);
  fixture.engine.setSystemStates(restricted, {playing});
  fixture.engine.setTimingEnabled(true);
  fixture.engine.frame(0.0);
  fixture.engine.pause();

  fixture.step();

  const std::span<const SystemTiming> timings = fixture.engine.systemTimings();
  REQUIRE(timings.size() == 3);
  CHECK_FALSE(timings[0].ran);
  CHECK(timings[0].nanoseconds == 0);
  CHECK(timings[1].ran);
  CHECK_FALSE(timings[2].ran);
  CHECK(timings[2].nanoseconds == 0);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("each gameplay phase and Render has its flush entry, in phase order") {
  Fixture fixture;
  fixture.engine.setTimingEnabled(true);
  fixture.engine.frame(0.0);

  fixture.step();

  const std::span<const FlushTiming> flushes = fixture.engine.flushTimings();
  REQUIRE(flushes.size() == 4);
  CHECK(flushes[0].phase == Phase::UpdateBefore);
  CHECK(flushes[1].phase == Phase::Update);
  CHECK(flushes[2].phase == Phase::UpdateAfter);
  CHECK(flushes[3].phase == Phase::Render);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a frame with no fixed step keeps the record of the last step") {
  Fixture fixture;
  fixture.engine.addSystem(Phase::Update, "update", recordName, &fixture.log);
  fixture.engine.setTimingEnabled(true);
  fixture.engine.frame(0.0);
  fixture.step();

  fixture.engine.frame(fixture.now);

  CHECK(fixture.log.count("update") == 1);
  CHECK(fixture.engine.systemTimings()[0].ran);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("systems added while timing is on are recorded with their names") {
  Fixture fixture;
  fixture.engine.setTimingEnabled(true);
  const std::vector<std::string> expected = addNumberedSystems(fixture, 40);
  fixture.engine.frame(0.0);

  fixture.step();

  CHECK(namesOf(fixture.engine.systemTimings()) == expected);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("turning timing off empties the record") {
  Fixture fixture;
  fixture.engine.addSystem(Phase::Update, "update", recordName, &fixture.log);
  fixture.engine.setTimingEnabled(true);
  fixture.engine.frame(0.0);
  fixture.step();

  fixture.engine.setTimingEnabled(false);

  CHECK_FALSE(fixture.engine.timingEnabled());
  CHECK(fixture.engine.systemTimings().empty());
  CHECK(fixture.engine.flushTimings().empty());
  CHECK_INVARIANTS(fixture.world);
}
```

Register it next to the other suites in `engine/runtime/app/tests/CMakeLists.txt`:
`oly_add_test(MODULE app NAME timing SOURCES timing.cpp SUPPORT engine/runtime/ecs/tests/support)`.

- [ ] **Step 2: Add the death case**

In `contract_death.cpp`, inside the anonymous namespace, after `addSystemDuringFrame`:

```cpp
void enableTimingFromSystem(SystemContext& context) {
  Engine& engine = *static_cast<Engine*>(context.userData());
  engine.setTimingEnabled(true);
}

void setTimingEnabledDuringFrame() {
  oly::ecs::World world;
  Engine engine(world);
  engine.addSystem(Phase::Update, "enabler", enableTimingFromSystem, &engine);

  engine.frame(0.0);
  engine.frame(1.0 / 60);
}
```

Table entry: `{"set_timing_enabled_during_frame", setTimingEnabledDuringFrame, {"setTimingEnabled", "during frame"}},`.
Append `set_timing_enabled_during_frame` to the `CASES` list of `oly_add_contract_death`.

- [ ] **Step 3: Add the plugin test in `engine/testing/fixture/plugins/native/plugins_test.cpp`**

`oly_add_test` cannot link a fixture plugin (`cmake/OlyModule.cmake:45`), so this test lives with the engine test game.
Add `#include <span>`.

```cpp
TEST_CASE("the Engine times a plugin's systems like the game's") {
  oly::ecs::World world;
  Engine engine(world);
  oly::app::setUpGame(engine);
  engine.setTimingEnabled(true);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  const std::span<const oly::app::SystemTiming> timings = engine.systemTimings();
  REQUIRE(timings.size() == 2);
  CHECK(timings[0].name == "count");
  CHECK(timings[0].ran);
  CHECK(timings[1].name == "writeDoubles");
  CHECK(timings[1].ran);
}
```

- [ ] **Step 4: Run to see them fail**

Run: `./spur test -R '^(app\.timing|app\.contract_death\.set_timing_enabled_during_frame|fixture\.plugins)$'`
Expected: build error, `setTimingEnabled` and `systemTimings` are not members of `Engine`.

- [ ] **Step 5: Implement**

`timing.hpp` as in **Interfaces**. In `engine.hpp`: include `<span>` and `"oly/app/timing.hpp"`; the four public members
with the contract as comments; private data, grouped after `plugins_`:

```cpp
// Slot i is the i-th system in dispatch order; each runPhase overwrites the slots of its phase in place.
std::vector<SystemTiming> systemTimings_;
std::array<uint32_t, PhaseCount> firstTimingSlot_{}; // [Phase]
std::vector<FlushTiming> flushTimings_; // [Phase]
bool timingEnabled_ = false;
```

Private helpers: `void rebuildTimingSlots();` and `template <bool Timed> void runSystems(Phase phase);` (defined and
instantiated only in `engine.cpp`).

In `engine.cpp`:
- `setTimingEnabled`: assert `!inFrame_`; on → `rebuildTimingSlots()`; off → clear both vectors.
- `rebuildTimingSlots`: walks `phases_` (phase, band, entry), writes `{entry.id, entry.name, phase, false, 0}` per entry
  and `firstTimingSlot_[phase]`, and resets `flushTimings_` to the four phases with 0 ns.
- `addSystem`: after pushing the entry, `if (timingEnabled_) rebuildTimingSlots();` (the push may have moved every name).
- `runPhase(phase)` becomes one branch per phase: `timingEnabled_ ? runSystems<true>(phase) : runSystems<false>(phase)`.
  `runSystems<false>` is today's body. `runSystems<true>` walks the same entries with a slot index starting at
  `firstTimingSlot_[phase]`: a skipped entry writes `ran = false, nanoseconds = 0`; a run entry reads `steady_clock`
  before and after the call. The flush is timed the same way into `flushTimings_[phase]`.

Why a template and not a branch per system: off must cost nothing measurable in S1 at 100 000 enemies; the template
leaves one branch per phase.

- [ ] **Step 6: Run to see them pass**

Run: `./spur test -R '^(app\.|fixture\.plugins$)'`
Expected: all pass; `app.timing` reports `7 passed`; `app.contract_death.set_timing_enabled_during_frame` passes.
Run: `./spur dev ./build/debug-asan/tests/app_contract_death set_timing_enabled_during_frame; echo "exit=$?"`
Expected: stderr shows `OLY_ASSERT failed: ... setTimingEnabled: called during frame ...` and `exit=0`.

- [ ] **Step 7: Document**

- `docs/reference/cpp/engine-and-systems.md`, new `## Time per system` at the end, entries `### Engine::setTimingEnabled`,
  `### Engine::systemTimings`, `### SystemTiming and FlushTiming`: the signatures, every bullet of the contract above, and
  an example that turns timing on, runs a frame and prints each entry that ran.
- `docs/reference/errors.md`, "At run time" table: `setTimingEnabled: called during frame` | `app` | none | the call ran
  inside a system, handler or hook | switch timing between frames.
- `docs/reference/limits.md`: "Time per system is C++ only: the C API and the TypeScript client do not expose it."
- `CHANGELOG.md` `### Migration`: `Engine::setTimingEnabled`, `systemTimings` and `flushTimings` record the time of each
  system and flush of the last step (link the new section); a game keeps working without them.
- `docs/explanation/architecture/app.md` does not change: timing adds no stage to the loop.

- [ ] **Step 8: Full suite, format, web**

Run: `./spur test` — Expected: `100% tests passed`.
Run: `./spur games-test` — Expected: `100% tests passed` (the timing code compiles for WASM).
Run: `./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')` — Expected: exit 0.

- [ ] **Step 9: Commit**

```bash
git add engine/runtime/app engine/testing/fixture/plugins/native/plugins_test.cpp docs/reference/cpp/engine-and-systems.md docs/reference/errors.md docs/reference/limits.md CHANGELOG.md
git commit  # subject: "feat(app): time each system and flush of the last step"
```

---

### Task 4: `oly --release`

**Files:**
- Modify: `tools/oly/Spurfile` (`_target` globals, `_cmake_directory`, `_build_native`, `_dispatch_docker`, `_start`)
- Test: `tools/oly/tests/configure_test.sh`, `tools/oly/tests/docker_test.sh`
- Docs: `docs/reference/cli.md:65-75`, `docs/reference/errors.md` (paragraph at `:42` and `oly` table),
  `docs/explanation/architecture/decisions.md:47`, `docs/explanation/architecture/publishing.md:126-128`,
  `docs/reference/limits.md`, `CHANGELOG.md`

**Interfaces:**
- Produces: `oly build|test|run --release` for desktop games: `CMAKE_BUILD_TYPE=Release` in
  `build/cmake-release[-windows][-docker]`, so a Debug and a Release build coexist. The Enxame release executable in the
  dev container is `games/enxame/build/cmake-release-docker/games/enxame`.
- On a web game, `--release` exits 64 with `--release is for native games` **[plan decision: refuse]**: phase 0 measures
  on `native` only, and no test could show a Release web game build works.

- [ ] **Step 1: Write the failing tests**

`configure_test.sh`, after the `pinned: the build type is Debug` check (still pinned, `TEST_ENGINE_DIR=`):

```sh
: > "$CMAKE_LOG"
run_task "$work/meu-jogo" build --release
check "release: build succeeds" exit_is 0
check "release: the build type is Release" sh -c "grep -q -- '-DCMAKE_BUILD_TYPE=Release' \"\$CMAKE_LOG\""
check "release: its own build directory" sh -c "grep -q -- '-B build/cmake-release ' \"\$CMAKE_LOG\""
check "release: the native host is on" sh -c "grep -q -- '-DOLY_BUILD_NATIVE_HOST=ON' \"\$CMAKE_LOG\""
```

`docker_test.sh`, before `finish`:

```sh
: > "$DOCKER_LOG"
run_task "$work/native-game" build --docker --release
check "release: passes --release on, after --host" docker_line_has " build --host --release"

: > "$DOCKER_LOG"
run_task "$work/native-game" run --docker --release
check "native run --release: exit 64" exit_is 64
check "native run --release: names the release directory" stderr_has "run build/cmake-release-docker/games/native_game on the host"

: > "$CMAKE_LOG"
OLY_IN_DOCKER=1
export OLY_IN_DOCKER
run_task "$work/native-game" build --host --release --platform windows
unset OLY_IN_DOCKER
check "release windows in the container: its own build directory" sh -c "grep -q -- '-B build/cmake-release-windows-docker ' '$CMAKE_LOG'"

: > "$CMAKE_LOG"
run_task "$work/native-game" build --host --platform windows --release
check "release after --platform: the same directory" sh -c "grep -q -- '-B build/cmake-release-windows ' '$CMAKE_LOG'"

run_task "$work/web-game" build --release
check "web --release: exit 64" exit_is 64
check "web --release: message" stderr_has "--release is for native games"
```

- [ ] **Step 2: Run to see them fail**

Run: `./spur oly-tools-test`
Expected: `FAIL release: ...` lines (today `--release` reaches `_build` as an unknown argument).

- [ ] **Step 3: Implement in `tools/oly/Spurfile`**

- Next to `_target=`: `_release=` with the comment "Set by `_start`: 1 for `--release`, or empty."
- `_start`: a `--release)` case that sets `_release=1` and consumes one argument. After the `--platform windows` check,
  and before the runner dispatch: `--release` on a non-native game → `_fail 64 "--release is for native games"`.
- `_cmake_directory`: `-release` right after `build/cmake`, before `-windows` and `-docker`.
- `_build_native`: `-DCMAKE_BUILD_TYPE=$(_build_type)`, where `_build_type` prints `Release` or `Debug`.
- `_dispatch_docker`: after the `--platform` forward, `if [ -n "$_release" ]; then set -- --release "$@"; fi`.
- The `oly run` refusal message: `run $(_cmake_directory)-docker/games/...` instead of the literal `build/cmake-docker`.

- [ ] **Step 4: Run to see them pass**

Run: `./spur oly-tools-test`
Expected: every line `ok`, exit 0.

- [ ] **Step 5: Document**

- `cli.md`: an option row `--release` — "build with `CMAKE_BUILD_TYPE=Release` in `build/cmake-release`, for
  measuring: `OLY_ASSERT` is off. Desktop games only." Line 67: "They build in `Debug` unless `--release`...".
- `errors.md:42`: name `oly build --release` among the release builds where the contract is not checked; `oly` table row:
  `--release is for native games` | `oly build`, `oly test`, `oly run` | `--release` on a web game | build a web game
  without it.
- `decisions.md:47` and `publishing.md` § "Game builds are `Debug`": Debug stays the default; `--release` exists for
  measuring, not for shipping (shipping builds are phase 7 of the roadmap).
- `limits.md`: "`--release` builds desktop games only."
- `CHANGELOG.md` `### Migration`: `oly build`, `oly test` and `oly run` accept `--release` (link `cli.md`); a game keeps
  working without it.

- [ ] **Step 6: Commit**

```bash
git add tools/oly docs/reference/cli.md docs/reference/errors.md docs/explanation/architecture/decisions.md docs/explanation/architecture/publishing.md docs/reference/limits.md CHANGELOG.md
git commit  # subject: "feat(oly): add --release to build, test and run"
```

---

### Task 5: The Enxame skeleton and its pure algorithms

**Files:**
- Create (by `oly new`): `games/enxame/{oly,oly.env,CMakeLists.txt,enxame.oly,.gitignore,.gitattributes,cmake/game.cmake,native/movement.cpp,native/systems.hpp,desktop/main.cpp}`
- Delete: `games/enxame/.github/` (no CI runs the game)
- Create: `games/enxame/core/{random.hpp,tile_map.hpp,tile_map.cpp,flow_field.hpp,flow_field.cpp,spatial_hash.hpp,spatial_hash.cpp,statistics.hpp,statistics.cpp,hashing.hpp}`
- Create: `games/enxame/tests/unit.cpp`
- Modify: `games/enxame/cmake/game.cmake`, `CLAUDE.md` (one line)

**Interfaces** (namespace `enxame`, all in `core/`, linked as the static library `enxame_core`, which depends only on
`oly::ecs`):

```cpp
// random.hpp — SplitMix64: the same sequence on every platform and standard library.
class Random {
 public:
  explicit Random(uint64_t seed) noexcept;
  uint64_t nextU64() noexcept;
  float nextFloat() noexcept; // [0, 1), from the top 24 bits
  uint32_t nextBelow(uint32_t bound) noexcept; // [0, bound), bound > 0: (nextU64() >> 32) * bound >> 32
  bool chance(float probability) noexcept; // nextFloat() < probability
};

// tile_map.hpp — y grows downwards, index = y * width + x; tile (x, y) covers [x, x + 1) * tileSize.
struct TilePosition {
  int32_t x;
  int32_t y;
};

class TileMap {
 public:
  TileMap(uint32_t width, uint32_t height, float tileSize, TilePosition base);
  // The base is the centre tile; it and its 8 neighbours are never blocked. Each other tile is blocked with
  // probability obstacleRate, drawn in index order.
  static TileMap generate(uint32_t width, uint32_t height, float tileSize, float obstacleRate, Random& random);
  uint32_t width() const noexcept;
  uint32_t height() const noexcept;
  float tileSize() const noexcept;
  TilePosition base() const noexcept;
  bool inside(TilePosition tile) const noexcept;
  bool blocked(TilePosition tile) const noexcept; // false outside the map
  // false, and no change, when the tile is outside, already blocked or the base. Otherwise blocks it and bumps version().
  bool block(TilePosition tile);
  uint32_t version() const noexcept;
  TilePosition tileAt(float x, float y) const noexcept; // floor of x / tileSize, y / tileSize; may be outside
  float centerX(TilePosition tile) const noexcept;
  float centerY(TilePosition tile) const noexcept;
};

// flow_field.hpp
struct Direction {
  int8_t x;
  int8_t y;
};

class FlowField {
 public:
  static constexpr uint32_t Unreachable = UINT32_MAX;
  // Breadth-first search from the base over 8 neighbours; a diagonal move needs both orthogonal tiles free (no corner
  // cutting). Records map.version() as builtVersion().
  void build(const TileMap& map);
  uint32_t distance(TilePosition tile) const noexcept; // Unreachable for blocked, enclosed or outside tiles
  // The neighbour with the smallest distance, first in the order N, S, W, E, NW, NE, SW, SE on a tie; {0, 0} at the
  // base and where distance is Unreachable.
  Direction direction(TilePosition tile) const noexcept;
  uint32_t builtVersion() const noexcept;
};

// spatial_hash.hpp — a uniform grid rebuilt by counting sort; SoA entries sorted by cell.
class SpatialHash {
 public:
  SpatialHash(float originX, float originY, float width, float height, float cellSize);
  // A point outside the area goes to the nearest edge cell.
  void rebuild(std::span<const oly::ecs::Entity> entities, std::span<const float> xs, std::span<const float> ys);
  uint32_t size() const noexcept;
  // Calls visit(entity, x, y) for every entry at distance <= radius, cell by cell in row order, then in rebuild order.
  template <class Visit>
  void forEachWithin(float x, float y, float radius, Visit&& visit) const;
};

// statistics.hpp — nearest rank: sorts values, returns values[ceil(percent / 100 * n) - 1]; values not empty, percent in (0, 100].
uint64_t percentile(std::vector<uint64_t>& values, double percent);

// hashing.hpp — 64-bit FNV-1a.
inline constexpr uint64_t FnvOffsetBasis = 14695981039346656037ull;
uint64_t fnv1a(uint64_t hash, const void* bytes, size_t size) noexcept;
```

Data layout: `TileMap` keeps `std::vector<uint8_t> blocked_`; `FlowField` keeps `distance_`, `directionX_`,
`directionY_` as three vectors (SoA: the horde reads only the directions); `SpatialHash` keeps `cellStart_`
(cells + 1), `entities_`, `xs_`, `ys_` — the separation loop reads positions of neighbours, never the entities.

- [ ] **Step 1: Create the game from the template**

Run: `./spur dev sh -c 'mkdir -p games && cd games && OLY_ENGINE_DIR=/app sh /app/tools/oly/oly new enxame --environment native --version 0.2.0'`
Expected: `created enxame (native, oly 0.2.0)`.
Run on the host: `rm -rf games/enxame/.github`.

- [ ] **Step 2: Run the game test command once, unchanged**

Expected: `enxame.headless` passes (the template's game builds in the container with `OLY_IN_DOCKER=1`).

- [ ] **Step 3: Write the failing unit tests `games/enxame/tests/unit.cpp`**

A minimal harness: `check(bool condition, const char* description)` prints `FAIL <description>` to stderr and counts;
`main` calls every test function and returns 1 when any check failed. The tests, one function each, three blocks:

```cpp
void randomFollowsSplitMix64() {
  enxame::Random random(0);

  const uint64_t first = random.nextU64();
  const uint64_t second = random.nextU64();

  check(first == 0xe220a8397b1dcdafull, "Random(0): first value is SplitMix64's");
  check(second == 0x6e789e6aa1b965f4ull, "Random(0): second value is SplitMix64's");
}

void blockRefusesOutsideBaseAndBlockedTiles() {
  enxame::TileMap map(5, 5, 8.0f, {2, 2});

  const bool first = map.block({0, 0});
  const bool again = map.block({0, 0});
  const bool base = map.block({2, 2});
  const bool outside = map.block({5, 0});

  check(first && map.blocked({0, 0}), "block: a free tile becomes blocked");
  check(!again && !base && !outside, "block: blocked, base and outside tiles are refused");
  check(map.version() == 1, "block: only the accepted call bumps the version");
}

void flowFieldOnAnOpenMapGoesDiagonally() {
  enxame::TileMap map(5, 5, 8.0f, {2, 2});
  enxame::FlowField field;

  field.build(map);

  check(field.distance({0, 0}) == 2, "open map: corner is 2 steps from the base");
  check(field.direction({0, 0}).x == 1 && field.direction({0, 0}).y == 1, "open map: corner points diagonally");
  check(field.direction({2, 2}).x == 0 && field.direction({2, 2}).y == 0, "open map: base has no direction");
}

void flowFieldDoesNotCutCorners() {
  enxame::TileMap map(5, 5, 8.0f, {2, 2});
  map.block({1, 2});
  map.block({2, 1});
  enxame::FlowField field;

  field.build(map);

  check(field.distance({1, 1}) == 6, "walls: (1, 1) goes around, 6 steps");
  check(field.direction({1, 1}).x == 0 && field.direction({1, 1}).y == -1, "walls: (1, 1) points north first");
  check(field.distance({2, 1}) == enxame::FlowField::Unreachable, "walls: a blocked tile is unreachable");
}
```

Also, in the same style: `flowFieldMarksEnclosedTilesUnreachable` (block `(0,1)`, `(1,0)`, `(1,1)`; `(0,0)` is
`Unreachable` with direction `{0, 0}`); `spatialHashFindsPointsWithinRadius` (area 32 × 32, cell 8, points `(1,1)`,
`(5,5)`, `(20,20)`; a query at `(0,0)` radius 8 visits exactly the first two); `spatialHashClampsOutsidePoints` (a point
at `(-10, 40)` is found by a query at `(-10, 40)` radius 1); `spatialHashEmptyRebuild` (size 0, a query visits nothing);
`percentileIsNearestRank` (`{5,1,3,2,4}`: p50 = 3, p99 = 5, p100 = 5; `1..10`: p50 = 5, p99 = 10);
`fnv1aMatchesTheReference` (`fnv1a(FnvOffsetBasis, "a", 1) == 0xaf63dc4c8601ec8c`);
`generateKeepsTheBaseFree` (`obstacleRate 1.0`: base and its 8 neighbours free, every other tile blocked; rate 0: none
blocked). The expected values of the two flow-field cases come from a reference search run while writing this plan.

- [ ] **Step 4: Wire the library and the test in `games/enxame/cmake/game.cmake`**

Keep the template lines and add: `file(GLOB coreSources CONFIGURE_DEPENDS core/*.cpp)`; `add_library(enxame_core STATIC
${coreSources})` with `target_include_directories(enxame_core PUBLIC core)`, `target_link_libraries(enxame_core PUBLIC
oly::ecs)` and `oly_warnings(enxame_core)`; `target_link_libraries(enxame PRIVATE enxame_core)`;
`add_executable(enxame_unit tests/unit.cpp)` linked to `enxame_core`, `oly_warnings(enxame_unit)`,
`add_test(NAME enxame.unit COMMAND enxame_unit)`. The game's own sources stay `native/*.cpp desktop/*.cpp`; `tests/`
is never globbed into the game.

- [ ] **Step 5: Run to see `enxame.unit` fail to build**

Run the game test command. Expected: compile errors, the `core/` headers do not exist.

- [ ] **Step 6: Implement `core/` to the interfaces above**

- [ ] **Step 7: Run to see them pass**

Run the game test command. Expected: `enxame.unit` and `enxame.headless` pass.

- [ ] **Step 8: Engine checks with the game present**

Add to `CLAUDE.md`, after the line on `oly`: "`games/enxame/` is the disposable validation game of the roadmap, built
with `oly` in local mode; the engine never names it."
Run: `./spur test -R '^oly\.lint\.'` — Expected: all pass (boundaries and links walk `games/enxame/`).
Run: `./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')` — Expected: exit 0.

- [ ] **Step 9: Commit**

```bash
git add games/enxame CLAUDE.md
git commit  # subject: "feat(enxame): add the game with its map, flow field and spatial hash"
```

---

### Task 6: The horde, S1 and the headless runner

**Files:**
- Modify: `games/enxame/enxame.oly`; Delete: `native/movement.cpp`; Replace: `native/systems.hpp`, `desktop/main.cpp`
- Create: `native/simulation.hpp`, `native/workload.hpp`, `native/workloads/s1_horde.hpp`, `native/scenario.{hpp,cpp}`,
  `native/horde.cpp`, `native/command_line.{hpp,cpp}`, `native/headless.{hpp,cpp}`, `desktop/window_game.{hpp,cpp}`
- Create: `games/enxame/tests/expect_exit.cmake`, `games/enxame/tests/check_run.cmake`; Modify: `cmake/game.cmake`

**Interfaces:**
- Consumes: Task 5's `core/` types; Task 3's `Engine::setTimingEnabled`, `systemTimings`, `flushTimings`.
- Produces:

```cpp
// workload.hpp
struct RunLength {
  uint32_t warmup;
  uint32_t steps;
};

struct HordeParameters {
  uint32_t mapWidth;
  uint32_t mapHeight;
  float tileSize;
  float obstacleRate;
  float enemySpeed; // world units per second
  float separationRadius;
  float separationStrength; // world units per second at distance 0
};

// scenario.hpp
enum class ScenarioId : uint8_t { Playable, S1, S2, S3, S4, S5, S6, S7, S8 };

struct ScenarioInfo {
  std::string_view name; // "S1"
  std::span<const uint32_t> sizes; // the frozen sizes; empty when blocked
  uint64_t seed;
  RunLength run;
  std::string_view blockedReason; // empty when runnable
};

std::optional<ScenarioId> parseScenario(std::string_view name); // only names with a row in the table
const ScenarioInfo& scenarioInfo(ScenarioId scenario);
std::span<const ScenarioId> listedScenarios(); // S1..S8 with a row, in order
// Binds actions::place to the left mouse button, registers the scenario's systems and spawns its world. The Engine
// already ran setUpGame. The returned Simulation is the userData of every system: keep it alive as long as the Engine.
std::unique_ptr<Simulation> setUpScenario(ScenarioId scenario, uint32_t size, oly::app::Engine& engine);

// command_line.hpp
struct LaunchOptions {
  bool headless = false;
  bool list = false;
  std::optional<ScenarioId> scenario;
  std::optional<uint32_t> size;
  std::optional<uint32_t> steps; // tests only: warmup 0 and this many steps
  bool timing = true;
  std::string commit;
  std::string out;
  std::vector<char*> passThrough; // argv[0] and the arguments Enxame does not know, for runGame
};

// std::nullopt after printing the reason and the usage to stderr (exit 64).
std::optional<LaunchOptions> parseCommandLine(int argumentCount, char** arguments);

// headless.hpp — 0, 64 (blocked scenario, size not frozen) or 1 (cannot write the output).
int runHeadless(const LaunchOptions& options);
```

- `Simulation` (in `simulation.hpp`) holds, for later tasks to extend: `Random random`, `Camera camera` (`x`, `y`,
  `zoom`), `std::optional<TileMap> map`, `FlowField flowField`, `std::optional<SpatialHash> hash`, `HordeParameters
  horde`, and the scratch arrays the hash rebuild fills. Without resources (phase 5), every system reaches this state
  through its `userData` (spec §5.5).
- Systems (declared in `native/systems.hpp`, all `void name(oly::app::SystemContext&)`): `updateFlowField`
  (UpdateBefore), `rebuildSpatialHash` (Update), `moveHorde` (Update, after `rebuildSpatialHash`).
- `enxame.oly`: `tag Enemy`; the actions of every later task: `action pan: axis2d`, `action zoomIn: button`,
  `action zoomOut: button`, `action place: button`, `action cursor: pointer`. Remove the template's `Player` and `move`.

**S1 workload** (`native/workloads/s1_horde.hpp`, frozen at C0; values are **[plan decision, not measured]**):
seed 1; sizes 1 000, 10 000, 100 000; warmup 60; steps 600; map 512 × 512; tile 8; obstacle rate 0.10; enemy speed 24;
separation radius 6; separation strength 30. The hash cell is the tile size in every scenario: the separation radius
(6) fits in one cell, so a 3 × 3 cell query finds every neighbour.

**Mechanics:**
- Spawn: each enemy on a random free tile other than the base, at the tile centre plus a jitter in `[-tile/2, tile/2)`,
  with `Position` and `Enemy`.
- `updateFlowField`: rebuilds when `map.version() != flowField.builtVersion()` (built once at setup in S1).
- `rebuildSpatialHash`: gathers every `Enemy` position into the scratch arrays (query order) and rebuilds the hash.
- `moveHorde`: per enemy, the flow direction of its tile (normalized; straight to the base when the direction is
  `{0, 0}` and the tile is not the base) times `enemySpeed`, plus separation: for each neighbour from the hash within
  `separationRadius`, `(p - q) / |p - q| * (1 - d / r) * separationStrength` (skip `d == 0`). Then `Position +=
  velocity * fixedDeltaTime`. An enemy within one tile of the base centre respawns on a random free tile of the map
  border. Neighbours come from the hash snapshot, so the result does not depend on the order enemies are moved in.
  Enemies do not collide with obstacles.

**Command line:**

```sh
enxame [--scenario <name>] [--size <n>] [--frames <n>]            # window
enxame --headless --scenario <name> --size <n> --commit <sha> --out <file.json> [--timing on|off] [--steps <n>]
enxame --list                                                    # one "<scenario> <size>" line per runnable pair
```

- Headless needs `--scenario`, `--size`, `--commit` and `--out`; a missing one gives `enxame: --headless needs <option>`
  (the first missing, in that order). `--commit` must match `[0-9A-Za-z._-]+`, so the JSON needs no escaping; else
  `enxame: --commit must match [0-9A-Za-z._-]+`. An unknown name gives `enxame: unknown scenario <name>`. An unknown
  option in headless mode is an error; in window mode it goes to `passThrough`.
- Checks in this order: blocked, then size. A blocked scenario (S4, S7), headless or window: exit 64,
  `enxame: <name> is blocked: needs stable ids` (S4) or `... blocked: needs assets` (S7). `--size` not in the
  scenario's sizes: exit 64, `enxame: size <n> is not one of <name>'s: <sizes>`.
- The runner opens `--out` before the first frame, so a bad path fails at once: exit 1, `enxame: cannot write <path>`.
- Window mode: `main` stores the scenario and size with `setWindowScenario(ScenarioId, uint32_t)` (declared in
  `desktop/window_game.hpp`), then calls `oly::native_host::runGame<EnxameGame>(passThrough)`: `runGame<G>` needs a
  default-constructible game, so the choice travels through that setter. Default scenario: S1 at its first size, until
  Task 8 adds the playable world. `EnxameGame::onStart` calls `setUpScenario` and keeps the `Simulation`; it draws
  nothing yet (Task 7).

**Runner loop:** build the `World` and `Engine` (default `EngineConfig`, 60 Hz), `setUpGame`, `setUpScenario`,
`setTimingEnabled(options.timing)`. For `k = 0 … warmup + steps`: `frame(k / 60.0)` between two `steady_clock` reads;
frames `1 … warmup` are warmup; for every later frame record the wall time, and when timing is on, the `nanoseconds` of
each system entry with `ran` and of each flush. With `--steps n`, warmup is 0 and steps is n.

**Output JSON** (one object; integers in nanoseconds; percentiles by `enxame::percentile`; written with `fprintf`):

```json
{
  "commit": "<sha>", "scenario": "S1", "size": 10000, "seed": 1,
  "warmup": 60, "steps": 600, "stepsOverridden": false, "timing": true,
  "step": { "p50": 0, "p99": 0, "max": 0 },
  "systems": [ { "name": "moveHorde", "phase": "Update", "ran": 600, "median": 0 } ],
  "flushes": [ { "phase": "Update", "median": 0 } ],
  "measures": {}
}
```

`systems` lists every entry in dispatch order (empty when timing is off); `median` is over the steps where it ran.
Prints `wrote <path>` and exits 0. Phase names: `UpdateBefore`, `Update`, `UpdateAfter`, `Render`.

- [ ] **Step 1: Write the failing CTest cases in `cmake/game.cmake`**

`tests/expect_exit.cmake` runs `COMMAND` (a `;` list) with `execute_process`, and fails unless the exit code is
`EXIT_CODE` and stderr contains `TEXT`. `tests/check_run.cmake` runs `COMMAND`, requires exit 0, reads `OUT` with
`file(READ)` and checks with `string(JSON ... GET)` that `scenario` equals `SCENARIO`, `step.p50` exists and, when
`TIMING` is `on`, `systems` has at least one entry. Helper in `game.cmake`:

```cmake
# enxame_expect_exit(<name> <exit code> <stderr text> <arguments...>)
function(enxame_expect_exit name exitCode text)
  add_test(NAME enxame.usage.${name}
    COMMAND ${CMAKE_COMMAND} "-DCOMMAND=$<TARGET_FILE:enxame>;${ARGN}" -DEXIT_CODE=${exitCode} "-DTEXT=${text}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_exit.cmake)
endfunction()
```

Cases:

| Name | Exit | Text | Arguments |
|---|---|---|---|
| `size_not_frozen` | 64 | `is not one of S1's` | `--headless --scenario S1 --size 7 --commit test --out x.json` |
| `missing_out` | 64 | `needs --out` | `--headless --scenario S1 --size 1000 --commit test` |
| `missing_commit` | 64 | `needs --commit` | `--headless --scenario S1 --size 1000 --out x.json` |
| `unknown_scenario` | 64 | `unknown scenario S9` | `--headless --scenario S9 --size 1 --commit test --out x.json` |
| `bad_commit` | 64 | `--commit must match` | `--headless --scenario S1 --size 1000 --commit a/b --out x.json` |
| `blocked_s4` | 64 | `blocked: needs stable ids` | `--headless --scenario S4 --size 1 --commit test --out x.json` |
| `blocked_s7` | 64 | `blocked: needs assets` | `--headless --scenario S7 --size 1 --commit test --out x.json` |
| `unwritable_out` | 1 | `cannot write` | `--headless --scenario S1 --size 1000 --steps 3 --commit test --out missing/folder/x.json` |

And `enxame.run.S1` with `check_run.cmake`: `--headless --scenario S1 --size 1000 --steps 5 --commit test --out
${CMAKE_CURRENT_BINARY_DIR}/run_S1.json`, `TIMING on`; `enxame.run.S1_untimed` the same with `--timing off`, `TIMING off`.
`enxame.headless` (template) stays.

- [ ] **Step 2: Run to see them fail**

Run the game test command. Expected: the new tests fail (the template's `main` refuses every option with 64 and
`usage: ... [--frames <n>]`).

- [ ] **Step 3: Implement** the schema, `Simulation`, the S1 workload, `scenario.cpp` (rows S1, S4, S7), `horde.cpp`,
  `command_line.cpp`, `headless.cpp`, the minimal `window_game.*` and `main.cpp`, as described above.

- [ ] **Step 4: Run to see them pass**

Run the game test command. Expected: `100% tests passed` (unit, headless window, 8 usage, 2 runs).

- [ ] **Step 5: One release run of S1 at 100 000, for the size guard**

Run: `./spur dev sh -c 'cd games/enxame && OLY_ENGINE_DIR=/app OLY_IN_DOCKER=1 sh ./oly build --release && time build/cmake-release-docker/games/enxame --headless --scenario S1 --size 100000 --commit test --out build/s1.json'`
Expected: `wrote build/s1.json`. If `real` is above 60 s, set S1's steps to 300 in `s1_horde.hpp` and say so in the
commit body: the comparison runs it 20 times.

- [ ] **Step 6: Format and commit**

Run the format check. Then:

```bash
git add games/enxame
git commit  # subject: "feat(enxame): run the horde scenario headless"
```

---

### Task 7: The window — camera and squares

**Files:**
- Modify: `games/enxame/desktop/window_game.{hpp,cpp}`, `native/systems.hpp`, `native/scenario.cpp` (camera start)

**Interfaces:**
- Consumes: `Simulation::camera`, `Simulation::map`, the actions of Task 6.
- Produces: window-only systems `moveCamera` (Update) and `drawWorld` (Render). They are registered by `EnxameGame`,
  never by `setUpScenario`, so no headless run has them.

**Behaviour:**
- Bindings (window only): `pan` → W, S, A, D; `zoomIn` → E; `zoomOut` → Q. The zoom uses keys because the engine has no
  wheel input (spec §3).
- `moveCamera`: `camera.x/y += pan * 400 / zoom * dt`; while `zoomIn` is held `zoom *= 1 + 1.5 * dt`, while `zoomOut`
  is held `zoom /= 1 + 1.5 * dt`; zoom clamped to `[0.05, 8]`.
- Camera start (set by `setUpScenario`, so a replay sees the same one): centre of the base for map scenarios, the
  centre of the spawned world otherwise; zoom 1.
- `drawWorld` (userData: a struct with the `SDL_Renderer*` and the `Simulation*`): logical size 640 × 360; screen =
  `(world - camera) * zoom + (320, 180)`. Draws the visible blocked tiles (60, 60, 70) and the base (255, 255, 255),
  then every entity from its `WorldTransform` (it already holds the parent chain), as a square of side `size * zoom`,
  at least 1 px. Sizes and colours: enemy 4 (220, 60, 60); later tasks add turret 8 (80, 140, 255), projectile 2
  (255, 220, 80), belt item 3 (170, 170, 170), assembler 8 (230, 140, 40), vehicle 6 (80, 200, 120), vehicle turret 3
  (160, 240, 170).

- [ ] **Step 1: Implement** the bindings, the two systems and the camera start.

- [ ] **Step 2: Run the game test command**

Expected: `100% tests passed`; `enxame.headless` (`--frames 120`, dummy driver) now runs `moveCamera` and `drawWorld`.

- [ ] **Step 3: See the window (by the user, on the Windows host)**

Run: `./spur windows sh -c 'cd games/enxame && OLY_ENGINE_DIR=/app OLY_IN_DOCKER=1 sh ./oly build --platform windows'`
Then on the host: `games/enxame/build/cmake-windows-docker/games/enxame.exe --scenario S1`.
Expected: red squares stream around the obstacles toward the base; WASD pans, Q/E zoom. **[hypothesis until run]**:
the MinGW build links statically (`cmake/toolchains/mingw-w64.cmake:17`), so the `.exe` should start without DLLs. This
step is a visual check and does not block the task.

- [ ] **Step 4: Format and commit**

```bash
git add games/enxame
git commit  # subject: "feat(enxame): draw the world and move the camera"
```

---

### Task 8: Turrets, the click, the playable world and S5

**Files:**
- Create: `native/combat.cpp`, `native/world_hash.{hpp,cpp}`, `native/workloads/s5_replay.hpp`
- Modify: `enxame.oly`, `native/simulation.hpp`, `native/systems.hpp`, `native/scenario.cpp`, `native/headless.cpp`,
  `desktop/main.cpp` (default scenario), `desktop/window_game.cpp` (colours), `cmake/game.cmake`,
  Create: `games/enxame/tests/replay_determinism.cmake`

**Interfaces:**
- Consumes: Task 6's horde systems, `Simulation`, `SpatialHash::forEachWithin`, `TileMap::block`.
- Produces: `enxame.oly` gains `component Turret { reload: f32 }` and
  `component Projectile { velocityX: f32, velocityY: f32, life: f32 }`. Systems: `placeTurret` (UpdateBefore, registered
  before `updateFlowField`, so the field sees the new tile in the same step), `fireTurrets` and `moveProjectiles`
  (UpdateAfter, in that order). `uint64_t worldHash(oly::ecs::World& world)` in `world_hash.hpp`.
- `ScenarioId::Playable` and `S5` both build the S5 world; only the runner of S5 replays clicks. The window default
  becomes `Playable`.

**S5 workload** (`native/workloads/s5_replay.hpp`, **[plan decision, not measured]**): seed 5; sizes 2 000; map 128 ×
128; tile 8; obstacle rate 0.10; enemy speed 24; separation radius 6; separation strength 30; turret reload 0.5 s, range
64, projectile speed 160, projectile life 1.0 s, hit radius 4; warmup 0; steps 10 000. Clicks: 50, click `i` (0 … 49)
at step `100 + 200 i`, on screen `(200 + 24 (i mod 10), 120 + 30 (i div 10))`, generated by a `constexpr` function in
the same header (the "file in the game" of the spec). Some clicks land on obstacles or the base and are refused: that
is part of the frozen input.

**Mechanics:**
- `placeTurret`: when `place` is pressed this step and `cursor` is valid, world = `camera + (pointer - (320, 180)) /
  zoom`; if `map.block(tileAt(world))` succeeds, spawn `Position` (tile centre) and `Turret{0}`.
- `fireTurrets`: `reload -= dt`; at `reload <= 0`, the nearest enemy within range in the hash (first visited on a tie)
  gets a projectile aimed at its position, with speed and life from the workload, and `reload = reloadSeconds`. No
  target: `reload` stays at 0.
- `moveProjectiles`: move; the first enemy within `hitRadius` (hash order) not already hit this step is destroyed with
  the projectile; else `life -= dt` and destroy at `life <= 0`. "Hit this step" is a vector cleared at the start of the
  system: two `destroy` commands for one entity in the same flush are never recorded.
- `worldHash`: collect every entity with `Position`, sort by `(index, generation)`, and fold with `fnv1a` its index,
  generation, `Position` bytes, then for each game component and tag of `enxame.oly`, in `.oly` order, a one-byte
  presence flag and the component's bytes. Tasks 9 and 10 append their components to that list.
- Runner, S5: before frame `step + 1` of each click, `actions().pointerMove(x, y)` and `buttonDown(MouseButton::Left)`;
  after that frame, `buttonUp`. `measures`: `"worldHash": "0x<16 hex digits>"`, `"enemiesLeft"`, `"turrets"`.

- [ ] **Step 1: Write the failing test**

`tests/replay_determinism.cmake` runs `COMMAND` twice with `--out` set to two files, reads `measures.worldHash` of
each with `string(JSON)`, and fails unless both exist and are equal. Register `enxame.replay_determinism` with
`--headless --scenario S5 --size 2000 --steps 2500 --commit test` (2 500 steps cover 12 clicks); and `enxame.run.S5`
with `check_run.cmake`, `--steps 5`.

- [ ] **Step 2: Run to see them fail** — the game test command. Expected: `unknown scenario` for S5.

- [ ] **Step 3: Implement** as above.

- [ ] **Step 4: Run to see them pass** — the game test command. Expected: `100% tests passed`.

- [ ] **Step 5: Check the hash sees a change**

Temporarily change one click of `s5_replay.hpp` to a free tile far from the others, run
`enxame --headless --scenario S5 --size 2000 --steps 2500 ...` and compare `worldHash` with a run of the original file:
they must differ. Restore the file by undoing the edit (not with `git checkout`: the file is not committed yet).

- [ ] **Step 6: Format and commit**

```bash
git add games/enxame
git commit  # subject: "feat(enxame): add turrets, clicks and the replay scenario"
```

---

### Task 9: S2 — belts, assemblers and allocations per step

**Files:**
- Create: `native/factory.cpp`, `native/allocation_counter.{hpp,cpp}`, `native/workloads/s2_factory.hpp`
- Modify: `enxame.oly`, `native/simulation.hpp`, `native/systems.hpp`, `native/scenario.cpp`, `native/headless.cpp`,
  `native/world_hash.cpp`, `desktop/window_game.cpp`, `cmake/game.cmake`

**Interfaces:**
- Produces: `component BeltItem { belt: u32, offset: f32 }`, `component Assembler { belt: u32, stock: u32, timer: f32 }`.
  Systems: `moveBeltItems`, then `runAssemblers` (both Update). `uint64_t allocationCount() noexcept` in
  `allocation_counter.hpp`.

**S2 workload** (**[plan decision, not measured]**): seed 2; sizes 50 000 (items); assemblers 500; belt 64 tiles; tile
8; item speed 16; craft 0.25 s; assembler capacity 4; warmup 60; steps 600. Each assembler has one belt of its own:
belt `b` runs along `y = 16 b` from `x = 0` to `x = 512`, its assembler sits at `x = 520`. 100 items per belt, evenly
spaced at the start. An item arrives every 0.32 s and the assembler crafts every 0.25 s, so stock stays bounded and the
item count stays near 50 000.

**Mechanics:**
- `moveBeltItems`: `offset += speed * dt`; at the end of the belt, if the belt's assembler has `stock < capacity`, destroy
  the item and `stock += 1`; else the item waits at the end. `Position` = belt start + offset.
- `runAssemblers`: `timer -= dt`; at `timer <= 0` with `stock >= 1`: `stock -= 1`, spawn an item at offset 0 of its belt,
  `timer = craftSeconds`. `Simulation` keeps the assembler entity of each belt, so an item reaches it with `get`.
- `allocation_counter.cpp` replaces the global `operator new` and `operator new[]`, plain and `std::align_val_t`, and every
  matching `operator delete` (plain, sized, aligned, sized aligned). Each `new` adds 1 to a
  `std::atomic<uint64_t>` (relaxed) and allocates with `std::malloc`, or for aligned forms `std::aligned_alloc`
  (size rounded up to the alignment) on Linux and `_aligned_malloc` on Windows (MinGW has no `aligned_alloc`); a size of
  0 allocates 1 byte, and a null result throws `std::bad_alloc`. Each `delete` frees with the matching call. The aligned forms matter: the engine's columns come from
  `oly::core::alignedAllocate`, which calls the aligned `operator new`.
- Runner, S2: `allocationCount()` before and after each recorded frame. `measures`: `"allocationsPerStep": { "p50",
  "max", "total" }`.

- [ ] **Step 1: Failing test** — `enxame.run.S2` with `check_run.cmake`, `--steps 5`; and in `check_run.cmake`, when
  `SCENARIO` is `S2`, require `measures.allocationsPerStep.total` to exist.
- [ ] **Step 2: Run to see it fail** — `unknown scenario`.
- [ ] **Step 3: Implement** as above; draw items and assemblers; append both components to the hash list.
- [ ] **Step 4: Run to see it pass** — the game test command, `100% tests passed`.
- [ ] **Step 5: Check the counter counts** — run S2 with `--steps 60`: `allocationsPerStep.total` must be above 0
  (spawns through the command buffer allocate); then `enxame.unit` must still pass (the replacement is linked only into
  `enxame`).
- [ ] **Step 6: Format and commit** — subject: `feat(enxame): add the factory scenario`.

---

### Task 10: S3, S6 and S8

**Files:**
- Create: `native/status.cpp`, `native/vehicles.cpp`, `native/waves.cpp`, `native/workloads/s3_status.hpp`,
  `native/workloads/s6_hierarchy.hpp`, `native/workloads/s8_waves.hpp`
- Modify: `enxame.oly`, `native/simulation.hpp`, `native/systems.hpp`, `native/scenario.cpp`, `native/headless.cpp`,
  `native/world_hash.cpp`, `desktop/window_game.cpp`, `cmake/game.cmake`

**Interfaces:**
- Produces: tags `Burning`, `Slowed`, `Stunned`; `tag Vehicle`, `tag VehicleTurret`,
  `component VehicleMotion { centerX: f32, centerY: f32, radius: f32, angle: f32, angularSpeed: f32 }`; modifier tags
  `Armored`, `Swift`, `Regenerating`, `Shielded`, `Explosive`, `Splitting`, `Cloaked`, `Enraged` (no behaviour: S8 only
  counts their archetypes). Systems: `toggleStatus` (UpdateAfter), `moveVehicles` (Update), `runWaves` (UpdateBefore).
  `Simulation` gains `statusTogglesLastStep` and `wavesCompleted`.

**Workloads** (**[plan decision, not measured]**):
- S3: seed 3; sizes 10 000; the horde parameters of S1 (the header includes `s1_horde.hpp` and uses its values);
  toggle probability 0.10; warmup 60; steps 600. Systems: S1's plus `toggleStatus`.
- S6: seed 6; sizes 1 000 (vehicles); 4 turrets each at local `(±6, ±6)`; circle radius 32; angular speed 1.0 rad/s;
  vehicle centres on a grid of 32 columns, 80 units apart; warmup 60; steps 600. S6's world holds only the vehicles and
  their turrets.
- S8: seed 8; sizes 2 000 (enemies per wave); 50 waves of 60 steps; modifier probability 0.5 each; spawn area 1024 ×
  1024 centred on the origin; warmup 0; steps 3 000.

**Mechanics:**
- `toggleStatus`: resets `statusTogglesLastStep`; for each `Enemy` in query order, with `random.chance(0.10)`, picks one
  of the three statuses with `nextBelow(3)` and adds it if absent or removes it if present (deferred), counting toggles.
  An entity toggles at most once per step, so toggles equal archetype moves.
- `moveVehicles`: `angle += angularSpeed * dt`; `Position` = centre + radius (cos, sin)(angle); `Rotation` = angle. The
  turrets (`Position` local, `Parent` = the vehicle, `VehicleTurret`) follow through `propagateTransforms`.
- `runWaves`: at step 0 of a wave, spawn the wave: `Position` in the area, `Enemy`, and each modifier with
  `chance(0.5)`; at the last step of the wave, destroy every `Enemy` and `wavesCompleted += 1`.
- Runner: S3 `measures`: `"transitionsPerStep": { "p50", "max" }` and `"updateAfterFlushMedian"`. S6: `"visitedEntities":
  5000` and `"propagateTransformsNanosecondsPerEntity"` = median of `propagateTransforms` / 5 000 (needs timing on; null
  when off). S8: after each frame where `wavesCompleted` grew, one entry of `"waves": [ { "wave", "archetypes",
  "liveEntities", "columnBytes", "residentBytes" } ]` from `oly::ecs::stats(world)` (sum of `columnBytes`) and
  `/proc/self/statm` (resident pages × `sysconf(_SC_PAGESIZE)`) under `#ifdef __linux__`; `null` elsewhere.

- [ ] **Step 1: Failing tests** — `enxame.run.S3`, `enxame.run.S6` (`--steps 5`), `enxame.run.S8` (`--steps 120`: two
  waves); `check_run.cmake` requires `measures.waves` with 2 entries for S8.
- [ ] **Step 2: Run to see them fail** — `unknown scenario`.
- [ ] **Step 3: Implement** as above; draw vehicles and their turrets; append the new components to the hash list.
- [ ] **Step 4: Run to see them pass** — the game test command, `100% tests passed`; then `enxame --list` prints eight
  lines: `S1 1000`, `S1 10000`, `S1 100000`, `S2 50000`, `S3 10000`, `S5 2000`, `S6 1000`, `S8 2000`.
- [ ] **Step 5: Format and commit** — subject: `feat(enxame): add the status, hierarchy and waves scenarios`.

---

### Task 11: The comparison script

**Files:**
- Create: `games/enxame/compare/compare.sh` (host), `games/enxame/compare/inside.sh` (dev container),
  `games/enxame/compare/summarize.py`, `games/enxame/compare/test_summarize.py`

**Interfaces:**
- Consumes: `enxame --list`, the runner's JSON, `oly build --release`.
- Produces:

```sh
sh games/enxame/compare/compare.sh <commit-A> <commit-B>   # results in games/enxame/build/compare-results/<stamp>-<a7>-<b7>/
sh games/enxame/compare/compare.sh --overhead <commit>     # S1 at 10 000, a = timing off, b = timing on
```

`summarize.py <results> [--a-label <text>] [--b-label <text>]` writes, per `<scenario>-<size>`, a JSON with: `a` and `b`
(`commit`, `label`, every repetition's run JSON, and `median`: the median over repetitions of `step.p50`, `step.p99`,
`step.max`, each system's `median` and each scalar measure); `ratio` (`p50`, `p99`: B median / A median); `pairs`
(`p50`, `p99`: the per-pair ratios `B_i / A_i`, their `min`, `max` and `iqr` = Q3 − Q1 from
`statistics.quantiles(n=4, method="inclusive")`); for S5, `worldHashEqual` (every A equal, every B equal, A equal to B).
And `summary.md`, a table with one row per scenario and size.

**`compare.sh` (host, Git Bash):**
1. `cd "$(git rev-parse --show-toplevel)"`; resolve both commits with `git rev-parse --verify "<c>^{commit}"` (fail 1
   with the name if one does not exist).
2. Rule 7 guard: `git diff --quiet A B -- games/enxame/native/workloads/` or fail 1 with "the workloads differ between
   A and B: measure A with B's workloads first".
3. `results=games/enxame/build/compare-results/<YYYYmmdd-HHMMSS>-<a7>-<b7>` (`<a7>`: the first 7 characters of A);
   `work=$(mktemp -d games/enxame/build/compare/run.XXXXXX)` (after `mkdir -p` of its parent);
   `trap` on `EXIT INT TERM` removes every worktree this run added (`git worktree remove --force`), then
   `git worktree prune` and `rm -rf "$work"`. Then `git worktree add --detach "$work/a" A` and the same for `b`
   (`--detach` lets A and B be the same commit).
4. `./spur dev env ENXAME_REPETITIONS="${ENXAME_REPETITIONS:-10}" ENXAME_STEPS="${ENXAME_STEPS:-}" sh
   games/enxame/compare/inside.sh compare "$work/a" "$work/b" A B "$results"` — paths relative to the checkout, which
   the container mounts at `/app`. The container does not inherit host variables, so the two are passed with `env`.
   They exist only for the short check of Step 5: the baseline uses 10 repetitions and the frozen steps.

**`inside.sh` (dev container):**
- `build <dir>`: `cd "/app/<dir>/games/enxame" && OLY_ENGINE_DIR="/app/<dir>" OLY_IN_DOCKER=1 sh ./oly build --release`.
- `compare`: build both; take the runs from A's `enxame --list` and fail unless B's list is the same; for each run,
  for repetition 1 … `ENXAME_REPETITIONS`, run A then B (one process at a time, with `--steps "$ENXAME_STEPS"` only
  when it is set, and then a line in `summary.md` says so), each with `--commit` and `--out
  <results>/<a|b>/<scenario>-<size>-<rep>.json`; then `python3 -I games/enxame/compare/summarize.py <results> --a-label
  A --b-label B`.
- `overhead <dir> <commit> <results>`: build once; S1 at 10 000, 10 repetitions, `--timing off` into `a/` then
  `--timing on` into `b/`; summarize with labels `timing off` and `timing on`.

- [ ] **Step 1: Write the failing `test_summarize.py`** (`unittest`; imports `summarize` after
  `sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))`, as `tools/test_check_links.py` does). It writes run
  JSONs into a temporary results folder and checks:
  - A `step.p50` = 100, 110, 120, 130 and B = 200, 220, 240, 260 → `ratio.p50` = 2.0; `pairs.p50.min` = `max` = 2.0;
    `iqr` = 0.0.
  - Per-pair ratios 1.0, 1.1, 1.2, 1.3 (A = 100 each; B = 100, 110, 120, 130) → `iqr` ≈ 0.15 (`assertAlmostEqual`,
    places 9): Q1 = 1.075, Q3 = 1.225.
  - S5 runs whose hashes differ in B → `worldHashEqual` is false.
- [ ] **Step 2: Run to see it fail** — `./spur dev python3 -I games/enxame/compare/test_summarize.py`. Expected:
  `ModuleNotFoundError: summarize`.
- [ ] **Step 3: Implement** `summarize.py`, `inside.sh` and `compare.sh` as above.
- [ ] **Step 4: Run to see it pass** — the same command. Expected: `OK`.
- [ ] **Step 5: A short real run** — commit first (the worktrees check out commits), then
  `ENXAME_REPETITIONS=2 ENXAME_STEPS=5 sh games/enxame/compare/compare.sh HEAD HEAD`. Expected: a `summary.md` with
  eight rows and the line about the step override, and `git worktree list` the same as before the run.
- [ ] **Step 6: Failure path** — `sh games/enxame/compare/compare.sh HEAD not-a-commit; echo "exit=$?"`. Expected:
  `exit=1`, the name of the bad commit, and no new line in `git worktree list`.
- [ ] **Step 7: Format and commit** — `git add games/enxame/compare`; subject: `feat(enxame): compare two commits
  side by side`. This commit is **C0**: the code-complete commit that is measured. Note its hash.

---

### Task 12: Baseline, noise and overhead

**Files:**
- Create: `docs/reference/benchmarks/enxame/README.md` and one JSON per scenario and run
- Modify: `docs/reference/benchmarks/README.md` ("Where the results live" table), `docs/README.md` (index row)

- [ ] **Step 1: Check C0** — `git status` clean and `git log -1` is the Task 11 commit.

- [ ] **Step 2: The A/A run** — `sh games/enxame/compare/compare.sh <C0> <C0>` with `run_in_background`, nothing else
  running on the machine. Expected: eight rows in `summary.md`. A's medians are the baseline; the per-pair ratios are each
  scenario's noise.

- [ ] **Step 3: The overhead run** — `sh games/enxame/compare/compare.sh --overhead <C0>`. Expected: one row; `ratio.p50`
  is timing on / timing off and confirms or rejects the "~1 µs per step" hypothesis of spec §5.3.

- [ ] **Step 4: Write the results**

Copy each `<scenario>-<size>.json` of the A/A run to `docs/reference/benchmarks/enxame/<c0-7>-aa-<scenario>-<size>.json`
and the overhead one to `<c0-7>-overhead-S1-10000.json`. `README.md` there holds:
- method: C0's hash, the command, 10 alternating repetitions, the machine line of `benchmarks/README.md`; the game named
  by path in plain text (`games/enxame/`), never as a link — the results outlive the game;
- the baseline table: per scenario and size, median step p50, p99 and max, the three slowest systems, and the scenario's
  measures (S2 allocations, S3 transitions and UpdateAfter flush, S5 hash equality, S6 ns per visited entity next to the
  26–36 ns of the review — context only, the review measured children alone, S8 archetypes, column bytes and resident
  memory after the last wave);
- the noise table: per scenario, min, max and IQR of the per-pair ratios for p50 and p99, with the rule: a later gate
  closer to 1 than that spread cannot be trusted;
- the timing overhead ratio;
- S4 blocked (no stable ids; baseline in phase 2) and S7 blocked (no assets; baseline in phase 5);
- limits: absolute numbers are context; allocations count `operator new` only.
Add the `enxame/` row to the "Where the results live" table and the page to the index in `docs/README.md`.

- [ ] **Step 5: The gate commands (spec §6)**

| Command | Expected |
|---|---|
| `./spur test` | `100% tests passed` |
| `./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')` | exit 0 |
| `./spur format-check` (no argument, from this worktree) | exit 1, "git cannot list the tracked files here" |
| `./spur oly-tools-test` | every line `ok` |
| `./spur games-test` | `100% tests passed` |
| `./spur native-games-test` | `100% tests passed` |
| the game test command | `100% tests passed` |
| `./spur dev python3 -I games/enxame/compare/test_summarize.py` | `OK` |
| `git worktree list` | no `compare/run.*` entry |

- [ ] **Step 6: Commit (documentation only, after C0)**

```bash
git add docs/reference/benchmarks docs/README.md
git commit  # subject: "docs(bench): record the Enxame phase 0 baseline"
```
