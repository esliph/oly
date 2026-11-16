# Oly — plugins and `essentials` — design

Date: 2026-11-16. Base: `develop` at `b0f1206`.
Specs that remain valid: `2026-10-16-ecs-archetype-spec.md`, `2026-11-09-game-engine-design.md`,
`2026-11-13-multi-environment-design.md`. This design extends the ECS (§5), the schema tooling (§6) and the game build
(§7).

Marking: **[user decision]** was decided in the conversation; **[author decision]** was my choice during the design,
which the user may review; the rest is a direct consequence of the two.

## 1. Intent

Oly must accept **plugins**: units of engine-level functionality, compiled together with the core, that a game turns
on in its build **[user decision]**. Plugins serve three purposes at once **[user decision]**:

1. Split a game into parts that other games reuse.
2. Ship ready-made extensions: physics, audio, camera, assets and UI.
3. Give every game a shared set of base components, so that each game stops declaring its own `Position`. Today
   `arena.oly` and `drift.oly` both declare the same `Position` and `Velocity`.

Oly is now meant to become a publishable, installable product, not only a study project **[user decision]**. The
repository is not ready for publishing yet. This project comes **before** publishing **[author decision, accepted by
the user]**: publishing freezes the public API, and the plugin mechanism changes exactly that API (`Engine`
registration, `oly-schema` inputs, `oly_add_game`, the schema hash). The order of the projects is:

1. Plugin mechanism + `essentials` (this document).
2. Publishing (CMake package, `oly-schema` binary, npm packages, versioning).
3. Extensions, one project each: physics, audio, camera, assets, UI.

The first plugin is `essentials`: a plugin every game gets without turning it on **[user decision]**, with a transform
hierarchy in the style of Unity's `Transform` **[user decision]**, in a 2D and a 3D flavour **[user decision: option
B]**. 3D exists to keep the door open; no 3D game is planned **[user decision]**.

## 2. Goals, non-goals, anti-goals

**Goals**

- A plugin is declared in CMake (`oly_add_plugin`) and turned on per game in `oly_add_game(... PLUGINS ...)`
  **[user decision: option 1]**. The build installs the plugins, in dependency order, with no game code (§7).
- A plugin may bring its own `.oly`. `oly-schema` merges the plugins' schemas with the game's into one layout and one
  hash, so the TS client sees plugin components with no extra code (§6).
- The ECS gets lifecycle observers (on add, on remove; destroy counts as remove) **[user decision: option A]**, so
  plugins react to entity changes, and implied components, so a component can bring others in the same transition
  **[user decision: option B]** (§5).
- A plugin acts **through data**: a game, in C++ or TS, uses a plugin by adding and removing its components
  **[user decision: option A]**. A plugin may also expose functions (§4.3) **[user decision]**.
- `essentials2d` and `essentials3d` exist, with `Position`, `Rotation`, `Scale`, `Parent` and `WorldTransform`, a
  parent-child hierarchy and cascade destroy (§8) **[user decision]**.
- Every game declares its `DIMENSION` (`2d` or `3d`); the matching `essentials` is always on (§7.2)
  **[user decision: option B]**.
- `arena` and `drift` drop their own `Position` and use `essentials2d`'s.
- Architecture docs, the user guide `docs/guide/`, the glossary and `CLAUDE.md` describe plugins and `essentials`.

**Non-goals**

- Any extension other than `essentials` (physics, audio, camera, assets, UI). Each one is its own project.
- Loading plugins at run time (`.so`, `.dll`, WASM dynamic linking).
- Publishing: `install()`, CMake package config, npm publishing, versioning of plugins. Plugins live in this
  repository for now (§3).
- Building the C API part or the TS package part of a plugin. The shape is defined (§4.3); the build support comes with
  the first plugin that needs it (a math library, physics raycasts) **[author decision]**.
- A math library for games. `essentials3d` needs quaternion math internally; a public math plugin is a later project.
- Registering observers from TS or through the C API.
- A 3D game, 3D render or 3D physics.

**Anti-goals** (these make the project a failure even if it works)

- A hot-path regression when no observer is registered: NF-P4 (`add`/`remove` with a cached edge: zero allocations,
  zero hash lookups) must stay met, and iteration must not change.
- `engine/` depending on any plugin. The core stays complete without plugins; plugins depend on the core, never the
  reverse.
- A plugin that needs a line of game code to be installed, or a plugin list written in two places.
- A layout written twice: a plugin component declared in a plugin `.oly` and again by hand in a client.
- A 2D game paying for 3D (bigger components, quaternion math).

## 3. Where plugins live

Plugins live in this repository, in a top-level folder `plugins/`, one folder per plugin **[author decision]**:

```
plugins/
  essentials2d/
    essentials2d.oly        the plugin's data (optional)
    native/                 C++: include/oly/essentials2d/, src/, tests/, bench/
  essentials3d/
    ...
```

`plugins/` sits beside `engine/` and `games/`, not inside `engine/`: plugins depend on the core, and a separate root
makes the later publishing split (core package, plugin packages) a move of folders, not a rewrite.

Rules that hold from day one, so that publishing later costs nothing **[author decision]**:

- A plugin includes only public headers of the core modules and plugins it declares (no `detail/` of others).
- A plugin is its own CMake target, with no path relative to the repository root.
- A plugin's `.oly` reaches `oly-schema` as an argument, never through a fixed path.

## 4. What a plugin is

### 4.1 Parts

A plugin has up to three parts. Only the C++ part is required.

| Part | Contents | Built in this project |
|---|---|---|
| C++ | data (`.oly`), systems, handlers, observers, public C++ functions | yes |
| C API | functions exported to clients: per batch or per system, never per entity | no (shape only) |
| TS package `@oly/<plugin>` | wrappers of the C API part and pure-TS helpers | no (shape only) |

A plugin with only functions and no `install` (a math library) is valid: it is a C++ library that the game links.

### 4.2 Install and ownership

- A plugin is a C++ type that installs itself into an `Engine`: it registers systems, handlers, observers and its own
  state. It installs once per `Engine`, before the first `frame`; installing during `frame` is an `OLY_ASSERT`, like
  every other registration.
- **The `Engine` owns the plugin instances** **[author decision]** and destroys them in reverse install order. A
  plugin's state (for example the children index of `essentials`) lives in its instance and reaches its systems
  through `userData`.
- A game reaches an installed plugin with a typed accessor (`engine.plugin<T>()`, name left to the builder) to
  configure it before the first `frame`. `essentials` needs no configuration.
- Plugins declare their dependencies on other plugins in CMake (§7.1). Install order is a topological order of those
  dependencies; among independent plugins, the order of the `PLUGINS` list. `essentials` is always first.

### 4.3 Functions and the boundary

A plugin may expose functions **[user decision]**:

- In C++, they are ordinary public functions in the plugin's headers. A C++ game calls them directly and the compiler
  inlines them.
- For a client, a call crosses the WASM boundary at ~40–50 ns (`docs/architecture/performance.md`), while a helper such
  as `lerp` costs ~1 ns. Rule 2 of the architecture applies: **a plugin's C API part exposes only functions that work
  per batch or per system** (`raycastBatch`), never per entity. Pure helpers (math) are rewritten in the plugin's TS
  package, not called through the boundary **[author decision]**.

### 4.4 Order inside a phase

Today the order inside a phase is the registration order, and plugins register before the game. `essentials` must
compute `WorldTransform` after the game's movement systems. The `Engine` gets an **order band** per system
**[author decision]**: a system may ask to run at the end of its phase (`Last`); everything else keeps the default band.
Inside a band, the order stays the registration order. The band is fixed at registration. The exact API is left to the
builder; it must not add a branch per system in `runPhase` beyond what the existing `whilePaused` and state checks cost
(for example, two vectors per phase, or one vector kept sorted by band).

## 5. ECS lifecycle observers **[user decision: option A]**

### 5.1 Contract

- An observer is a function pointer plus `userData`, registered per `ComponentId` and kind: **on add** or **on
  remove**. Tags can be observed.
- **On add** runs after the component is in place, so the observer reads the new value.
- **On remove** runs before the component leaves, so the observer reads the old value.
- **Destroy** runs on remove for every observed component of the entity, before the entity dies.
- **Assigning an existing component** through `add` (today: `get<T>(e) = std::move(v)`, no structural change) runs on
  remove with the old value and then on add with the new value **[author decision]**. Replacing a value is how a game
  re-parents an entity (§8.2).
- **Direct field writes** (through `get` or a query's write view) are not observed. This is a documented limit, not a
  bug.
- Observers run in every path that changes the `World`: direct `World` calls, the `CommandBuffer` flush and the state
  scope cleanup.
- Observers never run inside an iteration: systems change structure only through the `CommandBuffer`, whose flush runs
  outside iteration.
- **Structural changes requested by an observer** (for example, destroying the children of a destroyed parent) are
  deferred and applied right after the operation that triggered the observer, before that operation returns, until
  nothing is pending **[author decision]**. No observer re-enters a transition in progress.
- **Inside a `CommandBuffer` flush**, observers see the **net change per entity**, the same change the flush applies in
  its single transition (R-CB-3, R-CB-4): an add followed by a remove of the same component in one buffer runs no
  observer, and repeated adds run one on add with the last value. Commands ignored for stale handles run nothing. On
  remove observers (including destroy) run before each entity's transition; **on add observers run after every
  transition of the flush**, in the order of each entity's first command **[author decision]**. So an on add observer
  sees the world as the whole buffer left it, and the outcome does not depend on the order of the second pass.

### 5.2 Cost

- With no observer on any component of the transition, the cost must be within noise of today's. The observed flag
  belongs where the hot path already looks (for example, cached in the edge when the edge is created, or a bit per
  archetype), never a hash lookup per transition. NF-P4 stays met.
- Validation: `bench-compare` on `ecs_bench_transitions`, `ecs_bench_flush` and `ecs_bench_iterate`, before and after,
  with zero observers; the `perf-reviewer` and `spec-conformance-reviewer` agents (rule of `CLAUDE.md`).
- `checkInvariants` covers the new state (observer flags consistent with the registered observers).

### 5.3 Implied components **[user decision: option B]**

A component may **imply** others: adding it adds them in the **same transition**. `essentials` uses it so that every
entity with `Position` has `WorldTransform` without a second transition.

- An implication is registered on the `World` (`Position` implies `WorldTransform`) before any archetype contains the
  implying component; registering it later is an `OLY_ASSERT`. A plugin registers its implications at install.
- The edge `+Position` of an archetype leads straight to the archetype with `Position` and `WorldTransform`; the
  closure is computed once, when the edge is created (cold path). `spawn<Ts...>`, `addById` and the `CommandBuffer`
  flush apply the same closure, so the flush keeps one transition per entity (R-CB-3).
- An implied component starts value-initialized and then runs its on add observers like any other; an observer may
  write its value (no structural change), which is how `essentials` fills `WorldTransform` at once (§8.3).
- Removing the implying component removes the components it implied, in the same transition. Removing an implied
  component while its implying one stays is an `OLY_ASSERT`.
- Invariant I-9 changes: an edge `a --±c--> b` satisfies `b.signature == resolve(a.signature ± c)`, where `resolve`
  applies the closure on add and the removal of what no remaining component implies (ECS spec I-9).

**Cost target** (written into the ECS spec): with no hierarchy in use, spawn and flush make the same number of
transitions as today, and NF-P4 stays met; the per-step cost of `essentials` is one copy per root, per block. Both are
measured and reported.

### 5.4 Where it lives

Observers belong to the `World` (`ecs`), because every structural path goes through it. The `Engine` does not wrap
them; a plugin registers observers on `engine.world()`. The ECS spec gets new requirements (`R-OB-*`) and the
invariants for observers.

## 6. Schema merge in `oly-schema`

- `oly-schema` accepts the plugins' `.oly` files, in install order, plus the game's `.oly` **[author decision]**. The
  merged schema is: the plugins' declarations in install order, then the game's. That order gives the `ComponentId`
  order, the event dispatch order, the `ActionId` and the `StateId`.
- Names stay flat (no namespaces): a name repeated across the merged files is an error that names both
  `<file>:<line>:<column>` locations. The 32-state limit counts the merged set.
- The hash is computed over the merged canonical form. Adding or removing a plugin changes the hash, so
  `SchemaMismatchError` keeps catching a TS client built with another plugin set.
- **C++ structs are generated once.** A plugin's build generates the header of its own types; the game's generated
  header includes the plugins' headers and defines only the game's types, so no type is defined twice (ODR). The
  declaration, the registry and the TS output cover the whole merged set **[author decision]**.
- **A plugin `.oly` declares only components, tags and events** **[author decision]**. Actions and states in a plugin
  `.oly` are an `oly-schema` error. Reason: their ids are baked into the generated header as `constexpr` values
  starting at 0 (`actions::`, `states::`), and a plugin's header is generated once, while its ids would depend on each
  game's install order. Events are keyed by C++ type, so they work. Revisit when a plugin needs an action or a state
  (UI focus, camera modes); the fix then is runtime-resolved handles for plugins.
- **The TS output comes from the same place as the C++ output** **[author decision]**. Today `./spur ts` runs
  `oly-schema` on the game's `.oly` alone, before CMake configures the games, so it would not see the plugin list. TS
  emission moves into `oly_add_game` (a custom command, like the C++ one), and `./spur ts` configures and runs the
  codegen target before `tsc`. The plugin list stays in one place (CMake) and the C++ registry and the TS file always
  hash the same merged schema. The `spur` task names do not change.
- The exact command line (flags, how the plugin files are passed) is left to the builder.

## 7. Build

### 7.1 `oly_add_plugin`

`oly_add_plugin(NAME <plugin> [SCHEMA <file.oly>] SOURCES ... DEPENDS <modules> [PLUGINS <plugins>])` defines a plugin
target **[author decision on the shape; names left to the builder]**. It generates the plugin's own C++ header from
its `.oly`, registers the target like `oly_add_module` does, and records the plugin's `.oly` and dependencies so that
`oly_add_game` can find them.

### 7.2 `oly_add_game`

- New arguments: `DIMENSION 2d|3d` (required, no default **[author decision]**) and `PLUGINS <plugins>` (optional).
- `DIMENSION 2d` turns on `essentials2d`; `3d` turns on `essentials3d`. The game cannot turn `essentials` off and does
  not list it.
- **Engine test games** (`fixture_game` in `engine/testing/`, used by `ts.testing.wasm` and the `SchemaMismatchError`
  fixtures) do not get `essentials`: `engine/` must not depend on `plugins/` **[author decision]**. They use an internal
  CMake function (name left to the builder) that shares the body of `oly_add_game` but takes no `DIMENSION` and turns
  no plugin on, except test plugins from `engine/testing/fixture/`. `fixture.oly` keeps its own `Position`. The lint
  rejects that function outside `engine/`.
- `oly_add_game` resolves the transitive plugin dependencies, sorts them (§4.2), passes the `.oly` files to
  `oly-schema` and links the plugin targets.
- The generated code installs the plugins in order. Where it happens: right after the schema is declared, in the same
  places that declare it today (`olyEngineCreate` in `capi`, and the native game's `main`, which already calls
  `declareSchema`). A TS game writes nothing; a native game keeps one call **[author decision; the name of the
  generated entry point is left to the builder]**.

### 7.3 Boundaries

The `oly.lint.boundaries` lint gets new rules:

- nothing in `engine/` includes or imports anything from `plugins/`;
- a plugin includes only the modules and plugins it declares, and no `detail/` of another module or plugin;
- game names never appear in `plugins/` (the existing vocabulary rule extends to plugins).

## 8. `essentials`

### 8.1 Components

Two plugins, one per dimension **[user decision: option B]**. A game uses only one, so the same names do not collide.

| Component | `essentials2d` | `essentials3d` |
|---|---|---|
| `Position` | `{ x, y: f32 }` | `{ x, y, z: f32 }` |
| `Rotation` | `{ angle: f32 }` (radians) | `{ x, y, z, w: f32 }` (unit quaternion) |
| `Scale` | `{ x, y: f32 }` | `{ x, y, z: f32 }` |
| `Parent` | `{ entity: entity }` | same |
| `WorldTransform` | position, angle and scale in the world | position, quaternion and scale in the world |

- `Position`, `Rotation` and `Scale` are **separate components** **[author decision]**, not one `Transform`. A system
  that only moves entities reads 8 B per entity in 2D instead of 20 B (2.5×) **[hypothesis, to measure]**. Revisit if
  a measurement shows the split costs more than it saves (for example, propagation dominated by three column reads).
- Missing `Rotation` means angle 0 (identity); missing `Scale` means 1.
- `Velocity` stays out of `essentials` **[author decision]**: it is movement, not transform, and the physics plugin will
  own it. Until then each game declares its own.
- `Position`, `Rotation` and `Scale` are **local**: relative to the parent, or to the world when there is no parent.

### 8.2 Hierarchy **[user decision]**

- An entity becomes a child by getting `Parent { entity }`. Re-parenting is assigning a new `Parent` (§5.1); removing
  `Parent` makes it a root.
- Re-parenting and un-parenting keep the **local** values, so the entity may jump in the world **[author decision]**.
  Keeping the world position is a helper for later.
- `essentials` keeps a children index in its instance, updated by observers on `Parent`. How the index is stored (and
  whether the parent gets an internal marker so that its destroy is observable) is left to the builder.
- **Cascade destroy** **[user decision]**: destroying a parent destroys its children, recursively, in every path
  (direct, `CommandBuffer`, state scope cleanup), through the observers' deferred changes (§5.1).
- Contract violations (`OLY_ASSERT`, with the operation, the `Entity` and the type name): a `Parent` pointing to a dead
  entity, to itself or to one of its descendants (a cycle). The check runs in the on add observer of `Parent`, so in a
  flush it sees the world after the whole buffer (§5.1): a parent spawned in the same buffer is valid; a parent
  destroyed in the same buffer that also gives it a child is a violation, whatever the recording order.
- Writing `Parent.entity` directly through `get` or a query is a contract violation that observers cannot see. In debug
  builds, `essentials` checks that its index matches the `Parent` components (in its own `checkInvariants`-style
  validation, run by the tests).

### 8.3 `WorldTransform`

- `WorldTransform` holds the entity's position, rotation and scale **in the world**: the local values combined with the
  parent's, recursively. The game writes the local components and only reads `WorldTransform` (Unity's
  `localPosition` versus `position`).
- Every entity with `Position` has `WorldTransform`, through an implication (§5.3) **[user decision: option B]**, so a
  game reads world values the same way for roots and children and spawning stays one transition.
- An on add observer on `WorldTransform` computes its value at once (from the local values and, for a child, the
  parent's `WorldTransform`) **[author decision]**. Without it, an entity spawned by a `CommandBuffer` at the end of
  `update` would carry a zero `WorldTransform` into `updateAfter` (a bullet colliding at the origin for one step).
- Composition is translate-rotate-scale with no shear: world scale = parent world scale × local scale, component-wise;
  world rotation = parent rotation ∘ local rotation; world position = parent position + parent rotation applied to
  (parent scale × local position) **[author decision]**. Non-uniform scale under rotation therefore loses the shear, as
  Unity's `lossyScale` does.
- A propagation system computes `WorldTransform` once per fixed step, in the `Last` band of `update` **[author
  decision]**: systems in `updateAfter` (collision, later physics) read up-to-date world values. A change to `Position`
  made in `updateAfter` reaches `WorldTransform` in the next step; this is documented in the guide.
- Roots are copied per block (fast path); only children pay the random access to their parent's `WorldTransform`.
  Parents are computed before their children; the algorithm (depth order cached and rebuilt when the hierarchy
  changes, or another) is left to the builder.
- `render` interpolation stays the game's job, as today.

### 8.4 3D

`essentials3d` is validated by unit tests only (propagation, cascade destroy, quaternion composition), with no game
**[author decision]**. Its quaternion math is internal to the plugin (`detail/`).

## 9. Migration of the games

- `drift`: `DIMENSION 2d`; drops `Position` from `drift.oly`, keeps `Velocity` and `Extent`.
- `arena`: `DIMENSION 2d`; drops `Position` from `arena.oly`, keeps `Velocity` and the rest. The TS code keeps using
  `Position` from the generated schema; the layout `{ x, y: f32 }` is the same. The schema hash changes.
- Systems that today read `Position` as the world position keep working: neither game uses `Parent`, so local equals
  world. Moving collision and render to `WorldTransform` is optional in this project.

## 10. Docs

- `docs/architecture/`: a new `plugins.md` (model, install, schema merge, observers, `essentials`); `ecs.md` (observers);
  `app.md` (order bands, plugin ownership); `schema-and-codegen.md` (merge); `modules-and-boundaries.md` (graph and
  lint); `repository.md` (`plugins/`); `limits.md`; `decisions.md`; `glossary.md` (plugin, observer, `essentials`,
  `WorldTransform`, `Parent`).
- `docs/guide/` (rule of `CLAUDE.md`: public API changes update the guide, following `docs/development/guide-style.md`):
  a concept page for plugins and one for transforms and hierarchy; the getting-started pages show `DIMENSION`.
- The ECS spec gets the observer requirements and invariants.
- `CLAUDE.md`: plugins in the structure, the `plugins/` rules, and "2D game engine" reworded where `essentials3d` makes
  it false. Rewording "study and benchmark project" is part of the publishing project.

## 11. Order of work **[author decision]**

1. **ECS observers and implied components** (§5): contract, flags, deferred changes, closure in edges, I-9,
   `checkInvariants`, tests (add, remove, assign, destroy, flush, cascade, implication), death cases, `bench-compare`
   with zero observers and with an implication, both reviewer agents.
2. **Order bands** in the `Engine` (§4.4).
3. **Plugin mechanism** (§4, §6, §7): `oly_add_plugin`, plugin ownership in the `Engine`, schema merge in `oly-schema`
   (with corpus cases for name conflicts), `DIMENSION` and `PLUGINS` in `oly_add_game`, generated install, lint rules.
   A minimal test plugin in `engine/testing/fixture/` proves the mechanism before `essentials` exists.
4. **`essentials2d`** (§8) and the migration of `drift` and `arena` (§9); smoke tests of both games pass.
5. **`essentials3d`** (§8.4).
6. **Docs** (§10).

Each step ends with the full test suite green on `native`, `web` and the Windows cross-build.

## 12. Left to the builder

- Names of the new APIs (observer registration, plugin type shape, plugin accessor, order band, generated install
  entry point, `oly-schema` flags), following the glossary rule (same verb across surfaces, no aliases).
- The storage of the observer flags, as long as §5.2 holds.
- The storage of the children index and the propagation order algorithm (§8.2, §8.3).
- The folder layout inside a plugin beyond §3.
- Whether a benchmark of propagation (roots versus children) is worth adding; if added, it lives in the plugin's
  `bench/`.
