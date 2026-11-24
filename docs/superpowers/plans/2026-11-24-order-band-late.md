# A `Late` order band between `Default` and `Last` — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `OrderBand::Late` runs inside each phase after the `Default` band and before the `Last` band, so a plugin
(the coming `motion2d`) can run after the game's `Update` systems and before `propagateTransforms`.

**Architecture:** one enumerator in `oly/app/phase.hpp`. `OrderBandCount` is derived from `Last`, so `Engine::phases_`
grows one vector per phase and `Engine::runPhase`, which already walks the bands in index order, does not change. The
phase still flushes its `CommandBuffer` once, after `Last`.

**Tech Stack:** C++20, doctest, CMake/CTest, the `dev` Docker image through `./spur`.

**Spec:** `docs/superpowers/specs/2026-11-24-order-band-late-design.md`

## Global Constraints

- Work on a branch `feat/order-band-late` from `develop`.
- Toolchain only through `./spur …` from Git Bash; there is no compiler on the host.
- `enum class OrderBand : uint8_t { Default, Late, Last };` — this order, this name.
- No change to `Engine::runPhase`, `Engine::addSystem`, the C API or the TS client: they keep registering in `Default`.
- `plugins/essentials2d` and `plugins/essentials3d` keep `propagateTransforms` in `OrderBand::Last`.
- `CHANGELOG.md`, `## Unreleased`: `### Breaking` stays `None.`; the entry goes in `### Migration`.
- `docs/superpowers/` is history: not edited (except this plan's checkboxes).
- New tests follow the repository's `add-test-case` skill (`CHECK_INVARIANTS` after the frame, ctest filter shows the
  new cases running).
- A temporary mutation is made and undone with the edit tool (the same text swap both ways), never with
  `git checkout`/`git restore`: the file holds uncommitted work of the task.
- Comments and commit messages in English. Commit subjects short, `type(scope): subject`, with no spec, plan or task
  reference.
- Code style (personal `code-style` skill): blank lines separate steps, not statements; a multi-line control block gets
  one before and after; never after `{` or before `}`; at most one in a row; consecutive guards stay together; tests
  have exactly three blocks (Arrange, Act, Assert); no whitespace changes in untouched code. Comments only for reason,
  measurement, external constraint or non-local invariant; no narration of the edit; a comment touched by the change
  remains true.
- Every doc change follows `docs/reference/contributor/docs-style.md` and uses the glossary names.

## Review Focus

1. **A plugin installs before the game, so its `Late` system is registered first.** It must still run after the game's
   `Default` systems. Task 1's first test registers the `Late` system before the `Default` ones.
2. **The `motion2d` case end to end:** a `Late` write to `Position` in `Update` must reach `WorldTransform` before
   `UpdateAfter` reads it. Task 1, `essentials2d.transforms` test, guarded by a mutation that moves
   `propagateTransforms` to `Late`.
3. **`setWhilePaused` and `setSystemStates` on a `Late` system** find it through `SystemLocation{phase, band, position}`.
   Task 1 adds the `Late` counterpart of the existing `Last` test.
4. **A `Late` system reading a `Default` spawn** expects it alive and finds it dead (one flush per phase). Task 1's third
   test pins that it sees the field write and not the spawn, and that the spawn exists after the frame.
5. **Code that enumerates the bands** (a `switch` over `OrderBand`, a table sized by hand, a C API or TS mapping). Checked
   on 2026-11-24: `OrderBand` appears only in `phase.hpp`, `engine.hpp`, `engine.cpp`, tests and three `addSystem`
   calls; no `switch`, no `.ts` or C header. Task 1, Step 6 re-runs the grep.

---

### Task 1: `OrderBand::Late` in the engine, with its tests

**Files:**
- Modify: `engine/runtime/app/include/oly/app/phase.hpp` (`OrderBand` and its comment)
- Modify: `engine/runtime/app/include/oly/app/engine.hpp` (the comment above `addSystem`)
- Test: `engine/runtime/app/tests/systems.cpp` (suite `app.systems`, already in `tests/CMakeLists.txt`)
- Test: `plugins/essentials2d/native/tests/transforms.cpp` (suite `essentials2d.transforms`, already registered)

**Interfaces:**
- Produces: `oly::app::OrderBand::Late`, with `static_cast<uint32_t>(OrderBand::Late) == 1` and `OrderBandCount == 3`.

- [ ] **Step 1: Write the failing tests in `systems.cpp`**

In the anonymous `namespace`, after `observeAfterLastBand`:

```cpp
  struct LateProbe {
    oly::ecs::Entity moved;
    oly::ecs::Entity spawned;
    float xInLateBand = 0;
    bool spawnedAliveInLateBand = true;
  };

  void moveAndSpawnInDefaultBand(SystemContext& context) {
    LateProbe& probe = *static_cast<LateProbe*>(context.userData());
    context.get<Position>(probe.moved).x = 5;
    probe.spawned = context.spawn(Position{});
  }

  void observeInLateBand(SystemContext& context) {
    LateProbe& probe = *static_cast<LateProbe*>(context.userData());
    probe.xInLateBand = context.get<Position>(probe.moved).x;
    probe.spawnedAliveInLateBand = context.isAlive(probe.spawned);
  }
```

At the end of the file. The registration orders are chosen so that registration order alone, `Late` treated as
`Default` and `Late` treated as `Last` each give a different log:

```cpp
TEST_CASE("a Late system runs after every Default system and before every Last system of its phase, whatever the registration order") {
  oly::ecs::World world;
  Engine engine(world);
  Log log;
  engine.addSystem(Phase::Update, "last1", recordName, &log, OrderBand::Last);
  engine.addSystem(Phase::Update, "late1", recordName, &log, OrderBand::Late);
  engine.addSystem(Phase::Update, "a", recordName, &log);
  engine.addSystem(Phase::Update, "b", recordName, &log);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(log.calls == std::vector<std::string>{"a", "b", "late1", "last1"});
  CHECK_INVARIANTS(world);
}

TEST_CASE("Late systems keep their registration order") {
  oly::ecs::World world;
  Engine engine(world);
  Log log;
  engine.addSystem(Phase::Update, "late2", recordName, &log, OrderBand::Late);
  engine.addSystem(Phase::Update, "a", recordName, &log);
  engine.addSystem(Phase::Update, "late1", recordName, &log, OrderBand::Late);
  engine.addSystem(Phase::Update, "last", recordName, &log, OrderBand::Last);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(log.calls == std::vector<std::string>{"a", "late2", "late1", "last"});
  CHECK_INVARIANTS(world);
}

TEST_CASE("a Late system sees the Default band's field writes but not its spawns") {
  oly::ecs::World world;
  Engine engine(world);
  LateProbe probe;
  probe.moved = world.spawn(Position{1, 2});
  engine.addSystem(Phase::Update, "observeLate", observeInLateBand, &probe, OrderBand::Late);
  engine.addSystem(Phase::Update, "moveAndSpawn", moveAndSpawnInDefaultBand, &probe);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(probe.xInLateBand == 5);
  CHECK(!probe.spawnedAliveInLateBand);
  CHECK(world.isAlive(probe.spawned));
  CHECK_INVARIANTS(world);
}
```

And `TEST_CASE("setWhilePaused and setSystemStates apply to a Late system")`: the body of the existing
`"setWhilePaused and setSystemStates apply to a Last system"` with `OrderBand::Late` in its three `addSystem` calls; same
expected log `{"plain", "whilePaused", "plain", "inMenu"}`.

- [ ] **Step 2: Write the failing test in `essentials2d` `transforms.cpp`**

After `"propagation runs after the Default systems of update, so updateAfter reads the new world values"`. The plugin is
installed in `Fixture`, before this registration, as `motion2d` will be:

```cpp
TEST_CASE("a Late system of update moves before the propagation, so updateAfter reads the new world values") {
  Fixture fixture;
  fixture.engine.addSystem(Phase::Update, "moveRight", moveRightByOne, nullptr, OrderBand::Late);
  Probe probe;
  probe.entity = fixture.world.spawn(Position{0, 0});
  probe.moved = true;
  fixture.engine.addSystem(Phase::UpdateAfter, "read", readWorldThenMove, &probe);

  fixture.step();

  CHECK(probe.worldXSeen == doctest::Approx(1));
}
```

- [ ] **Step 3: Run them to verify they fail**

Run: `./spur test -R '^(app\.systems|essentials2d\.transforms)$'`
Expected: the build stops in `systems.cpp` and `transforms.cpp` on `OrderBand::Late` (not a member of
`oly::app::OrderBand`).

- [ ] **Step 4: Add `Late` to `OrderBand` and update the two comments**

In `phase.hpp`: `enum class OrderBand : uint8_t { Default, Late, Last };`, `OrderBandCount` unchanged, and the comment
above it becomes `// Inside a phase, Default, then Late, then Last; inside a band, registration order.`

In `engine.hpp`, the comment above `addSystem` becomes:

```cpp
    // Cannot be called during frame (OLY_ASSERT). Within a phase, the Default band runs first, then Late, then Last,
    // and within a band systems run in registration order. The phase applies its commands once, after the Last band.
```

- [ ] **Step 5: Run them to verify they pass**

Run: `./spur test -R '^(app\.systems|essentials2d\.transforms)$'`
Expected: both PASS; the doctest output of `app.systems` counts 4 more test cases than before, `essentials2d.transforms` 1
more.

- [ ] **Step 6: See each guard fail against a mutation, then undo it with the edit tool**

1. `phase.hpp`: `{ Default, Late, Last }` → `{ Late, Default, Last }`. Run the Step 5 command: the three ordering and
   visibility tests of Step 1 FAIL (`setWhilePaused … Late` may pass: it does not depend on the order). Swap back.
2. `plugins/essentials2d/native/src/plugin.cpp`, the `propagateTransforms` registration: `app::OrderBand::Last` →
   `app::OrderBand::Late`. Run the Step 5 command: the Step 2 test FAILS (`worldXSeen` is 0). Swap back.

Then `git diff --stat` shows only the four files of this task, and
`grep -rn "OrderBand" --include=*.h --include=*.ts --include=*.cpp --include=*.hpp engine plugins` shows no `switch`
over `OrderBand` and no use outside `phase.hpp`, `engine.hpp`, `engine.cpp`, the tests, the two `essentials*` plugins
and the `doubling` fixture plugin.

- [ ] **Step 7: Run the whole suite and the format check**

Run: `./spur test`, then `./spur format-check`
Expected: every test passes; `format-check` exits 0 (if not, `./spur format <file>` on the files of this task).

Also: `grep -n "Default band runs before the Last band" engine/runtime/app/include/oly/app/*.hpp` prints nothing (two
lines before the change).

- [ ] **Step 8: Commit**

```bash
git add engine/runtime/app/include/oly/app/phase.hpp engine/runtime/app/include/oly/app/engine.hpp engine/runtime/app/tests/systems.cpp plugins/essentials2d/native/tests/transforms.cpp
git commit -m "feat(app): add the Late order band"
```

### Task 2: Documentation and changelog

**Files:**
- Modify: `docs/reference/glossary.md` (the order band row)
- Modify: `docs/reference/cpp/engine-and-systems.md` (`Phase` and `OrderBand`, `Engine::addSystem`)
- Modify: `docs/explanation/concepts/engine-and-systems.md` (`### Order bands`)
- Modify: `docs/explanation/architecture/app.md` (the phase list and `### Order bands`)
- Modify: `docs/explanation/architecture/plugins.md` (`## Order bands`)
- Modify: `docs/explanation/concepts/plugins.md` (`## Run after the game's systems`)
- Modify: `docs/explanation/architecture/decisions.md` (the order bands row)
- Modify: `docs/explanation/concepts/transforms.md` (`## When WorldTransform is up to date`)
- Modify: `docs/how-to/write-a-plugin.md` (the sentence that names the `Last` band for plugin systems)
- Modify: `CHANGELOG.md` (`## Unreleased`, `### Migration`)

**Interfaces:**
- Consumes: `OrderBand::Late` from Task 1.

What every page must say, in its own words (wording is the builder's, per the spec):

- The order: inside a phase, `Default`, then `Late`, then `Last`; inside a band, registration order. The enum in code
  blocks is `enum class OrderBand : uint8_t { Default, Late, Last };`.
- The rule for plugins: a system that must run after the game's systems of its phase and before the dimension plugin's
  propagation (`propagateTransforms`, `Last` of `Update`) registers in `Late`; one that must read the propagated
  `WorldTransform` registers in `Last`.
- Visibility: a `Late` system, like a `Last` one, sees the field writes of the earlier bands but not their deferred
  spawns, adds or removes; the phase applies its commands once, after `Last`.
- `concepts/engine-and-systems.md`: the band table gets a `OrderBand::Late` row between the two existing ones.
- `decisions.md`: the row names three bands (`Default`, `Late`, `Last`); its cost column drops "only two bands" and says
  that inside a band registration order is the only order, and an order outside the three bands (a physics plugin that
  needs a second propagation) waits for labels with before/after.
- `transforms.md`: the row reads "after the `Default` and `Late` systems of `Update`".
- `write-a-plugin.md`: the plugin system registers in the `Late` or the `Last` band, as the rule above says.
- Clients: the C API and TS still register in `Default` (the glossary row keeps "always `Default`").
- `gameplay-recipes.md` stays as is: the camera reads positions after movement, which is the `Last` case it shows.
- `CHANGELOG.md`, `### Migration`: replace `None.` with one entry: `OrderBand::Late` (`oly/app/phase.hpp`) runs between
  `Default` and `Last`; a plugin system that must run after the game's systems and before `propagateTransforms`
  registers there ([Order bands](docs/explanation/concepts/engine-and-systems.md#order-bands)); a game that does not use
  it keeps working as before. `### Breaking` stays `None.`

- [ ] **Step 1: Edit the ten files**

- [ ] **Step 2: Verify that no page still describes two bands**

Run:

```bash
grep -rn -e 'the `Default` band runs before the `Last` band' -e 'band runs first, then the `Last` band' \
  -e 'Default, Last }' -e 'only two bands' -e '(`Default`, `Last`)' -e '`Default` first, then `Last`' \
  -e 'after the `Default` systems of `Update`' docs --include=*.md | grep -v docs/superpowers
grep -L "Late" docs/reference/glossary.md docs/reference/cpp/engine-and-systems.md \
  docs/explanation/concepts/engine-and-systems.md docs/explanation/architecture/app.md \
  docs/explanation/architecture/plugins.md docs/explanation/concepts/plugins.md \
  docs/explanation/architecture/decisions.md docs/explanation/concepts/transforms.md \
  docs/how-to/write-a-plugin.md CHANGELOG.md
```

Expected: both print nothing. Before this task the first prints nine lines (`app.md` 2, `decisions.md`, `plugins.md`,
`transforms.md`, `cpp/engine-and-systems.md` 3, `glossary.md`) and the second lists every file.

- [ ] **Step 3: Check the links**

Every link added or changed resolves to an existing file and heading (`#order-bands` exists in
`docs/explanation/concepts/engine-and-systems.md`, `docs/explanation/architecture/app.md` and
`docs/explanation/architecture/plugins.md`).

- [ ] **Step 4: Commit**

```bash
git add CHANGELOG.md docs/reference docs/explanation docs/how-to/write-a-plugin.md
git commit -m "docs: document the Late order band"
```
