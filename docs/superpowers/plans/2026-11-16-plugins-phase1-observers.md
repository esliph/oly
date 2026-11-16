# Plugins, phase 1 (ECS observers and implied components) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The `World` runs lifecycle observers (on add, on remove; destroy counts as remove) in every structural path, and a
component can imply others that join it in the same transition.

**Architecture:** A registry in the `World` keeps the observers per `ComponentId` and kind, and a table keeps the
implications and their closure. Both are cold data: the hot path reads only an `observed` byte cached in each `Edge` and
`Archetype` when they are created, and an edge's list of implied columns to value-initialize. Observers run with the
`World` locked for structure (the iteration counter), so they can read and write values but record structural changes
in a `World`-owned `CommandBuffer`; the triggering operation applies it before it returns, until nothing is pending.
The flush computes the net change per entity as today, runs on remove observers before each entity's transition and on
add observers after the last transition.

**Tech Stack:** C++20, doctest, Google Benchmark, CMake presets, Docker through `./spur`.

**Spec:** `docs/superpowers/specs/2026-11-16-plugins-design.md` (§5, §11 step 1). The ECS contract it extends:
`docs/superpowers/specs/2026-10-16-ecs-archetype-spec.md` (R-CB-3, R-CB-4, I-9, NF-P4).

## Global Constraints

- The toolchain runs only in Docker, through `./spur <task>` from Git Bash. Every existing test passes after each task
  (`./spur test`, preset `debug-asan`).
- Spec §5.2: "With no observer on any component of the transition, the cost must be within noise of today's. The
  observed flag belongs where the hot path already looks [...] never a hash lookup per transition. NF-P4 stays met."
- Spec §5.3 cost target: "with no hierarchy in use, spawn and flush make the same number of transitions as today, and
  NF-P4 stays met".
- Anti-goal: "A hot-path regression when no observer is registered: NF-P4 [...] must stay met, and iteration must not
  change."
- `CLAUDE.md`: a change to `table`, `archetype`, `query*`, `command_buffer` or `world` keeps `checkInvariants` covering
  it and runs the `spec-conformance-reviewer` and `perf-reviewer` agents.
- New tests follow the `add-test-case` skill. New contract violations get a death case in `ecs.contract_death` (or
  `app.contract_death`).
- `OLY_ASSERT` messages include the operation, the `Entity` (`e=%u:%u`) and the type name.
- Glossary rule: same verb across surfaces, no alias, no compatibility `using`.
- Commits, comments and assert messages in English. Commit subject: one short line on what changes in the repository,
  conventional prefix as in `git log`; never cite the spec, the plan or task numbers.
- `.cpp`/`.hpp` are formatted by the hook; do not format by hand.
- Comments only for a reason, a non-local invariant or a contract. Add no step-by-step or edit-narrating comment.
- Public API changes update `docs/guide/` in the same PR (Task 6), following `docs/development/guide-style.md`.

**New names (decided here):**

| Name | Where | What |
|---|---|---|
| `ObserverId`, `ObserverKind { OnAdd, OnRemove }` | `oly/ecs/observer.hpp` | handle and kind |
| `ObserverContext` | `oly/ecs/observer.hpp` | `world()`, `entity()`, `componentId()`, `userData()`, `commands()` |
| `ObserverFunction` | `oly/ecs/observer.hpp` | `void (*)(ObserverContext& context)` |
| `World::onAdd<T>`, `World::onRemove<T>` | `world.hpp` | `ObserverId (ObserverFunction, void* userData = nullptr)` |
| `World::observeById` | `world.hpp` | `ObserverId (ObserverKind, ComponentId, ObserverFunction, void* userData = nullptr)` |
| `World::removeObserver` | `world.hpp` | `void (ObserverId)` |
| `World::imply<T, Implied>` | `world.hpp` | `void ()` |
| `detail::ObserverRegistry` | `oly/ecs/detail/observer_registry.hpp`, `src/observer_registry.cpp` | lists per component and kind, mask per component |
| `detail::ImplicationTable` | `oly/ecs/detail/implication_table.hpp`, `src/implication_table.cpp` | direct implications, closure, value-initializers |
| `detail::ObservedOnAdd = 1`, `detail::ObservedOnRemove = 2` | `observer_registry.hpp` | bits of the cached `observed` byte |
| requirements `R-OB-1..8`, `R-IM-1..4`; invariants `I-12`, `I-13` | ECS spec (Task 6) | below |

**Requirements this plan implements (written into the ECS spec in Task 6; test names cite them):**

- **R-OB-1.** On add runs after the component is in place: the observer reads the new value.
- **R-OB-2.** On remove runs before the component leaves: the observer reads the old value.
- **R-OB-3.** `destroy` runs on remove for every observed component of the entity, in `ComponentId` order, before the
  entity dies.
- **R-OB-4.** `add` of a data component the entity already has runs on remove (old value) and then on add (new value).
  `add` of a tag the entity already has runs nothing.
- **R-OB-5.** Direct field writes (through `get`, `tryGet` or a query) are not observed.
- **R-OB-6.** An observer changes structure only through `context.commands()`. Those commands are applied right after
  the operation that triggered the observer, before it returns, until nothing is pending. A direct structural change
  or an observer registration inside an observer is a contract error, the same one as inside `each`.
- **R-OB-7.** Inside a flush, observers see the net change per entity (R-CB-3, R-CB-4). On remove observers (destroy
  included) run before each entity's transition; on add observers run after every transition of the flush, in the order
  of each entity's first command, and within one entity in `ComponentId` order. Stale handles run nothing.
- **R-OB-8.** Observers of the same component and kind run in registration order. Registering or removing an observer
  during an iteration or inside an observer is a contract error; removing an unknown or removed `ObserverId` too.
- **R-IM-1.** `imply<T, Implied>()` is registered before any archetype contains `T`; later is a contract error, and
  `T == Implied` does not compile. Implications are transitive; a cycle is valid and closes to one set.
- **R-IM-2.** Adding `T` (direct `add`, `addById`, `spawn<Ts...>`, flush) adds the closure of `T` in the same
  transition. A component of the closure that the entity lacked is value-initialized and runs its on add observers
  like any other.
- **R-IM-3.** Removing `T` also removes each component of its closure that no component left in the entity implies.
  An implied component leaves with its last implier, even if it was added explicitly.
- **R-IM-4.** Removing a component while a component that implies it stays is a contract error.
- **I-9 (changed).** Every edge `a --±c--> b` satisfies `b.signature == resolve(a.signature ± c)` (the closure rules
  above), its `sourceToDestination` is consistent and its `initializedColumns` are exactly the destination columns of
  the components that `a` lacks, minus `c`.
- **I-12 (new).** The `observed` byte of every archetype and cached edge matches the registered observers.
- **I-13 (new).** Every archetype signature is closed under implication.

**Decided here (not in the spec):**

- **Observers run under the iteration lock.** While observers run, `iterating_` is incremented with the existing
  `detail::IterationGuard`, so a direct structural change inside an observer fails the existing asserts at zero new
  cost on the hot path. I-11 still holds between public calls.
- **The deferred buffer.** The `World` owns two `CommandBuffer`s, `deferred_` (what observers record into) and
  `applying_`. A private `applyDeferred()` loops `while (!deferred_.empty()) { std::swap(deferred_, applying_); flushWithoutDeferred(applying_); }`.
  Public operations call it only on the paths where an observer ran. A cycle of observers that always records more
  commands never ends; this is the contract of "until nothing is pending" and is documented, not guarded.
- **Observer function shape.** One context argument, like `SystemFunction` and `StateHookFunction`. The context gives
  no value pointer: the observer reads with `context.world().get<T>(context.entity())`.
- **Flags.** `Edge::observed` holds `ObservedOnAdd` if a component the edge adds has on add observers and
  `ObservedOnRemove` if a component it removes has on remove observers. `Archetype::observed` holds the same bits over
  its whole signature (used by `destroy` and `spawn<Ts...>`). Both are set when the edge or archetype is created and
  recomputed over every archetype and cached edge when an observer is registered or removed (cold). Jump edges carry no
  flag: the flush reads the bits from its memo.
- **Value-initialization of implied components.** `imply<T, Implied>()` stores, per implied `ComponentId`, a function
  `void (*)(void* slot) noexcept` that runs `::new (slot) Implied()`. `Implied` must be nothrow default constructible
  (`static_assert`). `TypeInfo` does not change.
- **One signature resolver.** `ImplicationTable::resolve(const Signature& source, Signature changed) const` applies
  R-IM-2 and R-IM-3 to a signature after the raw operations: first it removes, transitively, each implied component
  none of whose impliers is left in `changed` while some implier was in `source`; then it adds the closure of
  everything in `changed`. `edgeFor`, `spawn<Ts...>` and the flush all call it, so the three paths cannot disagree.
  With no implication registered it returns `changed` untouched.
- **R-IM-4 is checked where the edge is built.** `edgeFor` for a removal, and the flush memo when it computes a new
  destination, assert that no component left in the signature implies the removed one. That runs once per (archetype,
  component) and costs nothing on the cached path; with asserts off the removal proceeds and I-13 reports it.
- **State scope cleanup skips dead entities.** `Engine::clearStateScope` collects the scoped entities and then destroys
  them one by one; an observer of one of them may destroy another first. It now destroys only the ones still alive.
- **The test oracle.** `flushSequential` applies each command through the direct calls, so with observers it runs the
  sequential semantics. The fuzz keeps running without observers; the flush semantics of R-OB-7 are pinned by unit
  tests.
- **Stats.** `ArchetypeStats::edgeBytes` also counts the capacity of `initializedColumns` (NF-M2).

## Review Focus

1. **A cascade that re-enters a destroyed entity.** An on remove observer of a parent records the destroy of a child,
   and another observer, or the buffer, already destroyed that child. Expected: the stale handle is ignored, as R-CB-4
   ignores stale commands, and no assert fires. Pinned in Task 1 and Task 2.
2. **The state scope cleanup meeting a cascade.** Two scoped entities, and an observer of the first destroys the second.
   Expected: the cleanup does not destroy a dead entity. Pinned in Task 4.
3. **An add and a remove of the same component in one buffer.** Expected: no observer runs, and repeated adds run one on
   add with the last value. Pinned in Task 2.
4. **An on add observer in a flush reading another entity of the same buffer.** Expected: it sees the world after the
   whole buffer, whatever the recording order (a parent spawned later in the buffer is already in place). Pinned in
   Task 2.
5. **A registered observer on another component slowing the hot path.** Expected: NF-P4 holds for an edge whose
   components have no observer, even with observers registered elsewhere and with an implication registered for other
   components. Pinned in Task 5.

## File Structure

| File | Responsibility |
|---|---|
| `engine/runtime/ecs/include/oly/ecs/observer.hpp` (new) | `ObserverId`, `ObserverKind`, `ObserverContext`, `ObserverFunction` |
| `engine/runtime/ecs/include/oly/ecs/detail/observer_registry.hpp`, `src/observer_registry.cpp` (new) | registry and mask per component |
| `engine/runtime/ecs/include/oly/ecs/detail/implication_table.hpp`, `src/implication_table.cpp` (new) | implications, closure, `resolve`, value-initializers |
| `engine/runtime/ecs/include/oly/ecs/detail/archetype.hpp`, `src/archetype.cpp` | `Edge::initializedColumns`, `Edge::observed`, `Archetype::observed` |
| `engine/runtime/ecs/include/oly/ecs/world.hpp`, `src/world.cpp` | registration API, triggers in every path, deferred buffers, I-9/I-12/I-13 |
| `engine/runtime/ecs/src/diagnostics.cpp` | `edgeBytes` with `initializedColumns` |
| `engine/runtime/ecs/tests/observers.cpp`, `implications.cpp` (new), `contract_death.cpp`, `unit_world.cpp`, `CMakeLists.txt` | tests |
| `engine/runtime/ecs/bench/flush.cpp`, `transitions.cpp` | implication benchmarks |
| `engine/runtime/app/src/engine_states.cpp`, `tests/state_scope.cpp` | cleanup skips dead entities |
| ECS spec, `docs/architecture/{ecs,limits,decisions,glossary}.md`, `docs/guide/` | Task 6 |

---

### Task 1: Observers on direct `World` calls

**Files:**
- Create: `engine/runtime/ecs/include/oly/ecs/observer.hpp`, `engine/runtime/ecs/include/oly/ecs/detail/observer_registry.hpp`, `engine/runtime/ecs/src/observer_registry.cpp`
- Modify: `engine/runtime/ecs/include/oly/ecs/detail/archetype.hpp`, `engine/runtime/ecs/src/archetype.cpp`, `engine/runtime/ecs/include/oly/ecs/world.hpp`, `engine/runtime/ecs/src/world.cpp`
- Test: `engine/runtime/ecs/tests/observers.cpp` (new suite `ecs.observers`), `engine/runtime/ecs/tests/contract_death.cpp`, `engine/runtime/ecs/tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  namespace oly::ecs {
    using ObserverId = uint32_t;
    enum class ObserverKind : uint8_t { OnAdd, OnRemove };
    class ObserverContext;  // world(), entity(), componentId(), userData(), commands(); valid only during the call
    using ObserverFunction = void (*)(ObserverContext& context);

    // World
    template <class T> ObserverId onAdd(ObserverFunction function, void* userData = nullptr);
    template <class T> ObserverId onRemove(ObserverFunction function, void* userData = nullptr);
    ObserverId observeById(ObserverKind kind, ComponentId componentId, ObserverFunction function, void* userData = nullptr);
    void removeObserver(ObserverId observer);
  }
  namespace oly::ecs::detail {
    inline constexpr uint8_t ObservedOnAdd = 1, ObservedOnRemove = 2;
    class ObserverRegistry {
     public:
      ObserverId add(ObserverKind kind, ComponentId componentId, ObserverFunction function, void* userData);
      void remove(ObserverId observer);            // OLY_ASSERT on an unknown or removed id
      uint8_t mask(ComponentId componentId) const noexcept;  // 0 for an id never observed
      // calls function(entry) for each observer of (kind, componentId), in registration order
    };
    // Edge gains: uint8_t observed = 0;  Archetype gains: uint8_t observed = 0;
  }
  ```
  The `ObserverId` stays valid until `removeObserver`; ids are never reused.

- [ ] **Step 1: Write the failing tests** in `observers.cpp` (register the suite with `oly_add_test(MODULE ecs NAME observers SOURCES observers.cpp)`). Use a small recorder: a `std::vector<std::string>` in `userData` where each observer appends `"<kind>:<entity index>:<value read through get>"`. Each test ends with `CHECK_INVARIANTS(world)`.

  ```cpp
  TEST_CASE("R-OB-1: on add reads the new value");          // add(e, A{7}) → log == {"add:e:7"}
  TEST_CASE("R-OB-2: on remove reads the old value");       // remove<A>(e) → {"remove:e:7"}; has<A>(e) false afterwards
  TEST_CASE("R-OB-3: destroy runs on remove for every observed component in ComponentId order");
                                                             // observers on A and B; destroy → {"remove A", "remove B"}; isAlive false
  TEST_CASE("R-OB-4: add of an existing component runs on remove with the old value, then on add with the new one");
                                                             // add(e, A{1}); add(e, A{2}) → {"add:1", "remove:1", "add:2"}
  TEST_CASE("R-OB-4: add of an existing tag runs nothing");
  TEST_CASE("R-OB-5: a write through get is not observed");
  TEST_CASE("spawn<Ts...> runs on add for each observed component after every value is in place");
                                                             // the A observer reads B through get and sees B's value
  TEST_CASE("R-OB-6: commands recorded by an observer are applied before the operation returns");
                                                             // on remove of A destroys a second entity via context.commands();
                                                             // after destroy(first), isAlive(second) is false
  TEST_CASE("R-OB-6: a chain of recorded destroys runs until nothing is pending");
                                                             // three entities linked by a component holding the next Entity
  TEST_CASE("R-OB-6: a recorded command for an entity that died meanwhile is ignored");
  TEST_CASE("R-OB-8: observers of the same component and kind run in registration order");
  TEST_CASE("removeObserver stops the observer and the flags follow");
                                                             // after removeObserver, add runs nothing; CHECK_INVARIANTS covers I-12
  TEST_CASE("an observer registered after the edge was cached still runs");
                                                             // add/remove A once, then onAdd<A>, then add → observer runs
  ```

  Death cases in `contract_death.cpp`, added to `CASES` in `tests/CMakeLists.txt`, each with its required substrings:

  ```cpp
  structural_change_in_observer   // world.add inside an on add observer → "add e=" and the type name
  observe_during_each             // onAdd<A> inside query<A>().each → "onAdd" and "iteration"
  observe_in_observer             // onAdd<B> inside an observer → "onAdd" and "iteration"
  remove_unknown_observer         // removeObserver(12345) → "removeObserver" and "12345"
  ```

- [ ] **Step 2: Run the tests to verify they fail**

  Run: `./spur test -R 'ecs\.(observers|contract_death)'`
  Expected: build error, `onAdd` is not a member of `oly::ecs::World`.

- [ ] **Step 3: Implement**

  - `ObserverRegistry` as specified above, in `detail/observer_registry.hpp` and `src/observer_registry.cpp`: per kind a
    `std::vector<std::vector<Entry>>` indexed by `ComponentId`, a `std::vector<uint8_t>` of masks, and a vector indexed
    by `ObserverId` with `(kind, componentId, removed)` for `remove`.
  - `World` members: `detail::ObserverRegistry observers_`, `CommandBuffer deferred_`, `CommandBuffer applying_`
    (declared after `store_`, constructed with it).
  - `observeById` and `removeObserver` assert `iterating_ == 0` (message names the operation, the type name and
    `iteration=%u`), update the registry and call a private `refreshObservedFlags()` that rewrites
    `Archetype::observed` and `Edge::observed` of every archetype and cached add/remove edge. `findOrCreate` and
    `edgeFor` set the byte when they create an archetype or an edge.
  - Triggers: `addById` (absent: after `moveEntity` and the value, if `edge.observed & ObservedOnAdd`, run on add for
    the components in `edge.target->signature` that are not in the source; present data component: on remove, assign,
    on add, each only if `observers_.mask(componentId)` has the bit), `removeById` (if `edge.observed &
    ObservedOnRemove`, run on remove before `moveEntity`), `destroy` (if `archetype.observed & ObservedOnRemove`, run on
    remove for each observed component of the signature before `destroyRow`), `spawn<Ts...>` (after every value is
    constructed, if `archetype->observed & ObservedOnAdd`). Each observed path ends with `applyDeferred()`.
  - Observers run inside one `detail::IterationGuard guard(&iterating_)` per batch of calls.
  - `checkInvariants` gets `checkObservedFlags` (I-12): recompute each byte from the registry and compare; failure
    reports `"I-12 arch=<id>"` or `"I-12 add cid=<c> arch=<id>"`.

- [ ] **Step 4: Run the tests to verify they pass**

  Run: `./spur test -R '^ecs\.'`
  Expected: every `ecs.*` test passes, including `ecs.unit_world` (NF-P4) and the four new death cases.

- [ ] **Step 5: Commit**

  ```bash
  git add engine/runtime/ecs
  git commit -m "feat(ecs): run lifecycle observers on direct World changes"
  ```

### Task 2: Observers inside the flush

**Files:**
- Modify: `engine/runtime/ecs/src/world.cpp` (`FlushMemo`, `flush`, the deferred loop)
- Test: `engine/runtime/ecs/tests/observers.cpp`

**Interfaces:**
- Consumes: Task 1's registry, `applyDeferred()`, the guard.
- Produces: `World::flush` with R-OB-7. `FlushMemo` gains `uint8_t observed` (bits for the components the memo's
  operations add, assign or remove) computed in `assign`; the `World` gains a reused
  `std::vector<std::pair<Entity, ComponentId>> pendingOnAdd_`.

- [ ] **Step 1: Write the failing tests**

  ```cpp
  TEST_CASE("R-OB-7: an add followed by a remove in one buffer runs no observer");
  TEST_CASE("R-OB-7: repeated adds in one buffer run one on add with the last value");     // {"add:3"}
  TEST_CASE("R-OB-7: an existing component added again in a buffer runs on remove (old) then on add (new)");
  TEST_CASE("R-OB-7: destroy in a buffer runs on remove with the entity still complete");
  TEST_CASE("R-OB-7: spawn and destroy in the same buffer run nothing");
  TEST_CASE("R-OB-7: on remove runs before each transition, on add after the last one");
            // two entities; the log is {"remove e1", "remove e2", "add e1", "add e2"}
  TEST_CASE("R-OB-7: an on add observer sees the world after the whole buffer");
            // e1 recorded first gets A whose observer reads B of e2, which the buffer adds later; it reads e2's new B
  TEST_CASE("R-OB-7: commands for a stale handle run nothing");
  TEST_CASE("R-OB-6: commands an observer records during a flush are applied before flush returns");
  ```

- [ ] **Step 2: Run the tests to verify they fail**

  Run: `./spur test -R 'ecs\.observers'`
  Expected: the new cases fail (no observer runs inside `flush`).

- [ ] **Step 3: Implement** in `flush`: compute `memo.observed` in `assign` from the operations (present and absent in
  the source → on add; present and existed → both; absent and existed → on remove; tags that existed and are added again
  → none). Per entity: destroyed and alive → run on remove for the observed components of its archetype, then destroy;
  otherwise, if `memo.observed & ObservedOnRemove`, run on remove before `transitionTo`; if `memo.observed &
  ObservedOnAdd`, append `(entity, componentId)` pairs to `pendingOnAdd_` after `writeValues`. After the loop and
  `commandBuffer.clear()`, run the on add observers of `pendingOnAdd_` for entities still alive, clear it, then
  `applyDeferred()`. The deferred loop calls a private `flushWithoutDeferred(CommandBuffer&)` that holds the body of
  today's `flush` plus these observer steps; the public `flush` is that call followed by `applyDeferred()`.

- [ ] **Step 4: Run the tests to verify they pass**

  Run: `./spur test -R '^ecs\.'`
  Expected: all pass, including `ecs.fuzz_vs_model` and `ecs.command_buffer`.

- [ ] **Step 5: Commit**

  ```bash
  git add engine/runtime/ecs
  git commit -m "feat(ecs): run observers on the net change of a flush"
  ```

### Task 3: Implied components

**Files:**
- Create: `engine/runtime/ecs/include/oly/ecs/detail/implication_table.hpp`, `engine/runtime/ecs/src/implication_table.cpp`
- Modify: `detail/archetype.hpp`, `src/archetype.cpp` (`buildEdge` fills `initializedColumns`), `world.hpp`, `src/world.cpp`, `src/diagnostics.cpp`
- Test: `engine/runtime/ecs/tests/implications.cpp` (new suite `ecs.implications`), `contract_death.cpp`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 and 2 triggers (implied components run on add).
- Produces:
  ```cpp
  // World
  template <class T, class Implied> void imply();  // static_assert: distinct types, Implied nothrow default constructible
  namespace oly::ecs::detail {
    using ValueInitializer = void (*)(void* slot) noexcept;
    class ImplicationTable {
     public:
      void add(ComponentId implying, ComponentId implied, ValueInitializer initializer);
      bool empty() const noexcept;
      std::span<const ComponentId> impliersOf(ComponentId implied) const noexcept;
      Signature resolve(const Signature& source, Signature changed) const;  // R-IM-2, R-IM-3; changed untouched when empty()
      ValueInitializer initializerOf(ComponentId implied) const noexcept;
    };
    // Edge gains: std::vector<uint16_t> initializedColumns;  // destination columns to value-initialize
  }
  ```

- [ ] **Step 1: Write the failing tests** (components `Mark { uint32_t value = 0; }`, `Derived { float value = 5.f; }`,
  `Chained { uint32_t value = 9; }`; value-initialization keeps the default member initializers):

  ```cpp
  TEST_CASE("R-IM-2: add brings the implied component in the same transition, value-initialized");
            // imply<Mark, Derived>; add(e, Mark{1}) → has<Derived>, get<Derived>(e).value == 5.f;
            // archetypeCount == 2 (root + {Mark, Derived}: no intermediate archetype)
  TEST_CASE("R-IM-2: spawn<Ts...> brings the closure");      // spawn(Mark{1}) → Derived present
  TEST_CASE("R-IM-2: a flush brings the closure in one transition");
            // buffer spawn + add Mark → archetypeCount == 2 after flush
  TEST_CASE("R-IM-2: an implied component the entity already has keeps its value");
  TEST_CASE("R-IM-2: implications are transitive");          // imply<Mark, Derived>, imply<Derived, Chained>
  TEST_CASE("R-IM-2: an implied component runs its on add observers after the implying one");
            // log {"add Mark", "add Derived"}; the Derived observer reads Mark's value
  TEST_CASE("R-IM-3: removing the implying component removes what it implied");
  TEST_CASE("R-IM-3: an implied component stays while another implier stays");
            // imply<Mark, Derived>, imply<Other, Derived>; remove<Mark> → Derived stays
  TEST_CASE("R-IM-3: an explicitly added implied component leaves with its last implier");
  TEST_CASE("with no implication registered, resolve returns the signature untouched");  // unit test of ImplicationTable
  ```

  Death cases: `imply_after_archetype` (spawn `Mark`, then `imply<Mark, Derived>` → `"imply"` and `"Mark"`),
  `remove_implied_component` (`remove<Derived>` while `Mark` stays → `"remove e="`, `"Derived"` and `"implied by"`).
  `imply<Mark, Mark>` is a `static_assert`, not a death case. A cycle (`imply<Mark, Derived>` and `imply<Derived, Mark>`
  before any entity) is valid: the closure is a set. Add the test `TEST_CASE("R-IM-1: a cycle of implications closes
  to the same set")`.

- [ ] **Step 2: Run the tests to verify they fail**

  Run: `./spur test -R 'ecs\.(implications|contract_death)'`
  Expected: build error, `imply` is not a member of `oly::ecs::World`.

- [ ] **Step 3: Implement**
  - `imply` asserts `iterating_ == 0` and `componentIndex_[componentId<T>()]` empty (R-IM-1), registers in
    `implications_` with `[](void* slot) noexcept { ::new (slot) Implied(); }`.
  - `edgeFor` and the flush memo get the destination from `implications_.resolve(source.signature, changed)`. R-IM-4
    asserts there (see "Decided here").
  - `buildEdge(source, destination, added)` fills `initializedColumns` with the destination columns of data components
    absent from the source, minus `added`. `moveEntity` callers run each `initializerOf` on those columns after the
    move; the flush memo keeps the same list per (source, operations) and runs it after `writeValues` for the
    destination columns no operation wrote.
  - `spawn<Ts...>` resolves the signature only when `!implications_.empty()` and value-initializes the columns not in
    `Ts`.
  - On add for implied components: the edge diff of Task 1 already includes them; order is the added component first,
    then the rest in `ComponentId` order.
  - `checkInvariants`: `checkCachedEdges` compares with `resolve` and checks `initializedColumns` (I-9); a new
    `checkImplications` checks I-13 per archetype.
  - `diagnostics.cpp`: `edgeBytes` adds `initializedColumns.capacity() * sizeof(uint16_t)`.

- [ ] **Step 4: Run the tests to verify they pass**

  Run: `./spur test -R '^ecs\.'`
  Expected: all pass.

- [ ] **Step 5: Commit**

  ```bash
  git add engine/runtime/ecs
  git commit -m "feat(ecs): let a component imply others in the same transition"
  ```

### Task 4: State scope cleanup with observers

**Files:**
- Modify: `engine/runtime/app/src/engine_states.cpp` (`clearStateScope`)
- Test: `engine/runtime/app/tests/state_scope.cpp`

**Interfaces:**
- Consumes: Task 1 (`onRemove`, `context.commands()`).

- [ ] **Step 1: Write the failing test**

  ```cpp
  TEST_CASE("the scope cleanup runs on remove observers and tolerates a cascade");
  // Two scoped entities a and b; a has Link{b}; onRemove<Link> records destroy(b).
  // After the state change: both dead, the observer ran once, CHECK_INVARIANTS(world).
  ```

- [ ] **Step 2: Run the test to verify it fails**

  Run: `./spur test -R 'app\.state_scope'`
  Expected: abort on `destroy e=... during iteration=0` (destroy of the already dead `b`).

- [ ] **Step 3: Implement:** in `clearStateScope`, destroy only `world_.isAlive(entity)`.

- [ ] **Step 4: Run the tests to verify they pass**

  Run: `./spur test -R '^app\.'`
  Expected: all pass.

- [ ] **Step 5: Commit**

  ```bash
  git add engine/runtime/app
  git commit -m "fix(app): skip entities an observer already destroyed in the scope cleanup"
  ```

### Task 5: Cost validation

**Files:**
- Modify: `engine/runtime/ecs/tests/unit_world.cpp`, `engine/runtime/ecs/bench/flush.cpp`, `engine/runtime/ecs/bench/transitions.cpp`

**Interfaces:**
- Consumes: Tasks 1–3.

- [ ] **Step 1: Write the NF-P4 tests**, next to the existing one and with the same warm-up and counters:

  ```cpp
  TEST_CASE("NF-P4: add/remove with a cached edge stays free with observers on other components");
            // onAdd<A>, onRemove<B> registered; the C1..C4 loop → allocations and lookups unchanged
  TEST_CASE("NF-P4: add/remove with a cached edge stays free with an implication");
            // imply<C4, C5>; add C4 / remove<C4> loop → allocations and lookups unchanged, C5 present after each add
  ```

- [ ] **Step 2: Run them**

  Run: `./spur test -R 'ecs\.unit_world'`
  Expected: PASS (they pin Tasks 1–3; a failure here is a regression to fix in the code, not in the test).

- [ ] **Step 3: Add the benchmarks** `BM_FlushImplied/<N>` in `flush.cpp` (same shape as `BM_FlushCoalesced`, where one
  of the three added components implies a fourth) and `BM_TransitionImplied` in `transitions.cpp` (the add/remove pair
  of the existing transition benchmark with an implication on the added component). They report the per-entity cost of
  an implication; they are measured and reported, with no target.

- [ ] **Step 4: Measure before and after.** Build `release` at the base commit in a worktree
  (`git worktree add ../oly-base <base>`) and at `HEAD`. Run `ecs_bench_transitions`, `ecs_bench_flush` and
  `ecs_bench_iterate` with `--benchmark_repetitions=10 --benchmark_report_aggregates_only=true`, alternating base and
  head twice (base, head, base, head). Report, per existing benchmark, the median of each run and Δ head/base.
  Acceptance: every existing benchmark's Δ lies within the spread between the two base runs, and the NF-P4 tests pass.
  Then ask the user to run `/bench-compare <prefix>` (the skill is user-invoked) for the NF-P1, NF-P2 and NF-P5 ratios.

- [ ] **Step 5: Run the reviewer agents** `perf-reviewer` and `spec-conformance-reviewer` on the branch diff (rule of
  `CLAUDE.md`), fix what they confirm, and run `./spur test` again.

- [ ] **Step 6: Commit**

  ```bash
  git add engine/runtime/ecs
  git commit -m "test(ecs): pin NF-P4 with observers and implications and measure implied transitions"
  ```

### Task 6: Docs

**Files:**
- Modify: `docs/superpowers/specs/2026-10-16-ecs-archetype-spec.md` (§4.7 or a new §4.8 "Observers and implications" with R-OB-1..8 and R-IM-1..4; §6 I-9 changed, I-12, I-13; §5 API table rows), `docs/architecture/ecs.md`, `docs/architecture/limits.md` (direct writes not observed; observer cycles; implied components leave with their last implier), `docs/architecture/decisions.md` (observers under the iteration lock; flags cached in edges), `docs/architecture/glossary.md` (observer, on add, on remove, implied component), `docs/guide/concepts/world-and-entities.md` (a section "React to changes: observers" and one on implications), `docs/guide/cheat-sheet.md`, `docs/guide/troubleshooting.md` (the new asserts)

- [ ] **Step 1:** Read `docs/development/guide-style.md` and write the docs above. The guide section starts with what
  the reader can do ("run code when an entity gains or loses a component"), shows one C++ example with `onAdd<T>` and
  `context.commands()`, and states the contract: when each kind runs, what a flush changes, direct writes not observed,
  structural changes only through `context.commands()`.
  The ECS spec also gets the cost target of plugins spec §5.3 in §7 ("with no hierarchy in use, spawn and flush make
  the same number of transitions as today, and NF-P4 stays met"), with the Task 5 measurement as its status.
- [ ] **Step 2:** Search `docs/` for "I-9" and for the old I-9 wording and update each mention.
- [ ] **Step 3:** Run `./spur test`, `./spur native-games-test`, `./spur games-test` and `./spur windows-test` (spec
  §11: each step ends green on `native`, `web` and the Windows cross-build). Expected: all pass.
- [ ] **Step 4: Commit**

  ```bash
  git add docs
  git commit -m "docs: describe ECS observers and implied components"
  ```
