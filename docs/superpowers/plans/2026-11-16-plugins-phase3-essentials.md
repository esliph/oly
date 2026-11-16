# Plugins, phase 3 (`essentials`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every game gets, by its `DIMENSION`, an `essentials` plugin with `Position`, `Rotation`, `Scale`, `Parent`
and `WorldTransform`, a parent-child hierarchy with cascade destroy, and `arena` and `drift` drop their own `Position`.

**Architecture:** `essentials2d` and `essentials3d` are plugins under `plugins/`, each with its `.oly` and a C++ part.
At install, the plugin registers `Position` implies `WorldTransform`, observers that compute `WorldTransform` on add,
keep a children index from `Parent`, validate parents and cascade destroys, and a propagation system in the `Last` band
of `Update`. `oly_add_game` requires `DIMENSION 2d|3d` and turns the matching plugin on first. The 3D plugin is
validated by unit tests only.

**Tech Stack:** C++20, CMake, doctest, Google Benchmark, TypeScript (arena), Docker through `./spur`.

**Spec:** `docs/superpowers/specs/2026-11-16-plugins-design.md` (§8, §9, §10, §11 steps 4–6).
**Depends on:** `2026-11-16-plugins-phase1-observers.md` and `2026-11-16-plugins-phase2-mechanism.md` (merged first).

## Global Constraints

- The toolchain runs only in Docker, through `./spur <task>`. After each task: `./spur test`; from Task 3 on also
  `./spur native-games-test`, `./spur games-test` and `./spur windows-test` (smoke tests `drift.desktop` and
  `ts.arena.smoke` included).
- Spec §8.1, components (exact layouts, `.oly` field names decided here):

  | Component | `essentials2d` | `essentials3d` |
  |---|---|---|
  | `Position` | `{ x: f32, y: f32 }` | `{ x: f32, y: f32, z: f32 }` |
  | `Rotation` | `{ angle: f32 }` (radians) | `{ x: f32, y: f32, z: f32, w: f32 }` (unit quaternion) |
  | `Scale` | `{ x: f32, y: f32 }` | `{ x: f32, y: f32, z: f32 }` |
  | `Parent` | `{ entity: entity }` | same |
  | `WorldTransform` | `{ x: f32, y: f32, angle: f32, scaleX: f32, scaleY: f32 }` | `{ x: f32, y: f32, z: f32, rotationX: f32, rotationY: f32, rotationZ: f32, rotationW: f32, scaleX: f32, scaleY: f32, scaleZ: f32 }` |

- Spec §8.1: missing `Rotation` = identity; missing `Scale` = 1; `Position`, `Rotation`, `Scale` are local. `Velocity`
  stays out.
- Spec §8.2: re-parenting and un-parenting keep the local values; cascade destroy in every path (direct,
  `CommandBuffer`, state scope cleanup); `OLY_ASSERT` (operation, `Entity`, type name) for a `Parent` to a dead entity,
  to itself or to a descendant, checked in the on add observer of `Parent`.
- Spec §8.3: composition translate-rotate-scale with no shear: world scale = parent world scale × local scale
  (component-wise); world rotation = parent rotation ∘ local rotation; world position = parent position + parent
  rotation applied to (parent scale × local position). Propagation once per fixed step in the `Last` band of `Update`;
  roots copied per block; parents computed before their children.
- Spec §7.2: `DIMENSION` required, no default; the game cannot turn `essentials` off and does not list it; engine test
  games get no `essentials`; `fixture.oly` keeps its own `Position`.
- Anti-goal: a 2D game pays nothing for 3D. `essentials2d` includes no quaternion code.
- Spec §3 rules for plugins (public headers only, own target, `.oly` as an argument); the phase 2 lint enforces them,
  including "game names never appear in `plugins/`".
- New tests follow the `add-test-case` skill. Commits, comments and assert messages in English; commit subject one
  short line, conventional prefix, no spec, plan or task references. `.cpp`/`.hpp` formatted by the hook. Comments only
  for a reason or a contract.
- Public API changes update `docs/guide/` in the same PR (Task 6).

**New names (decided here):**

| Name | Where |
|---|---|
| `plugins/essentials2d/essentials2d.oly`, `plugins/essentials2d/native/{CMakeLists.txt,include/oly/essentials2d/plugin.hpp,src/*.cpp,tests/,bench/}` | 2D plugin (`oly::essentials2d`) |
| `plugins/essentials3d/...` with `include/oly/essentials3d/detail/quaternion.hpp` | 3D plugin (`oly::essentials3d`) |
| `plugins/CMakeLists.txt` | adds both plugins |
| `class Plugin final : public oly::app::Plugin` with `bool checkHierarchy(ecs::World& world, std::string* why = nullptr) const` | each `essentials` plugin |
| `detail::HasChildren` (tag, C++ only, not in the `.oly`) | each `essentials` plugin |
| system `propagateTransforms` (`Phase::Update`, `OrderBand::Last`) | each `essentials` plugin |
| `oly_add_game(... DIMENSION 2d|3d ...)`, `oly_dimension_plugin(<outputVariable> <dimension>)` | `cmake/OlyGame.cmake` |
| ctests `essentials2d.<suite>`, `essentials3d.<suite>`, `oly.cmake.dimension.<case>`; bench `essentials2d_bench_propagation` | |

**Decided here (not in the spec):**

- **Children index.** `std::unordered_map<uint64_t, std::vector<ecs::Entity>> children_`, keyed by
  `(uint64_t(index) << 32) | generation` of the parent. Cold path: it changes only in observers.
- **The parent's marker.** A parent gets `detail::HasChildren` (recorded through `context.commands()`) when it gets its
  first child, and loses it when its last child leaves. Its on remove observer cascades: if the index still lists
  children for that entity, the entity is being destroyed, so it records `destroy` for each child and erases the entry.
  When the plugin itself removes the marker the list is already empty and the observer does nothing. Cost: parents and
  non-parents live in different archetypes.
- **Observers.** `onAdd<Parent>`: validate (dead, self, cycle by walking `Parent` up from the new parent), add to the
  index, record `HasChildren` on the parent if it is its first child, mark the order dirty. `onRemove<Parent>` (remove,
  re-parent, destroy of the child): remove from the index; if the list empties and the parent is alive, record
  `remove<HasChildren>`; mark the order dirty. `onAdd<WorldTransform>`: compute the value at once from the local
  components and, for a child, the parent's `WorldTransform`. `onRemove<HasChildren>`: the cascade above.
- **A parent without `WorldTransform`** (an entity with no `Position`, used as a group) counts as the identity.
- **Propagation.** Roots: three per-block passes over entities without `Parent`: `Position → WorldTransform` (position,
  identity rotation, unit scale), then `Rotation`, then `Scale`, each with `Without<Parent>`. Children: a cached vector
  in parent-before-child order, rebuilt when dirty by a breadth-first walk from the index entries whose parent has no
  `Parent`; each child reads its parent's `WorldTransform` (random access) and its own locals (`get<Position>`,
  `tryGet<Rotation>`, `tryGet<Scale>`). A child without `Position` has no `WorldTransform` and is skipped, but its
  children use the identity for it.
- **Plugin destructor.** Removes its four observers (phase 2 contract). The implication cannot be removed and stays on
  the `World`; documented.
- **`DIMENSION` mapping.** `oly_dimension_plugin` maps `2d → essentials2d`, `3d → essentials3d`, anything else →
  `FATAL_ERROR "<value>: DIMENSION is 2d or 3d"`. `oly_add_game` puts the result first in the plugin list; listing
  `essentials2d` or `essentials3d` in `PLUGINS` is `FATAL_ERROR "<plugin> comes from DIMENSION; do not list it in
  PLUGINS"`.
- **The propagation benchmark** is added (spec §12 leaves it open): `BM_PropagateRoots/<N>` and
  `BM_PropagateChildren/<N>` (one parent per 8 children), in `plugins/essentials2d/native/bench/`. It reports the
  per-step cost with no target, in `docs/architecture/plugins.md`, dated.

## Review Focus

1. **A cascade through every path.** Expected: destroying a parent destroys its children and grandchildren the same way
   by direct `destroy`, by a `CommandBuffer` and by the state scope cleanup. Pinned in Task 1.
2. **A child spawned by a `CommandBuffer` at the end of `update`.** Expected: its `WorldTransform` is already right in
   `updateAfter` (not zero at the origin). Pinned in Task 1.
3. **A `Parent` and its parent's destroy in the same buffer**, in either recording order. Expected: `OLY_ASSERT` on the
   dead parent. A parent spawned in the same buffer is valid. Pinned in Task 1.
4. **Re-parenting an entity.** Expected: local values kept, the old parent loses `HasChildren` when it was its last
   child, the index matches (`checkHierarchy`). Pinned in Task 1.
5. **The migrated games.** Expected: `arena`'s TS keeps using `Position` from the generated schema, the module loads with
   the new hash, and both smoke tests pass. Pinned in Task 3.

## File Structure

| File | Responsibility |
|---|---|
| `plugins/CMakeLists.txt` (new), `CMakeLists.txt` | add `plugins/` after `engine/` and before `games/`, on native always and on WASM with games |
| `plugins/essentials2d/` (new) | the 2D plugin, tests and benchmark |
| `plugins/essentials3d/` (new) | the 3D plugin and tests |
| `cmake/OlyGame.cmake`, `cmake/tests/dimension.cmake` (new) | `DIMENSION` |
| `games/{arena,drift}/{*.oly,CMakeLists.txt}` | migration |
| `docs/`, `CLAUDE.md`, `README.md` | Task 6 |

---

### Task 1: `essentials2d`

**Files:**
- Create: `plugins/CMakeLists.txt`, `plugins/essentials2d/essentials2d.oly`, `plugins/essentials2d/native/CMakeLists.txt`, `plugins/essentials2d/native/include/oly/essentials2d/plugin.hpp`, `plugins/essentials2d/native/src/{plugin,observers,propagation}.cpp`, `plugins/essentials2d/native/tests/{transforms,hierarchy}.cpp`, `plugins/essentials2d/native/tests/contract_death.cpp`, `plugins/essentials2d/native/tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: phase 1 (`onAdd`, `onRemove`, `imply`, `context.commands()`), phase 2 (`oly_add_plugin`, `Plugin`,
  `installPlugin`, `OrderBand::Last`).
- Produces: `oly::essentials2d::Plugin` (`install`, `checkHierarchy`), the components of the table above in
  `oly/essentials2d/schema.generated.hpp`.

- [ ] **Step 1: Write the failing tests.** Each test builds a `World`, an `Engine`, calls
  `engine.installPlugin<oly::essentials2d::Plugin>()`, and ends with `CHECK(plugin.checkHierarchy(world, &why))` and
  `CHECK_INVARIANTS(world)`. Float checks use `doctest::Approx`. Register the suites with
  `oly_add_test(MODULE essentials2d ... SUPPORT engine/runtime/ecs/tests/support)` for `CHECK_INVARIANTS`; never the
  `app` support folder, whose `log.hpp` defines its own `Position`.

  `transforms.cpp`:
  ```cpp
  TEST_CASE("an entity with Position gets WorldTransform in the same spawn");  // archetypeCount grows by 1
  TEST_CASE("a root's WorldTransform copies its locals, with identity rotation and unit scale when missing");
  TEST_CASE("a child's WorldTransform composes translate-rotate-scale");
            // parent Position{10,0} Rotation{π/2} Scale{2,2}; child Position{1,0} Rotation{0.5} Scale{3,1}
            // → world x 10, y 2, angle π/2 + 0.5, scaleX 6, scaleY 2
  TEST_CASE("propagation runs after the Default systems of update, so updateAfter reads the new world values");
  TEST_CASE("a Position change in updateAfter reaches WorldTransform in the next step");
  TEST_CASE("a child spawned by a CommandBuffer during update has its WorldTransform in updateAfter");
  TEST_CASE("a parent without Position counts as the identity");
  TEST_CASE("non-uniform scale under rotation keeps no shear");     // the child's world scale is component-wise
  ```

  `hierarchy.cpp`:
  ```cpp
  TEST_CASE("re-parenting keeps the local values and moves the entity in the world");
  TEST_CASE("removing Parent makes the entity a root with its local values");
  TEST_CASE("the last child leaving removes the parent's marker");   // has<detail::HasChildren> false
  TEST_CASE("destroying a parent destroys its descendants (direct destroy)");
  TEST_CASE("destroying a parent destroys its descendants (CommandBuffer)");
  TEST_CASE("destroying a parent destroys its descendants (state scope cleanup)");
  TEST_CASE("a parent spawned in the same buffer as its child is valid");
  TEST_CASE("checkHierarchy reports a Parent written through get");  // world.get<Parent>(child).entity = other → false
  TEST_CASE("after the Engine is destroyed, the World keeps working without the plugin's observers");
  ```

  Death cases (`oly_add_contract_death(MODULE essentials2d ...)`): `parent_dead` (`"Parent"`, `"e="`, `"dead"`),
  `parent_self` (`"Parent"`, `"own parent"`), `parent_cycle` (`"Parent"`, `"descendant"`),
  `parent_destroyed_in_same_buffer` (record `add(child, Parent{p})` then `destroy(p)`, and in the other order a second
  case `parent_destroyed_before_in_same_buffer`; both → `"dead"`).

- [ ] **Step 2: Run to verify they fail.** `./spur test -R '^essentials2d\.'`. Expected: configure error (no
  `plugins/` yet) or the tests not found.
- [ ] **Step 3: Implement** as in "Decided here". `plugins/essentials2d/native/CMakeLists.txt`:
  `oly_add_plugin(NAME essentials2d SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/../essentials2d.oly DEPENDS ecs app core)` plus
  `tests/` and `bench/` under the usual options. The plugin keeps its observer ids and removes them in its destructor.
- [ ] **Step 4: Run.** `./spur test -R '^(essentials2d|oly\.lint)'`. Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "feat(plugins): add essentials2d with transforms and a parent hierarchy"`

### Task 2: Propagation benchmark

**Files:**
- Create: `plugins/essentials2d/native/bench/{CMakeLists.txt,propagation.cpp}`

- [ ] **Step 1:** Write `BM_PropagateRoots/<N>` (N roots with `Position`, `Rotation`, `Scale`) and
  `BM_PropagateChildren/<N>` (N/8 parents with 8 children each), N ∈ {1000, 100000}; each iteration runs one fixed step
  with only the plugin's system.
- [ ] **Step 2:** `PRESET=release ./spur build`, then
  `./spur dev ./build/release/bench/essentials2d_bench_propagation --benchmark_repetitions=10 --benchmark_report_aggregates_only=true`.
  Expected: it runs; note the medians (ns per entity) for Task 6.
- [ ] **Step 3: Commit** `git commit -m "bench(essentials2d): measure root and child propagation"`

### Task 3: `DIMENSION` and the game migration

**Files:**
- Create: `cmake/tests/dimension.cmake`
- Modify: `cmake/OlyGame.cmake`, `CMakeLists.txt` (the `oly.cmake.dimension.*` tests), `games/drift/drift.oly`, `games/drift/CMakeLists.txt`, `games/arena/arena.oly`, `games/arena/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 (`essentials2d`), phase 2 (`_oly_add_game_target`).
- Produces: `oly_add_game(... DIMENSION ...)`, `oly_dimension_plugin`.

- [ ] **Step 1: Write the failing tests:** `cmake/tests/dimension.cmake` cases `two_d` → `plugin=essentials2d`, `three_d`
  → `plugin=essentials3d`, `invalid` → `DIMENSION is 2d or 3d`.
- [ ] **Step 2: Run to verify they fail.** `./spur test -R 'oly\.cmake\.dimension'`. Expected: the cases fail.
- [ ] **Step 3: Implement** `DIMENSION` (see "Decided here"); a missing `DIMENSION` is
  `FATAL_ERROR "<target>: oly_add_game needs DIMENSION 2d|3d"`. Migrate: `DIMENSION 2d` in both games; remove
  `component Position` from `drift.oly` and `arena.oly`. No C++ or TS source of the games changes.
- [ ] **Step 4: Run.** `./spur test`, `./spur native-games-test`, `./spur games-test`, `./spur windows-test`. Expected:
  all pass, `drift.desktop` and `ts.arena.smoke` included.
- [ ] **Step 5: Commit** `git commit -m "feat(build): turn essentials on from each game's DIMENSION"`

### Task 4: `essentials3d`

**Files:**
- Create: `plugins/essentials3d/essentials3d.oly`, `plugins/essentials3d/native/{CMakeLists.txt,include/oly/essentials3d/plugin.hpp,include/oly/essentials3d/detail/quaternion.hpp,src/*.cpp,tests/*.cpp,tests/CMakeLists.txt}`
- Modify: `plugins/CMakeLists.txt`

**Interfaces:**
- Produces: `oly::essentials3d::Plugin`, same shape as the 2D one. `detail/quaternion.hpp`: `multiply(a, b)`,
  `rotate(q, x, y, z)` on a plain `{ x, y, z, w }` struct; internal to the plugin.

- [ ] **Step 1: Write the failing tests:** the 2D suites translated to 3D where the case is not dimension-specific
  (implication, root identity, cascade in the three paths, re-parent, death cases), plus:

  ```cpp
  TEST_CASE("quaternion composition: two 90° turns about z equal one 180° turn");
  TEST_CASE("a child's position is rotated by the parent's quaternion");
            // parent Rotation = 90° about z, child Position{1,0,0} → world (0,1,0)
  TEST_CASE("a missing Rotation is the identity quaternion");
  ```

- [ ] **Step 2: Run to verify they fail.** `./spur test -R '^essentials3d\.'`. Expected: tests not found.
- [ ] **Step 3: Implement** the 3D plugin with the composition of the Global Constraints. The quaternion helpers stay in
  `detail/`.
- [ ] **Step 4: Run.** `./spur test` and `./spur windows-test`. Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "feat(plugins): add essentials3d with quaternion transforms"`

### Task 5: Reviewer pass

- [ ] **Step 1:** Run the `spec-conformance-reviewer` agent on the phase diff against spec §8 and §9, and the
  `perf-reviewer` agent on the propagation system and the observers. Fix what they confirm.
- [ ] **Step 2:** `./spur test`, `./spur native-games-test`, `./spur games-test`, `./spur windows-test`. Expected: all
  pass.
- [ ] **Step 3: Commit** the fixes, if any, with a subject naming what changed.

### Task 6: Docs

**Files:**
- Create: `docs/guide/concepts/transforms.md` (local vs world, hierarchy, cascade, the `updateAfter` timing, no shear)
- Modify: `docs/architecture/plugins.md` (`essentials`: components, implication, observers, index and marker,
  propagation, the benchmark medians with date and machine), `docs/architecture/{README,games,limits,decisions,glossary,repository,ecs}.md`, `docs/guide/{README,cheat-sheet,recipes,troubleshooting}.md`, `docs/guide/concepts/{schema,world-and-entities}.md`, the getting-started pages of both environments (`DIMENSION`), `docs/development/guide-style.md` (`DIMENSION` in the build surface), `README.md`, `CLAUDE.md`

- [ ] **Step 1:** Read `docs/development/guide-style.md`; write the pages above. Every guide example that declared
  `component Position` in a game `.oly` drops it and shows `DIMENSION 2d`.
- [ ] **Step 2:** Reword "2D game engine" in `CLAUDE.md`, `README.md` and `docs/architecture/README.md` so it stays true
  with `essentials3d` (for example "game engine in C++20 for 2D games, with 3D transforms in `essentials3d`"); keep
  "study and benchmark project" (that wording belongs to the publishing project). Add `plugins/` and its rules to the
  structure in `CLAUDE.md`.
- [ ] **Step 3:** Search `docs/guide/` for `component Position` and for `Position` used as a world position; update.
- [ ] **Step 4:** `./spur test`. Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "docs: describe essentials, transforms and DIMENSION"`
