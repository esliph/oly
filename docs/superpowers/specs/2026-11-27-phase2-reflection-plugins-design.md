# Oly — phase 2: reflection and plugin extensibility — design

Date: 2026-11-27. Written on `feature/engine-AAA` at `893fe5a`. Phase 2 of `2026-11-24-aaa-roadmap-design.md` (called *the
roadmap* below); its gate is rows 5, 6, 7 and 8 of the roadmap's acceptance matrix, and it gives S4 its baseline.

Marking: **[user decision]** was decided in the conversation; **[author decision]** was my choice while writing, which
the user may review; **[fact]** can be checked in the repository; **[hypothesis]** needs a measurement or a build.

## 1. Intent

Every type a game or a plugin declares gets a name that is the same in every build, and the engine can describe it as
data. Four things depend on that, and phase 2 delivers them:

- **Plugins that do not know each other's order.** A plugin compiled once works in any game, whatever the other plugins
  are and in whatever order they install (row 5).
- **Plugins and mods that do not know each other's names.** Two plugins may each declare a `Body`; a game may declare
  its own plugins, a mod among them (row 6).
- **A save that outlives the build that wrote it**, and an inspector that edits any field with no code per component
  (row 7).
- **A schema wide enough for real data**: fixed arrays, enums, nested structs, 64-bit integers, resources (row 8).

Why now: today a type's id is the order of the first `componentId<T>()` call
(`engine/runtime/ecs/include/oly/ecs/component.hpp:60-61`: "not suitable for persisting"), action and state ids are
`constexpr` positions in each game's generated header, and names are flat across the merged schema. Each of these
blocks one of the four, and each costs more to change as games and plugins are written against it.

The phase is one spec carried out as three plans, in order **[user decision]**:

| Plan | Content | Rows |
|---|---|---|
| 2a — identity and plugins | qualified names, stable ids, runtime handles, namespaces, plugins declared by the game, plugins with no `install`, actions and state machines in plugins | 5, 6 |
| 2b — grammar and reflection | `resource`, `struct`, `enum`, `u64`/`i64`, fixed arrays, references to types of other namespaces; the reflection table in C++ and TS | 8 |
| 2c — save, load and inspector | snapshot save and restore, the rebuild phase, the cross-build policy, the `inspector` and `inspector_imgui` plugins, S4 | 7 |

## 2. Goals, non-goals, anti-goals

Goals:

- A type's identity is its qualified name, `<namespace>::<Type>`, and its stable id is a 64-bit hash of that name. The
  same declaration gives the same id in every build, environment and compiler.
- A plugin `.oly` may declare actions and state machines; their handles resolve at run time, so one plugin binary works
  in games with different plugin sets and orders (row 5).
- A game declares its own plugins, in its own repository, with no engine change; names live in per-plugin namespaces
  (row 6).
- `Inventory { slots: [ItemStack; 16] }`, `enum Recipe` and `seed: u64` compile in `.oly` with the same layout in C++
  and TS (row 8).
- A save written by one build loads in a build with another plugin installed; the inspector edits any reflected field
  with no per-component code (row 7).
- Saving at step k, restoring in a new process and running to step n gives the same world hash as running straight to
  n (S4).

Non-goals:

- **Strings and asset handles in the grammar.** Handles arrive with assets in phase 5; strings when a row asks for them
  **[user decision]**. The roadmap's phase 2 line lists "strings or asset handles"; this spec defers both.
- **Loading mods at run time.** Mods are built in phase 2 **[user decision]**; §5.6 keeps the door open for a later
  loader, which no phase of the roadmap plans yet.
- **`World::instantiate`**: loading a scene into a populated `World` with new entities and remapped `entity` fields.
  Designed (§5.10) and deferred **[user decision]**.
- **Migrations** (a function per type and version that converts old saves). The cross-build policy (§5.11) is built so
  they can be added without a new format.
- **A visual inspector on `web`** (roadmap rule 2).
- **A renderer plugin.** `render2d` is phase 6; the Enxame keeps drawing in the game **[user decision]**.

Anti-goals (a result that does one of these fails even if it works):

- An id that depends on install order, registration order or the compiler's spelling of a type name.
- A component that can be in a saved world but is invisible to reflection, so that a load silently changes the world.
- A save made by a build that some other build loads into a different world without saying so.
- A 2D-only type or rule in `core`, `ecs`, `input`, `app` or `capi` (roadmap rule 1).

## 3. Constraints

- **[fact]** `ComponentId` is dense and per process (`component.hpp:12`, `:55`); resource ids are assigned the same way
  (`engine/runtime/ecs/include/oly/ecs/resource.hpp:23`). Phase 1's conflict check uses bitsets over these ids.
- **[fact]** `oly-schema` hashes the merged canonical form with a 32-bit FNV-1a (`engine/schema/codegen/src/hash.cpp`);
  the C API exposes it as `olySchemaHash` (`engine/runtime/capi/include/oly/capi/c_api.h:32`) and the TS client rejects
  a WASM module whose hash, type counts or type sizes differ (`engine/environments/web/clients/typescript/core/src/runtime.ts:210-226`).
- **[fact]** Names are flat: a name repeated across the merged files is an error (`engine/schema/codegen/src/merge.cpp`,
  `checkNamesAcrossFiles`). A plugin `.oly` declares only components, tags and events, because action and state ids are
  `constexpr` positions (`docs/explanation/architecture/plugins.md:63`; `docs/reference/limits.md:24`).
- **[fact]** `oly_add_plugin` requires `src/*.cpp` and fixes the namespace to `oly::<name>` (`cmake/OlyPlugin.cmake:1-2`,
  `:34-38`); `setUpGame` includes `oly/<plugin>/plugin.hpp` and installs `oly::<plugin>::Plugin`
  (`cmake/OlyGame.cmake:81-82`). `oly_add_engine_test_game` is the precedent for an engine-only CMake function the
  boundaries lint refuses elsewhere (`cmake/OlyGame.cmake:46-50`, `tools/check_boundaries.py:27`, `:146`).
- **[fact]** States are one flat list of at most 32 (`engine/runtime/app/include/oly/app/state.hpp:9-15`); the engine
  holds one current state, `NoState` until the first request (`engine.hpp:120`, `:285-287`). An entity spawned while a
  state is active is born scoped by the tag `detail::StateScoped` until `persist` (`context.hpp:87-98`).
- **[fact]** The grammar has components, events, tags, actions and states, with the scalars `f32 f64 i8 u8 i16 u16 i32
  u32 entity` (`engine/schema/format/README.md:10-21`). The docs promise "no 64-bit integers (they would require
  BigInt)" (`docs/explanation/architecture/schema-and-codegen.md:16`, `docs/reference/typescript/schema.md:39`).
- **[fact]** TS reads fields through typed-array views with a stride and reads `entity` as two `u32`
  (`engine/environments/web/clients/typescript/core/src/fields.ts:212`).
- **[fact]** `essentials2d` and `essentials3d` add `detail::HasChildren` to a parent through an observer when a `Parent`
  is added (`plugins/essentials2d/native/src/observers.cpp:68`). Neither tag is in a `.oly`.
- **[fact]** The Enxame world hash sorts entities by index and folds index, generation and the game's components
  (`games/enxame/native/world_hash.cpp:44-83`); the horde consumes the random generator in iteration order
  (`horde.cpp:54`, `Res<Random>`), so a change of row order changes the continuation.
- **[fact]** The Enxame's derived state: `SpatialHash` is rebuilt every step (`horde.cpp:40-52`); `FlowField` is rebuilt
  when the map's version changes (`horde.cpp:34-38`); `Belts::assemblerOfBelt` is filled once, during setup
  (`scenario.cpp:136-146`); the map is generated from the random generator at setup (`scenario.cpp:65-66`).
- **[fact]** The native host and SDL3 build only with `OLY_BUILD_NATIVE_HOST` (`cmake/Dependencies.cmake:24-40`), which
  the `native-games*` and `windows*` presets turn on and `debug-asan` does not (`CMakePresets.json`).
- **[fact]** S4 is blocked today: `enxame --scenario S4` exits 64 with "blocked: needs stable ids"
  (`games/enxame/cmake/game.cmake`, test `enxame.usage.blocked_s4`). Roadmap rule 5: a blocked scenario gets its
  baseline in the phase that unblocks it.
- **[fact]** Restore writes tables, archetypes and the entity allocator, so the change touches `world`:
  `checkInvariants` covers it and the `spec-conformance-reviewer` and `perf-reviewer` agents run (`CLAUDE.md`).
- Public API changes update `docs/` and `CHANGELOG.md` `## Unreleased` in the same change (`CLAUDE.md`).

## 4. Approaches

**What is reflected, saved and inspected.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| A | Only what a `.oly` declares; C++-only types stay outside reflection | one source of truth in two languages; true state moves into `.oly` and behaviour into free functions | **chosen [user decision]** |
| B | Also C++ types, registered by a macro or traits | no schema change for the game; two descriptions of a type that can drift, and TS sees half of them | rejected |

**Where a type's identity comes from.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| i | Hash of the qualified name as the `.oly` declares it, `<namespace>::<Type>` | the same in C++, TS, every compiler and every environment | **chosen [author decision]** |
| ii | Hash of `oly::core::typeName<T>()` | free in C++; GCC, Clang and Emscripten spell type names differently (phase 1 met the same for system names), and TS has no C++ name | rejected |

**Internal and external ids.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| 1 | The dense `ComponentId` stays the internal identity (signatures, columns, phase 1 bitsets); the 64-bit stable id is the external identity (save, TS, C API, inspector, future mods), mapped once when the process first registers the type (§5.2) | no change to the hot paths or to phase 1's bitsets | **chosen [author decision]** |
| 2 | The stable id everywhere | 64-bit keys in signatures and hash lookups in transitions | rejected |

**Namespaces.** Every `.oly` type is namespaced **[user decision]**. A plugin's types live under the plugin's name; the
game's under the fixed namespace `game`, not the game's own name **[user decision]**. `oly::` is reserved to the engine
and its own plugins; a third-party plugin and the game are not under it **[user decision]**. The public
`oly_add_plugin` takes no namespace argument: the namespace is the plugin's name **[user decision]**.

**States.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| a | One flat machine (today) | one mask per condition; a plugin's states would replace the game's | rejected |
| b | Several machines, declared in `.oly`; the game binds a plugin's machine under one of its states | a plugin owns its states and never names the game's; one mask per machine a condition cites | **chosen [user decision]** |

**State scope.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| A | Implicit, as today: what is spawned while a state is active is scoped, until `persist` | with several machines, "which machine?" has no good default | rejected |
| B | Explicit at spawn: `scopedTo(machine)`; without it an entity is born unscoped | one more call where scope is wanted; no `persist` | **chosen [user decision]** |

**Mods.** Built with the game in phase 2; loading at run time is wanted later **[user decision]**.

**64-bit integers in TS.** `bigint` **[user decision]**, with the cost (an allocation per read) written in the docs.
Two `u32` fields were rejected: they push the split onto every reader.

**The save.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| a | Exact snapshot: only into a `World` with no entities; same entity ids, allocator state, archetypes in creation order, rows in the same order | the continuation is bit-equal, which phase 3's replay needs; a storage change (phase 4) means a new format version | **chosen [user decision]** |
| b | Remap: new entities in any `World`, `entity` fields fixed through reflection | also serves prefabs and scenes; ids and hash change, the continuation may diverge | rejected for the save; kept as the deferred `instantiate` **[user decision]** |

**Derived state after a restore.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| a | A phase that runs only after a restore | no setup change; two paths build the same state, and the load path is tested by S4 only | rejected |
| b | A phase that runs once on the first frame and again after each restore; setup creates only true state | one path, exercised by every run | **chosen [user decision]** |
| c | No engine support: derived state heals itself by version stamps or per-step rebuilds | nothing new; `Belts` and the hierarchy would scan every step, and the rule is a convention | rejected |

**A save from another build.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| a | Strict: an unknown type or a changed layout refuses the load | simplest; removing a mod or adding a field breaks every save | rejected |
| b | Tolerant by name: an unknown type is dropped, a changed type is copied field by field by name, and a load report says what happened | saves survive mods removed and fields added; the report is the game's to show | **chosen [user decision]** |
| c | b plus per-type migrations | renames and conversions; no row asks for them | deferred: built on b with no format change |

**Format.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| a | An engine binary format: little-endian, fixed-width, a column copied as one block | columns are already contiguous typed arrays, so save and restore are one copy per column | **chosen [author decision]** |
| b | JSON, CBOR or FlatBuffers | readable or standard; field-by-field encoding and a new dependency | rejected |

**Inspector.** Dear ImGui as the native front end, pinned, over today's `SDL_Renderer` **[user decision]**. The
inspector is two plugins: a core `inspector` that reads and writes any field through reflection and is testable with no
window, and an `inspector_imgui` front end **[user decision]**.

## 5. The design

The code below sketches the shape. Names and exact signatures are the builder's (§8).

### 5.1 Qualified names and stable ids (2a)

- A `.oly` file belongs to one namespace: a plugin's file to the plugin's name, the game's file to `game`, the engine's
  own file (§5.8) to `app` **[author decision]**. A declaration's qualified name is `<namespace>::<Name>`; a state's is
  `<namespace>::<state>`.
- `game`, `app` and `oly` are reserved: no plugin may take them as its name **[author decision]**.
- Names are unique per namespace across every kind of declaration (components, tags, events, actions, machines,
  states, and in 2b resources, structs and enums). The same short name in two namespaces is allowed; `physics2d::Body`
  and `railgun::Body` coexist (row 6). `checkNamesAcrossFiles` gives way to this rule.
- The stable id is the 64-bit FNV-1a of the qualified name's bytes **[author decision]**. `oly-schema` refuses a merged
  schema in which two qualified names hash to the same id, naming both; the registry checks it again when registration
  closes, for the day a mod arrives at run time.
- The C++ namespace of the generated types follows the `.oly` namespace: `game::Turret`, `railgun::Body`; for engine
  plugins, `oly::<name>::` (`oly::physics2d::Body`). The identity string never carries `oly::`, so it is the same
  whether a plugin is the engine's or a third party's **[user decision]**.
- `schemaHash` and `olySchemaHash` go away. Agreement between two sides is checked per type: same stable id, same
  layout (§5.7).
- Phases and sets keep their dense per-process keys and get no stable id in phase 2 **[author decision]**. The phase 1
  spec planned to hash their `typeName<T>()` here (its §2 and §5.1); approach ii rejects that hash, because compilers
  spell type names differently, and no client, save or inspector needs a phase's identity yet. `limits.md` keeps "a
  phase a plugin declares is not reachable from the C API or TS"; the phase that needs it declares phases by name.

### 5.2 Runtime handles (2a)

- Actions, machines, states, events and components each get a dense id per process, resolved from the stable id the
  first time the process registers it, the same pattern as `componentId<T>()` **[author decision]**. A plugin's
  generated header declares its handles as values that resolve on first use, not as `constexpr` positions, so the
  header is generated once per plugin and every game gets the right ids.
- Per call there is no lookup: a handle is the dense id. The stable id maps to it once.
- An `Engine` declares which actions, machines and events it has; using a handle the `Engine` did not declare is an
  `OLY_ASSERT` naming the operation and the qualified name.
- The C API identifies by stable id at the boundary: a client resolves each stable id to a handle once (on connect),
  then calls with handles **[author decision]**. TS does that when it loads the module.

### 5.3 Plugins (2a)

- **Public `oly_add_plugin(NAME <p> ...)`.** For any plugin outside the engine: a game's own plugins and mods. The
  namespace is `<p>`; the header is `<p>/plugin.hpp`, the class `<p>::Plugin`, the generated header
  `<p>/schema.generated.hpp` **[user decision]**. It never places anything under `oly::`.
- **Internal `oly_add_engine_plugin(NAME <p> ...)`** (name provisional) for `plugins/` and the engine's test plugins:
  today's convention, `oly::<p>` and `oly/<p>/plugin.hpp`. The boundaries lint refuses it outside `plugins/` and
  `engine/`, as it does `oly_add_engine_test_game`.
- **Plugins declared by the game.** A game calls `oly_add_plugin` in its own repository and lists the plugin in
  `oly_add_game(... PLUGINS ...)` like an engine plugin. `oly_plugin_order` already orders them; `setUpGame` installs
  `<namespace>::Plugin` with the right include for both kinds.
- **A plugin with no `install`.** `src/` becomes optional; with no sources the plugin is a header-only or schema-only
  library. `setUpGame` installs `<namespace>::Plugin` only when the plugin has a `plugin.hpp`, so no new CMake argument
  is needed **[author decision]**.
- A plugin `.oly` may declare actions and machines (§5.4, §5.5); `limits.md` lines 24, 25 and 30 go away.
- The C API and TS reach parity with C++ in 2a for what 2a adds **[user decision]** (§5.12).

### 5.4 Actions in plugins (2a)

- A plugin declares its actions (`camera2d.oly`: `action pan: axis2d`); a system reads `actionMap->axis2d(camera2d::pan)`.
- The game binds keys to any action, its own or a plugin's. No rule refuses one key bound to two actions; `bind`
  already accepts it (`engine/runtime/input/src/action_map.cpp:56-65`) **[user decision]**.

### 5.5 State machines (2a)

The design is the user's **[user decision]**:

```
# waves.oly (plugin): the machine is its own and knows nothing about the game
machine waves { intermission, assault }

# the game's .oly
machine game { menu, playing, gameOver }
```

```cpp
engine.bindSubMachine(waves::waves, game::playing);
engine.addSystem<&spawnWave>(app::Update{}, "spawnWave").runIf(inState(waves::assault));
```

- A machine bound under a state exists only while its parent machine is in that state; the parent is implicit in a
  condition on the child's state.
- A condition costs one mask per machine it cites. `MaxStates` (32) applies per machine.

The rest is mine **[author decision]**:

- A machine may be bound under one or more states of one parent. The bindings form a tree; a cycle or a second parent
  is an error when registration closes.
- A machine starts in its first declared state: a root machine on the first frame, a sub-machine when its parent enters
  a state it is bound under. This replaces today's `NoState` until the first request.
- Transitions cascade: exiting a parent state exits the children first, then the parent; entering enters the parent,
  then the children. Requests of one step are applied parent first; a request to a machine that stopped existing in the
  same step is discarded.
- A state request is a write of that machine's request, already in 2a under phase 1's conflict check. Today one
  `Res<StateRequest>` is one write, so a `waves` system and a game system that each request their own machine in one
  phase would be refused, and plugins would need each other's order again (row 5). The generated header therefore also
  declares a C++ type per machine, and the request parameter names it (`Res<StateRequest<waves::Waves>>` in sketch
  form); the access id is the machine's dense id. A script system stays exclusive, as in phase 1.
- Pause stays an engine control, outside the machines.
- Scope is explicit (§4): `commands.spawn(...).scopedTo(waves::waves)`. An entity scoped to a machine is destroyed when
  that machine leaves the state it was in, including by cascade. Each machine has its own scope tag, generated with the
  machine and reflected like any tag, so it is saved (§5.9). `persist` goes away from `Commands`, the C API and TS, and
  so does `detail::StateScoped`; scheduled tasks follow the same rule (born unscoped, `scopedTo` to scope them).
- The tag's qualified name is the machine's name plus a suffix outside the identifier grammar, so it can never collide
  with a declared name; the suffix is the builder's.
- `limits.md` line 28 (two `Engine`s share the scope tag) is rewritten: they share each machine's tag.

### 5.6 Mods: what keeps run-time loading open

Mods are built in phase 2. These four properties keep a later run-time loader possible, and phase 2 must not break any
of them **[author decision]**:

- the reflection table is run-time data (descriptors), not templates;
- ids come from name hashes, not from the order of a build;
- registration closes on the first frame, so a mod loaded at startup fits;
- a run-time ABI would be the C API.

### 5.7 Grammar and the reflection table (2b)

New declarations and field types **[user decision]**:

```
enum Recipe { gear, circuit, motor }
struct ItemStack { item: u16, count: u16 }
component Inventory { slots: [ItemStack; 16] }
resource Random { state: u64 }
```

- `resource Name { fields }`: a resource whose data the schema owns. It is data only; behaviour lives in free functions
  (`nextU64(game::Random&)`).
- `struct Name { fields }`: a nested type, usable as a field and inside arrays; not a component by itself.
- `enum Name { a, b, c }`: stored in the smallest unsigned integer that fits; in TS an `as const` object of numbers.
- `u64` and `i64`: in TS a `bigint`. The docs warn that each read allocates, and the two docs that promise "no 64-bit
  integers" are rewritten.
- `[T; N]`: a fixed array of any scalar, enum or struct, `N` a positive literal.
- `entity` stays a distinct field type, in components, structs and arrays alike, so a later `instantiate` can find every
  reference (§5.10).
- Layout stays natural alignment in declaration order, checked by `static_assert` in C++ and by the per-type check in
  TS.
- **References across namespaces** **[user decision]**: a field may name a `struct` or `enum` of another namespace by its
  qualified name (`slots: [inventory::ItemStack; 8]`); an unqualified name means the file's own namespace. A plugin's
  file may name the namespaces of the plugins in its `PLUGINS` (directly or transitively) and `app`; the game's file,
  every plugin the game turns on and `app`. Any other reference is an `oly-schema` error naming the field and the
  namespace. Plugin dependencies are acyclic (`oly_plugin_order`), so references are too.
- To generate a plugin's header, `oly-schema` receives the `.oly` of the plugin's dependencies, and `oly_add_plugin`
  forwards them; the generated header includes the dependencies' generated headers, and the TS module imports theirs
  **[author decision]**.

The reflection table, generated as data in C++ and TS **[user decision]**: for each type, its stable id, qualified
name, kind, size and alignment, and its fields (name, field type, offset, element count, and the nested type's stable
id); for an enum, its value names in order **[author decision]**. It records whether a type has padding, so the save
can avoid writing undefined bytes (§5.9). "Same layout", in the TS check and in the load, includes field names and
enum value names in order, so a reordered enum is never read as other values.

TS keeps one module per namespace and identifies types by stable id **[user decision]**. On load, TS resolves each of
its types against the WASM module's table: a type TS uses must exist with the same layout, or the load fails naming it;
a type only the WASM module has is allowed, so a TS client need not import every plugin **[author decision]**.

The row 8 types (`Inventory`, `ItemStack`, `enum Recipe`) are test types of the `oly-schema` corpus and of the TS suite,
not Enxame mechanics (roadmap rule 6) **[author decision]**. `seed: u64` is real: the Enxame's random generator becomes
`resource Random { state: u64 }`, and the map keeps its seed in an `.oly` resource (§5.15).

### 5.8 The engine's own `.oly`

The engine gets its own `.oly`, merged first into every game, under the namespace `app` **[author decision]**. It
declares, as `struct`s (a description, not `World` resources), the engine's part of the save (§5.9): the clock and the
fixed-step accumulator, and pause. These stay owned by the `Engine`, not the `World`, as phase 1 decided (two `Engine`s
may share a `World`); the `.oly` makes them self-describing and lets the tolerant load read them by name. It needs 2b's
grammar, so it arrives with 2b.

The current state of each machine is not in that file, because the machines depend on the game. The engine part of the
save lists them as pairs of stable ids (machine, current state), in machine declaration order **[author decision]**.

### 5.9 Save and restore (2c)

```cpp
std::vector<std::byte> bytes = engine.save();
app::LoadReport report = engine.restore(bytes);
```

- **What is saved.** Everything reflected: the entities with their reflected components and tags, the `.oly`
  resources, and the engine part of §5.8 **[user decision]** for the scope.
- **What is not.** C++-only resources, which setup creates and the rebuild phase fills (§5.13). Scheduled tasks are not
  saved in phase 2: `save` with a pending task is an `OLY_ASSERT` naming it, written in `limits.md` **[author
  decision]**. Input state is not saved: restore releases every action, as `releaseAll` does **[author decision]**.
- **Every component that can be in a saved world is reflected** **[user decision]**. `save` on an archetype with a
  component outside reflection is an `OLY_ASSERT` naming the type. `HasChildren` moves into `essentials2d.oly` and
  `essentials3d.oly` as a tag (its C++ name loses `detail::`); the scope tags are generated (§5.5). Without this, a
  restore would rebuild archetypes without those tags, the rebuild would move entities between archetypes, and row
  order would change.
- **Exact snapshot** **[user decision]**. `World::restore` takes a `World` that has never held an entity, with no
  archetype besides the empty one (an `OLY_ASSERT` otherwise), and recreates the entity allocator (generations and the
  free list in order), the archetypes in creation order (empty ones included) and the rows in the same order, updating
  the cached queries. Resources from configuration that are not in `.oly` stay as they are; `.oly` resources are
  overwritten. `Engine::restore` calls it before the first frame (see below), restores the engine part and sets each
  machine's state. Save runs between frames, with no event or state request pending (`OLY_ASSERT`).
- **No observers and no hooks run on restore** **[author decision]**: the world is restored, not built; machines are
  restored, not entered. The rebuild phase is where derived state is made again.
- **The world after restore passes `checkInvariants`.**

Format **[author decision]**, little-endian, fixed-width:

1. header: magic and format version;
2. type table: the reflection descriptor of every type present;
3. the entity allocator;
4. the engine part (§5.8);
5. the `.oly` resources, each as stable id and bytes;
6. the archetypes in creation order: the signature as stable ids, the row count, the entity column, then each column
   as one block.

A type with padding is copied field by field into a zeroed buffer, so the save never holds undefined bytes. No
compression in phase 2. A build reads only its own format version; reading an older one is decided by the phase that
writes a new one (phase 4).

#### Loading in a fresh process

An exact snapshot recreates archetypes in creation order, so it needs a `World` that has never held an entity. Phase 2
removes no archetype (that is phase 4), so a `World` that has played cannot be restored into. That fixes the flow
**[author decision]**:

1. **Configure**: declare the schema, install the plugins, register the systems and insert the C++-only resources.
   `setUpGame` and the game's own configuration do this; they spawn no entity.
2. **Either** create the world of a new game (spawn the true state, insert the `.oly` resources) **or** restore a save.
   Restore runs here, before the first frame: dense ids are resolved when the schema is declared (§5.2), not when
   registration closes, so they already exist.
3. **The first frame**: registration closes, the rebuild phase runs once (§5.13), and each root machine starts in its
   first state unless a restore set it.

A game that loads in the middle of a session creates a new `World` and `Engine` and goes through the three steps; the C++
resources are then fresh from configuration, so nothing from the previous play reaches the continuation. "The rebuild
phase runs on the first frame and after each restore" (§4) holds, since every restore precedes a first frame. The
Enxame's setup (`scenario.cpp`), which today spawns while inserting resources, splits along steps 1 and 2. How a native
game swaps its `World` and `Engine` under `runGame` is the builder's; the template shows it.

### 5.10 `instantiate`, designed and deferred

`World::instantiate(scene)` reads the same format into any `World`: new entities, a map from saved id to new id, and
every `entity` field (in components, nested structs and arrays) rewritten through the reflection table. Phase 2 adds
nothing for it beyond keeping the ids and the `entity` field type in the format. Three questions wait for a real use: a
reference that points outside the scene (null or error), whether a scene carries scope tags, and whether it carries
resources.

### 5.11 A save from another build (2c)

The save describes itself (§5.9, the type table), and the load is tolerant by name **[user decision]**:

- **The loading build has a plugin more.** Its types are absent from the save; its resources keep what setup made.
  This is the case row 7 gates.
- **A type unknown to the loading build** (a plugin or mod removed): its components are dropped, the entity keeps the
  rest, and the report lists the type.
- **A known type whose layout changed:** fields are copied by name. A new field is zero, and a new `entity` field is
  the null entity, not zero bits (zero bits name a valid entity). A removed field is dropped. A field whose type
  changed refuses the load.
- **An enum whose values changed** is read by value name; a saved value whose name no longer exists refuses the load,
  like a field whose type changed **[author decision]**.
- **Machines** **[author decision]**: a saved machine the loading build does not know is dropped and reported; a machine
  the save lacks starts in its first state, as in a new game, and is reported; a saved state that no longer exists
  refuses the load, because no state is a safe guess for the game's logic.
- The common case, an equal layout, stays one copy per column. S4, which loads in the same build, never takes the
  field-by-field path.
- The load report is a returned value: dropped types, dropped fields and zeroed fields **[author decision]**. The game
  decides whether to tell the player.

### 5.12 C API and TS

| Plan | C API | TS |
|---|---|---|
| 2a | resolve a stable id to a handle; actions and machines from plugins; request and read a state per machine; `scopedTo` on spawn; `olySchemaHash` and `olyContextPersist` removed | one module per namespace; per-type check by stable id and layout replaces the hash check; machines and `scopedTo`; `persist` removed |
| 2b | the reflection table readable through the C API | the new field types (`bigint`, enums as numbers, fixed arrays, nested structs) in `fields.ts`; the reflection table as data |
| 2c | `olySave` and `olyRestore`, the latter with the load report | `engine.save(): Uint8Array`, `engine.restore(bytes)` returning the report; no visual inspector (`limits.md`) |

The inspector's core API is C++ only in phase 2, written in `limits.md` **[author decision]**.

### 5.13 The rebuild phase (2c)

- A third schedule, fixed by the engine, beside the fixed step and the frame. It runs once at the start of the first
  frame, before the first fixed step **[user decision]**; since every restore precedes a first frame (§5.9, loading in
  a fresh process), that one run also follows each restore. The
  engine declares one phase in it; plugins add systems and phases as in the other two, with phase 1's rules (declared
  access, the conflict check, a total order of phases) **[author decision]**.
- Setup creates true state only. Derived state is built by systems in the rebuild phase, the same code after a new game
  and after a load.
- `essentials2d` and `essentials3d` rebuild their children index from `Parent` there.

### 5.14 The inspector (2c)

- **`inspector`** (`plugins/inspector`, an engine plugin): lists entities and their reflected components and reads and
  writes any field by path (`game::Assembler.stock`, `game::Inventory.slots[3].count`), through the reflection table
  only (row 7). It is testable with no window.
- **`inspector_imgui`** (`plugins/inspector_imgui`, an engine plugin, depends on `inspector`): Dear ImGui with its SDL3
  and `SDL_Renderer` backends, fetched at a pinned release tag in `cmake/Dependencies.cmake`. It draws in its own frame
  phase after `Render`, as an exclusive system. It builds only with `OLY_BUILD_NATIVE_HOST`.
- **The host exposes the surface** (window, renderer, and the frame's SDL events) as resources, and forwards each SDL
  event to the front end before the action map, so a click on the inspector does not place a turret. Which events the
  front end consumes must not depend on install order **[author decision]**.
- The Enxame's drawing stays in the game. Choosing between two providers of one role (two inspectors, two renderers)
  belongs to phase 5 **[user decision]**.

### 5.15 The Enxame

Every change points to a row (roadmap rule 6) and keeps the frozen workloads (rule 7). Where a change moves work, the
builder keeps the work the same or re-measures on the phase 0 commit first, as phase 1 did for `physics2d`.

- **`camera2d`** (row 5): an engine plugin in `plugins/` that declares `pan`, `zoomIn` and `zoomOut` and owns the
  camera resource and `moveCamera`, which today live in `games/enxame/desktop/window_game.cpp:65-81` and
  `native/resources.hpp:18-22`. No scenario runs `moveCamera`.
- **`waves`** (row 5): a game plugin in `games/enxame/` declaring `machine waves { intermission, assault }`; `runWaves`
  (`native/waves.cpp`) moves into it. S8's workload (spawn at the start of a wave, destroy at its end) must stay the same.
- **`railgun`** (row 6): a mod, a game plugin with its own `Body`, running after combat in `UpdateAfter` (roadmap
  §5). **[author decision]** `physics2d` declares `Body` (a velocity) and integrates `Position` by it in `Physics`; the
  railgun's slugs move by it. No frozen scenario spawns a `Body`, so their work is unchanged, apart from one empty
  system per step, recorded (rule 4). The horde's integration stays in the game (`limits.md` line 3).
- **Row 8 and the save:** `Random` becomes `resource Random { state: u64 }` with free functions; the map's seed becomes
  a `u64` in an `.oly` resource, and the map is derived: rebuilt from the seed and the `Turret` entities in the rebuild
  phase. `Belts::assemblerOfBelt` is rebuilt there from `Assembler::belt`. The game's types move to `game::`.
- **The inspector:** the desktop Enxame turns on `inspector_imgui`.
- **S4** (row 7): a headless scenario that runs S1's and S2's frozen worlds to step k, saves to a file, restores in a
  new process, runs to step n and compares the world hash with a straight run to n; it also checks that
  `save(restore(save(w)))` equals `save(w)` byte for byte. Time and size are recorded. The first measurement is the
  baseline (rule 5); phase 0 cannot run S4, so phase 2 records it with no ratio.

### 5.16 What else changes

- `docs/`: `schema-and-codegen.md`, the schema reference and `engine/schema/format/README.md` (grammar, namespaces,
  stable ids); `plugins.md`, `docs/how-to/write-a-plugin.md` and `use-a-plugin.md` (public and engine plugins, game
  plugins, plugins with no `install`, actions and machines in plugins); the states pages (machines, sub-machines,
  explicit scope); new pages for save and restore and for the inspector; `docs/reference/c-api.md`, the TS guides,
  `errors.md`, `cmake.md`, `limits.md`; `glossary.md` gains qualified name, stable id, namespace, reflection table,
  machine, sub-machine, scope, snapshot, restore, load report and the rebuild phase.
- `decisions.md`: rows for stable ids by qualified name, internal and external ids, the exact snapshot, the tolerant
  load, the rebuild phase, machines and explicit scope, the two plugin functions, and Dear ImGui as the native front
  end.
- `CHANGELOG.md` `## Unreleased`, with a `### Migration` entry: types under `game::` and `<plugin>::`, `state` replaced
  by `machine`, `persist` removed, `scopedTo`, `schemaHash` removed, `HasChildren` out of `detail`.
- The game template (`templates/game/`), the fixture schemas and every test that names a schema type, a state or
  `persist`.

## 6. Verification

The first commit of 2c, before any save code, replaces the blocked S4 with its instrument and records the S1 and S2
world hashes it will compare against (roadmap rule 5).

| Criterion | Proof | Command |
|---|---|---|
| Stable ids: same id in every build; collisions refused | `oly-schema` corpus cases (collision, reserved namespace, repeated name in one namespace, same name in two) | `./spur test` |
| Row 5: plugin actions and machines resolve at run time; one plugin binary in two games with different plugin orders | two engine test games (`oly_add_engine_test_game`) that list the same two native test plugins in opposite orders and link the same plugin library. "Works" means each plugin's own checks pass in both games; the order of handlers, hooks and observers across plugins stays as `limits.md` line 10 records, so the test plugins do not depend on it **[author decision]** | `./spur test` (the native fixture games build in `debug-asan`, `engine/testing/CMakeLists.txt:17-18`) |
| Row 5 in the Enxame: `camera2d` declares `pan` and zoom, `waves` declares `intermission` and `assault` | the Enxame's tests | `./spur dev sh -c 'cd games/enxame && OLY_ENGINE_DIR=/app OLY_IN_DOCKER=1 sh ./oly test'` |
| Machines: sub-machine entry and exit cascade, request order, explicit scope, a cycle or second parent refused | `app` suites and `contract_death` cases | `./spur test` |
| Row 6: `physics2d::Body` and `railgun::Body` coexist; the mod needs no engine change | an engine test with a fixture plugin built by the public `oly_add_plugin`, plus the Enxame | `./spur test`; the Enxame command above |
| `oly_add_engine_plugin` refused outside the engine | `test_check_boundaries.py` | `./spur test` (`oly.lint.boundaries_selftest`) |
| Row 8: same layout in C++ and TS | the corpus's `Inventory`, `ItemStack` and `Recipe` in the C++ `static_assert`s and in the TS suite's per-type check | `./spur test`; `./spur games-test` |
| References across namespaces: a type from a dependency is accepted, from a non-dependency refused | `oly-schema` corpus merge cases, and a native fixture plugin whose struct a fixture game uses in a field | `./spur test` |
| Row 7: a save from a build with one plugin less loads; a type unknown to the loading build is dropped and reported; a changed field is copied by name | two engine test games that differ by one test plugin, and a fixture type in two versions | `./spur test` |
| Row 7: the inspector edits a field of a nested struct, an array element, an enum and a `u64` with no per-component code | `inspector` plugin tests, headless | `./spur test` |
| The inspector front end builds and draws | `inspector_imgui` tests with SDL's dummy driver | `./spur native-games-test`; `./spur windows-test` |
| Restore keeps the invariants | `checkInvariants` after every restore in the `ecs` suites | `./spur test` |
| S4: same hash after save, restore in a new process and continuation; byte-equal round trip | the S4 scenario's test in the Enxame | the Enxame command above |
| S4 baseline: time and size | S4 in release, recorded with its commit, no ratio. `compare.sh` builds two commits and phase 0 cannot run S4, so the plan names a single-commit run (an S4 mode of `compare.sh`, or the game's release path) **[hypothesis until then]** | named by the 2c plan |
| Cost of the other scenarios | S1–S3 and S5–S8 against the phase 0 commit, ratios recorded, not gated (rule 4) | `sh games/enxame/compare/compare.sh 5eec212 <head>` on the host |
| No behavior change in the other scenarios | every scenario's `worldHash` against the values recorded in phase 1, unless a rule 7 re-measure changed one | the Enxame command above |
| C API and TS | the TS tests of the engine and of the fixture game | `./spur games-test` |
| The template game | create, build and test a game in local mode | `./spur consumer-test` |

## 7. Checked against the later phases

- **Phase 3 (jobs, determinism, change detection).** Dense ids stay internal, so phase 1's bitsets are unchanged. A
  state request is a write per machine. The rebuild phase follows the declared-access rules. The exact snapshot is what
  replay needs. Change detection must treat a restore as a change of every restored column, and inspector writes must
  go through a path it can mark; phase 3 owns both. The format does not depend on threads.
- **Phase 4 (sparse storage, relations, archetype removal).** The format describes tables, so phase 4 writes a new
  format version and decides whether to read version 1 (accepted with the exact snapshot). Relations replace `Parent`
  and `HasChildren`; the reflected tag goes with them. Sparse tags and removed archetypes change section 6 of §5.9's format.
- **Phase 5 (resources and assets).** `.oly` resources are data only; services stay C++ objects, outside the save.
  Asset handles become a field type there, with a reflection descriptor and a save encoding. Choosing between providers
  of one role (the inspector's front end) is phase 5's.
- **Phase 6 (extraction and `render2d`).** Dear ImGui has a backend for SDL3's GPU API, so `inspector_imgui` follows
  `render2d` off `SDL_Renderer`. When step N+1 simulates while step N renders, the inspector must read a consistent
  world (the snapshot, or between steps); phase 6 places it.

## 8. Left to the builder

- The exact names: of `oly_add_engine_plugin`, of the handle types and their generated spelling (the sketch's
  `waves::waves` for a machine named after its namespace), of `bindSubMachine`, `runIf`, `inState`, `scopedTo`, the
  rebuild schedule and phase, `save`, `restore`, `LoadReport`, and the C API calls.
- The suffix of the scope tags' qualified names.
- How the TS modules per namespace are laid out on disk and how `TYPESCRIPT` names them.
- The byte layout inside each section of the format, and the magic.
- The `.oly` field paths the inspector accepts, and how `inspector_imgui` presents them.
- How the host hands SDL events to the front end, under the rule of §5.14.
- The Dear ImGui release tag.
- What `waves` does with S8's spawn and destroy under rule 7, and how the railgun's slugs play, kept to row 6.
- Where in 2a, 2b and 2c each Enxame change lands, as long as each plan closes its rows.
