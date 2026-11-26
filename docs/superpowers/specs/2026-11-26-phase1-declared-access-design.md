# Oly — phase 1: declared access and system sets — design

Date: 2026-11-26. Written on `feature/engine-AAA` at `61e9c84`. Phase 1 of `2026-11-24-aaa-roadmap-design.md` (called *the
roadmap* below); its gate is rows 1 and 4 of the roadmap's acceptance matrix.

Marking: **[user decision]** was decided in the conversation; **[author decision]** was my choice while writing, which
the user may review; **[fact]** can be checked in the repository; **[hypothesis]** needs a measurement or a build.

## 1. Intent

A system's registration answers three questions, and the engine answers each one without help from registration or
install order **[user decision]**:

- **Where it runs:** in which phase of which schedule. Plugins can add phases.
- **What it accesses:** which data it reads and which it writes, taken from the function's signature, not written by
  hand.
- **Who it must follow or precede:** `before`/`after` constraints between named groups (sets), never the order of the
  calls.

Why now: phase 3 runs systems on several threads and trusts the declared access to keep them apart. A declaration that
can be wrong only on an untested path becomes a data race in a release build. The roadmap already accepted that the
system signature breaks once, before games outside the repository depend on it.

## 2. Goals, non-goals, anti-goals

Goals:

- Every access of a system to a component or a resource is visible in its signature, and the compiler refuses an access
  that is not there.
- Registration refuses, with the names of the systems involved, two systems of one phase with no order between them
  where one writes what the other reads or writes, and any cycle of constraints (rows 1 and 4).
- The dispatch order inside a phase depends on constraints and names only. Installing the same plugins in another order
  gives the same order.
- The Enxame runs on the new API with the same world hash as the phase 0 commit in every scenario (§6).

Non-goals:

- Threads, jobs and per-thread command buffers: phase 3. Phase 1 builds the graph that phase 3 schedules.
- Services, plugin-replaceable resources and assets: phase 5. Phase 1 brings only typed resources in the conflict graph.
- Stable ids and namespaces: phase 2. Phase 1 names phases and sets by C++ type, whose qualified name phase 2 hashes.
- Order of event handlers, state hooks, scheduled tasks and observers. They keep registration order in phase 1 (§5.7);
  `limits.md` records it as the one order rule the roadmap goal still has left **[author decision]**.

Anti-goals (a result that does one of these fails even if it works):

- A way for a system to reach data its signature does not declare: a captured lambda, a `void*`, a `World&` outside an
  exclusive system.
- A tie between unordered systems broken by registration or install order.
- A 2D-only type or rule in `app` or `ecs` (roadmap rule 1).

## 3. Constraints

- **[fact]** Today `Engine::addSystem(Phase, name, function, userData, OrderBand)` (`engine/runtime/app/include/oly/app/engine.hpp:157`);
  `Phase` is a fixed enum of four (`phase.hpp:6`); inside a phase the `Default` band runs before `Last`, and inside a
  band the registration order rules. Each phase ends with one flush of the commands.
- **[fact]** The signature `void(SystemContext&)` says nothing about access: the query is created in the body
  (`games/enxame/native/horde.cpp`), random access goes through `context.get<T>(entity)` and `has<T>`
  (`factory.cpp:19`, `status.cpp:10`), and state reaches systems through `userData` (`Simulation` in the Enxame; the
  plugin instance in `essentials2d`, which also keeps a `World*`).
- **[fact]** `World::query` builds a `std::vector` key and searches a `std::map` on every call (`world.hpp:377`), so
  every system pays one lookup per query per step.
- **[fact]** The order rules being replaced are written in force elsewhere: `2026-11-09-game-engine-design.md:248-249`
  ("within a phase, the order is the registration order; there is no dependency graph"), `2026-11-16-plugins-design.md`
  §4.4 (order bands) and `docs/explanation/architecture/decisions.md`, rows "Order bands", "The `Engine` owns the plugin
  instances" (state reaches systems through `userData`) and "The host is a library the game calls" (members reach
  systems through `userData = this`). This phase supersedes those lines and updates the three rows.
- **[fact]** The C API takes a phase as `uint32_t` (`engine_api.h:78-81`) and the TS client mirrors the four phases
  (`engine/environments/web/clients/typescript/core/src/engine.ts:15`). Roadmap rule 2: the C API and TS may lag, and this
  spec says what they get (§5.8).
- **[fact]** Resources live in the `World` (§5.3), so the change touches `world`: `checkInvariants` covers it and the
  `spec-conformance-reviewer` and `perf-reviewer` agents run (`CLAUDE.md`).
- **[fact]** The Enxame world hash folds entity indices (`games/enxame/native/world_hash.cpp`), so a change in spawn order
  changes it. Only S5 writes it today (`docs/reference/benchmarks/enxame/README.md`: `0x21c16d888746fd84`).
- Public API changes update `docs/` and `CHANGELOG.md` `## Unreleased` in the same change (`CLAUDE.md`).

## 4. Approaches

**Can a system's body reach data outside its signature?**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| A | Strict: the body sees only its parameters; an exclusive system gets the whole `World` and runs alone | declarations cannot lie; every system is rewritten | **chosen [user decision]** |
| B | Hybrid: the signature covers the common case, the context keeps `query`/`get`, a Debug assert checks them against the declaration | smaller break; a wrong declaration shows only on a path that runs in Debug | rejected: phase 3 trusts the declaration in release |

**State that is not a component (today's `userData`).**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| i | Minimal typed resources now: `Res<const T>` reads, `Res<T>` writes, in the conflict graph like components | brings part of row 9 forward from phase 5 | **chosen [user decision]** |
| ii | A typed `userData` counted as one resource per pointer | cheap, but every system of the game's `Simulation` serializes | rejected |
| iii | Invisible, ordered by the game | the hole option A exists to close | rejected |

**Two unordered systems of one phase that touch the same data.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| a | Error for write–write and read–write | results never depend on a tie-break; plugins that must follow game systems declare their own phase | **chosen [user decision]** |
| b | Error for write–write only; read–write broken by name | deterministic, but silent: in `scenario.cpp:75-76` `moveHorde` would run before `rebuildSpatialHash` | rejected: changes behavior against phase 0 with no message |

**Do query filters prove that two systems never touch the same entities?** Example: `moveHorde` writes `Position`
`With<Enemy>` (`horde.cpp:70`) and `moveBeltItems` writes `Position` on entities with `BeltItem` (`factory.cpp:15`),
both in `Update`.

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| 1 | No: conflict per component type, filters declare nothing | simplest descriptor and check; the two systems above must be ordered | **chosen [user decision]** |
| 2 | Yes, statically: two accesses to `T` are disjoint when one requires `X` and the other excludes `X` | keeps the parallelism row 2 wants; every pair that must run side by side writes an explicit `Without<>` | rejected |
| 3 | Yes, by the archetypes that exist at run time | cannot be judged when registration closes, so it breaks approach a | rejected |

Consequence, recorded for phase 3: row 2 wants horde, projectiles and belts in parallel, and with approach 1 systems
that write the same component are ordered. Phase 3 revisits the access descriptor (filters or archetypes) and the
constraints the Enxame gains here.

**How the signature becomes declared access.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| H1 | Compile-time adapter over a function pointer: the template reads the parameter types, builds the access list at registration, resolves the parameters once when registration closes, and calls through one fixed-signature trampoline | honest access, no per-step lookup, one function pointer as the C API and the named registry already use; template code in `app` | **chosen [user decision]** |
| H2 | Runtime parameter objects with virtual `access()`/`fetch()`, system as `std::function` | less template code; captured lambdas reopen undeclared access, and a `std::function` is not reachable from the C API | rejected |
| H3 | System as a struct with `Query`/`Res` members and `run()` | natural local state; without reflection in C++20 the members are listed by hand or by macro | rejected: access no longer comes from the signature |

## 5. The design

The code below sketches the shape. Names and exact signatures are the builder's (§7).

### 5.1 Where a system runs: schedules and phases

- Two schedules, fixed by the engine: the fixed step and the frame **[author decision]**. A plugin adds phases, not
  schedules.
- The engine declares `UpdateBefore`, `Update` and `UpdateAfter` in the fixed step and `Render` in the frame. A plugin
  declares a phase in one schedule with `before`/`after` against other phases.
- Each phase ends with one flush of the commands, as today. Because each flush changes what the next phase sees, the
  phases of a schedule must form a total order: two phases with no order between them are an error **[author decision]**.
- Constraints between phases can come from any plugin or from the game: two plugins that each add a phase after `Update`
  and do not know each other are an error the game fixes with one constraint between the two **[author decision]**.
- Phases are named by a C++ marker type; its name is `oly::core::typeName<T>()`, the qualified name phase 2 hashes into a
  stable id **[author decision]**.
- The `Phase` enum, `PhaseCount` and the arrays sized by it (`phases_`, `firstTimingSlot_`, `flushTimings_`) become
  dynamic. `SystemTiming` names its phase by id, and `flushTimings` has one entry per phase.

```cpp
struct Physics {};

void Plugin::install(app::Engine& engine) {
  engine.addPhase<Physics>(app::FixedStep{}).after<app::Update>().before<app::UpdateAfter>();
}
```

### 5.2 What a system accesses: its parameters

A system is a free function, or a lambda without captures, whose parameters are all of these kinds **[author decision]**:

| Parameter | Access it declares | Conflicts with |
|---|---|---|
| `Query<Ts..., Filters...>` | read of each `const T`, write of each `T`; `With`/`Without` filters declare nothing and never make two accesses disjoint (§4) | a write of `T` (for a read), any access to `T` (for a write) |
| `Lookup<const T>` / `Lookup<T>` | read / write of `T` through `get`/`tryGet`/`has` by entity | same as `Query` |
| `Res<const T>` / `Res<T>` | read / write of resource `T` | same rule, on the resource |
| `Commands` | structural changes, deferred to the end of the phase | nothing (§5.5) |
| `Events<E>` | emitting `E` | nothing (§5.5) |
| `const StepInfo&` | the clock (`time`, `fixedDeltaTime`, `interpolation`), `state()`, `paused()`; constant during a phase | nothing |

- `SystemContext` with `query`, `get`, `tryGet`, `has` and `userData` goes away, and so does the `userData` argument of
  `addSystem` **[user decision]**.
- The engine controls a system used to reach through its context become engine resources, so they take part in the graph
  **[author decision]**: requesting a state and pausing or resuming are writes (the last request wins, so their order
  matters); reading the actions is `Res<const input::ActionMap>`, rebinding is `Res<input::ActionMap>`. Scheduling a task
  is an append, like `Commands`.
- `Commands` keeps today's state scope: what a system spawns while a state is active is born scoped.
- A parameter of any other type is a `static_assert` naming the parameter and the system.

```cpp
void moveHorde(Query<Position, With<Enemy>> enemies, Res<const SpatialHash> hash, Res<const FlowField> flowField, const StepInfo& step) {
  enemies.each([&](Position& position) {
    // ...
  });
}
```

### 5.3 Resources

- A resource is one object per type, owned by the `World` (the roadmap puts resources there in phase 5), inserted
  before the first frame and reached by systems through `Res` **[author decision]**. Phase 1 brings no more than that:
  no services, no replacement, no assets.
- Each resource type gets a dense id per process, the same way a component does.
- The engine controls of §5.2 (pause, state request, `ActionMap`) stay owned by the `Engine`, with ids in the same
  conflict space as the `World`'s resources, because two `Engine`s may share one `World` (`engine.hpp:121-122`) and each
  has its own controls **[author decision]**.
- A resource may be a non-owning handle to an object the host owns (the renderer the template passes today through
  `userData`, the Enxame's view). The game inserts it in its setup; the native host gets no new API **[author
  decision]**.
- The Enxame's `Simulation` splits into resources (map, flow field, spatial hash, random generator, scenario
  parameters). A plugin's state becomes a resource of the plugin; the `Plugin` instance keeps only what its destructor
  needs (the observers it registered).
- The spec records that row 9 of the acceptance matrix ("resources take part in the conflict graph like components") is
  met in phase 1; phase 5 keeps the rest of its content.

### 5.4 Order: sets and constraints

- A system belongs to one phase and to any number of sets.
- A system's name is an explicit string, as today, unique per `Engine` **[author decision]**. The name, not a name derived
  from the function, breaks ties (§5.5), so the order is the same with GCC and Clang, whose `__PRETTY_FUNCTION__`
  spellings differ. One function may be registered under several names; a constraint that names the function applies to
  every registration of it, and one that names a system's string applies to that registration only.
- Sets are named by a C++ marker type, like phases. A set has no phase: it groups the members it has in each phase.
- `before`/`after` link sets and systems. A constraint between members of different phases is accepted when it agrees
  with the order of the phases and is an error when it contradicts it **[author decision]**.
- A constraint may name a set or system that is never registered; the check when registration closes rejects it, so a
  typo is not a silent no-op **[author decision]**.
- Restricting a system to states or to paused steps does not exempt it from the conflict check **[author decision]**.

```cpp
struct Spatial {};

engine.addSystem<&rebuildSpatialHash>(app::Update{}, "rebuildSpatialHash").inSet<Spatial>();
engine.addSystem<&moveHorde>(app::Update{}, "moveHorde").after<Spatial>();
```

### 5.5 Closing registration: building the schedule

Registration closes on the first `frame` call (it already locks registration). Only then is the graph complete: a later
constraint can resolve an earlier ambiguity. So "at registration" in rows 1 and 4 means "when registration closes"
**[author decision]**. In order, per schedule:

1. Order the phases by their constraints; a cycle or two phases with no order is an error.
2. In each phase, build the graph of systems from the constraints (sets expanded to their members in that phase); a
   cycle is an error naming the systems on it.
3. For every pair of systems in the phase with no path between them, compare their access; a write–write or read–write
   overlap on a component or a resource is an error naming both systems and the type. The whole list of conflicts is
   reported, not only the first.
4. Order each phase topologically, breaking ties by the system's name; a repeated name is an error. Registration and
   install order are never used.
5. Resolve each parameter once (the `Query&`, the resource pointer, the `Lookup` table) into the system's state.

Appends (`Commands`, `Events`, scheduled tasks) from systems of one phase are applied in the order of step 4, so the
result is deterministic without making two spawners conflict. Phase 3 keeps that order when it merges per-thread
buffers.

Errors at this point are `OLY_ASSERT`s, like the rest of the registration contract, and each one names the operation,
the systems and the type.

### 5.6 Exclusive systems

An exclusive system takes `World&` (and the engine controls) and declares access to everything: it conflicts with
every system of its phase, so it must be ordered against each of them, usually by being alone in a phase or in a set
ordered as a whole. It is the way out for code that needs the whole `World`.

### 5.7 What stays outside the graph

Event handlers, state hooks, scheduled tasks and observers keep their direct access and their registration order. They
run in serial stages outside the phases (`engine.cpp:190-212`), so they never overlap a system **[author decision]**.

### 5.8 C API and TS

- `OLY_SYSTEM` keeps registering by name, now with the adapter: a native system registered from the C API carries its
  declared access **[author decision]**.
- A script system (TS) has no C++ signature: it is exclusive.
- The C API gains one call to order two systems by `SystemId`. Phases stay the four built-in ones, by number; a phase a
  plugin declares is not reachable from the C API in phase 1, written in `limits.md`.
- The TS client chains its own script systems of a phase in declaration order (it emits the ordering calls), and
  `SystemRegistration` gains `before`/`after` against another registration, for a script system next to a native one in
  the same phase **[author decision]**.

### 5.9 Plugins: `essentials` and `physics2d`

- `essentials2d` and `essentials3d` declare their own propagation phase after `Update` and before `UpdateAfter`, where
  the `Last` band ran it, replacing `OrderBand::Last`. `OrderBand` goes away.
- `physics2d` depends on `essentials2d` (every 2D game installs it through `DIMENSION`) and declares `Physics` after
  `Update` and before the propagation phase, so `WorldTransform` is computed from the positions physics wrote
  **[author decision]**. The suspended `Late` band (`2026-11-24-order-band-late-design.md`) stays suspended; no line of it
  changes.
- `physics2d` is created in `plugins/` and declares `Physics`. The roadmap places it after `Update` and has it take over
  the Enxame's integration and separation with no new work (rule 7). **[fact]** In `moveHorde`, separation and
  integration run in the same per-entity loop as the steering by flow field, so "with no new work" is a
  **[hypothesis]**: splitting the loop needs a velocity column, which is new work under rule 7. The builder decides what
  `physics2d` takes over under two rules: the scenario's work stays the same (or is re-measured on the phase 0 commit
  first, rule 7) and the world hash stays the same (§6).

### 5.10 What else changes

- The Enxame, the game template (`templates/game/native/desktop/main.cpp`), the native host, the test fixtures and every
  test that registers a system move to the new signature.
- `decisions.md`: the rows "Order bands" (removed), "The `Engine` owns the plugin instances" and "The host is a library
  the game calls" (state as resources, not `userData`), plus a row for declared access and sets.
- `docs/`: the pages on systems, phases, plugins and the C API (`docs/reference/cpp/engine-and-systems.md`,
  `docs/explanation/concepts/engine-and-systems.md`, `docs/explanation/architecture/app.md`, `plugins.md`,
  `docs/how-to/write-a-plugin.md`, `gameplay-recipes.md`, `docs/reference/c-api.md`, `errors.md`, the TS guides), and
  `CHANGELOG.md` `## Unreleased` with a `### Migration` entry.

## 6. Verification

The first commit of the phase, before any engine change, makes every Enxame scenario write its `worldHash` and records
the value of each on the phase 0 code (roadmap rule 5: the instrument before the gate) **[author decision]**. From then
on, the migrated Enxame must give the same hash. A hash that changes only because of the new tie-break shows an order
that phase 0 kept implicitly: the Enxame gets the missing constraint, not a new hash.

| Criterion | Proof | Command |
|---|---|---|
| Row 1: two unordered systems that write the same component, or one writes what the other reads, are refused | `contract_death` cases in `engine/runtime/app/tests/` | `./spur test` |
| Row 4: a cycle of constraints is refused, between systems and between phases | `contract_death` cases | `./spur test` |
| Row 4: order independent of install order | the native fixture plugins installed in both orders give the same dispatch order (`systemTimings`) | `./spur test` (the fixture builds in `debug-asan`, `engine/testing/CMakeLists.txt:18`) |
| Row 4: `physics2d` adds `Physics`, `combat` runs after it | the Enxame's dispatch order | the Enxame test below |
| No behavior change against phase 0 | every scenario's `worldHash` equals the value recorded in the first commit | `./spur dev sh -c 'cd games/enxame && OLY_ENGINE_DIR=/app OLY_IN_DOCKER=1 sh ./oly test'` |
| An access outside the signature does not compile | a negative compile check of the adapter | named by the plan **[hypothesis until then]** |
| Native host and essentials | their suites | `./spur native-games-test`, `./spur windows-test` |
| C API and TS | the TS tests of the engine and of the fixture game | `./spur games-test` |
| The template game | create, build and test a game in local mode | `./spur consumer-test` |
| Cost of dispatch | S1–S8 against the phase 0 commit, ratios recorded, not gated (rule 4) | `sh games/enxame/compare/compare.sh 5eec212 <head>` on the host |

## 7. Left to the builder

- The exact names: of the parameter types, of `addPhase`/`addSystem`/`inSet`, of the built-in phase and schedule types,
  of the resource insertion call, and of the C API ordering call.
- How a `Query` parameter tells filter types from component types, and whether `Query` is the `ecs` type or an `app`
  wrapper over it.
- The data structure of the conflict check (bitsets over component and resource ids are the obvious one) and of the
  topological order.
- What `physics2d` takes over, under rule 7 and the world hash (§5.9).
- Where the `worldHash` of each scenario is recorded (the scenario's test, the result file, or both).
- How the negative compile check runs in CMake.
