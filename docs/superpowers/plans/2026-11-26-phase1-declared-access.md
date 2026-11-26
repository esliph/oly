# Phase 1 — declared access and system sets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A system declares what it reads and writes through its parameters, runs in a phase that plugins can add,
and is ordered by `before`/`after` constraints and its name only; registration refuses unordered conflicting systems
and constraint cycles, and the Enxame keeps the world hash of phase 0.

**Architecture:** `ecs` gains resources owned by the `World`. `app` gains typed system parameters (`Query`, `Lookup`,
`Res`, `Commands`, `Events`, `Tasks`, `StepInfo`, `SystemInfo`, `ecs::World&`) read by a compile-time adapter that
turns a function pointer into a `SystemDescriptor` (declared access, one-time resolution, one fixed-signature
trampoline). The `Engine` keeps phases, sets and constraints, and when the first `frame` call closes registration a
pure scheduling pass (`detail::buildSchedule`) orders the phases, orders each phase topologically with ties broken by
name, rejects cycles and access conflicts, and the `Engine` resolves every parameter once. A transitional bridge keeps
the old `SystemContext` API working, as a chain of exclusive systems, until every consumer has moved; the last engine
task deletes it.

**Tech Stack:** C++20, CMake, doctest, TypeScript (client and WASM tests), POSIX sh, Docker through `./spur`.

**Spec:** `docs/superpowers/specs/2026-11-26-phase1-declared-access-design.md` (called *the spec*), phase 1 of
`docs/superpowers/specs/2026-11-24-aaa-roadmap-design.md` (called *the roadmap*). Read both before any task.

## Decisions this plan adds to the spec

The spec leaves these to the builder (§7) or does not cover them. Each is fixed here so that every task uses the same
names.

- **`physics2d` takes over nothing in phase 1** [user decision, 2026-11-26]: it declares the `Physics` phase and has no
  system. Taking over the horde's separation and integration needs a velocity column (new work under roadmap rule 7)
  and moving the projectile integration changes `Projectile`'s bytes, which the world hash folds. `limits.md` records it.
- **`SystemInfo` is one more parameter kind** (`const SystemInfo&`: id, name, phase, schedule). It declares nothing. The
  C API needs the running system's id to call the script, and the `app` tests need the name; neither fits `StepInfo`,
  which is constant for a whole phase.
- **`StepInfo` is a plain aggregate** filled once per phase run: `time`, `fixedDeltaTime`, `interpolation`, `state`,
  `paused`. `paused` is the pause request read when the phase starts, so a `pause()` made by a system of the same phase
  does not change what its neighbours see.
- **`Commands::isAlive`** answers liveness: the entity store changes only at the phase's flush, so reading it declares
  nothing.
- **Engine controls are resources owned by the `Engine`:** `app::StateRequest` (`request`), `app::PauseRequest`
  (`pause`, `resume`, `requested`), `input::ActionMap` and `app::Scheduler` (for `cancel`, `isPending`, `persist`).
  `Tasks` appends only (`after`, `every`, `schedule`) and declares nothing. The `Engine` resolves a resource id against
  its controls first, then against the `World`.
- **Registration closes on the first `frame` call** for every registration, systems included (today `addSystem` only
  refuses calls during a frame). A later `addSystem`, `addPhase`, `set<S>()` or constraint is an `OLY_ASSERT`.
- **Phases and sets share one key space**, `scheduleKey<T>()`, dense per process like `pluginKey`; a type used as both
  is an error. A system function is identified by `systemFunctionKey<F>()`, dense per process.
- **Errors at close** are collected as strings by `detail::buildSchedule` and by resolution, then raised by one
  `OLY_ASSERT` that prints all of them. Without asserts, a cycle's systems run in name order and a conflict is ignored.
- **Lookup of a tag:** `Lookup<const Tag>` gives `has` only; `Lookup<Tag>` is a `static_assert`.
- **`OLY_SYSTEM(name);` follows the function's definition** (it no longer opens the body), and registers
  `systemDescriptor<&name>()` by name. The Enxame stops using it: native code calls `addSystem<&f>` directly.

## Global Constraints

- Toolchain only in Docker through `./spur` from Git Bash; there is no C++ compiler on the host. Git runs on the host.
- Code follows `docs/reference/contributor/code-style.md` and the project copy of the style skill in
  `.claude/skills/code-style/` (in this repository a blank line separates a block's preparation from the block). The
  Enxame follows the same rules.
- Glossary names only, no aliases or compatibility `using` with old names. Game names never in `engine/` or `plugins/`;
  `engine/` never depends on `plugins/`; the engine never names `games/`.
- No 2D-only type or rule in `app` or `ecs` (roadmap rule 1). `essentials3d`'s tests stay green in every task.
- New tests use the `add-test-case` skill. Every `app` and plugin test ends with `CHECK_INVARIANTS(...world)` after
  its last frame, as the existing suites do; a death case is listed in the `CASES` of its module's `contract_death`.
- `OLY_ASSERT` messages name the operation, the systems and the type (code-style.md). Close-time messages start with
  `frame: ` because the first `frame` call raises them.
- A task that changes `world` (Task 2) keeps `checkInvariants` covering the change and runs the
  `spec-conformance-reviewer` and `perf-reviewer` agents before its commit.
- A public API change updates `docs/` and `CHANGELOG.md` `## Unreleased` in the same commit, following
  `docs/reference/contributor/docs-style.md`. Until Task 13, new calls go under `### Migration`; Task 13 writes
  `### Breaking`.
- Engine test command: `./spur test` (preset `debug-asan`); a subset with `./spur test -R '<regex>'`. Expected end:
  `100% tests passed`.
- The game test command, which builds and tests the Enxame in local mode:
  `./spur dev sh -c 'cd games/enxame && OLY_ENGINE_DIR=/app OLY_IN_DOCKER=1 sh ./oly test'`. Expected end:
  `100% tests passed`.
- Format check, from Task 2 on: `./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')`.
- Every shell block in a step is self-contained: it starts at the repository root and defines its own variables,
  because each step may run in a new shell.
- Commits: English, conventional prefix, short subject, no spec or plan citation. Each message ends with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. One function registered under two names. Expected: `after<&f>()` orders both registrations, `after("name")` only
   the named one. Test: Task 4, "a function constraint orders every registration of the function".
2. A plugin's phase constraint that names a phase another plugin declares later. Expected: the same phase order
   whichever plugin installs first. Test: Task 4, "phase order does not depend on which plugin declares first".
3. Two `Engine`s over one `World`. Expected: each system's `Res<StateRequest>` and `Res<PauseRequest>` reach its own
   `Engine`; `World` resources are shared. Test: Task 4, "engine controls resolve to the system's own Engine".
4. A system that throws in the middle of a phase. Expected: the next frame recovers as today, registration does not
   close a second time, and the dispatch order and resolved queries stay valid. Test: Task 4, "a frame after an
   exception keeps the closed schedule".
5. A set with members in two phases. Expected: `set<S>().before<&f>()` orders the members of each phase against `f`
   where `f` shares the phase, and is accepted where the phases agree. Test: Task 4, "a set orders its members in each
   phase".

## File map

| Path | Task | Responsibility |
|---|---|---|
| `games/enxame/native/headless.cpp`, `games/enxame/cmake/game.cmake`, `games/enxame/tests/check_hash.cmake` (new), `games/enxame/tests/replay_determinism.cmake` | 1 | world hash of every scenario, recorded |
| `engine/runtime/ecs/include/oly/ecs/resource.hpp` (new), `ecs/src/resource.cpp` (new), `ecs/include/oly/ecs/world.hpp`, `ecs/src/world.cpp`, `ecs/src/diagnostics.cpp`, `ecs/tests/resources.cpp` (new), `ecs/tests/contract_death.cpp`, `ecs/tests/CMakeLists.txt` | 2 | resources |
| `engine/runtime/app/include/oly/app/{access,system_parameters,system_descriptor}.hpp` (new), `app/include/oly/app/detail/system_adapter.hpp` (new), `app/include/oly/app/phase.hpp`, `app/tests/system_parameters.cpp` (new), `app/tests/negative/*.cpp` (new), `app/tests/CMakeLists.txt` | 3 | parameters and adapter |
| `app/include/oly/app/{engine,phase,timing,controls,order}.hpp`, `app/include/oly/app/detail/schedule_graph.hpp` (new), `app/src/{engine,schedule_graph,controls}.cpp`, `app/tests/system_order.cpp` (new), `app/tests/timing.cpp`, `app/tests/contract_death.cpp`, `games/enxame/native/headless.cpp` | 4 | phases, sets, order, dispatch, bridge |
| `app/src/schedule_graph.cpp`, `app/tests/system_order.cpp`, `app/tests/contract_death.cpp` | 5 | conflicts and constraint checks |
| `plugins/essentials2d/native/**`, `plugins/essentials3d/native/**` | 6 | propagation phase, hierarchy resource |
| `plugins/physics2d/native/**` (new), `plugins/CMakeLists.txt` | 7 | `Physics` phase |
| `games/enxame/native/*`, `games/enxame/desktop/*`, `games/enxame/cmake/game.cmake`, `games/enxame/tests/check_phase_order.cmake` (new) | 8 | Enxame on the new API |
| `engine/runtime/app/tests/{loop,systems,pause,recovery,timing,plugins,states,state_scope}.cpp`, `app/tests/support/log.hpp` | 9 | `app` tests, part 1 |
| `engine/runtime/app/tests/{events,events_loop,actions,schedule,contract_death}.cpp` | 10 | `app` tests, part 2 |
| `engine/testing/fixture/plugins/{counting,doubling}/**`, `engine/testing/fixture/plugins/native/plugins_test.cpp` | 11 | fixture plugins, install order |
| `engine/runtime/app/include/oly/app/system.hpp`, `app/src/system.cpp`, `engine/runtime/capi/**`, `engine/testing/fixture/systems.cpp`, `engine/environments/web/clients/typescript/core/src/{engine,bridge,module}.ts`, `engine/testing/tests/wasm/*.ts`, `templates/game/**` | 12 | C API, named systems, TS, template |
| `engine/environments/native/host/**`, every remaining legacy use, `docs/**`, `CHANGELOG.md` | 13 | remove the bridge, documentation |
| `docs/reference/benchmarks/enxame/` | 14 | cost of dispatch, recorded |

---

### Task 1: Every Enxame scenario records its world hash

The instrument comes before any engine change (roadmap rule 5). Today only S5 writes `worldHash`
(`measuresOf` in `games/enxame/native/headless.cpp`). The values recorded here are the phase 0 values: the engine has
not changed since `5eec212` (`git log --oneline 5eec212..HEAD -- engine plugins` lists no commit).

**Files:**
- Modify: `games/enxame/native/headless.cpp` (`measuresOf`)
- Create: `games/enxame/tests/check_hash.cmake`
- Modify: `games/enxame/tests/replay_determinism.cmake`, `games/enxame/cmake/game.cmake`

**Interfaces:**
- Produces: `measures.worldHash` (string `0x%016llx`) in the result JSON of every runnable scenario, and the ctest
  tests `enxame.hash.S1`, `.S2`, `.S3`, `.S6`, `.S8` and `enxame.replay_determinism` checking recorded values. Task 8
  must keep them passing unchanged.

- [ ] **Step 1: Write `check_hash.cmake`**

It runs `COMMAND` (a `;` list) with `--out ${OUT}`, requires exit 0, reads `measures.worldHash` and fails with
`world hash of ${SCENARIO} is <value>, expected ${HASH}` when it differs. Same structure as `check_run.cmake`.

- [ ] **Step 2: Register the hash tests with a placeholder value, and make the replay test check a value**

In `game.cmake`, add `enxame_check_hash(<scenario> <hash> <arguments...>)`, a function shaped like `enxame_check_run`.
Register one test per runnable scenario at its first frozen size and 120 steps (S8 completes two waves in 120 steps):

```cmake
enxame_check_hash(S1 0x0 --headless --scenario S1 --size 1000 --steps 120 --commit test)
enxame_check_hash(S2 0x0 --headless --scenario S2 --size 50000 --steps 120 --commit test)
enxame_check_hash(S3 0x0 --headless --scenario S3 --size 10000 --steps 120 --commit test)
enxame_check_hash(S6 0x0 --headless --scenario S6 --size 1000 --steps 120 --commit test)
enxame_check_hash(S8 0x0 --headless --scenario S8 --size 2000 --steps 120 --commit test)
```

`replay_determinism.cmake` gains `-DHASH=...` (also `0x0` for now) and fails when run `a`'s hash differs from it, after
the existing a/b comparison. S5 is covered by the replay test (2500 steps, with clicks).

- [ ] **Step 3: Run the game test command to verify the new tests fail**

Expected: `enxame.hash.*` and `enxame.replay_determinism` fail with `measures.worldHash` missing (S1, S2, S3, S6, S8)
or `expected 0x0` (S5).

- [ ] **Step 4: Write the hash for every scenario**

In `measuresOf`, every scenario's object starts with `"worldHash": "0x%016llx"` from `worldHash(world)`; S5 keeps
`enemiesLeft` and `turrets`, the others keep their measures after it, and S1 gets `{ "worldHash": ... }` instead of
`{}`. Grow the `text` buffers if a measure no longer fits in 200 bytes (`snprintf` truncates silently).

- [ ] **Step 5: Record the values**

Run the game test command once. Each failing hash test prints the actual value (`world hash of S1 is 0x...`). Copy
each one into its `enxame_check_hash` line and into the replay test's `HASH`. These are the phase 0 values.

- [ ] **Step 6: Run the game test command to verify it passes**

Expected: `100% tests passed`, including `enxame.hash.S1` … `enxame.hash.S8` and `enxame.replay_determinism`.

- [ ] **Step 7: Commit**

```bash
git add games/enxame/native/headless.cpp games/enxame/cmake/game.cmake games/enxame/tests/check_hash.cmake games/enxame/tests/replay_determinism.cmake
git commit -m "test(enxame): record the world hash of every scenario" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Resources in the `World`

**Files:**
- Create: `engine/runtime/ecs/include/oly/ecs/resource.hpp`, `engine/runtime/ecs/src/resource.cpp`
- Modify: `engine/runtime/ecs/include/oly/ecs/world.hpp`, `engine/runtime/ecs/src/world.cpp`,
  `engine/runtime/ecs/src/diagnostics.cpp` (or wherever `World::checkInvariants` lives)
- Create: `engine/runtime/ecs/tests/resources.cpp`; Modify: `engine/runtime/ecs/tests/CMakeLists.txt`,
  `engine/runtime/ecs/tests/contract_death.cpp`
- Docs: `docs/reference/cpp/world-and-entities.md` (new section "Resources"), `docs/reference/glossary.md` (row
  "resource"), `CHANGELOG.md` (`### Migration`)

**Interfaces:**
- Produces (namespace `oly::ecs`):

```cpp
using ResourceId = uint32_t;

// Dense, assigned on the first call for each type; ignores const, volatile and reference. Not for persisting.
template <class T>
ResourceId resourceId();
std::string_view resourceName(ResourceId id);
uint32_t registeredResourceCount() noexcept;

class World {
 public:
  // One object per type, owned by the World, with an address that never changes until ~World, which destroys the
  // resources in reverse insertion order. Inserting a type twice is a contract error (OLY_ASSERT).
  template <class T>
  std::remove_cvref_t<T>& insertResource(T&& value);
  template <class T>
  T& resource();  // OLY_ASSERT when not inserted
  template <class T>
  T* tryResource() noexcept;
  void* tryResourceById(ResourceId id) noexcept;
};
```

The id counter mirrors `componentId` (a function-local static per type in `detail`); `resourceName` comes from
`core::typeName<U>()`. Storage: a vector indexed by `ResourceId` of owning slots (object pointer, destroy function),
plus the insertion order for destruction. A resource is not a component: it has no archetype, no observer and is
never in a query.

- [ ] **Step 1: Write the failing tests in `resources.cpp`** (new suite `ecs.resources`)

```cpp
TEST_CASE("an inserted resource is reachable by type and by id") {
  oly::ecs::World world;
  Score& inserted = world.insertResource(Score{7});

  Score& reached = world.resource<Score>();

  CHECK(&reached == &inserted);
  CHECK(reached.value == 7);
  CHECK(world.tryResource<const Score>() == &inserted);
  CHECK(world.tryResourceById(oly::ecs::resourceId<Score>()) == &inserted);
  CHECK_INVARIANTS(world);
}
```

Also: "a resource keeps its address when other resources are inserted" (insert 20 distinct types after `Score`, the
first address is unchanged); "a missing resource is nullptr" (`tryResource`, `tryResourceById` with an id never
inserted and with an id beyond the registry); "resources are destroyed in reverse insertion order" (two non-trivial
types that push their name into a shared `std::vector<std::string>` in their destructors; after the `World`'s scope:
`{"second", "first"}`); "resourceId ignores const and reference" (`resourceId<const Score&>() == resourceId<Score>()`);
"resourceName is the type name" (`resourceName(resourceId<Score>())` contains `Score`).

- [ ] **Step 2: Add the death cases**

`insert_resource_twice` (required substrings: `insertResource`, `Score`, `already inserted`) and `resource_not_inserted`
(`resource`, `Score`, `not inserted`), listed in the `CASES` of `ecs/tests/CMakeLists.txt`.

- [ ] **Step 3: Run to verify they fail**

Run: `./spur test -R '^ecs\.(resources|contract_death\.(insert_resource_twice|resource_not_inserted))'`
Expected: the build fails on the missing `insertResource` (the suite does not compile yet).

- [ ] **Step 4: Implement `resource.hpp`, the `World` members and `checkResources`**

`checkInvariants` gains `checkResources`: every non-empty slot `i` holds an object and records id `i`, and the
insertion order lists each non-empty slot exactly once.

- [ ] **Step 5: Run to verify they pass**

Run: `./spur test -R '^ecs\.'`
Expected: `100% tests passed`.

- [ ] **Step 6: Review the `world` change**

Dispatch the `spec-conformance-reviewer` and `perf-reviewer` agents on the diff; fix what they confirm. Expected from
`perf-reviewer`: no change on the hot paths (`spawn`, `add`, `flush`, iteration), since resources touch none of them.

- [ ] **Step 7: Docs, format check, commit**

`world-and-entities.md` gets a "Resources" section (contract above: ownership, address stability, destruction order,
insert-once, not a component). Glossary row: "resource: one object per type owned by the `World`" with the C++ names,
`—` for C API and TS. `CHANGELOG.md` `### Migration`: a line for `World::insertResource`/`resource`/`tryResource`,
ending "A game keeps working without them."

```bash
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add engine/runtime/ecs docs/reference/cpp/world-and-entities.md docs/reference/glossary.md CHANGELOG.md
git commit -m "feat(ecs): add resources owned by the World" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: System parameters and the adapter

No `Engine` change: this task builds what a function's signature declares and how each parameter is resolved and
fetched, and tests it against hand-built contexts.

**Files:**
- Create: `engine/runtime/app/include/oly/app/access.hpp`, `.../system_parameters.hpp`, `.../system_descriptor.hpp`,
  `.../detail/system_adapter.hpp`
- Modify: `engine/runtime/app/include/oly/app/phase.hpp` (add `PhaseId`, `ScheduleKind`)
- Create: `engine/runtime/app/tests/system_parameters.cpp`, `engine/runtime/app/tests/negative/{unknown_parameter,write_through_const_query,write_through_const_resource}.cpp`
- Modify: `engine/runtime/app/tests/CMakeLists.txt`
- Docs: `docs/reference/cpp/engine-and-systems.md` (new section "System parameters"), `CHANGELOG.md`

**Interfaces:**
- Consumes: `ecs::ResourceId`, `World::tryResourceById` (Task 2); `detail::EventQueues` (`event_queues.hpp`),
  `Scheduler`, `StateId`, `SystemId` (existing).
- Produces:

```cpp
namespace oly::app {
  using PhaseId = uint32_t;  // phase.hpp
  enum class ScheduleKind : uint8_t { FixedStep, Frame };

  // access.hpp. Each list sorted, without repetition; a write of a type removes its read.
  struct Access {
    std::vector<ecs::ComponentId> componentReads, componentWrites;
    std::vector<ecs::ResourceId> resourceReads, resourceWrites;
    bool exclusive = false;

    void readComponent(ecs::ComponentId id);
    void writeComponent(ecs::ComponentId id);
    void readResource(ecs::ResourceId id);
    void writeResource(ecs::ResourceId id);
  };

  // system_parameters.hpp
  struct StepInfo {
    double time;
    float fixedDeltaTime;
    float interpolation;
    StateId state;
    bool paused;
  };
  struct SystemInfo {
    SystemId id;
    std::string_view name;
    PhaseId phase;
    ScheduleKind schedule;
  };

  template <class... Ts> class Query;   // Ts: T or const T, plus ecs::With<...> / ecs::Without<...> in any position.
                                        // each, eachBlock, single, trySingle, matchedCount, as ecs::Query.
  template <class T> class Lookup;      // get(Entity) -> T&, tryGet(Entity) -> T*, has(Entity) const; a tag: has only.
  template <class T> class Res;         // get() -> T&, operator*, operator->.
  class Commands;                       // spawn(), spawn(Ts&&...), destroy, add, remove<T>, addById, removeById,
                                        // persist, isAlive(Entity) const. Scoped spawn as SystemContext::spawn.
  template <class E> class Events;      // emit(const E&)
  class Tasks;                          // schedule, after, every: the Scheduler's overloads; no cancel.

  // system_descriptor.hpp
  using SystemFunctionKey = uint32_t;
  template <auto Function>
  SystemFunctionKey systemFunctionKey();  // dense per process

  struct SystemDescriptor {
    void (*declare)(Access& access);
    void (*resolve)(detail::ResolveContext& context, detail::SystemSlots& slots);
    void (*run)(const detail::SystemSlots& slots, detail::RunContext& context);
    SystemFunctionKey key;
  };

  // Function: a function pointer (a captureless lambda passes as +[](...) {}). One instance per Function.
  template <auto Function>
  const SystemDescriptor& systemDescriptor();
}

namespace oly::app::detail {
  inline constexpr uint32_t MaxSystemParameters = 16;
  using SystemSlots = std::array<void*, MaxSystemParameters>;

  // The Engine's own resources (StateRequest, PauseRequest, ActionMap, Scheduler), looked up before the World's.
  struct EngineControls {
    std::array<std::pair<ecs::ResourceId, void*>, 4> entries;
    void* find(ecs::ResourceId id) const noexcept;
  };

  struct ResolveContext {
    ecs::World& world;
    const EngineControls& controls;
    EventQueues& queues;
    Scheduler& scheduler;
    std::string_view systemName;
    std::vector<std::string>& errors;
  };

  struct RunContext {
    ecs::World& world;
    ecs::CommandBuffer& commands;
    EventQueues& queues;
    const StepInfo& step;
    const SystemInfo& system;
    bool spawnsScoped;
  };

  // One specialization per parameter kind: declare(Access&), resolve(ResolveContext&) -> void*,
  // fetch(void* slot, RunContext&) -> P. The primary template is a static_assert.
  template <class P>
  struct SystemParameter;
}
```

What each kind declares, resolves into its slot and fetches:

| Parameter | `declare` | slot (`resolve`) | `fetch` |
|---|---|---|---|
| `Query<...>` | read of each `const T`, write of each `T`; filters nothing | `&world.query<Components...>(filters...)` | wraps the `ecs::Query&` |
| `Lookup<const T>` / `Lookup<T>` | read / write of `T` | `&world` | wraps the `World&` |
| `Res<const T>` / `Res<T>` | read / write of resource `T` | `controls.find(id)`, else `world.tryResourceById(id)`; missing: error | wraps the `T*` |
| `Commands` | nothing | unused | `commands`, `spawnsScoped` |
| `Events<E>` | nothing | the `EventTypeId` of `E` (in the pointer); undeclared: error | `queues`, the type |
| `Tasks` | nothing | `&scheduler` | wraps the `Scheduler&` |
| `const StepInfo&` / `const SystemInfo&` | nothing | unused | `context.step` / `context.system` |
| `ecs::World&` | `exclusive = true` | `&world` | the `World&` |

Resolution errors, exact text (Task 4 prefixes them with `frame: `):
`system "<name>" needs resource "<resourceName>", which is not inserted` and
`system "<name>" emits event "<typeName>", which is not declared`.

The unknown-parameter `static_assert` lives in the primary `SystemParameter<P>`, instantiated from
`systemDescriptor<Function>()`, so the compiler's instantiation context names both the parameter type and the
function; its literal text is `addSystem: this parameter type is not a system parameter (Query, Lookup, Res, Commands,
Events, Tasks, const StepInfo&, const SystemInfo&, ecs::World&)`. More than `MaxSystemParameters` parameters is a
`static_assert` too. `run` is the trampoline: one `fetch` per parameter, then the call.

- [ ] **Step 1: Write the failing access test**

```cpp
namespace {
  void sample(Query<Position, const Velocity, oly::ecs::With<Enemy>>, Lookup<const Health>, Res<Score>, Res<const Config>, Commands, Events<Ping>, Tasks, const StepInfo&, const SystemInfo&) {}
}

TEST_CASE("a system declares the access of its parameters") {
  Access access;

  oly::app::systemDescriptor<&sample>().declare(access);

  CHECK(access.componentWrites == std::vector{oly::ecs::componentId<Position>()});
  CHECK(access.componentReads == sorted({oly::ecs::componentId<Velocity>(), oly::ecs::componentId<Health>()}));
  CHECK(access.resourceWrites == std::vector{oly::ecs::resourceId<Score>()});
  CHECK(access.resourceReads == std::vector{oly::ecs::resourceId<Config>()});
  CHECK_FALSE(access.exclusive);
}
```

`sorted` and the component, resource and event types (`Score`, `Config`, `Health`, `Enemy`, `Ping`, `Seen`) are local
to the suite. Also: "a World parameter makes the system exclusive"; "a write of a type
supersedes its read" (`Query<const Position>` plus `Lookup<Position>`: `Position` only in `componentWrites`); "a filter
declares nothing" (`Query<const Position, oly::ecs::With<Enemy>>`: `Enemy` in no list); "two functions have two keys
and one function one key".

- [ ] **Step 2: Write the failing resolve-and-run tests**

Build a `World`, an `EventQueues` with `Ping` declared, an old-API `Engine` (only for its `schedule()`), the contexts,
then `resolve` and `run`:
- "run hands the system the entities of its query, the inserted resource and the step" (the system writes what it saw
  into a `Res<Seen>`; checks the entity count, `Score` value, `step.time`);
- "Commands defer until the flush and spawn scoped while a state is active" (after `run` the `World` has no new
  entity; after `world.flush(commands)` it has one, with `oly::app::detail::StateScoped` when `spawnsScoped`);
- "Events push to the queue of their type" (`queues.beginRound()` then `batchCount` is 1);
- "a missing resource and an undeclared event are reported with the system's name" (`errors` equals the two exact
  strings above, for system name `"needy"`).
Each test ends with `CHECK_INVARIANTS(world)`.

- [ ] **Step 3: Write the negative compile cases and their CMake wiring**

Each file holds one system the adapter must refuse, passed to `systemDescriptor<&f>()`:
- `unknown_parameter.cpp`: a parameter `int`.
- `write_through_const_query.cpp`: `Query<const Position>` whose `each` assigns `position.x`.
- `write_through_const_resource.cpp`: `Res<const Score>` whose body assigns `score->value`.

In `app/tests/CMakeLists.txt`, for each case: an `EXCLUDE_FROM_ALL` object library linked to `oly::app`, and a test
`app.negative_compile.<case>` running `${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target <library>` with
`PASS_REGULAR_EXPRESSION` (the exit code is ignored, so the test passes only when the expected error is printed):
- `unknown_parameter`: `is not a system parameter`
- the two writes: `read-only|const-qualified` (GCC says "assignment of member ... in read-only object", Clang "cannot
  assign to ... with const-qualified type"; the dev image's GCC and the `native-clang` preset both print one of them).

Register the suite `oly_add_test(MODULE app NAME system_parameters SOURCES system_parameters.cpp SUPPORT engine/runtime/ecs/tests/support)`.

- [ ] **Step 4: Run to verify they fail**

Run: `./spur test -R '^app\.(system_parameters|negative_compile)'`
Expected: the build fails on the missing headers.

- [ ] **Step 5: Implement the four headers**

- [ ] **Step 6: Run to verify they pass, then check each negative case fails for the right reason**

Run: `./spur test -R '^app\.(system_parameters|negative_compile)'`
Expected: `100% tests passed`. Then make each negative file valid in turn (remove the assignment, or turn `int` into
`Commands`) and rerun its test: it must fail (no error printed). Restore the file.

- [ ] **Step 7: Docs, format check, commit**

`engine-and-systems.md` gets "System parameters": the table above (what each declares), `StepInfo`/`SystemInfo`
fields, the exclusive system, and that a parameter of any other type does not compile. `CHANGELOG.md` `### Migration`:
one line pointing at it, ending "A game keeps working without them."

```bash
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add engine/runtime/app docs/reference/cpp/engine-and-systems.md CHANGELOG.md
git commit -m "feat(app): declare a system's access through its parameters" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Phases, sets, constraints and the dispatch order

**Files:**
- Modify: `engine/runtime/app/include/oly/app/phase.hpp`, `.../engine.hpp`, `.../timing.hpp`, `.../system.hpp`
  (`SystemContext::phase()` maps back to the legacy enum)
- Create: `engine/runtime/app/include/oly/app/order.hpp`, `.../controls.hpp`, `.../detail/schedule_graph.hpp`,
  `engine/runtime/app/src/schedule_graph.cpp`, `engine/runtime/app/src/controls.cpp`
- Modify: `engine/runtime/app/src/engine.cpp`, `engine/runtime/app/src/engine_states.cpp` (state checks reused by
  `StateRequest`)
- Create: `engine/runtime/app/tests/system_order.cpp`; Modify: `engine/runtime/app/tests/timing.cpp`,
  `engine/runtime/app/tests/contract_death.cpp`, `engine/runtime/app/tests/CMakeLists.txt`
- Modify: `games/enxame/native/headless.cpp` (`phaseName` and the S3 flush lookup use the new timing fields)
- Docs: `docs/reference/cpp/engine-and-systems.md`, `docs/explanation/concepts/engine-and-systems.md`,
  `docs/reference/errors.md`, `docs/reference/glossary.md`, `CHANGELOG.md`

**Interfaces:**
- Consumes: Task 3's `SystemDescriptor`, contexts and parameter kinds; Task 2's resources.
- Produces:

```cpp
namespace oly::app {
  // phase.hpp: schedule tags and the built-in phases.
  struct FixedStep {};
  struct Frame {};
  struct UpdateBefore {};
  struct Update {};
  struct UpdateAfter {};
  struct Render {};

  using ScheduleKey = uint32_t;
  template <class T>
  ScheduleKey scheduleKey();  // phases and sets, dense per process

  // controls.hpp: the Engine owns one of each.
  class StateRequest { public: void request(StateId state); };
  class PauseRequest { public: void pause() noexcept; void resume() noexcept; bool requested() const noexcept; };

  // order.hpp. Each call adds a constraint; first() runs before second().
  class PhaseBuilder {
   public:
    template <class Q> PhaseBuilder& before();
    template <class Q> PhaseBuilder& after();
  };
  class SetBuilder {
   public:
    template <class S> SetBuilder& before();   // a set
    template <auto F> SetBuilder& before();    // every registration of a function
    SetBuilder& before(std::string_view systemName);
    // after: the same three forms
  };
  class SystemBuilder {
   public:
    template <class S> SystemBuilder& inSet();
    // before / after: the same three forms as SetBuilder
    SystemBuilder& whilePaused(bool whilePaused = true);
    SystemBuilder& inStates(std::initializer_list<StateId> states);
    SystemId id() const noexcept;
  };

  class Engine {
   public:
    // Built in: UpdateBefore -> Update -> UpdateAfter in the fixed step, Render in the frame.
    template <class P, class Schedule> PhaseBuilder addPhase(Schedule schedule);  // Schedule: FixedStep or Frame
    template <class P> PhaseBuilder phase();  // constraints on a phase declared now or later
    template <class S> SetBuilder set();
    template <auto Function, class P> SystemBuilder addSystem(P phase, std::string_view name);
    SystemBuilder addSystem(ScheduleKey phase, std::string_view name, const SystemDescriptor& descriptor);
    void orderSystems(SystemId first, SystemId second);
    template <class P> PhaseId phaseId() const;   // OLY_ASSERT when not declared
    std::string_view phaseName(PhaseId phase) const;
    // Existing and kept: setWhilePaused, setSystemStates, setSystemStateMask, systemTimings, flushTimings,
    // and, until Task 13, the legacy addSystem(Phase, name, SystemFunction, userData, OrderBand).
  };

  // timing.hpp
  struct SystemTiming { SystemId system; std::string_view name; PhaseId phase; std::string_view phaseName; bool ran; uint64_t nanoseconds; };
  struct FlushTiming { PhaseId phase; std::string_view phaseName; uint64_t nanoseconds; };
}

namespace oly::app::detail {
  enum class TargetKind : uint8_t { Set, Function, Name, System };
  struct OrderTarget { TargetKind kind; uint32_t key; std::string name; };  // key: ScheduleKey, SystemFunctionKey or SystemId
  struct GraphPhase { ScheduleKey key; std::string name; ScheduleKind schedule; bool declared; };
  struct GraphSystem { std::string name; ScheduleKey phase; SystemFunctionKey function; std::vector<ScheduleKey> sets; Access access; };
  struct OrderConstraint { OrderTarget first; OrderTarget second; };
  struct ScheduleInput {
    std::vector<GraphPhase> phases;
    std::vector<std::pair<ScheduleKey, ScheduleKey>> phaseConstraints;  // (first, second)
    std::vector<GraphSystem> systems;                                   // index = SystemId
    std::vector<OrderConstraint> constraints;
  };
  struct SchedulePlan {
    std::vector<uint32_t> fixedStepPhases;              // indices into ScheduleInput::phases, in run order
    std::vector<uint32_t> framePhases;
    std::vector<std::vector<SystemId>> systemsByPhase;  // [phase index], dispatch order
    std::vector<std::string> errors;                    // without the "frame: " prefix
  };
  SchedulePlan buildSchedule(const ScheduleInput& input);
}
```

`buildSchedule` is a pure function: no `Engine`, no `World`. Its algorithm, per schedule:
1. Phase graph from `phaseConstraints`; Kahn's algorithm. A cycle: `cycle of phase constraints: A -> B -> A`. Two
   phases with no path between them: `phases "A" and "B" of the fixed step have no order between them` (`of the
   frame` for the frame). A constraint between phases of two schedules: `phases "A" and "B" are in different schedules`.
   A constraint naming a phase never declared: `phase constraint names "Q", which is not declared`.
2. Per phase, a graph of its systems: each constraint expands `first` and `second` to the systems of that phase (a set
   to its members, a function to its registrations, a name or `System` to one system); one edge per pair. A system on
   both sides is a self-edge, so it is a cycle.
3. Kahn's algorithm with a min-heap on the system name: the order depends on names and constraints only. A cycle:
   `cycle of system constraints in phase "P": a -> b -> a`; its remaining systems go last, in name order.

The `Engine` closes registration at the top of the first `frame` call (before the time is recorded): it builds the
`ScheduleInput` from its registrations, calls `buildSchedule`, resolves every system's parameters through Task 3's
`resolve` (prefixing errors with `frame: `), and raises every collected error in one `OLY_ASSERT(false, "%s", text)`
whose lines start with `frame: `. Then it stores, per phase, the entries in dispatch order; `runFixedStep` runs the
fixed-step phases in plan order and `frame` runs the frame phases after the steps; each phase still ends with one flush.

Dispatch fills one `StepInfo` per phase run and one `SystemInfo` per system, keeps the `whilePaused` and state checks
of `runSystems`, and keeps the timing branch. `systemTimings` is empty until registration closes; after it, one
entry per system in dispatch order (fixed-step phases, then frame phases); `flushTimings` has one entry per phase in
the same order.

Phase ids: declaration order, built-ins first (0..3). The legacy `Phase` enum maps to the built-ins.

**The legacy bridge** (deleted in Task 13): the existing `addSystem(Phase, name, SystemFunction, userData, OrderBand)`
registers an exclusive system in the matching built-in phase whose entry keeps the function and `userData`, and
`runSystems` builds the `SystemContext` for it. Legacy systems of one phase are chained: `System` constraints link each
`Default`-band one to the previous `Default` one, and each `Last`-band one to the previous legacy system of the phase,
so the old order survives. A new-API system in a phase that holds legacy systems conflicts with them (Task 5) until a
constraint orders it.

Repeated system names are refused by `addSystem` itself: `addSystem: system "x" already registered`. Every
registration call after the first `frame` asserts `<operation>: "<name>" registered after the first frame`
(`addSystem`, `addPhase`, `set`, `phase`, `orderSystems`); `addSystem` during a frame keeps its message.
`whilePaused` on a frame-schedule system asserts `setWhilePaused: frame system "x" cannot be whilePaused`.

- [ ] **Step 1: Write the failing pure-graph tests in `system_order.cpp`**

Build `ScheduleInput` by hand and call `buildSchedule`. Helpers local to the suite, reused by Task 5:
`builtInPhases()` (the four built-in phases and their two constraints), `pluginPhase<P>(ScheduleKind)`,
`systemIn<P>(name, access = {})`, `before(first, second)` (an `OrderConstraint` between two targets),
`namesIn<P>(plan, input)` (the names of phase `P` in dispatch order), `writes<T>()` / `reads<T>()` (an `Access` with one
component), `phaseNames(flushTimings)`:

```cpp
TEST_CASE("systems with no constraint run in name order") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("charlie"), systemIn<Update>("alpha"), systemIn<Update>("bravo")};

  const SchedulePlan plan = buildSchedule(input);

  CHECK(plan.errors.empty());
  CHECK(namesIn<Update>(plan, input) == std::vector<std::string>{"alpha", "bravo", "charlie"});
}
```

Also: "a constraint beats the name order" (`charlie` before `alpha`); "a cycle of systems is an error naming the
cycle" (`errors == {"cycle of system constraints in phase \"oly::app::Update\": a -> b -> a"}`); "phases follow their
constraints" (a phase `Physics` after `Update`, before `UpdateAfter`: fixed-step order `UpdateBefore, Update, Physics,
UpdateAfter`); "two phases with no order are an error" (two plugin phases after `Update` with no constraint between
them); "a phase cycle is an error"; "a constraint naming an undeclared phase is an error"; "phases of two schedules
cannot be ordered against each other"; "the registration order does not change the dispatch order" (the same systems
and constraints in reversed `systems` order give the same names).

- [ ] **Step 2: Write the failing `Engine` tests in the same suite**

Each through a `World`, an `Engine`, `addSystem<&f>`, two frames, a `Res<Trace>` (`std::vector<std::string>`) the
systems append their `SystemInfo::name` to, and `CHECK_INVARIANTS(world)`:
- "a plugin phase runs between the phases it is ordered against" (`addPhase<Physics>(FixedStep{}).after<Update>().before<UpdateAfter>()`);
- "systems of a set run after the set they follow" (`inSet<Spatial>()` / `after<Spatial>()`);
- "a function constraint orders every registration of the function" (one function registered as `"a1"` and `"a2"`,
  `after<&f>()` on a third; and `after("a1")` orders against `a1` only) — Review Focus 1;
- "phase order does not depend on which plugin declares first" (two tiny plugins, one declaring `Physics` after
  `Update`, one declaring `Late` after `Physics` before `UpdateAfter`, installed in both orders on two engines:
  `flushTimings` phase names equal) — Review Focus 2;
- "engine controls resolve to the system's own Engine" (two `Engine`s on one `World`, each with a system taking
  `Res<PauseRequest>` that pauses; only that engine's `paused()` turns true; both see the same `Res<Score>`) — Review
  Focus 3;
- "a frame after an exception keeps the closed schedule" (a system that throws on step 1 only; the frame after it
  runs every system once, in the same order, and its query still sees the entities) — Review Focus 4;
- "a set orders its members in each phase" (members in `Update` and `UpdateAfter`, `set<S>().before<&g>()` with `g`
  in `UpdateAfter`: `g` runs after the `UpdateAfter` member, no error) — Review Focus 5;
- "spawns of two unordered systems apply in dispatch order" (two systems of `Update` with only `Commands`, named so
  that name order differs from registration order: no conflict, and the spawned entities' indices follow name order);
- "StepInfo holds the clock, the state and the pause read at the phase start" (a system that pauses and a later
  system of the same phase both see `paused == false`; the next step's see `true`);
- "Res<StateRequest> requests a state that changes in stage 5", "Res<const ActionMap> reads the step's actions",
  "Tasks schedule a task on the game clock" (one test each, mirroring the existing tests of `context.requestState`,
  `context.actions()` and `context.schedule()`).

- [ ] **Step 3: Write the failing death cases**

In `contract_death.cpp`, listed in `CASES`: `system_cycle` (substrings `frame: cycle of system constraints`, `a`, `b`),
`phase_cycle`, `unordered_phases` (`have no order between them`), `duplicate_system_name` (`already registered`),
`add_system_after_first_frame` (`registered after the first frame`), `missing_resource` (`frame: system "needy" needs
resource`), `undeclared_event` (`emits event`), `while_paused_frame_system` (replaces `while_paused_render`; substring
`cannot be whilePaused`).

- [ ] **Step 4: Run to verify they fail**

Run: `./spur test -R '^app\.(system_order|contract_death)'`
Expected: the build fails on the missing `buildSchedule` and `addPhase`.

- [ ] **Step 5: Implement `buildSchedule`, the controls, the builders and the `Engine` changes, with the bridge**

`EngineControls` of each `Engine` holds its `StateRequest`, `PauseRequest`, `actions_` and `scheduler_`.
`Engine::requestState`/`pause`/`resume` stay public for the host.

- [ ] **Step 6: Adapt `timing.cpp` and the Enxame's timing reader**

`timing.cpp`: compare `phase` against `engine.phaseId<oly::app::UpdateBefore>()` and so on, and replace the tests that
read `systemTimings` before the first frame ("systems added while timing is on are recorded with their names") with the
same checks after `frame(0.0)`. `headless.cpp`: write `systemTimings[slot].phaseName` and `flushes[slot].phaseName`
in the JSON (drop `phaseName(Phase)`), and find the S3 flush by `engine.phaseId<oly::app::UpdateAfter>()`.

- [ ] **Step 7: Run the engine and game tests**

Run: `./spur test` then the game test command.
Expected: both `100% tests passed`. Every legacy suite passes unchanged through the bridge, except a test that
registered after the first frame or reused a name in one `Engine`: move the registration before the first frame or
rename it, and say which in the commit body. The Enxame hash tests still pass (the Enxame is on the bridge).

- [ ] **Step 8: Docs, format check, commit**

`engine-and-systems.md` (reference): phases and schedules, `addPhase`/`phase`, sets, constraints, closing at the
first frame, the order rule (constraints, then name; never registration or install order), timing by phase id.
`concepts/engine-and-systems.md`: the same in prose. `errors.md`: every `frame: ` message of this task with its fix.
Glossary: rows for phase, schedule, system set, constraint, `StepInfo`, `SystemInfo`, the engine controls.
`CHANGELOG.md` `### Migration`: a line for phases, sets and `addSystem<&f>`.

```bash
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add engine/runtime/app games/enxame/native/headless.cpp docs CHANGELOG.md
git commit -m "feat(app): order systems by phases, sets and constraints" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Access conflicts and constraint checks

Rows 1 and 4 of the acceptance matrix. `buildSchedule` gains the checks the spec puts at close (§5.4, §5.5 step 3).

**Files:**
- Modify: `engine/runtime/app/src/schedule_graph.cpp`, `engine/runtime/app/include/oly/app/access.hpp`
- Modify: `engine/runtime/app/tests/system_order.cpp`, `engine/runtime/app/tests/contract_death.cpp`,
  `engine/runtime/app/tests/CMakeLists.txt`
- Docs: `docs/reference/errors.md`, `docs/reference/cpp/engine-and-systems.md`

**Interfaces:**
- Consumes: Task 4's `ScheduleInput`, `SchedulePlan`.
- Produces, in `access.hpp`:

```cpp
enum class ConflictKind : uint8_t { Component, Resource, Exclusive };
struct Conflict {
  ConflictKind kind;
  uint32_t id;           // ComponentId or ResourceId; unused for Exclusive
  bool firstWrites;
  bool secondWrites;
};
// Every type both touch where at least one writes, in id order; one Exclusive entry when either is exclusive.
std::vector<Conflict> conflictsBetween(const Access& first, const Access& second);
```

New errors of `buildSchedule`, exact text:
- per conflicting pair and type: `systems "a" and "b" of phase "P" have no order and both access component "Position"
  (a writes, b reads)` — `resource` for a resource, `writes`/`reads` per side; for an exclusive system: `systems "a"
  and "b" of phase "P" have no order and "a" is exclusive`. Every conflict of the phase is reported, in dispatch order
  of the first system, then of the second.
- `constraint names set "S", which has no member`; `constraint names system "typo", which is not registered`;
  `constraint names a function that is never registered`.
- a system constraint across phases that contradicts the phase order: `system "a" (phase "X") must run before "b"
  (phase "Y"), but phase "Y" runs before "X"`; one that agrees is accepted and adds no edge.
- a type used both as a phase and as a set: `"T" is both a phase and a set`.

Reachability between two systems of a phase uses the graph of Task 4 (one search per system; phases hold tens of
systems). Restricting a system to states or to paused steps does not exempt it.

- [ ] **Step 1: Write the failing pure-graph tests**

```cpp
TEST_CASE("two unordered systems that write one component are refused") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("moveA", writes<Position>()), systemIn<Update>("moveB", writes<Position>())};

  const SchedulePlan plan = buildSchedule(input);

  CHECK(plan.errors == std::vector<std::string>{"systems \"moveA\" and \"moveB\" of phase \"oly::app::Update\" have no order and both access component \"Position\" (moveA writes, moveB writes)"});
}
```

The exact component name is `ecs::typeInfo(id).name`; write the expected string from the `Position` of the test
support and check it once with a failing run before freezing it. Also: "a read and a write are refused"; "two reads are
accepted"; "a constraint accepts the pair, directly or through a chain"; "a resource write and read are refused"; "an
exclusive system conflicts with every unordered system of its phase"; "every conflict of a phase is reported" (three
systems, three errors); "a system restricted to a state still conflicts"; "systems of two phases never conflict";
"constraints naming a missing set, name or function are errors"; "a cross-phase constraint that agrees is accepted and
one that contradicts is an error"; "a type cannot be both a phase and a set".

- [ ] **Step 2: Write the failing death cases**

Listed in `CASES`: `write_write_conflict` (substrings `frame: systems`, `have no order`, `component`, `writes`),
`read_write_conflict`, `resource_conflict` (`resource`), `missing_constraint_target` (`which is not registered`),
`contradicting_cross_phase_constraint` (`runs before`). Each registers through `addSystem<&f>` and calls `frame(0.0)`.

- [ ] **Step 3: Run to verify they fail**

Run: `./spur test -R '^app\.(system_order|contract_death)'`
Expected: the new tests fail (no error reported); the death cases end with exit 0 instead of the assert.

- [ ] **Step 4: Implement `conflictsBetween` and the checks**

- [ ] **Step 5: Run the engine and game tests**

Run: `./spur test` then the game test command.
Expected: `100% tests passed` for both. Legacy systems are chained, so the bridge raises no conflict.

- [ ] **Step 6: Docs, format check, commit**

`errors.md`: each new message with its fix (add a constraint, move one system to another phase, or read instead of
write). `engine-and-systems.md`: the conflict rule (component and resource, write–write and read–write; filters never
make two accesses disjoint; restrictions do not exempt).

```bash
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add engine/runtime/app docs CHANGELOG.md
git commit -m "feat(app): refuse unordered systems with conflicting access" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: `essentials2d` and `essentials3d` on the new API

**Files:**
- Modify: `plugins/essentials2d/native/include/oly/essentials2d/plugin.hpp`, `.../src/plugin.cpp`,
  `.../src/propagation.cpp`, `.../src/observers.cpp`, `.../tests/{transforms,hierarchy,contract_death}.cpp`,
  `.../tests/support/fixture.hpp`, `.../bench/propagation.cpp` (only if it registers a system)
- The same files under `plugins/essentials3d/native/`
- Docs: `docs/explanation/architecture/plugins.md` (`#essentials`), `docs/explanation/concepts/plugins.md`,
  `docs/reference/limits.md`, `CHANGELOG.md`

**Interfaces:**
- Consumes: Tasks 2–5.
- Produces (`oly::essentials2d`, and the same in `oly::essentials3d`):

```cpp
struct Propagation {};  // the phase, in the fixed step, after app::Update and before app::UpdateAfter

namespace detail {
  // The plugin's state, a World resource: the children index, the slots and the propagation order. Its observers get
  // its address as userData.
  struct Hierarchy { /* children_, slots_, order_, orderDirty_ of today's Plugin, as public members */ };
}
```

`Plugin` keeps `world_` and `observers_` (its destructor removes the observers) and `checkHierarchy`, which reads the
resource. `install` inserts `detail::Hierarchy{}`, declares the phase
(`engine.addPhase<Propagation>(app::FixedStep{}).after<app::Update>().before<app::UpdateAfter>()`) and registers
`propagateTransforms` in it under the name `"propagateTransforms"`. The system's parameters: the three root queries of
`propagateRoots` (each `Query<const X, WorldTransform, ecs::Without<Parent>>`), `Res<detail::Hierarchy>`,
`Lookup<WorldTransform>`, `Lookup<const Position>`, `Lookup<const Rotation>`, `Lookup<const Scale>`,
`Lookup<const Parent>`. `rebuildOrder` takes `Hierarchy&` and `Lookup<const Parent>`.

Behavior change, written in `limits.md` and `plugins.md`: the propagation runs after `Update`'s flush instead of before
it, so an entity spawned in `Update` already has its `WorldTransform` in `UpdateAfter` of the same step. "Until the next
`Last` band" in `limits.md` becomes "until the next propagation phase".

- [ ] **Step 1: Rewrite the transform tests that name the `Last` band**

In `transforms.cpp` (2D and 3D), the test systems move to `addSystem<&f>` with their own parameters (`Res<Probe>` in
place of `userData`). `phaseNames` is a helper of the plugin's test support returning the `phaseName` of each
`FlushTiming`; Task 7's test uses the same shape. The test "propagation sees the step's `Default`-band changes" becomes "a system of `Update` and
a reader of `UpdateAfter` see the propagated `WorldTransform` in the same step", and a new test checks the phase order:

```cpp
TEST_CASE("the propagation phase runs after Update and before UpdateAfter") {
  Fixture fixture;
  fixture.engine.setTimingEnabled(true);

  fixture.start();

  CHECK(phaseNames(fixture.engine.flushTimings()) == std::vector<std::string>{"oly::app::UpdateBefore", "oly::app::Update", "oly::essentials2d::Propagation", "oly::app::UpdateAfter", "oly::app::Render"});
  CHECK_INVARIANTS(fixture.world);
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `./spur test -R 'essentials'`
Expected: the phase-order test fails (no `Propagation` phase).

- [ ] **Step 3: Move the plugin state into `detail::Hierarchy` and register the system in the new phase**

- [ ] **Step 4: Run the engine and game tests**

Run: `./spur test` then the game test command.
Expected: `100% tests passed` for both, including the Enxame hash tests (`WorldTransform` is not hashed and positions do
not depend on it).

- [ ] **Step 5: Docs, format check, commit**

```bash
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add plugins/essentials2d plugins/essentials3d docs CHANGELOG.md
git commit -m "feat(essentials): propagate transforms in their own phase" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: `physics2d` declares the `Physics` phase

**Files:**
- Create: `plugins/physics2d/native/CMakeLists.txt`, `plugins/physics2d/native/include/oly/physics2d/plugin.hpp`,
  `plugins/physics2d/native/src/plugin.cpp`, `plugins/physics2d/native/tests/CMakeLists.txt`,
  `plugins/physics2d/native/tests/phases.cpp`
- Modify: `plugins/CMakeLists.txt`
- Docs: `docs/explanation/architecture/plugins.md`, `docs/reference/limits.md`, `docs/README.md` if it lists plugins,
  `CHANGELOG.md`

**Interfaces:**
- Consumes: `essentials2d::Propagation` (Task 6), `addPhase` (Task 4).
- Produces: `oly::physics2d::Physics` (phase marker) and `oly::physics2d::Plugin`, whose `install` does only
  `engine.addPhase<Physics>(app::FixedStep{}).after<app::Update>().before<essentials2d::Propagation>()`.
  CMake: `oly_add_plugin(NAME physics2d DEPENDS app PLUGINS essentials2d)`, no schema.

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("Physics runs after Update and before the propagation") {
  oly::ecs::World world;
  oly::app::Engine engine(world);
  engine.installPlugin<oly::essentials2d::Plugin>();
  engine.installPlugin<oly::physics2d::Plugin>();
  engine.setTimingEnabled(true);

  engine.frame(0.0);

  CHECK(phaseNames(engine.flushTimings()) == std::vector<std::string>{"oly::app::UpdateBefore", "oly::app::Update", "oly::physics2d::Physics", "oly::essentials2d::Propagation", "oly::app::UpdateAfter", "oly::app::Render"});
  CHECK_INVARIANTS(world);
}
```

Also "install order does not change the phase order" (`physics2d` installed before `essentials2d`, same names).

- [ ] **Step 2: Run to verify it fails**

Run: `./spur test -R 'physics2d'`
Expected: configure or build fails (no plugin yet), or no test runs; `--no-tests=error` reports it.

- [ ] **Step 3: Create the plugin**

- [ ] **Step 4: Run to verify it passes**

Run: `./spur test -R 'physics2d'`
Expected: `100% tests passed`.

- [ ] **Step 5: Docs, format check, commit**

`plugins.md`: `physics2d` declares `Physics` between `Update` and the propagation, and has no system in phase 1.
`limits.md`: the "Not yet" bullet keeps physics under phase 1 with "`physics2d` declares the `Physics` phase; taking
over the game's integration and separation is not done yet: it needs a velocity column, new work the roadmap's rule 7
re-measures first".

```bash
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add plugins docs CHANGELOG.md
git commit -m "feat(physics2d): declare the Physics phase" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: The Enxame on the new API

The world hash of every scenario must stay at Task 1's values. A hash that changes because of the name tie-break means
the Enxame lacks a constraint that registration order kept implicitly: add the constraint, never a new hash (spec §6).

**Files:**
- Modify: every file in `games/enxame/native/` that registers or defines a system, `games/enxame/desktop/window_game.*`,
  `games/enxame/cmake/game.cmake`
- Rename: `games/enxame/native/simulation.hpp` → `games/enxame/native/resources.hpp`
- Create: `games/enxame/tests/check_phase_order.cmake`

**Interfaces:**
- Consumes: Tasks 2–7.
- Produces: `setUpScenario(ScenarioId, uint32_t size, oly::app::Engine&)` returns `void`; the state lives in `World`
  resources. `Simulation` is gone.

Resources, all in `namespace enxame`: the core types `TileMap` (horde scenarios only), `FlowField`, `SpatialHash`,
`Random`; `Camera`; the parameter structs `HordeParameters`, `CombatParameters`, `FactoryParameters`,
`WaveParameters` (only where used); and, in `resources.hpp`:
`GatheredEnemies { std::vector<Entity> entities; std::vector<float> xs, ys; }`, `HitEnemies { std::vector<Entity> entities; }`,
`Belts { std::vector<Entity> assemblerOfBelt; }`, `StatusToggles { float probability; uint32_t lastStep; }`,
`WaveClock { uint32_t waveSize, stepInWave, wavesCompleted; }`, and in the window `View { SDL_Renderer* renderer; }`.
`systems.hpp` declares the functions below; `OLY_SYSTEM` is no longer used by the Enxame.

| System | Phase | Parameters | Constraint |
|---|---|---|---|
| `placeTurret` | `UpdateBefore` | `Res<TileMap>`, `Res<const Camera>`, `Res<const oly::input::ActionMap>`, `Commands` | `before<&updateFlowField>()` |
| `updateFlowField` | `UpdateBefore` | `Res<const TileMap>`, `Res<FlowField>` | |
| `runWaves` | `UpdateBefore` | `Query<const Position, With<Enemy>>`, `Res<Random>`, `Res<WaveClock>`, `Res<const WaveParameters>`, `Commands` | |
| `rebuildSpatialHash` | `Update` | `Query<const Position, With<Enemy>>`, `Res<SpatialHash>`, `Res<GatheredEnemies>` | |
| `moveHorde` | `Update` | `Query<Position, With<Enemy>>`, `Res<const SpatialHash>`, `Res<const FlowField>`, `Res<const TileMap>`, `Res<const HordeParameters>`, `Res<Random>`, `const StepInfo&` | `after<&rebuildSpatialHash>()` |
| `moveBeltItems` | `Update` | `Query<BeltItem, Position>`, `Lookup<Assembler>`, `Res<const Belts>`, `Res<const FactoryParameters>`, `Commands`, `const StepInfo&` | |
| `runAssemblers` | `Update` | `Query<Assembler>`, `Res<const FactoryParameters>`, `Commands`, `const StepInfo&` | `after<&moveBeltItems>()` |
| `moveVehicles` | `Update` | `Query<Position, Rotation, VehicleMotion>`, `const StepInfo&` | |
| `moveCamera` (window) | `Update` | `Res<Camera>`, `Res<const oly::input::ActionMap>`, `const StepInfo&` | |
| `fireTurrets` | `UpdateAfter` | `Query<Turret, const Position>`, `Res<const SpatialHash>`, `Res<const CombatParameters>`, `Commands`, `const StepInfo&` | |
| `moveProjectiles` | `UpdateAfter` | `Query<Position, Projectile>`, `Res<const SpatialHash>`, `Res<const CombatParameters>`, `Res<HitEnemies>`, `Commands`, `const StepInfo&` | `after<&fireTurrets>()` |
| `toggleStatus` | `UpdateAfter` | `Query<const Position, With<Enemy>>`, `Lookup<const Burning>`, `Lookup<const Slowed>`, `Lookup<const Stunned>`, `Res<Random>`, `Res<StatusToggles>`, `Commands` | |
| `drawMap` (window, horde scenarios only) | `Render` | `Res<View>`, `Res<const Camera>`, `Res<const TileMap>` | |
| `drawEntities` (window) | `Render` | one `Query<const WorldTransform, With<Tag>>` per drawn type, `Res<View>`, `Res<const Camera>` | `after<&drawMap>()` only when `drawMap` is registered |

The four constraints reproduce the registration order of phase 0 where two systems of a phase conflict; every other
pair has no conflict, and the name order changes nothing the hash sees. Keep the comment "placeTurret goes before
updateFlowField, so the field sees the new tile in the same step" next to its constraint.

`game.cmake`: `PLUGINS physics2d` in `oly_add_game`, and a test `enxame.phase_order` running
`--headless --scenario S5 --size 2000 --steps 3 --commit test` through `check_phase_order.cmake`, which requires
`flushes[].phase` to equal `oly::app::UpdateBefore, oly::app::Update, oly::physics2d::Physics,
oly::essentials2d::Propagation, oly::app::UpdateAfter, oly::app::Render` and the `phase` of `fireTurrets` and
`moveProjectiles` in `systems` to be `oly::app::UpdateAfter` (roadmap row 4: `combat` runs after `physics2d`).

- [ ] **Step 1: Add `enxame.phase_order` and run it to verify it fails**

Run: the game test command.
Expected: `enxame.phase_order` fails (no `Physics` phase); every hash test passes.

- [ ] **Step 2: Move the state into resources and the systems to their parameters**

`setUpScenario` inserts the scenario's resources into `engine.world()` and registers the systems of the table with
`engine.addSystem<&f>(oly::app::Update{}, "f")`. `headless.cpp` reads `StatusToggles::lastStep` and
`WaveClock::wavesCompleted` from `world.resource<...>()`. `EnxameGame` inserts `View` and `Camera` is inserted by
`setUpScenario` (every scenario sets a camera).

- [ ] **Step 3: Run the game test command**

Expected: `100% tests passed`, with every `enxame.hash.*` and `enxame.replay_determinism` at Task 1's values. If a hash
differs, find the pair of systems whose order changed (compare `systems[]` of a `--timing on` run on this commit and on
Task 1's commit) and add the missing constraint.

- [ ] **Step 4: Format check, commit**

```bash
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add games/enxame
git commit -m "refactor(enxame): declare each system's access and install physics2d" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: `app` tests off the legacy API, part 1

**Files:**
- Modify: `engine/runtime/app/tests/{loop,systems,pause,recovery,timing,plugins,states,state_scope}.cpp`,
  `engine/runtime/app/tests/support/log.hpp`

**Interfaces:**
- Consumes: Tasks 3–5.
- Produces: `log.hpp`'s `recordName(Res<Log> log, const SystemInfo& self, const StepInfo& step)`: appends
  `self.name`, and `step.interpolation` when `self.schedule == ScheduleKind::Frame`, else `step.time`. The suites of
  Task 10 use it as is.

How each legacy pattern moves:
- `userData` → a `Res<T>` of a type the test inserts into its `World` before the first frame.
- A system that called `Engine` members through `userData` (the "during frame" tests) → an event handler or a task,
  which keep their `userData`, or a test-only resource holding `Engine*` (spec §5.3 allows a non-owning handle).
- Tests that asserted registration order or band order inside a phase now assert the new rule: name order with no
  constraint, constraint order with one. Rename such tests after what they now check.
- Several systems of one phase that write the same `Res<Log>` must be ordered (`after`) in the order the test expects;
  that order is what the test asserts now.

- [ ] **Step 1: Migrate the files one by one, running each suite after its file**

Run (per file, e.g. `systems`): `./spur test -R '^app\.systems$'`
Expected: passes before the next file starts. A test that only existed to check the `Last` band or registration order
is replaced by its new-rule counterpart, not deleted.

- [ ] **Step 2: Confirm no legacy use is left in these files**

```bash
grep -n "SystemContext\|OrderBand\|Phase::\|userData" engine/runtime/app/tests/{loop,systems,pause,recovery,timing,plugins,states,state_scope}.cpp engine/runtime/app/tests/support/log.hpp
```

Expected: no output, except `userData` of handlers, tasks or state hooks.

- [ ] **Step 3: Run the `app` tests, format check, commit**

```bash
./spur test -R '^app\.'
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add engine/runtime/app/tests
git commit -m "test(app): move the loop, pause, state and timing suites to declared access" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: `app` tests off the legacy API, part 2

**Files:**
- Modify: `engine/runtime/app/tests/{events,events_loop,actions,schedule,contract_death}.cpp`

**Interfaces:**
- Consumes: Task 9's `recordName` and the migration rules of Task 9.

The death cases keep their names and required substrings; only the way they reach the `Engine` changes. Delete
`add_system_during_frame`'s legacy body only if Task 4's `add_system_after_first_frame` already covers it; otherwise
keep it on the new API (a handler that calls `addSystem<&f>` during dispatch).

- [ ] **Step 1: Migrate the files one by one, running each suite after its file**

Run (per file): `./spur test -R '^app\.events$'`, `'^app\.events_loop$'`, `'^app\.actions$'`, `'^app\.schedule$'`,
`'^app\.contract_death'`
Expected: each passes before the next file starts.

- [ ] **Step 2: Confirm no legacy use is left in `engine/runtime/app/tests/`**

```bash
grep -rn "SystemContext\|OrderBand\|Phase::" engine/runtime/app/tests
```

Expected: no output.

- [ ] **Step 3: Run the `app` tests, format check, commit**

```bash
./spur test -R '^app\.'
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add engine/runtime/app/tests
git commit -m "test(app): move the event, action and schedule suites to declared access" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: Fixture plugins and the install-order test

Row 4: "order from `before`/`after`, independent of install order".

**Files:**
- Modify: `engine/testing/fixture/plugins/counting/{include/oly/counting/plugin.hpp,src/plugin.cpp}`,
  `engine/testing/fixture/plugins/doubling/{include/oly/doubling/plugin.hpp,src/plugin.cpp}`,
  `engine/testing/fixture/plugins/native/plugins_test.cpp`

**Interfaces:**
- Produces: `oly::counting::Counting` (set) with `count(Query<Counter>)` in `Update`, `inSet<Counting>()`;
  `doubling`'s `writeDoubles(Query<const Counter, Doubled>)` in `Update`, `after<counting::Counting>()` (it replaces the
  `Last` band). `system()` accessors stay: `SystemId` is still dense in registration order.

- [ ] **Step 1: Write the failing install-order test**

```cpp
TEST_CASE("the dispatch order does not depend on the install order") {
  const std::vector<std::string> countingFirst = dispatchOrder<oly::counting::Plugin, oly::doubling::Plugin>();

  const std::vector<std::string> doublingFirst = dispatchOrder<oly::doubling::Plugin, oly::counting::Plugin>();

  CHECK(countingFirst == std::vector<std::string>{"count", "writeDoubles"});
  CHECK(doublingFirst == countingFirst);
}
```

`dispatchOrder<A, B>()` builds a `World` and an `Engine`, installs `A` then `B` with `installPlugin` (no `setUpGame`),
turns timing on, runs `frame(0.0)` and `frame(1.0 / 60)`, checks `CHECK_INVARIANTS(world)` and returns the
`systemTimings` names. The existing "Last-band system" test becomes "the doubling system sees the step's counts" with
the game's `addTen` system ordered `before<oly::counting::Counting>()`.

- [ ] **Step 2: Run to verify it fails**

Run: `./spur test -R '^fixture\.plugins$'`
Expected: fails to build (no `counting::Counting`).

- [ ] **Step 3: Move both plugins to `addSystem<&f>` and the set**

- [ ] **Step 4: Run to verify it passes, format check, commit**

```bash
./spur test -R '^fixture\.plugins$'
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add engine/testing/fixture/plugins
git commit -m "test(fixture): order the fixture plugins by a set and check install order" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 12: C API, named systems, TS client and the game template

**Files:**
- Modify: `engine/runtime/app/include/oly/app/system.hpp`, `engine/runtime/app/src/system.cpp` (named registry)
- Modify: `engine/runtime/capi/include/oly/capi/engine_api.h`, `engine/runtime/capi/src/engine_api.cpp`,
  `engine/runtime/capi/src/handles.hpp`, `engine/runtime/capi/tests/{engine_api,contract_death}.cpp`
- Modify: `engine/testing/fixture/systems.cpp`
- Modify: `engine/environments/web/clients/typescript/core/src/{engine,bridge,module}.ts`,
  `engine/environments/web/clients/typescript/core/api/public-exports.txt` if the surface changes,
  `engine/testing/tests/wasm/*.test.ts`
- Modify: `templates/game/common/native/{movement.cpp,systems.hpp}`, `templates/game/native/desktop/main.cpp`,
  `templates/game/web/web/src/gameplay.ts`
- Docs: `docs/reference/c-api.md`, `docs/reference/typescript/engine-and-systems.md`,
  `docs/how-to/typescript/gameplay-recipes.md`, `docs/reference/game-template.md`, `docs/reference/limits.md`,
  `CHANGELOG.md`

**Interfaces:**
- Consumes: Tasks 3–5.
- Produces:

```cpp
// system.hpp
const SystemDescriptor* findNamedSystem(std::string_view name) noexcept;  // nullptr if the name does not exist
struct NamedSystemRegistration {
  NamedSystemRegistration(std::string_view name, const SystemDescriptor& descriptor);
};
#define OLY_SYSTEM(name) static const oly::app::NamedSystemRegistration name##Registration(#name, oly::app::systemDescriptor<&name>())
```

```c
// engine_api.h. first runs before second: the same phase, or phases in that order; checked when registration closes.
OLY_API void olyEngineOrderSystems(OlyEngine* engine, uint32_t firstSystemId, uint32_t secondSystemId);
```

- `olyEngineAddNativeSystem` registers the named descriptor in the built-in phase of the number (0..3, as today), so a
  native system from the C API carries its declared access. `olyEngineAddScriptSystem` registers
  `runScriptSystem(ecs::World&, Commands, Res<capi::ScriptHost>, const SystemInfo&)`, an exclusive system; `ScriptHost`
  (`{ OlyEngine* engine; }`, in `handles.hpp`) is inserted into `engine->world.world` by `olyEngineCreate`.
  `OlyEngine::currentContext` becomes `oly::app::Commands* currentCommands`, which `olySystemSpawn/Destroy/Add/Remove`
  use; `olyEngineRequestState`, `olyEnginePause` and the others keep calling the `Engine`.
- TS: `Engine.addSystem` chains the script systems of a phase in declaration order (it calls
  `bridge.orderSystems(handle, previous, id)`); `SystemRegistration` gains `before(other: SystemRegistration)` and
  `after(other: SystemRegistration)`; both throw `ContractError` for a registration of another `Engine` or after the
  first frame. `Phase` stays `{ UpdateBefore: 0, Update: 1, UpdateAfter: 2, Render: 3 }`.
- Template: `movePlayer(Query<Position, ecs::With<Player>>, Res<const input::ActionMap>, const StepInfo&)` followed by
  `OLY_SYSTEM(movePlayer);`; `main.cpp` registers it with `addSystem<&movePlayer>` and `draw(Query<const Position,
  ecs::With<Player>>, Res<const Renderer>)`, where `Renderer { SDL_Renderer* renderer; }` is inserted in `onStart`.

- [ ] **Step 1: Write the failing C API and TS tests**

C API (`capi/tests/engine_api.cpp`): "olyEngineOrderSystems orders two native systems of one phase" (two named
fixture systems whose names sort the other way; after ordering, the effect on a `Trace` shows the requested order) and
"a script system is ordered against a native one". `capi/tests/contract_death.cpp`: `order_systems_cycle` (substring
`frame: cycle of system constraints`). TS (`engine.test.ts`): "script systems of a phase run in declaration order",
"registration.after orders a script system after a native one" (the existing test with `u2`, `appendNine`, `u3`
becomes this: `u3` after `appendNine`, `appendNine` after `u2`), "before with a registration of another engine is
ContractError".

- [ ] **Step 2: Run to verify they fail**

Run: `./spur test -R '^capi\.'` then `./spur games-test`
Expected: the build fails on `olyEngineOrderSystems`; the TS tests fail on `before`/`after`.

- [ ] **Step 3: Implement the named registry, the C API, the TS client and move the fixture systems**

`fixture/systems.cpp`: each `OLY_SYSTEM(name) { ... }` becomes a function with its parameters followed by
`OLY_SYSTEM(name);` (`countFirePresses` takes `Res<const oly::input::ActionMap>`; `emitPings` takes `Events<Ping>`;
`spawnBatch` takes `Commands`). Update the existing TS tests that mixed native and script systems in one phase with the
`after` calls the conflict check now requires.

- [ ] **Step 4: Move the template**

- [ ] **Step 5: Run every affected suite**

```bash
./spur test
./spur games-test
./spur consumer-test
```

Expected: each ends with `100% tests passed` (the consumer test creates, builds and tests a game from the template).

- [ ] **Step 6: Docs, format check, commit**

`c-api.md`: `olyEngineOrderSystems`; native systems carry their access; script systems are exclusive. `limits.md`: "A
phase a plugin declares is not reachable from the C API or TS; they register in the four built-in phases by number."
TS reference and recipes: chaining and `before`/`after`. `game-template.md`: the new system shapes.

```bash
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add engine templates docs CHANGELOG.md
git commit -m "feat(capi): order systems by id and register named systems with their access" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 13: Remove the legacy API and close the documentation

**Files:**
- Modify: `engine/environments/native/host/tests/{game,window}.cpp`, `engine/environments/native/host/include/oly/native_host/game.hpp`
  (the comment on `userData`)
- Modify: `engine/runtime/app/include/oly/app/{system,phase,engine,context}.hpp`, `engine/runtime/app/src/{engine,system}.cpp`
  (delete `SystemContext`, `SystemFunction`, the enum `Phase`, `PhaseCount`, `OrderBand`, `OrderBandCount`,
  `isGameplayPhase`, the legacy `addSystem` and the bridge entries)
- Modify: `engine/runtime/app/include/oly/app/plugin.hpp` (the comment: state as resources, not `userData`)
- Docs: `docs/explanation/architecture/decisions.md`, `docs/explanation/architecture/app.md`,
  `docs/explanation/architecture/plugins.md`, `docs/how-to/write-a-plugin.md`, `docs/how-to/gameplay-recipes.md`,
  `docs/reference/cpp/*.md`, `docs/reference/native-host.md`, `docs/reference/glossary.md`, `docs/tutorials/*.md`,
  `docs/how-to/{keep-the-hot-path-fast,test-a-desktop-game}.md`, `README.md`, `CHANGELOG.md`, and any other file
  Step 3's grep lists
- Modify: `docs/superpowers/specs/2026-11-09-game-engine-design.md` and `2026-11-16-plugins-design.md` get one line each,
  at the superseded passage, pointing to the phase 1 spec (they keep their history)

- [ ] **Step 1: Move the native host tests to the new API**

`window.cpp`'s counters and `JumpProbe` become resources; `game.cpp`'s `countStep` becomes `addSystem<&countStep>`.

Run: `./spur native-games-test` then `./spur windows-test`
Expected: both `100% tests passed`.

- [ ] **Step 2: Delete the legacy API and the bridge**

Run: `./spur test`
Expected: `100% tests passed`. A compile error points at a use Tasks 6–12 missed: move it, do not keep the old name.

- [ ] **Step 3: Find every remaining mention**

```bash
grep -rn "SystemContext\|OrderBand\|Phase::\|PhaseCount\|isGameplayPhase\|userData = this\|context\.userData()" --include=*.cpp --include=*.hpp --include=*.ts --include=*.md . | grep -v "^./docs/superpowers/\|/build\|node_modules"
```

Expected after the edits of Step 4: no output (TS's own `Phase.Update` constants do not match `Phase::`).

- [ ] **Step 4: Rewrite the documentation**

- `decisions.md`: delete the row "Order bands"; rewrite "The `Engine` owns the plugin instances" and "The host is a
  library the game calls" so state reaches systems as resources, not `userData`; add a row "Declared access and system
  sets" (choice: access from the signature, conflicts refused at close, order by constraints then name; why: phase 3
  trusts the declaration in release builds; cost: every system declares its parameters, plugins that follow game
  systems declare a phase).
- `limits.md`: "Event handlers, state hooks, scheduled tasks and observers keep registration order"; "A query filter
  never makes two accesses disjoint: systems that write one component in one phase are ordered" (consequence for
  phase 3, spec §4); "A resource that holds a pointer to the `World` or the `Engine` bypasses the declared access".
- Every page Step 3 listed: the new parameters, phases and constraints in place of `SystemContext`, bands and
  `userData`.
- `CHANGELOG.md`: `### Breaking` replaces `None.` with the removal of `SystemContext`, `userData`, `OrderBand` and the
  `Phase` enum, and the order rule; `### Migration` gains the steps, in order: a system's parameters, its state as
  resources, `addSystem<&f>(phase, name)`, constraints where two systems conflict, `OLY_SYSTEM(name);` after the
  definition, a plugin that used `OrderBand::Last` declares a phase. Merge the earlier Migration lines of this phase
  into these steps.

- [ ] **Step 5: Run every suite**

```bash
./spur test
./spur native-games-test
./spur windows-test
./spur games-test
./spur consumer-test
```

Then the game test command. Expected: each ends with `100% tests passed`; `oly.lint.links` and `oly.lint.boundaries`
pass inside `./spur test`.

- [ ] **Step 6: Format check, commit**

```bash
./spur format-check $(git ls-files -co --exclude-standard '*.cpp' '*.hpp')
git add -A engine plugins templates docs README.md CHANGELOG.md
git commit -m "refactor(app)!: remove SystemContext, userData and order bands" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 14: Cost of dispatch against phase 0, recorded

Not a gate (roadmap rule 4): ratios recorded next to the phase 0 baseline.

**Files:**
- Create: the raw summaries written by `compare.sh` under `docs/reference/benchmarks/enxame/`
- Modify: `docs/reference/benchmarks/enxame/README.md` (a "Phase 1" section)

- [ ] **Step 1: Run the comparison on the host**

```bash
head=$(git rev-parse --short HEAD)
sh games/enxame/compare/compare.sh 5eec212 "$head"
git worktree list
```

Expected: the script ends with its summary lines; `git worktree list` shows only the main checkout (the script removes
its worktrees, including on failure).

- [ ] **Step 2: Record the ratios**

"Phase 1" section: the command, the two commits, the step p50/p99 ratio per scenario and size, the S5 world hash of
both sides (equal), and one sentence on where the ratio leaves the A/A noise of phase 0 (IQR in the same README). No
target, no verdict beyond "inside the noise" or "outside it, by how much".

- [ ] **Step 3: Commit**

```bash
git add docs/reference/benchmarks/enxame
git commit -m "docs(bench): record the phase 1 dispatch against phase 0" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
