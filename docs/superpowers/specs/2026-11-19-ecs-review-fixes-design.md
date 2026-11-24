# Oly — fixes from the ECS architecture review — design

Date: 2026-11-19. Base: `master` at `bb33c3e` (the commit the review covered).
Input: the architecture review of the ECS module (2026-11-19), findings 1 to 4. Specs that remain valid:
`2026-10-16-ecs-archetype-spec.md` (NF-P1..NF-P7, R-IM-1), `2026-11-13-multi-environment-design.md`.

Marking: **[user decision]** was decided in the conversation; **[author decision]** was my choice during the design,
which the user may review; the rest is a direct consequence of the two. Performance claims carry **[fact]**,
**[inference]** or **[hypothesis]**; a hypothesis names the measurement that settles it.

## 1. Intent

Oly is no longer a study project: it is a game engine for real games, meant to run in several environments
**[user decision]**. A cost that was a curiosity in a study is now paid by every game, every frame. The review found
that the per-frame path a system takes allocates, and that the benchmark targets do not measure that path. This work
fixes the four findings and makes the targets mean what they say, in C++ and in the TypeScript client
**[user decision]**.

## 2. What the review found, checked against the code

| Finding | Verdict | What the code shows |
|---|---|---|
| 1. `World::query()` allocates on every call | confirmed **[fact]** | `world.hpp:365-385` builds `with` (two allocations when a filter is pushed after the components), `QueryKey::components` and a copy of `with`, then searches a `std::map` comparing vectors. The template (`templates/game/common/native/movement.cpp`), `essentials2d`'s propagation and the README call `context.query<...>()` inside the system, every step. `docs/how-to/keep-the-hot-path-fast.md` §2 calls this "a map lookup" and does not mention the allocations. |
| 2. The benchmark does not measure the idiom; the baseline is not equivalent | confirmed **[fact]** for the idiom; **[inference]** for the baseline | `BM_IterateSingle_ECS` takes the query reference before the loop. `BM_IterateSingle_SoA` reads and writes through two `float3*`, which may alias; the ECS loop goes through `Position*` and `const Velocity*`, distinct types, and 64-byte aligned columns. Whether this explains the ratio of ~0.82 is a hypothesis settled by §5.3. |
| 3. `spawn<Ts...>` allocates and hashes | confirmed **[fact]** | `world.hpp:257-260` builds a `Signature` and calls `findOrCreate`, which looks it up in `bySignature_`. |
| 4. Memory only grows | confirmed **[fact]** | `Table::reserve` only grows; `decisions.md` records that archetypes are never removed, not that tables never shrink. |
| (not in the review) TS `world.query(spec)` allocates on a cache hit | **[fact]** | `world.ts:138-170` builds a `Set`, spreads, maps, sorts and joins a string key before the `Map` lookup. The idiom (`templates/game/web/web/src/game.ts`, `docs/how-to/typescript/gameplay-recipes.md`) calls it every step. |
| (not in the review) TS `eachBlock` allocates per block | **[fact]** | `query.ts:161-175` creates a `Block`, a `subarray`, a `Map` of views and one view per type read, per block. |

The C API does not have finding 1: `olyQueryCreate` runs once and the client keeps the handle **[fact]**
(`world.ts:167-177`).

## 3. Goals, non-goals, anti-goals

**Goals**

1. In C++, `query<...>()` and `spawn<Ts...>` called again with the same types do no allocation and no signature hash
   lookup, with no change to the public API.
2. A new target, **NF-P8**, states goal 1 and is enforced by tests in the style of NF-P4.
3. The iteration benchmark measures the idiom (a `query()` inside the measured loop) against a fair baseline, so that
   NF-P1 and NF-P2 measure what they claim.
4. In TypeScript, `world.query(spec)` on a cache hit allocates nothing on the engine's side (**NF-P9**).
5. The garbage the TS client produces per frame is measured in a scene shaped like the template game, and the
   result decides whether reusing `Block` (T2, §4) is worth its cost.
6. `decisions.md` records that tables never shrink.

**Non-goals**

- Reusing `Block` and its views in TS (T2). It enters only if §5.6 shows the cost **[user decision]**.
- A public `shrink` API **[author decision]**. It would exist in C++, the C API and TS;
  it is worth adding when a game shows a peak that stays reserved.
- `QueryById`/`olyQueryCreate` allocations: they happen once per query shape, not per frame.
- Zero allocation in `flush`, the path a system's deferred `spawn` takes (§5.2, alternative B).
- The review's minor points: `get<T>` through `columnOf`, and `isMatched` being linear (only the invariants use it).
- Rewording `CLAUDE.md` and the docs that still call Oly a "study and benchmark project": a separate change.

**Anti-goals**

- Relaxing NF-P1 or NF-P2 because the fair baseline makes the ratio worse. If a ratio goes above its target, it is a
  real cost to investigate, recorded as "not met" with the numbers, as NF-P5 is today **[author decision]**.
- A fast path that can return a `Query` of the wrong type, or an archetype from another `World`.
- Weakening the TS `Block` validity contract ("block used outside the eachBlock callback").

## 4. Approaches

**C++ query and spawn.** One sensible way **[author decision]**:

- **Chosen — a static slot per type instantiation, in front of the existing lookup.** Each instantiation gets a
  program-wide slot number on first use; each `World` keeps a vector indexed by slot. A filled slot returns at the
  cost of a load and a branch; an empty one takes the current path once and fills the slot.
- Rejected — an allocation-free key in the same `std::map` (inline arrays): it removes the allocations but keeps
  O(log n) vector comparisons per call, with more code than the slot.
- Rejected — documenting "take the reference once": the template, the README and `essentials2d` call `query()` per
  step, and so will games.

**TypeScript** **[user decision: T3]**:

| | What changes | Cost |
|---|---|---|
| T1. Lookup only | The cache key becomes a walk by type identity, with no `Set`, spread or string on a hit | small |
| T2. Lookup + iteration | `Block`, views and the descriptor buffer reused per `Query` | medium; a `Block` kept outside the callback would silently point at the next block instead of throwing `ContractError` |
| **T3. T1 + measure (chosen)** | T1, plus a measurement of the garbage per frame of a template-shaped scene | small; T2 only if the measurement shows the cost |

V8 collects short-lived garbage cheaply, so whether per-block allocation matters in a real game is a
**[hypothesis]**; T2 would pay with a safety contract before that is shown.

## 5. Design

### 5.1 C++ query slot

- `World::query<Ts...>(Filters...)` looks up a slot keyed by the instantiation (the `Ts...` with their `const`, and
  the `Filters...` types), kept apart so that a component list and a filter list cannot form the same key.
- The slot number comes from an out-of-line function in the `ecs` library, the way `componentIdOf` gets its id from
  `registerType` (`component.cpp`), and is cached in a function-local `static` of the instantiation
  **[author decision]**. Reason: one counter per program however the translation units are linked; an `inline`
  variable in the header would give each shared module its own counter, and two types could share a slot, which turns
  the `static_cast` into undefined behaviour.
- Each `World` owns a `std::vector<detail::QueryCacheBase*>` indexed by slot, grown on the slow path only. Slots are
  program-wide and the vector is per `World`: two worlds never share an instance.
- The slow path is today's path, unchanged; it then stores the pointer in the slot. Two spellings of the same query
  (`With<A, B>` and `With<B, A>`) take two slots that point to the same instance, so the semantics do not change.
- `checkQueries` also checks that every filled slot points to an instance owned by `queryCache_` (`CLAUDE.md`: the
  invariants cover every change to `world`/`query*`).

Sketch of the fast path (names are the builder's to settle):

```cpp
template <class... Ts, class... Filters>
Query<Ts...>& World::query(Filters... filters) {
  const uint32_t slot = detail::querySlot<detail::TypeList<Ts...>, detail::TypeList<Filters...>>();

  if (slot < querySlots_.size() && querySlots_[slot]) [[likely]] {
    return static_cast<Query<Ts...>&>(*querySlots_[slot]);
  }

  Query<Ts...>& query = cachedQuery<Ts...>(filters...);
  rememberQuerySlot(slot, query);

  return query;
}
```

### 5.2 C++ spawn slot

- `spawn<Ts...>` looks up a slot keyed by `std::remove_cvref_t<Ts>...` (in the given order) holding the destination
  `Archetype*`, closed under implication. Same counter mechanism as §5.1, a separate counter and a separate vector per
  `World`.
- **No invalidation is needed** **[fact, from R-IM-1]**. `imply<T, Implied>` asserts when an archetype already
  contains `T` (`world.hpp:336`). A cached archetype exists and contains every component of its spawn, so no new
  implication can start from one of them; an implication that starts elsewhere does not change the closure. The
  review proposed invalidating the slot on `imply`; it is not needed.
- `initializeImplied` today receives the constructed components as a sorted `Signature`. It changes to take a
  `std::span<const ComponentId>`, built from a sorted `std::array<ComponentId, sizeof...(Ts)>` on the stack, so the
  fast path allocates nothing when implications exist.
- The slow path is today's path (`findOrCreate`/`findOrCreateClosure`); it then stores the archetype in the slot.
  `spawn(A{}, B{})` and `spawn(B{}, A{})` take two slots and reach the same archetype.
- The invariants check that every filled spawn slot points to an archetype of this `World`.
- **Scope: the direct `spawn` only** **[author decision; the user may choose B instead]**. A C++ system does not call
  `World::spawn<Ts...>`: `SystemContext::spawn<Ts...>` (`app/system.hpp`) records into the command buffer, and the
  phase's `flush` creates the entity **[fact]**. `flushWithoutDeferred` builds a local `FlushMemo` per flush and calls
  `findOrCreate(resolveSignature(...))` each time the shape changes from one entity to the next, so its cost is per
  flush and per shape change, not per entity **[inference, from `FlushMemo::matches`]**. This work adds a test that a
  flush of N spawns of one shape makes the same number of allocations and signature lookups for two values of N. If
  it does not, the alternative becomes the next design: **B**, a `World`-owned memo that survives between flushes plus
  a persistent (source, operations) → destination cache, the way edges serve `add`/`remove`; it touches
  `command_buffer` and the flush, and NF-P5 with it.

### 5.3 Benchmarks

- Both copies of `BM_IterateSingle_SoA` (`iterate.cpp`, the NF-P1 baseline, and `fragmentation.cpp`, the NF-P2
  baseline): the two arrays are 64-byte aligned and accessed through `__restrict` pointers.
- The ECS cases of NF-P1 and NF-P2 call `world.query<Position, const Velocity>()` inside the measured loop, as a
  system does **[author decision]**. The old cases, which take the reference before the loop, stay under a name that
  says so (for example `..._HeldQuery`), so the lookup cost is the difference between the two. The NF-P1 and NF-P2
  rows name the per-step cases as their numerators.
- A case for repeated `spawn<Ts...>` + `destroy` after warm-up, in `transitions.cpp` (it has transition cases only).
- Record a new run in `docs/reference/benchmarks/ecs/` and update the NF-P1/NF-P2 rows with the new ratios.
  **[hypothesis]** NF-P1 moves from ~0.82 towards 1.0 once the baseline can vectorize the same way; the run settles it.
  If a ratio crosses its target, the row says "not met" with the numbers (anti-goal above).

### 5.4 NF-P8

"`query<...>()` and `spawn<Ts...>` repeated with the same types, after the first call: zero allocations and zero
signature hash lookups." Tested in `engine/runtime/ecs/tests/` with `allocationCount()` and `signatureLookupCount()`,
as the NF-P4 tests in `unit_world.cpp` do, including: a query with filters, two spellings of the same query, two
`World`s with the same query shape, a `spawn` with an implication registered, and a `spawn` whose archetype has an
`onAdd` observer. A row is added to the targets table in `docs/reference/benchmarks/README.md`. The flush test of §5.2
lives beside these tests but is not part of NF-P8.

### 5.5 TS lookup without allocation (T1, NF-P9)

- On a hit, `World.query(spec)` walks `spec.read`, `spec.write`, `spec.with` and `spec.without` as given, by object
  identity of the schema types, through a nested `Map` (or an equivalent structure), and returns the `Query` with no
  allocation: no `?? []` that creates an array (a shared empty constant instead), no spread, `Set`, `map`, `sort` or
  string.
- On a miss, it runs today's path: validation, normalization, the string-keyed cache, `queryCreate`. Then it records
  the walk that led there. Two spellings of the same query reach the same `Query`.
- Validation runs on the miss only. A hit means the same sequence of types was validated before.
- NF-P9: "TS `world.query(spec)` on a cache hit allocates nothing on the engine's side." JavaScript has no exact
  allocation counter, so the evidence is weaker than NF-P8: the heap delta over many calls, with `--expose-gc` and a
  `gc()` before and after, compared with an empty loop **[author decision]**. It lives in the measurement of §5.6,
  not in CI, because heap deltas are noisy. Correctness (same instance on a hit, two spellings, validation errors still
  thrown on a miss) is tested in the existing TS test suites.

### 5.6 TS garbage measurement and the T2 decision

- A new benchmark in `engine/testing/bench/` (beside `loop.ts`), run with `./spur wasm node --expose-gc ...` against
  `wasm-games-release`, as `docs/how-to/contributor/run-benchmarks.md` §3 describes. The scene has the shape of the
  template game, scaled: about 20 systems mixing `eachBlock`, `single` and `trySingle`, over a few thousand entities
  in a few archetypes **[author decision]**.
- It reports bytes allocated per frame, per query call and per block, and the GC events per 10 000 frames with their
  duration (`perf_hooks`, `gc` entries).
- **T2 enters if** a frame allocates 64 KB or more, or a single GC pause reaches 1 ms, or GC takes 1 % or more of the
  frame time **[author decision]**. Reason for the numbers: at 60 fps, 64 KB per frame is about 4 MB/s, which fills
  V8's young generation every few seconds; a 1 ms pause is 6 % of a 16.6 ms frame. Below all three, T2 stays out
  and the result is recorded as the reason.
- This work ends at the measurement and the recorded decision. If a threshold is crossed, T2 becomes a new design,
  not a task of this plan, because it has to keep the `Block` validity contract (anti-goal) and no way to do that is
  designed here **[author decision]**.
- The result goes into `docs/reference/benchmarks/` with the commit, like the boundary benchmarks.

### 5.7 Documentation

- `docs/how-to/keep-the-hot-path-fast.md` §2: in C++, a repeated `query()` costs a load and a branch, no allocation;
  in TS, a cached `query()` allocates nothing on the engine's side, while the object literal and the callback are the
  game's own allocations.
- `docs/explanation/architecture/decisions.md`: one row for the static slots (reason: zero allocation per frame;
  cost: one pointer vector per `World` sized by the number of query and spawn shapes in the program, and two
  spellings use two slots), one row for "tables never shrink" (reason: no reallocation after a peak; cost: memory
  stays reserved until the `World` is destroyed).
- `docs/reference/benchmarks/README.md`: NF-P8, NF-P9, the new NF-P1/NF-P2 ratios and the TS garbage result.
- `CHANGELOG.md`, `## Unreleased`: the change, if it has a section for non-breaking changes; nothing under
  `### Breaking` (no API change).

## 6. Constraints

- The public API of `World`, `Context` and the TS `World` does not change.
- `CLAUDE.md`: `checkInvariants` covers the change, and `spec-conformance-reviewer` and `perf-reviewer` run on it,
  because it touches `world` and `query*`. New tests go through the `add-test-case` skill.
- Toolchain only through `./spur` (C++ in `dev`, WASM and TS in `wasm`).
- Code style: `docs/reference/contributor/code-style.md`.

## 7. Left to the builder

- Names of the slot helpers and of the new benchmark cases.
- The structure of the TS identity cache, if it allocates nothing on a hit.
- The exact scene of §5.6, within the shape above.
- How to split the work into commits.
