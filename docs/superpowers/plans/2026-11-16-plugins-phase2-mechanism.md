# Plugins, phase 2 (order bands and the plugin mechanism) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A plugin is declared in CMake, turned on per game with `PLUGINS`, merged into the game's schema by
`oly-schema` and installed into the `Engine` by generated code, in dependency order, with no line of game code.

**Architecture:** The `Engine` gets an order band per system (`Default`, `Last`) and owns plugin instances
(`installPlugin<P>()`, `plugin<P>()`, destroyed in reverse order). `oly-schema` accepts `--plugin <name>=<file.oly>`
inputs and merges them before the game's file into one layout and one hash; a new `cpp-plugin` emitter writes a
plugin's own header, which the game's header includes. `oly_add_plugin` defines a plugin target; `oly_add_game(...
PLUGINS ...)` resolves the transitive plugins, runs `oly-schema` with them, links them and generates
`game_setup.generated.cpp`, which defines `oly::app::setUpGame(Engine&)`: declare the schema, then install the plugins.
TS emission moves into `oly_add_game`, so the plugin list lives only in CMake. Two test plugins in
`engine/testing/fixture/plugins/` prove the mechanism on `native` and `web`; `essentials` comes in phase 3.

**Tech Stack:** C++20, CMake 3.25, doctest, Python 3 (lint), TypeScript (node test), Docker through `./spur`.

**Spec:** `docs/superpowers/specs/2026-11-16-plugins-design.md` (§3, §4, §6, §7, §11 steps 2 and 3).
**Depends on:** `docs/superpowers/plans/2026-11-16-plugins-phase1-observers.md` (merged first).

## Global Constraints

- The toolchain runs only in Docker, through `./spur <task>`. After each task: `./spur test` (debug-asan) passes; from
  Task 4 on also `./spur native-games-test`, `./spur games-test` and `./spur windows-test`.
- Spec §3: a plugin includes only public headers of the modules and plugins it declares; a plugin is its own CMake
  target, with no path relative to the repository root; a plugin's `.oly` reaches `oly-schema` as an argument, never
  through a fixed path.
- Spec §4.2: a plugin installs once per `Engine`, before the first `frame`; installing during `frame` is an
  `OLY_ASSERT`. The `Engine` owns the instances and destroys them in reverse install order. Install order: topological
  over the dependencies; among independent plugins, the order of the `PLUGINS` list.
- Spec §4.4: the band is fixed at registration; inside a band, registration order; no branch per system in `runPhase`
  beyond today's `whilePaused` and state checks.
- Spec §6: merged schema = plugins in install order, then the game; flat names, a repeated name is an error naming both
  `<file>:<line>:<column>`; the 32-state limit counts the merged set; the hash covers the merged canonical form; C++
  structs generated once (ODR); a plugin `.oly` declares only components, tags and events.
- Anti-goals: `engine/` never depends on a plugin; a plugin never needs a line of game code; the plugin list is written
  in one place (CMake).
- `oly-schema` without `--plugin` behaves and hashes exactly as today (existing hash tests unchanged).
- New tests follow the `add-test-case` skill. `OLY_ASSERT` messages include the operation and the type name.
- Commits, comments and assert messages in English; commit subject one short line, conventional prefix, no spec, plan
  or task references. `.cpp`/`.hpp` formatted by the hook. Comments only for a reason or a contract.
- Glossary rule: no alias, no compatibility wrapper. `capi` and `drift` move from `declareSchema` to `setUpGame` with no
  forwarding shim.
- Public API changes update `docs/guide/` in the same PR (Task 7).

**New names (decided here):**

| Name | Where |
|---|---|
| `enum class OrderBand : uint8_t { Default, Last }`, `OrderBandCount` | `oly/app/phase.hpp` |
| `Engine::addSystem(Phase, std::string_view, SystemFunction, void* userData = nullptr, OrderBand band = OrderBand::Default)` | `engine.hpp` |
| `class Plugin { virtual ~Plugin(); virtual void install(Engine&) = 0; }` | `oly/app/plugin.hpp` |
| `template <class P> P& Engine::installPlugin()`, `template <class P> P& Engine::plugin()`, `Engine::~Engine()` | `engine.hpp` |
| `void setUpGame(Engine&)` | `oly/app/game.hpp`; defined by the generated `game_setup.generated.cpp` |
| plugin convention: target `oly_<name>` (alias `oly::<name>`), namespace `oly::<name>`, class `oly::<name>::Plugin` in `oly/<name>/plugin.hpp`, generated header `oly/<name>/schema.generated.hpp` | `cmake/OlyPlugin.cmake` |
| `oly_add_plugin(NAME <p> [SCHEMA <file.oly>] DEPENDS <modules...> [PLUGINS <plugins...>])` | `cmake/OlyPlugin.cmake` |
| `oly_plugin_order(<outputVariable> <plugins...>)` | `cmake/OlyPlugin.cmake` |
| `oly_add_game(<target> ENVIRONMENT ... SCHEMA ... SOURCES ... DEPENDS ... [PLUGINS ...] [TYPESCRIPT <file.ts>])` | `cmake/OlyGame.cmake` |
| `oly_add_engine_test_game(...)`: same arguments, only for `engine/` | `cmake/OlyGame.cmake` |
| `oly_typescript_schemas` (custom target) | `cmake/OlyGame.cmake` |
| `oly-schema <file.oly> [--plugin <name>=<file.oly> ...] --emit ...`; emitter `cpp-plugin` | `engine/schema/codegen/` |
| `struct PluginSource { std::string name; std::string fileName; std::string source; }`, `Schema mergeSchemas(std::span<const PluginSource>, std::string_view gameSource, std::string_view gameFileName)`, `Type::origin`, `Schema::plugins` | `oly/schema/schema.hpp` |
| test plugins `counting` and `doubling` | `engine/testing/fixture/plugins/` |

**Decided here (not in the spec):**

- **Bands are storage, not a branch.** `phases_` becomes `std::array<std::array<std::vector<SystemEntry>,
  OrderBandCount>, PhaseCount>`; `systemLocations_` keeps `(phase, band, position)`. `runPhase` walks the bands in order
  and flushes once per phase, as today. The C API and the TS client keep registering in `Default` (no client surface in
  this project).
- **Plugin keys.** `installPlugin<P>` identifies `P` with a dense per-process key, the same pattern as `eventKey<T>()`
  (`detail::nextPluginKey()`). `P` derives from `Plugin` and is default constructible (`static_assert`).
- **"Before the first frame".** The `Engine` gets `bool started_`, set by the first `frame` call (the one that only
  records the time). `installPlugin` asserts `!inFrame_ && !started_` and that `P` is not installed yet.
- **A plugin removes what it registered on the World.** The `World` outlives the `Engine` (`OlyEngine` and the native
  `main` declare it first), and observers keep a pointer to the plugin. The contract of `Plugin`: its destructor removes
  the observers it registered (`World::removeObserver`). Systems, handlers and hooks die with the `Engine`.
- **The game entry point.** `setUpGame` replaces `declareSchema` as the call that `olyEngineCreate` and a native `main`
  make. `declareSchema` stays declared in `oly/app/schema.hpp` and is called by the generated `setUpGame`. The capi test
  executables that define `declareSchema` today define `setUpGame` instead.
- **CMake generates `game_setup.generated.cpp`**, not `oly-schema`: plugin C++ types are a build fact, not schema data.
  Its body is `declareSchema(engine);` then one `engine.installPlugin<oly::<p>::Plugin>();` per plugin in install order.
- **Plugin sources.** `oly_add_plugin` globs `src/*.cpp` like `oly_add_module` (a `SOURCES` list would be the only
  difference between the two functions). Its include directory is `include/`, plus the generated directory when it has a
  `SCHEMA`. It requires `src/` and `oly::<name>::Plugin`: a plugin with no install (a pure function library) is not
  supported until one exists.
- **CLI.** `--plugin <name>=<file.oly>` may appear before or after any `--emit`; the order of the `--plugin` arguments is
  the install order (CMake passes them sorted). `cpp-plugin=<directory>` writes `<directory>/schema.generated.hpp` with
  the file's own types, no declaration and no registry; CMake passes `<generated>/oly/<name>`.
- **Merge errors.** A repeated name across files: `<later file>:<l>:<c>: repeated name "X"; first declared at <earlier
  file>:<l>:<c>`. An action or a state in a plugin file: `<file>:<l>:<c>: a plugin declares only components, tags and
  events; action "x" is not allowed` (`state "x"` for states). The game's header includes each plugin header with a
  schema, in install order, and defines only the types whose `origin` is empty.
- **Plugin order in CMake.** `oly_plugin_order` is a depth-first walk over `OLY_PLUGIN_DEPENDENCIES_<name>` global
  properties, dependencies first, each plugin once, list order among independent ones. An unknown plugin is
  `FATAL_ERROR "unknown plugin <p>"`; a cycle is `FATAL_ERROR "plugin cycle: a -> b -> a"`. It is tested by a CMake
  script (`cmake/tests/plugin_order.cmake`), one ctest per case, `oly.cmake.plugin_order.<case>`.
- **TS emission.** `oly_add_game(... TYPESCRIPT <file.ts>)` adds a custom command writing that file in the source tree
  and a per-game target that `oly_typescript_schemas` depends on. `./spur ts` builds `oly-schema`, configures
  `${PRESET:-wasm-games}`, builds `oly_typescript_schemas`, generates the test-only schemas of
  `engine/testing/fixture/schemas/` with the loop it has today (restricted to that folder), then runs `tsc`. Task names
  do not change.
- **Engine test games.** `oly_add_engine_test_game` shares the body of `oly_add_game` (a private
  `_oly_add_game_target`). In phase 2 the two functions differ only in that the lint forbids the second outside
  `engine/`; phase 3 adds `DIMENSION` to `oly_add_game` alone. `engine/testing/` is added on `web` with games (as today)
  and on `native` with tests, so `debug-asan` runs the native plugin fixture.
- **Test plugins.** `counting`: `counting.oly` with `component Counter { value: u32 }` and `event Counted { value: u32 }`;
  a `Default`-band `Update` system that adds 1 to every `Counter`. `doubling` (`PLUGINS counting`): `doubling.oly` with
  `component Doubled { value: u32 }`; a `Last`-band `Update` system that writes `Doubled.value = 2 * Counter.value`.
  The fixtures list only `PLUGINS doubling`, so `counting` comes in through the dependency.
- **Lint.** Module and plugin names may contain digits (`essentials2d`): the include patterns become `[a-z0-9_]+`.

## Review Focus

1. **A game listing a plugin and its dependency in the other order** (`PLUGINS doubling counting`). Expected: `counting`
   installs first and once. Pinned in Task 4 (`oly.cmake.plugin_order.dependency_listed_after`).
2. **Destroying the `Engine` while the `World` lives on.** Expected: no observer of a destroyed plugin runs afterwards; a
   later structural change on the `World` is clean under ASan. Pinned in Task 2.
3. **A TS client built with another plugin set.** Expected: the hash differs and `SchemaMismatchError` names it. Pinned
   in Task 3 (hash differs with and without `--plugin`) and in Task 5 (the web fixture loads with its own TS file).
4. **A plugin header and the game header both defining a type.** Expected: impossible by construction; a repeated name
   fails in `oly-schema` before any C++ compiles. Pinned in Task 3.
5. **A `Last` system registered before a `Default` one in the same phase.** Expected: `Default` runs first, and the
   phase flushes once. Pinned in Task 1.

## File Structure

| File | Responsibility |
|---|---|
| `engine/runtime/app/include/oly/app/phase.hpp`, `engine.hpp`, `src/engine.cpp` | bands, plugin ownership |
| `engine/runtime/app/include/oly/app/plugin.hpp`, `game.hpp` (new) | `Plugin`, `setUpGame` |
| `engine/runtime/app/tests/plugins.cpp` (new), `systems.cpp`, `contract_death.cpp` | tests |
| `engine/schema/codegen/include/oly/schema/schema.hpp`, `src/merge.cpp` (new), `src/generator.cpp`, `src/cpp_emitter.cpp`, `src/emitter.cpp` | merge, CLI, `cpp-plugin` |
| `engine/schema/codegen/tests/merge.cpp` (new), `generator.cpp`, `cpp_emitter.cpp`; `engine/schema/format/corpus/merge/` (new) | tests and corpus |
| `cmake/OlyPlugin.cmake` (new), `cmake/OlyGame.cmake`, `cmake/tests/plugin_order.cmake` (new), `CMakeLists.txt`, `engine/CMakeLists.txt` | build |
| `engine/runtime/capi/src/engine_api.cpp`, `capi/tests/*` | `setUpGame` |
| `engine/testing/CMakeLists.txt`, `engine/testing/fixture/plugins/` (new), `engine/testing/tests/wasm/load.test.ts` | fixtures |
| `games/drift/desktop/main.cpp`, `games/arena/CMakeLists.txt`, `Spurfile` | callers, TS emission |
| `tools/check_boundaries.py`, `tools/test_check_boundaries.py` | lint |
| `docs/`, `CLAUDE.md` | Task 7 |

---

### Task 1: Order bands

**Files:**
- Modify: `engine/runtime/app/include/oly/app/phase.hpp`, `engine/runtime/app/include/oly/app/engine.hpp`, `engine/runtime/app/src/engine.cpp`
- Test: `engine/runtime/app/tests/systems.cpp`

**Interfaces:**
- Produces: `OrderBand`, `OrderBandCount`, the `addSystem` overload above (the band is the last parameter, default
  `Default`, so every existing call compiles unchanged).

- [ ] **Step 1: Write the failing tests**

  ```cpp
  TEST_CASE("a Last system runs after every Default system of its phase, whatever the registration order");
            // register last1 (Last), then a, b (Default) in Update → log {"a", "b", "last1"}
  TEST_CASE("Last systems keep their registration order");
  TEST_CASE("a phase with Last systems flushes once, after the Last band");
            // a Default system spawns; a Last system sees isAlive == false (deferred); UpdateAfter sees it alive
  TEST_CASE("setWhilePaused and setSystemStates apply to a Last system");
  ```

- [ ] **Step 2: Run to verify they fail.** Run: `./spur test -R 'app\.systems'`. Expected: build error (`OrderBand`).
- [ ] **Step 3: Implement** the storage of "Decided here".
- [ ] **Step 4: Run.** `./spur test -R '^app\.'` Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "feat(app): run Last-band systems at the end of their phase"`

### Task 2: Plugin ownership in the `Engine`

**Files:**
- Create: `engine/runtime/app/include/oly/app/plugin.hpp`, `engine/runtime/app/tests/plugins.cpp` (suite `app.plugins`)
- Modify: `engine.hpp`, `src/engine.cpp`, `tests/CMakeLists.txt`, `tests/contract_death.cpp`

**Interfaces:**
- Consumes: Task 1 (`OrderBand`); phase 1 (`World::onAdd`, `removeObserver`).
- Produces: `Plugin`, `installPlugin<P>()` (returns the instance; runs `install(*this)` once), `plugin<P>()`
  (`OLY_ASSERT` if absent), `~Engine()` (plugins destroyed in reverse install order).

- [ ] **Step 1: Write the failing tests** with small plugins defined in the test file:

  ```cpp
  TEST_CASE("installPlugin runs install once and plugin<P> returns the same instance");
  TEST_CASE("plugins are destroyed in reverse install order");          // destructors append to a log
  TEST_CASE("a plugin keeps state reachable from its systems through userData");
  TEST_CASE("after the Engine is destroyed, the World runs no observer of its plugins");
            // the plugin registers onAdd<Position>, removes it in its destructor; add Position after ~Engine → log empty
  ```

  Death cases: `install_plugin_twice` (`"installPlugin"` and the type name), `install_plugin_after_first_frame`
  (`"installPlugin"`, `"frame"`), `plugin_not_installed` (`"plugin"` and the type name).

- [ ] **Step 2: Run to verify they fail.** `./spur test -R 'app\.(plugins|contract_death)'`. Expected: build error.
- [ ] **Step 3: Implement.** `std::vector<std::pair<PluginKey, std::unique_ptr<Plugin>>> plugins_`; `~Engine()` pops
  from the back. Document the destructor contract on `Plugin` (removes its observers).
- [ ] **Step 4: Run.** `./spur test -R '^app\.'`. Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "feat(app): let the Engine own and install plugins"`

### Task 3: Schema merge in `oly-schema`

**Files:**
- Create: `engine/schema/codegen/src/merge.cpp`, `engine/schema/codegen/tests/merge.cpp` (suite `schema.merge`), `engine/schema/format/corpus/merge/<case>/` (each case: `1-<plugin>.oly`, `2-<plugin>.oly`..., `game.oly` whose first line is `# expect: <line>:<column>: <message>` or `# expect: ok`)
- Modify: `include/oly/schema/schema.hpp`, `src/generator.cpp`, `src/cpp_emitter.cpp`, `include/oly/schema/cpp_emitter.hpp`, `src/emitter.cpp`, `tests/generator.cpp`, `tests/cpp_emitter.cpp`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `PluginSource`, `mergeSchemas`, `Type::origin`, `Schema::plugins` (names of the plugins with a schema, in
  order), `emitCppPluginHeader(const Schema&)`, emitter `cpp-plugin`, the CLI above. The TS emitter needs no change: it
  emits the merged set.

- [ ] **Step 1: Write the failing tests**

  ```cpp
  TEST_CASE("plugin declarations come first: ComponentId order, event order");   // types[0].name == "Counter", origin == "counting"
  TEST_CASE("without plugins the merged schema equals parseSchema, hash included");
  TEST_CASE("the hash changes when a plugin is added");
  TEST_CASE("a name repeated across files names both locations");
            // "game.oly:1:11: repeated name \"Counter\"; first declared at counting.oly:1:11"
  TEST_CASE("an action in a plugin file is an error");                     // "...: a plugin declares only components, tags and events; action \"jump\" is not allowed"
  TEST_CASE("a state in a plugin file is an error");
  TEST_CASE("the game header includes the plugin headers and defines only the game types");
            // contains "#include \"oly/counting/schema.generated.hpp\"" and no "struct Counter"
  TEST_CASE("the declaration and the registry cover the merged set");      // declareEvent<Counted> before the game's events
  TEST_CASE("cpp-plugin writes only the plugin types");                    // generator test with a temp directory
  TEST_CASE("--plugin is accepted before and after --emit, and a malformed --plugin prints the usage");
  ```

  Corpus test in `merge.cpp`: every folder of `corpus/merge/` merges its numbered plugin files (sorted) with
  `game.oly` and matches the `# expect:` line. Cases: `repeated_component`, `repeated_event`, `plugin_action`,
  `plugin_state`, `repeated_across_plugins`, `ok_two_plugins`.

- [ ] **Step 2: Run to verify they fail.** `./spur test -R '^schema\.'`. Expected: build error (`mergeSchemas`).
- [ ] **Step 3: Implement.** `mergeSchemas` parses each file with `parseSchema` (its own validation), checks the plugin
  rule, checks cross-file names, concatenates in order, recomputes the hash over `canonicalForm` of the merged schema.
  `parseSchema` sets `origin` empty; `mergeSchemas` sets it on plugin types. `runGenerator` parses the `--plugin` pairs,
  reads every file before writing anything, and builds the schema with `mergeSchemas` (with zero plugins this is the
  current path). Update the usage line to
  `usage: oly-schema <file.oly> [--plugin <name>=<file.oly> ...] --emit <emitter>=<target> [--emit <emitter>=<target> ...]`.
- [ ] **Step 4: Run.** `./spur test -R '^schema\.'` and `./spur test -R 'schema_typescript'`. Expected: all pass,
  existing hash values unchanged.
- [ ] **Step 5: Commit** `git commit -m "feat(schema): merge plugin schemas before the game's"`

### Task 4: Plugins in the build and the generated install

**Files:**
- Create: `cmake/OlyPlugin.cmake`, `cmake/tests/plugin_order.cmake`, `engine/runtime/app/include/oly/app/game.hpp`, `engine/testing/fixture/plugins/counting/{counting.oly,include/oly/counting/plugin.hpp,src/plugin.cpp}`, `engine/testing/fixture/plugins/doubling/{doubling.oly,include/oly/doubling/plugin.hpp,src/plugin.cpp}`, `engine/testing/fixture/plugins/native/plugin_fixture.oly`, `engine/testing/fixture/plugins/native/plugins_test.cpp`
- Modify: `CMakeLists.txt` (include `OlyPlugin`, plugins in the manifest, the `oly.cmake.plugin_order.*` tests), `cmake/OlyGame.cmake`, `engine/CMakeLists.txt`, `engine/testing/CMakeLists.txt`, `engine/runtime/capi/src/engine_api.cpp`, `engine/runtime/capi/tests/{contract_death,test_schema_registry}.cpp`, `games/drift/desktop/main.cpp`

**Interfaces:**
- Consumes: Tasks 1–3.
- Produces: `oly_add_plugin`, `oly_plugin_order`, `oly_add_game(... PLUGINS ...)`, `oly_add_engine_test_game`,
  `setUpGame`, the plugins `counting` and `doubling`, the ctest `fixture.plugins` (native).

- [ ] **Step 1: Write the failing tests**
  - `cmake/tests/plugin_order.cmake`, run as `cmake -DCASE=<case> -P`; cases and expectations (ctest
    `PASS_REGULAR_EXPRESSION`): `independent_keep_list_order` → `order=a;b`, `dependency_first` → `order=a;b` for
    `b` depending on `a` and the list `b`, `dependency_listed_after` → `order=a;b` for the list `b;a`, `transitive` →
    `order=a;b;c`, `shared_dependency_once` → `order=a;b;c` for `b` and `c` both depending on `a`, `cycle` →
    `plugin cycle: a -> b -> a`, `unknown` → `unknown plugin z`.
  - `plugins_test.cpp` (native, doctest), linked into `plugin_fixture_game` built by `oly_add_engine_test_game(...
    ENVIRONMENT native ... PLUGINS doubling)`:

    ```cpp
    TEST_CASE("setUpGame installs the dependency before the plugin that needs it");
              // plugin<oly::counting::Plugin>() and plugin<oly::doubling::Plugin>() both exist; install log {"counting", "doubling"}
    TEST_CASE("plugin events are declared before the game's");   // eventTypeOf(eventKey<Counted>()) == 0
    TEST_CASE("the Last-band system of a plugin sees the step's Default-band changes");
              // spawn Counter{0} + Doubled{0}; one step → Counter 1, Doubled 2
    ```

- [ ] **Step 2: Run to verify they fail.** `./spur test -R '(oly\.cmake|fixture\.plugins)'`. Expected: configure error
  (`oly_add_plugin` unknown) or the tests not found.
- [ ] **Step 3: Implement**
  - `oly_add_plugin`: target `oly_<name>` (STATIC, `src/*.cpp`), alias `oly::<name>`, `include/` PUBLIC; with `SCHEMA`,
    a custom command `oly-schema <file> --emit cpp-plugin=<binary>/oly_<name>_generated/oly/<name>` and that root as a
    PUBLIC include directory; links `oly::<module>` for each `DEPENDS` and `oly::<plugin>` for each `PLUGINS`; sets
    `OLY_PLUGIN_SCHEMA_<name>` and `OLY_PLUGIN_DEPENDENCIES_<name>`; `oly_register_target(OLY_PLUGINS ...)` with modules
    and plugins as dependencies. The schema-tool choice (`oly-schema` target or `OLY_SCHEMA_TOOL`) moves from
    `oly_add_game` into a shared `oly_schema_tool(<toolVariable> <dependencyVariable>)`.
  - `_oly_add_game_target`: `oly_plugin_order` over `PLUGINS`; `--plugin <p>=<schema>` for each plugin with a schema;
    `DEPENDS` on those files; links the plugin targets; writes `game_setup.generated.cpp` with `file(GENERATE)` and
    compiles it; registers the game with its modules and plugins.
  - `setUpGame` in `game.hpp`; `olyEngineCreate` and `drift`'s `main` call it; the capi tests define it.
  - `engine/CMakeLists.txt`: `add_subdirectory(testing)` on web with games, and on native with tests;
    `engine/testing/CMakeLists.txt` adds the test plugins first, then per environment its fixtures.
  - `CMakeLists.txt` writes `"plugins": [...]` into `oly_boundaries.json`.
- [ ] **Step 4: Run.** `./spur test`, `./spur native-games-test`, `./spur windows-test`. Expected: all pass, including
  `drift.desktop`.
- [ ] **Step 5: Commit** `git commit -m "feat(build): declare plugins in CMake and install them from generated code"`

### Task 5: TS emission in the build and the web fixture

**Files:**
- Modify: `cmake/OlyGame.cmake` (`TYPESCRIPT`, `oly_typescript_schemas`), `games/arena/CMakeLists.txt`, `engine/testing/CMakeLists.txt` (`fixture_game` with `PLUGINS doubling` and `TYPESCRIPT`), `Spurfile` (`ts`), `engine/testing/tests/wasm/load.test.ts`, `docs/development/commands.md` (the `ts` line)

**Interfaces:**
- Consumes: Task 4.

- [ ] **Step 1: Write the failing tests** in `load.test.ts`:

  ```ts
  test("the fixture schema carries its plugins' components first");
        // Counter and Doubled exist in fixture.generated.ts and their ids are 0 and 1; Position follows
  test("the plugins' systems run inside the fixture module");
        // spawn Counter{0} and Doubled{0}, run one step → Counter 1, Doubled 2
  ```

- [ ] **Step 2: Run to verify they fail.** `./spur games-test -R 'ts\.testing'`. Expected: the import of `Counter` fails.
- [ ] **Step 3: Implement** the "TS emission" decision. Remove the `games/*/web` and `fixture.oly` parts of the `ts`
  loop; keep the loop over `engine/testing/fixture/schemas/*.oly`.
- [ ] **Step 4: Run.** `./spur games-test` and `PRESET=wasm-games-release ./spur games-test -R '^ts\.'`. Expected: all
  pass, `ts.arena.smoke` included.
- [ ] **Step 5: Commit** `git commit -m "build: generate each game's TS schema from its CMake target"`

### Task 6: Boundary lint for plugins

**Files:**
- Modify: `tools/check_boundaries.py`, `tools/test_check_boundaries.py`

**Interfaces:**
- Consumes: the `plugins` entries of the manifest (Task 4).

- [ ] **Step 1: Write the failing tests** in `test_check_boundaries.py`, one per rule, each with a fixture tree:
  `engine_includes_plugin` (a file in `engine/` including `oly/counting/...` from a plugin under `plugins/` →
  `"engine cannot depend on plugin counting"`), `plugin_includes_undeclared` (→ `"... without declaring ... in
  DEPENDS"`), `plugin_includes_other_detail` (→ `"oly/<x>/detail is internal"`), `game_name_in_plugin` (a file under
  `plugins/` containing the word `arena` when `games/arena/` exists → `"game name arena in plugins/"`),
  `engine_test_game_outside_engine` (a `CMakeLists.txt` under `games/` calling `oly_add_engine_test_game` →
  `"oly_add_engine_test_game is only for engine/"`), `digit_in_module_name` (an include of `oly/essentials2d/...`
  resolves its owner).
- [ ] **Step 2: Run to verify they fail.** `./spur test -R 'oly\.lint'`. Expected: the new cases fail.
- [ ] **Step 3: Implement.** Owners include `manifest.get("plugins", [])`; the include patterns accept digits; a file
  under `engine/` that includes a module whose directory is under `plugins/` is a violation; game names are the folder
  names under `games/`, matched as whole lowercase words in files and paths under `plugins/`; `CMakeLists.txt` files are
  scanned for `oly_add_engine_test_game` outside `engine/`.
- [ ] **Step 4: Run.** `./spur test`. Expected: all pass, `oly.lint.boundaries` included.
- [ ] **Step 5: Commit** `git commit -m "build(lint): keep plugins out of engine/ and game names out of plugins"`

### Task 7: Docs

**Files:**
- Create: `docs/architecture/plugins.md` (model, the three parts of spec §4.1 with the C API and TS package parts as
  shape only and the per-batch rule of §4.3, install and ownership, schema merge, build, boundaries; `essentials`
  arrives in phase 3), `docs/guide/concepts/plugins.md`
- Modify: `docs/architecture/{README,app,schema-and-codegen,modules-and-boundaries,repository,decisions,limits,glossary,capi,build-and-tests}.md`, `docs/guide/{README,cheat-sheet,troubleshooting,recipes}.md`, `docs/guide/concepts/engine-and-systems.md` (bands), `docs/guide/environments/native/getting-started.md` and the web getting-started pages (`setUpGame`, `PLUGINS`), `docs/development/{commands,guide-style}.md` (the `ts` task; `oly_add_plugin` and `cmake/OlyPlugin.cmake` in the public surface table), `CLAUDE.md` (plugins in the structure and the `plugins/` rules of §3)

- [ ] **Step 1:** Read `docs/development/guide-style.md`; write the pages above. The guide page shows how to write a
  plugin (`oly_add_plugin`, the `Plugin` class, `install`, the destructor contract) and how a game turns it on
  (`PLUGINS`).
- [ ] **Step 2:** Search `docs/` for `declareSchema` and replace each game-facing mention with `setUpGame`.
- [ ] **Step 3:** `./spur test`. Expected: all pass.
- [ ] **Step 4: Commit** `git commit -m "docs: describe plugins, order bands and setUpGame"`
