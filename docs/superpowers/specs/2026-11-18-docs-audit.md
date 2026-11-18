# Oly — documentation audit

Date: 2026-11-18. Base: `develop` at `1d6ee51`, clean tree. Diagnosis only: every finding carries `file:line` evidence
and at most one line of recommendation. Pages are classified with Diátaxis (tutorial, how-to, reference, explanation).
The reorganization that closes these findings is `2026-11-18-docs-reorganization-design.md`, which cites them as
**A1–A10** (§1), **D1–D18** (§6) and **§3.2** (public API without docs).

**Status legend:** **verified** = the declaration in the code (or a grep that proves the absence) contradicts the doc;
**suspected** = likely but not proven, or a question of wording or intent.

**How the examples were checked:** no example was compiled. The toolchain runs only through `./spur`, which mounts the
checkout at `/app` and writes to the repository's `./build` (`docker-compose.yaml:24-26`); compiling would have dirtied
the repository. Every example was checked **statically** against the public headers, `cpp_emitter.cpp`,
`typescript_emitter.cpp`, `public-exports.txt`, `@oly/web-host/src/index.ts`, `c_api.h`/`engine_api.h`, the `.cmake`
files and the game template.

---

## 1. Prioritized summary (impact on someone using the engine in a game)

| # | Problem | Evidence | Status | Recommendation |
|---|---|---|---|---|
| A1 | The test harness of the web guide does not compile against a game made by `oly new`. The doc uses `installGame(engine, options: GameOptions)` with `{drawer, random}`; the template uses `installGame(engine, drawer)`. Other pages use two more signatures. | `docs/guide/environments/web/testing.md:9,13-21,63`; template `templates/game/web/web/src/game.ts:8-13`; `typescript/states.md:78`, `typescript/recipes.md:334` | verified | Align with the template's signature, or say explicitly that it is a bigger game. |
| A2 | The `destroy` contract is wrong for systems. The doc shows `context.destroy(e); context.isAlive(e); // false`, but in a `SystemContext` `destroy` is deferred and `isAlive` stays `true` until the end of the phase. "Destroying a dead entity is a contract violation" also holds only for direct contexts and the `World`. | `docs/guide/concepts/world-and-entities.md:88-92`; `engine/runtime/app/include/oly/app/system.hpp:63-65`; `engine/runtime/ecs/include/oly/ecs/world.hpp:128-130` | verified | Separate direct from deferred behaviour in that passage. |
| A3 | The "Common messages" table of Troubleshooting mixes TS `ContractError` texts with C++ messages without labels. A C++ user does not find the message they get (`get e=%u:%u type=%s`, `addSystem: system "x" registered during frame`, `frame: time %f less than the previous %f`). "frame is not reentrant" and "last frame was interrupted" do not exist in C++. | `docs/guide/troubleshooting.md:33-34,40,50,54-56`; `engine/runtime/ecs/include/oly/ecs/world.hpp:352`; `engine/runtime/app/src/engine.cpp:48,83,125-127,138` | verified | Label each row by language, or split into two tables. |
| A4 | There is no CHANGELOG, and the docs themselves warn that `0.x` minors break compatibility. The release uses only `--generate-notes`, with no migration notes. | `docs/guide/cli.md:140`; `.github/workflows/release.yml:224`; no `CHANGELOG.md` | verified | Create a CHANGELOG with a "Breaking" section per version. |
| A5 | Public API that games use has no guide docs: `Engine::emit` outside `frame` (from the host), `CommandBuffer::empty/clear/commandCount`, `oly sync`, `OLY_BUILD_NATIVE_HOST`/`OLY_BUILD_TESTS` (a game using the engine as a subproject), the C API constants (`OLY_NO_STATE`, `OLY_STATE_ENTER/EXIT`, `OLY_INVALID_SYSTEM`, `OLY_ACTION_*`) and `ActionKind`. | §3 | verified | Document these first; the rest of §3 is low level. |
| A6 | The native guide's "smallest game" diverges from the template it claims to mirror. The example has no movement system and uses a 640x480 window; the template registers `movePlayer` and uses 640x360. The square that `:31` promises does not move. | `docs/guide/environments/native/getting-started.md:69`; `templates/game/native/desktop/main.cpp:15,19-20,68` | verified | Reproduce the template's `main.cpp`, or stop calling the example "the template". |
| A7 | `concepts/schema.md` shows `struct Position` inside the game's `schema.generated.hpp`. `Position` comes from `essentials2d`: the game's header includes the plugin's header. The "Generated" header comment and the list of generated files are also inaccurate. | `docs/guide/concepts/schema.md:101,121,125-132`; `engine/schema/codegen/src/cpp_emitter.cpp:85,93,208`; `cmake/OlyGame.cmake:116,124-125` | verified | Fix the listing and mark it as an excerpt. |
| A8 | Troubleshooting tells a game user to run `./spur ts`, which exists only in the engine repository. On the same page, `:117` says `oly build`. | `docs/guide/troubleshooting.md:16` vs `:117`; `Spurfile:251` | verified (wrong audience) | Replace with `oly build`. |
| A9 | Broken anchor in the web guide: `#wasm-typescript-and-games` does not exist; the heading is `## WASM and TypeScript`. | `docs/guide/environments/web/boundary.md:57`; `docs/development/commands.md:104` | verified | Change to `#wasm-and-typescript`. |
| A10 | `guide-style.md` makes every declaration outside `detail/` public. That puts internal helpers on the public surface: the whole `core` module, `Named*Registration`/`findNamed*`, `capi::schema*`, `ActionMap::capture` and `setDeclarationsLocked`. The result is about 118 undocumented C++ names and a permanent debt. | `docs/development/guide-style.md:15`; §3 | verified | Decide the surface: move to `detail/`, or narrow the rule. |

---

## 2. Diátaxis classification

Audience: **U** = game user, **C** = engine contributor, **I** = internal (process/tooling). "Mixed" marks a page that
mixes types; the sections column says which part is which type.

### 2.1 Root and `.github/`

| Page | Type | Audience | Rationale / sections |
|---|---|---|---|
| `README.md` | **Mixed**: explanation + short tutorial + reference | U, C | Explanation: intro and "How the ECS works" (`:1-29`). Tutorial/example: "Examples" (`:31-102`). Contributor how-to: "Build and test", "Benchmarks" (`:104-132`). Reference: the "What already exists" and "Documentation" tables. |
| `CONTRIBUTING.md` | **Mixed**: how-to + reference | C | How-to: Workflow and Toolchain (`:24-44`). Reference: rules and checklist (`:46-95`). |
| `CODE_OF_CONDUCT.md` | Reference (policy) | all | Contributor Covenant 2.1. |
| `SECURITY.md` | Reference (policy) + short how-to | all | Reporting channel, scope and deadlines. |
| `CLAUDE.md` | Reference (rules) | I | Index and rules for Claude Code; imports `code-style.md` (`:22`). |
| `LICENSE` | Reference | all | MIT. |
| `.github/pull_request_template.md` | Reference (checklist) | C | |
| `.github/ISSUE_TEMPLATE/*.yml` | Form (outside Diátaxis) | U, C | Bug, feature and `config.yml`. |
| `.github/workflows/ci.yml`, `release.yml` | Code, not docs | I | The comments explain the why. |

### 2.2 `docs/architecture/` (explanation of "how it is built")

| Page | Type | Audience | Rationale / sections |
|---|---|---|---|
| `README.md` | Explanation (overview) + reference (index) | C | Overview (`:5-38`) is explanation; Index and Reference documents are navigation. |
| `app.md` | Explanation | C | Loop, bands and recovery, with the why. |
| `build-and-tests.md` | **Mixed**: explanation + reference | C | The service and preset tables are reference; the rest explains the organization. |
| `capi.md` | **Mixed**: reference + explanation | U (client in another language), C | The function table (`:5-15`) is reference, but without signatures; "Design points" is explanation. |
| `core.md` | Reference | C | List of the utilities. |
| `decisions.md` | Explanation (ADRs in a table) | C | Decision, reason and cost. |
| `ecs.md` | Explanation | C | Layers, costs and iteration. |
| `glossary.md` | Reference | U, C | C++ ↔ C API ↔ TS name map; the guide treats it as reference (`docs/guide/README.md:82`). |
| `input.md` | Explanation | C | |
| `limits.md` | Reference | U, C | List of limitations. |
| `modules-and-boundaries.md` | Explanation + reference | C | The lint rules (`:49-68`) are reference; "Why detail/" is explanation. |
| `performance.md` | Reference (targets and status) + explanation | C, U | `:23` states a consequence for the game author. |
| `plugins.md` | **Mixed**: explanation + reference | C | "Build" (`:67-118`) is reference for the CMake functions; the rest is explanation. |
| `publishing.md` | **Mixed**: explanation + reference | C | Launcher and local mode (`:67-101`) overlap `guide/cli.md`. |
| `repository.md` | Reference | C | Folder map. |
| `schema-and-codegen.md` | Explanation | C | |
| `ts-packages.md` | **Mixed**: reference + explanation | C | |

### 2.3 `docs/development/`, `docs/benchmarks/` and others

| Page | Type | Audience | Rationale |
|---|---|---|---|
| `development/commands.md` | Reference + how-to ("Cutting a release", `:82-90`) | C | |
| `development/code-style.md` | Reference | C | |
| `development/guide-style.md` | Reference (style guide) + how-to ("When the guide must change") | C | |
| `benchmarks/README.md` | Reference (results) + explanation (NF-P5) | C | |
| `benchmarks/bridge/README.md` | Reference (results) + how-to (commands) + explanation ("Reading") | C | |
| `engine/schema/format/README.md` | Reference (grammar, rules, errors) | U, C | Normative source of the `.oly` language, but the guide does not point to it. |
| `.claude/skills/add-test-case/SKILL.md` | How-to | C, I | CONTRIBUTING points to it as a doc readable without Claude Code (`CONTRIBUTING.md:59`). |
| `templates/game/`, `tools/oly/` | No README | U (indirect) | Documented by `guide/cli.md` and `architecture/publishing.md`; the comments in `install.sh:2-4` and in the template's `CMakeLists.txt` are the only local text. |

### 2.4 `docs/guide/` (game user)

| Page | Type | Audience | Rationale / sections |
|---|---|---|---|
| `README.md` | Reference (map) + explanation ("The map") | U | "Choose your path" is navigation; "The big picture in one example" is a miniature tutorial. |
| `cli.md` | **Mixed**: reference + how-to | U | The command, option and variable tables are reference; "Make a game", "Upgrade the engine" and "Build with an engine checkout" are how-to. |
| `cheat-sheet.md` | Reference | U | Promises "every public name" (`:5`); §3 shows the gaps. |
| `recipes.md` | How-to | U | One problem per section. |
| `troubleshooting.md` | **Mixed**: reference + how-to | U | The message tables are reference; "Rebuild checklist" is how-to; "What Oly does not do" (`:121-125`) duplicates `limits.md`. |
| `concepts/schema.md` | **Mixed**: explanation + reference | U | Field types and name rules (`:45-81`) are reference (they duplicate `format/README.md`); layout and order are explanation. |
| `concepts/world-and-entities.md` | **Mixed**: explanation + how-to | U | "Debugging the world" is how-to; the rest is contract (explanation) with examples. |
| `concepts/queries.md` | **Mixed**: explanation + how-to | U | "Common query patterns" (`:117-218`) is how-to. |
| `concepts/engine-and-systems.md` | **Mixed**: explanation + reference + how-to | U | Loop and phases are explanation; registration by name (`:110-131`) is reference; "Driving the loop yourself" is how-to. |
| `concepts/events.md` | Explanation | U | "Events vs. other tools" is comparative explanation. |
| `concepts/input.md` | Explanation + how-to | U | "Rebind at run time" and "Deliver input yourself" are how-to. |
| `concepts/schedule.md` | Explanation | U | |
| `concepts/states.md` | Explanation + how-to | U | "Complete pattern: menu → playing → game over" (`:143`) is how-to/recipe. |
| `concepts/performance.md` | Explanation + how-to | U | |
| `concepts/plugins.md` | **Mixed**: how-to + explanation | U | "Turn a plugin on" and "Write a plugin" are how-to; "Lifetime and ownership" is explanation. |
| `concepts/transforms.md` | Explanation + reference | U | Off convention: no `## Next`, no "In other clients" line, missing from the tree in `guide-style.md:63-65,83` (see §5.1). |
| `environments/web/getting-started.md` | **Mixed**: tutorial + explanation | U | Tutorial: `:7-35`. File-by-file reference/explanation: `:48-178`. Explanation: "How to split work" (`:180-200`). Contributor how-to: "Inside the engine repository" (`:205`). |
| `environments/native/getting-started.md` | **Mixed**: tutorial + how-to + explanation | U | "Test your game" (`:169`) is how-to; "What the host does" is explanation. |
| `environments/web/testing.md` | How-to | U | With an introductory explanation ("What makes a game testable"). |
| `environments/web/boundary.md` | Explanation + how-to | U | |
| `environments/web/typescript/README.md` | Reference (client map) | U | |
| `environments/web/typescript/{schema,world-and-entities,queries,engine-and-systems,events,input,schedule,states}.md` | Reference with examples (light how-to) | U | They mirror the concept pages, as `guide-style.md:81-82` asks. |
| `environments/web/typescript/recipes.md` | How-to | U | |

**Structural gap (Diátaxis):** there is no "pure" tutorial. Both `getting-started` pages begin as a tutorial and turn
into reference and explanation of the template. The concept pages carry contract, API and patterns together, by the
explicit decision of `guide-style.md:81`.

### 2.5 `docs/superpowers/`: internal process artefacts (not user docs)

None of these pages has a status or "superseded" header. Yet `README.md:144`, `CONTRIBUTING.md:22` and
`docs/architecture/README.md:67` call them "source of truth", and several describe an old state (`games/arena`, `make`).
For example, `specs/2026-11-09-game-engine-design.md` has 19 mentions of `arena`/`games/` (`:972`), and `games/` was
removed in `1d6ee51`.

| Page | Type | Audience |
|---|---|---|
| `specs/2026-10-16-ecs-archetype-spec.md` | Reference (requirements `R-*`, invariants `I-*`, `NF-*`) | I, C |
| `specs/2026-11-06-wasm-cost-study-design.md` | Explanation (design) | I |
| `specs/2026-11-09-game-engine-design.md` | Explanation + reference (design) | I |
| `specs/2026-11-09-oly-modularization-design.md` | Explanation (design) | I |
| `specs/2026-11-13-multi-environment-design.md` | Explanation (design) | I |
| `specs/2026-11-13-spur-task-runner-design.md` | Explanation (design) | I |
| `specs/2026-11-16-plugins-design.md` | Explanation (design) | I |
| `specs/2026-11-17-publishing-design.md` | Explanation (design); missing from the table in `architecture/README.md:69-80` | I |
| `plans/2026-10-16-ecs-archetype-v1.md` | How-to (implementation plan) | I |
| `plans/2026-11-09-engine-phase1-bridge.md` | How-to (plan) | I |
| `plans/2026-11-09-wasm-cost-study.md` | How-to (plan) | I |
| `plans/2026-11-10-engine-phase2-loop.md` | How-to (plan) | I |
| `plans/2026-11-10-engine-phase3-events.md` | How-to (plan) | I |
| `plans/2026-11-10-oly-modularization.md` | How-to (plan) | I |
| `plans/2026-11-11-engine-phase4-input.md` | How-to (plan) | I |
| `plans/2026-11-11-engine-phase5-schedule.md` | How-to (plan) | I |
| `plans/2026-11-12-engine-phase6-states.md` | How-to (plan) | I |
| `plans/2026-11-12-engine-phase7-arena.md` | How-to (plan); the game was already removed | I |
| `plans/2026-11-13-multi-environment-phase1-move.md` | How-to (plan) | I |
| `plans/2026-11-13-multi-environment-phase2-callback.md` | How-to (plan) | I |
| `plans/2026-11-13-multi-environment-phase3-schema.md` | How-to (plan); broken link at `:1953` | I |
| `plans/2026-11-13-multi-environment-phase4-native.md` | How-to (plan) | I |
| `plans/2026-11-13-spur-task-runner.md` | How-to (plan) | I |
| `plans/2026-11-14-multi-environment-phase5-docs.md` | How-to (plan); broken link at `:221` | I |
| `plans/2026-11-16-plugins-phase1-observers.md` | How-to (plan) | I |
| `plans/2026-11-16-plugins-phase2-mechanism.md` | How-to (plan) | I |
| `plans/2026-11-16-plugins-phase3-essentials.md` | How-to (plan) | I |
| `plans/2026-11-17-publishing-phase1-subproject.md` | How-to (plan) | I |
| `plans/2026-11-17-publishing-phase2-game-tasks.md` | How-to (plan) | I |
| `plans/2026-11-17-publishing-phase3-launcher.md` | How-to (plan) | I |
| `plans/2026-11-17-publishing-phase4-release.md` | How-to (plan) | I |
| `plans/2026-11-17-publishing-phase5-docs.md` | How-to (plan) | I |

Recommendation: put a status banner ("historical; the current state is in `docs/architecture/`") on the specs and plans,
or stop calling them the source of truth.

---

## 3. Public API without documentation

**Criterion:** the surfaces of `docs/development/guide-style.md:9-20`. A name counts as documented if it appears in
`docs/guide/` (or in `docs/architecture/capi.md`, for the C API). "arch" = appears only in `docs/architecture/`. The
inventory was made by script (a grep of each name with a distinctive regex) and checked by hand in the ambiguous cases.
C++ paths are relative to `engine/runtime/<module>/include/oly/<module>/`.

### 3.1 Counts

| Surface | Checked | Undocumented | Suspected (among the undocumented) |
|---|---|---|---|
| C++ headers (runtime, `native_host`, plugins) | 255 | ~118 | ~8 |
| C API (`c_api.h`, `engine_api.h`) | 89 (61 functions + 28 types/constants) | 17 (0 functions) | 0 |
| `@oly/core` and `@oly/web-host` | 99 | 33 | 6 |
| `oly` (launcher, `tools/oly/Spurfile`, template) | 42 | 7 | 4 (internal) |
| CMake and `toolchain.env` | 34 | 14 | 4 |
| Public `Spurfile` tasks (`schema-tool`, `ts`, `games-test`, `native-games-test`, `windows-test`) | 5 | 0 (only cited inline, outside `cli.md` and the cheat sheet) | — |
| `Spurfile` tasks in `development/commands.md` (contributor) | 30 | 1 (`configure`) + 8 halves cited only through the parent task | 8 |

### 3.2 Items relevant to games (high priority)

| Symbol | Declared in | Situation | Status |
|---|---|---|---|
| `Engine::emit` outside `frame` (from the host) | `app/engine.hpp:185` | arch (`glossary.md:23`); the guide shows only `context.emit` | verified |
| `CommandBuffer::commandCount`, `empty`, `clear` | `ecs/command_buffer.hpp:70,73,79` | none | verified |
| `Engine::declareState`, `stateCount` | `app/engine.hpp:213-214` | `declareState` only in one error row of troubleshooting | suspected / verified |
| `Engine::setSystemStateMask`, `setHandlerStateMask`; `StateMask`, `MaxStates` | `app/engine.hpp:233,235`; `app/state.hpp:11,15` | only the `initializer_list` forms are documented | verified |
| `HandlerContext::eventType()`, `handlerId()`, `eventData()`; `SystemContext::systemId()`; `StateContext::hookId()` (and `name()`/`userData()`, missing from the cheat sheet `:86`) | `app/handler.hpp:39,42,63`; `app/system.hpp:41`; `app/state_context.hpp:34-42` | none | verified |
| `Scheduler::schedule(delay, interval, fn, payload, discard)`, `pendingCount` | `app/scheduler.hpp:33,69` | none | verified |
| `MaxTaskPayloadSize`, `MaxTaskPayloadAlignment` | `app/task.hpp:18,20` | "≤ 32 B" in the cheat sheet; alignment 8 not stated | verified |
| `Entity.index`, `.generation` (C++) | `ecs/entity.hpp:8` | none (the TS `entityIndex` is documented) | verified |
| `World::isIterating`; direct `World::add/remove/destroy` | `ecs/world.hpp:102-117` | only through `context.*` and `CommandBuffer` | verified / suspected |
| `ObserverKind::OnRemove` | `ecs/observer.hpp:16` | the cheat sheet shows only `OnAdd` | suspected |
| `ActionKind`, `ActionId` (C++ and TS) | `input/action_map.hpp:13,15`; `core/src/schema.ts:34` | `ActionId` arch; `ActionKind` none | verified |
| `ActionMap::actionCount`, `kind`, `name` | `input/action_map.hpp:118-128` | none | verified |
| `native_host::WindowConfig` | `native_host/window.hpp:9` | used implicitly (`Window({"T", w, h})`), never named | suspected |
| `oly sync` | `tools/oly/Spurfile:593` | missing from `cli.md` and the cheat sheet | verified |
| `oly platform` | `tools/oly/Spurfile:564` | none | verified |
| `OLY_BUILD_NATIVE_HOST`, `OLY_BUILD_TESTS` (a game with the engine as a subproject) | `cmake/OlyOptions.cmake:5-11` | arch (`build-and-tests.md`, `publishing.md`) | verified |
| `OLY_ENABLE_ASSERTS` | `core/assert.hpp:3`; `cmake/OlyOptions.cmake:13` | arch (`core.md:5`) | verified |
| C API: `OLY_NO_STATE`, `OLY_INVALID_STATE_HOOK`, `OLY_STATE_ENTER`, `OLY_STATE_EXIT`, `OLY_INVALID_SYSTEM`, `OlyTaskHandle` | `engine_api.h:9,20-25` | none | verified |
| C API: `OlyActionState`, `OLY_ACTION_PRESSED/HELD/RELEASED/VALID`, `OLY_ACTION_BUTTON/AXIS2D/POINTER`, `OlyQuery`, `OlyEntity` | `engine_api.h:31-40`; `c_api.h:15,18,44-46` | none | verified |
| C API in general | `docs/architecture/capi.md:5-15` | names every function, but without signatures or parameter meaning | verified |
| TS: `Runtime.createEngine(config)` | `core/src/runtime.ts:132` | used in the guide, but left out of the `Runtime` description in `ts-packages.md:10` | verified |
| TS: `Runtime.schema`, `World.handle`, `EventBatch.type`, `Schema.types/events/actions/states/hash`, `SpawnValues`, `ValueOf` | `core/src/runtime.ts:70`, `world.ts:9,31`, `event_batch.ts:14`, `schema.ts:5,57-64` | none | verified |
| TS: `SystemFunction`, `TaskFunction`, `StateHookFunction`, `HandlerRegistration`, `SystemRegistration` (type names) | `system_context.ts:12`, `scheduler.ts:9`, `state_context.ts:10`, `engine.ts:24,33` | the members are documented, the type names are not | suspected |

### 3.3 Items public only because they sit outside `detail/` (low priority; decide before documenting)

| Group | Declared in | Situation |
|---|---|---|
| The whole `core` module: `alignUp`, `alignedAllocate`, `alignedFree`, `AssertHandler`, `setAssertHandler`, `SmallMap`, `typeName<T>` | `core/align.hpp:8,12,15`, `core/assert.hpp:12,16`, `core/small_map.hpp:13`, `core/type_name.hpp:8` | none |
| `NamedHandler`, `findNamedHandler`, `NamedHandlerRegistration`, `NamedSystemRegistration`, `findNamedSystem`, `NamedStateHookRegistration`, `findNamedStateHook`, `SystemFunction`, `StateHookFunction` | `app/handler.hpp:70-78`, `app/system.hpp:16,83,85`, `app/state.hpp:17-22` | none |
| `EventTypeId`, `EventKey`, `InvalidEventType`, `eventKey<T>`, `Engine::eventTypeOf`, `eventTypeCount`, `onById`, `emitById`, `context.emitById` | `app/event.hpp:6-18`, `app/engine.hpp:164-192`, `app/context.hpp:75` | none / arch |
| `PluginKey`, `pluginKey<P>`, `PhaseCount`, `OrderBandCount`, `isGameplayPhase`, `Engine::inFrame` | `app/plugin.hpp:8,29`, `app/phase.hpp:11-19`, `app/engine.hpp:266` | arch / none |
| `TaskPayload`, `TaskPayload::of`, `PayloadKey`, `payloadKey<P>`, `NoPayloadKey`, `InvalidTaskSlot`, `TaskHandle.slot/.generation`, `TaskFunction`, `TaskDiscardFunction` | `app/task.hpp:14-48` | none |
| `ById` variants: `World::addById/removeById/getById/tryGetById/hasById/queryById`, `context.addById/removeById`, `QueryByIdSpec`, `BlockById`, `QueryIteration`, `QueryById` | `ecs/world.hpp:121-177`, `app/context.hpp:116,119`, `app/system.hpp:74,77`, `ecs/query_by_id.hpp:24-95` | arch (`glossary.md:43`, `ecs.md`) |
| `Query::matchedCount`, `Query::writes`; `TypeInfo`, `typeInfo()`, `registeredTypeCount`; `ArchetypeStats`, `signatureLookupCount`, `archetypeCount()` | `ecs/query.hpp:49,58`, `ecs/component.hpp:14-27`, `ecs/diagnostics.hpp:14-38` | none / arch |
| `ActionState`, `ActionPressed/Held/Released/Valid`, `ActionMap::declare*`, `capture`, `snapshot`, `setDeclarationsLocked`, `PhysicalButton(uint32_t)`, `.usage` | `input/action_map.hpp:59-128`, `input/physical_button.hpp:123,129` | arch / none |
| `capi::SchemaEvent/SchemaAction/SchemaState`, `capi::schemaHash/schemaComponentIds/schemaEvents/schemaActions/schemaStates` | `capi/schema_registry.hpp:11-34` | none |
| `essentials2d::Plugin`, `essentials3d::Plugin`, `checkHierarchy` | `plugins/essentials{2d,3d}/native/include/oly/essentials{2d,3d}/plugin.hpp:30,49` | arch (`plugins.md`) |
| TS: `ComponentLayout`, `FieldLayout`, `ScalarType`, `RawField`, `ReadField`, `ReadView`, `WriteField`, `WriteView`, `Fields` | `core/src/layout.ts:1-9`, `core/src/fields.ts:7-24`, `core/src/schema.ts:3` | none (their use appears in the cheat sheet) |
| `@oly/web-host`: `FrameTarget`, `BrowserEnvironment`, `BrowserLoop`, `InputTarget`, `InputCanvas`, `InputEnvironment`, `BrowserInput` and the injection parameters (`runInBrowser(…, environment)`, `attachBrowserInput(…, environment)`, `moduleFactoryFromUrl(…, importModule)`) | `host/js/src/browser_loop.ts:1-36`, `browser_input.ts:4-77`, `module_factory.ts:13` | none (test hooks) |
| `oly version-at-least`, `OLY_TOOLS_DIR`, `OLY_GAME_DIR`, `OLY_IN_DOCKER`, `OLY_ENGINE_SOURCE_DIR` | `tools/oly/Spurfile:2,84,107,634` | internal; `OLY_ENGINE_SOURCE_DIR` appears in arch |
| CMake: `oly_typescript_schemas`, `oly_add_engine_test_game`, `OLY_WERROR`, `OLY_SANITIZE`, `OLY_BUILD_BENCH`, `OLY_VEC_REPORT`, `OLY_BUILD_GAMES` | `cmake/OlyGame.cmake:5,46`, `cmake/OlyOptions.cmake:5-11` | arch (engine-level) |
| `toolchain.env`: `GCC_VERSION`, `CMAKE_VERSION`, `CMAKE_MINIMUM`, `NINJA_MINIMUM`, `MINGW_GCC_MINIMUM` | `toolchain.env:7-12` | arch; the concept appears in `troubleshooting.md:28-30` |
| `Spurfile`: `configure` | `Spurfile:100` | missing from `development/commands.md` |

Note: the behaviour "the `Position` → `WorldTransform` implication stays in a `World` that outlives the `Engine`, and
then `WorldTransform` is no longer updated" (`plugins/essentials2d/native/include/oly/essentials2d/plugin.hpp:25-27`)
does not appear in the guide. Status: suspected.

---

## 4. Examples that are broken or do not match the API

**Overall result (static):** no wrong name, include, signature, macro, generated name, TS export, package or C
function. About 160 C++/`.oly` blocks, about 110 TS blocks, the C API table and about 65 error-message rows were
checked. The problems below are about described behaviour, conflict with the template, or "copy and paste does not
compile".

| Doc:line | Problem | Evidence | Method | Status |
|---|---|---|---|---|
| `docs/guide/environments/web/testing.md:9,13-21,63,207-218` | The harness uses `installGame(engine, options: GameOptions)` and a `Drawer` with `circle`/`rect`/`text`; the template uses `installGame(engine, drawer)` and a `Drawer {clear, square}`. `:237` says to put the tests in the game made by `oly new`. | `templates/game/web/web/src/game.ts:8-13`; `templates/game/web/web/tests/smoke.test.ts:23-33,55` | static | verified |
| `docs/guide/environments/web/typescript/states.md:78`, `typescript/recipes.md:334` | Two more signatures of `installGame` (`(engine): {kills}` and `(engine, options): Score`), different from the template. | `templates/game/web/web/src/game.ts:13` | static | suspected (illustrative, but inconsistent) |
| `docs/guide/concepts/world-and-entities.md:88-89` | `context.destroy(e); context.isAlive(e); // false` is false in a `SystemContext` (deferred). | `engine/runtime/app/include/oly/app/system.hpp:63-65`; `engine/runtime/ecs/src/command_buffer.cpp:209-211` | static | verified |
| `docs/guide/concepts/world-and-entities.md:92` | "Destroying a dead entity is a contract violation" does not hold for the `CommandBuffer`, which ignores stale handles. | `engine/runtime/ecs/include/oly/ecs/world.hpp:96,128-130` | static | verified |
| `docs/guide/environments/web/typescript/world-and-entities.md:23` | Says the client refuses operations on `engine.world` during `frame`; only `spawn/destroy/add/remove` are refused (`get/set/has/query` work). | `engine/environments/web/clients/typescript/core/src/world.ts:112-132,223-227,243-251` | static | verified |
| `docs/guide/environments/web/typescript/world-and-entities.md:101` | Suggests every typed view is invalidated by memory growth; `ReadField`/`WriteField` re-read the heap on every access, and only `raw()` goes stale. | `engine/environments/web/clients/typescript/core/src/fields.ts:15,26-27` | static | suspected (wording) |
| `docs/guide/troubleshooting.md:33-34,40,50,54-56` | TS messages presented as general; the C++ texts differ, and two have no C++ equivalent. | `engine/runtime/ecs/include/oly/ecs/world.hpp:352`; `engine/runtime/ecs/include/oly/ecs/query.hpp:95`; `engine/runtime/app/src/engine.cpp:48,83,125-127,138`; TS in `core/src/engine.ts:150,156,459,466`, `world.ts:217` | static | verified |
| `docs/guide/troubleshooting.md:16` | The fix for `SchemaMismatchError` uses `./spur ts`, an engine-repository command; in a game it is `oly build`. | `Spurfile:251`; `troubleshooting.md:117` | static | verified |
| `docs/guide/concepts/schema.md:125-132` | The `schema.generated.hpp` listing contains `struct Position`, which comes from the plugin; the listing leaves out `Weapon`, `Frozen` and the `offsetof` `static_assert`s without saying it is an excerpt. | `engine/schema/codegen/src/cpp_emitter.cpp:65,93,208`; `engine/schema/codegen/src/merge.cpp:42` | static | verified |
| `docs/guide/concepts/schema.md:101` | Comment `// Generated: do not edit.`; the real one is `// Generated by oly-schema: do not edit.` | `engine/schema/codegen/src/cpp_emitter.cpp:85` | static | verified (cosmetic) |
| `docs/guide/concepts/schema.md:121` | Lists `schema_registry.generated.cpp` among the game's C++ files; it is compiled only in web targets. | `cmake/OlyGame.cmake:116,124-125` | static | verified |
| `docs/guide/concepts/transforms.md:20` | Says a game's own `Position` "stops compiling"; `oly-schema` stops first, at the merge (`repeated name "Position"`). `troubleshooting.md:32` is right. | `engine/schema/codegen/src/merge.cpp:28` | static | verified (wording) |
| `docs/guide/environments/native/getting-started.md:69` | The example is described as the template's `desktop/main.cpp`, but it has no `movePlayer`, no include of `systems.hpp`, and uses 640x480 (the template uses 640x360). | `templates/game/native/desktop/main.cpp:15,19-20,68` | static | verified |
| `docs/guide/cheat-sheet.md:106,113` | `Key::W`, `Key::Space` with no qualification or `using`; `:105` uses `oly::input::Key`. Does not compile as written. | `engine/runtime/input/include/oly/input/physical_button.hpp:7` | static | verified |
| `docs/guide/cheat-sheet.md:86` | The context table leaves out `systemId()`, `eventType()`, `handlerId()`, `hookId()`, `name()`, `userData()`, despite promising "every public name" (`:5`). | `app/system.hpp:42`; `app/handler.hpp:39-44`; `app/state_context.hpp:34-42` | static | verified |
| `docs/guide/concepts/plugins.md:108` | Includes `oly/app/context.hpp` to use `SystemContext`, which lives in `oly/app/system.hpp`. Compiles only because `engine.hpp` includes it. | `engine/runtime/app/include/oly/app/engine.hpp:20` | static | verified (fragile) |
| `docs/guide/recipes.md:692` | `SOURCES ${nativeSources}` with the variable undefined in the excerpt (it is the name the template uses). | `templates/game/web/cmake/game.cmake:1` | static | suspected |
| `docs/architecture/ts-packages.md:3` | Lists esbuild 0.24.2 among the workspace's pinned versions; the root `package.json` has no esbuild (only the template does). | `package.json:10-13`; `templates/game/web/package.json:12` | static | verified |
| `docs/architecture/ts-packages.md:10` | `Runtime` described as `schema` + `createWorld()`, without `createEngine(config)`. | `engine/environments/web/clients/typescript/core/src/runtime.ts:132` | static | verified |
| `engine/runtime/capi/include/oly/capi/engine_api.h:53-55` (header comment, not docs) | Says `olyEngineCreate` calls `declareSchema`; the code calls `setUpGame`. `capi.md:21` is right. | `engine/runtime/capi/src/engine_api.cpp:127` | static | verified |

**`spur`/`oly` commands cited in the docs:** all exist. Sources: the 30 `Spurfile` tasks (`name: ## …`,
`Spurfile:97-488`); `--list`, `--describe`, `-x`, `-n` (`spur:37-42,360-380`); `oly new|upgrade|self-update|--version`
(`tools/oly/oly:230-243`); `build|test|run|clean|doctor` (`tools/oly/Spurfile:567-600`). The 16 `oly` messages cited
in `troubleshooting.md:80-95` exist literally in `tools/oly/oly` and `tools/oly/Spurfile`, and the `oly:` and
`oly <task>:` prefixes match (`tools/oly/oly:12`, `tools/oly/Spurfile:6`). Method: static.

**Correct but fragile (no immediate action):** several excerpts use components that no excerpt declares (`Velocity`,
`Score`, `Collider`, `Chaser`, `Bullet`). `Dash` has different fields in `concepts/schema.md:61` and `recipes.md:150`.
`EngineContext::fixedDeltaTime()` returns `float`, but `Engine::fixedDeltaTime()` returns `double`
(`app/context.hpp:34`, `app/engine.hpp:260`). The cheat sheet's plugin skeleton (`cheat-sheet.md:188`) declares
`install` in the `private` section.

---

## 5. Broken links

**Script:** a Node checker written for this audit (the design ports it into the repository as `tools/check_links.py`).
It walks every `.md` in the repository except `.git`, `node_modules`, `build*` and `.superpowers`; ignores fenced code
blocks and inline code spans; resolves relative paths; builds anchors with GitHub's rule (lowercase, punctuation removed,
spaces → `-`, a `-N` suffix for repeats).

**Coverage:** 98 `.md` files, **547 relative links checked**, **3 broken**, 9 external.

| Doc:line | Link | Problem | Status |
|---|---|---|---|
| `docs/guide/environments/web/boundary.md:57` | `../../../development/commands.md#wasm-typescript-and-games` | the anchor does not exist; the heading is `## WASM and TypeScript` (`commands.md:104`) → `#wasm-and-typescript` | verified |
| `docs/superpowers/plans/2026-11-13-multi-environment-phase3-schema.md:1953` | `02-schema.md#naming-rules` | the file does not exist (old name in the guide) | verified (internal) |
| `docs/superpowers/plans/2026-11-14-multi-environment-phase5-docs.md:221` | `../../../troubleshooting.md` | the file does not exist (it should be `../../guide/troubleshooting.md`) | verified (internal) |

**External links (listed, not fetched):** `CODE_OF_CONDUCT.md:39` (`https://github.com/esliph`), `:79-83`
(contributor-covenant.org ×4, `github.com/mozilla/diversity`); `CONTRIBUTING.md:81` (conventionalcommits.org), `:99`
(claude.com/claude-code); `SECURITY.md:9` (`github.com/esliph/oly/security/advisories/new`). Outside `.md`:
`.github/ISSUE_TEMPLATE/config.yml:4` (the same advisories link), `templates/game/common/.github/workflows/ci.yml:13`
and `docs/guide/cli.md:26` (`releases/latest/download/install.sh`).

### 5.1 Broken references that are not links (navigation and index)

| Location | Problem | Status | Recommendation |
|---|---|---|---|
| `docs/guide/concepts/plugins.md:3` and `:162` | Navigation ends at Plugins: `transforms.md:3` points "← Plugins", but `plugins.md` does not point "Next: Transforms" (only an inline link at `:21`). | verified | Close the navigation chain. |
| `docs/guide/concepts/transforms.md` (end) | No `## Next` and no "In other clients" line, as `guide-style.md:96` asks. | verified | Follow the concept-page pattern. |
| `docs/development/guide-style.md:63-65,83` | The file tree and the navigation order leave out `transforms.md`. | verified | Include the page. |
| `docs/guide/README.md:39` | The "TypeScript on the web" path leaves out Performance, Plugins and Transforms. | verified | Complete the path, or say they are optional. |
| `docs/architecture/README.md:69-80` | The "Reference documents" table leaves out `specs/2026-11-17-publishing-design.md`. | verified | Add the row. |
| `docs/architecture/modules-and-boundaries.md:58` | Describes the lint rule about "a game name (folder in `games/`)", but `games/` no longer exists since `1d6ee51`; the rule checks nothing today (`tools/check_boundaries.py:102-105`). | verified | Update the text (or the rule). |
| `docs/architecture/glossary.md:62` | "`create` … do not exist as names" contradicts `olyWorldCreate`, `olyEngineCreate`, `olyQueryCreate`, `Runtime.createWorld`/`createEngine` (`glossary.md:13,35`; `capi.md:7-10`). | verified | Restrict the rule to entities. |
| `engine/schema/format/README.md` | The normative reference of the `.oly` language is not linked from the guide (`concepts/schema.md` rewrites the rules). | verified | Link it from `concepts/schema.md`. |

---

## 6. Duplicated content

| # | Content | Locations | Suggested single source | Status |
|---|---|---|---|---|
| D1 | Introduction to the toolchain and `spur` (Git Bash + Docker, `./spur images`, `--list`, `-x`/`-n`) | `README.md:104-119`; `CONTRIBUTING.md:24-35`; `docs/development/commands.md:3-10`; `docs/architecture/build-and-tests.md:5-16`; `CLAUDE.md:11` | `commands.md` (the why in `build-and-tests.md`); the others with one line and a link | verified |
| D2 | Benchmark command `PRESET=release ./spur build` + `ecs_bench_iterate` | `README.md:127-130`; `CONTRIBUTING.md:67-70`; `commands.md:94-97` | `commands.md` | verified |
| D3 | Code rules (English, `-Werror`, formatting, `OLY_ASSERT`) | `CLAUDE.md:12-14`; `CONTRIBUTING.md:50-55`; `docs/development/code-style.md:5-12,24`. `CLAUDE.md:22` imports `code-style.md`, so `:12` appears twice in the same context | `code-style.md` | verified |
| D4 | The Claude Code `clang-format` hook | `CLAUDE.md:13`; `code-style.md:11`; `build-and-tests.md:90`; `CONTRIBUTING.md:99` | `build-and-tests.md` (the `.claude/` section) | verified |
| D5 | PR checklist (already diverging: the template has no "hot path reports the benchmark" and no "branch starts from develop") | `CONTRIBUTING.md:87-95`; `.github/pull_request_template.md:9-16` | the PR template; CONTRIBUTING only links | verified |
| D6 | Out of scope ("does not draw, play audio or do physics"; ECS v1 without chunks, etc.) | `README.md:7`; `docs/guide/README.md:20`; `docs/guide/troubleshooting.md:121-125`; `docs/architecture/limits.md:3,7`; `.github/ISSUE_TEMPLATE/feature_request.yml:8` | `limits.md` | verified |
| D7 | The "local mode" table (pinned version × checkout) | `docs/guide/cli.md:156-162`; `docs/architecture/publishing.md:89-95`; also `commands.md:62` | `cli.md` for the user; `publishing.md` only with the why | verified |
| D8 | Project description (C++20, 2D, archetype ECS, study and benchmark) | `README.md:3-5`; `CLAUDE.md:3`; `docs/architecture/README.md:7-17`; `CONTRIBUTING.md:3`; `docs/guide/README.md:9` | `README.md` | verified |
| D9 | How the ECS works (archetypes, SoA, edges, `CommandBuffer`, view validity) | `README.md:23-29`; `docs/architecture/ecs.md`; `docs/guide/concepts/performance.md` and `world-and-entities.md:119-135` | `ecs.md` (explanation); the guide only with the contract | suspected (partial overlap) |
| D10 | Name rules, scalar types and limits of `.oly` | `engine/schema/format/README.md:31-43`; `docs/guide/concepts/schema.md:45-81`; `docs/architecture/schema-and-codegen.md:15-17` | `format/README.md` (normative); the guide summarizes and links | verified |
| D11 | Names per surface (C++ / C API / TS) | `docs/architecture/glossary.md:5-43`; `docs/guide/cheat-sheet.md` (whole) | the cheat sheet for names and signatures; the glossary only for the concept mapping | suspected (distinct purposes, drift risk) |
| D12 | List of C API functions | `docs/architecture/capi.md:5-15`; `docs/guide/cheat-sheet.md:193-208` | one C API reference with signatures; the other links | verified |
| D13 | Rule "a public API change updates `docs/guide/`" | `CLAUDE.md:18`; `CONTRIBUTING.md:76,94`; `guide-style.md:7` | `guide-style.md` | verified (intentional, short duplication) |
| D14 | Boundary rules (the engine does not depend on plugins, game names) | `CLAUDE.md:14-15`; `CONTRIBUTING.md:55`; `modules-and-boundaries.md:72-74`; `docs/architecture/plugins.md:155-168` | `modules-and-boundaries.md` | verified |
| D15 | `checkInvariants` when changing `table/archetype/query/command_buffer/world` | `CLAUDE.md:16`; `CONTRIBUTING.md:61` | `CONTRIBUTING.md` | verified |
| D16 | Installing `oly` with `curl … install.sh \| sh` | `README.md:7`; `docs/guide/cli.md:26`; `tools/oly/install.sh:3` | `cli.md` | verified |
| D17 | TS packages (`@oly/core`, `@oly/web-host`) | `docs/architecture/ts-packages.md:5-35`; `docs/guide/environments/web/typescript/README.md:9-16` | the guide for the API; the architecture for internal rules | suspected |
| D18 | Plugins: install mechanism, order and bands | `docs/architecture/plugins.md:27-56`; `docs/guide/concepts/plugins.md:136-153` | architecture for the why; the guide only with the contract (`guide-style.md:3`) | suspected (not compared line by line) |

---

## 7. Community health files

| File | Exists? | Evident gaps |
|---|---|---|
| `CONTRIBUTING.md` | yes | Says "Questions … go in an issue too" (`:16`), but there is no question template and no link to Discussions; no "first contribution"/good first issue section. Checklist diverges from the PR template (D5). |
| `SECURITY.md` | yes | Has a channel (GitHub private advisory, `:9`), a deadline (`:13`) and a scope (`:15-19`). Missing an alternative channel in case private reporting is disabled in the repository (not verifiable locally: suspected). |
| `SUPPORT.md` | **no** | Nothing says where to ask questions other than "open an issue". |
| `CODE_OF_CONDUCT.md` | yes | The reporting channel is "the contact on that GitHub profile" (`:39`), with no fixed e-mail or private channel. |
| `.github/ISSUE_TEMPLATE/` | yes: `bug_report.yml`, `feature_request.yml`, `config.yml` | `config.yml:1` leaves blank issues enabled and has no questions/support contact link. `bug_report.yml:21` uses `render: cpp` for the reproduction, which does not fit TS or commands. |
| `.github/pull_request_template.md` | yes | No hot-path benchmark item in the checklist (only in the comment `:7`) and no "branch from develop" (`CONTRIBUTING.md:89`). |
| `LICENSE` | yes (MIT, 2026 Esliph) | — |
| `CODEOWNERS` | **no** | Single maintainer, so low impact; no automatic review request. |
| `FUNDING.yml` | **no** | Optional. |
| `CHANGELOG.md` | **no** | High impact: `0.x` breaks in minors (`cli.md:140`) and the notes are only `--generate-notes` (`release.yml:224`). |
| README in `templates/game/` and `tools/oly/` | **no** | Covered by `guide/cli.md` and `architecture/publishing.md`; low impact. |

---

## 8. Verified × suspected (summary)

- **Verified:** every item in §1; §4 except the rows marked suspected; the 3 broken links; the §5.1 items; D1–D8, D10,
  D12–D16; the existence and absence of the community health files.
- **Suspected:** the intent of the alternative `installGame` signatures (`states.md:78`, `typescript/recipes.md:334`);
  the wording of `typescript/world-and-entities.md:101`; `recipes.md:692`; D9, D11, D17, D18; the "suspected" items in
  §3; the `SECURITY.md` alternative channel.
