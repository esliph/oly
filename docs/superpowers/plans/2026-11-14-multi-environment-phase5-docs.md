# Multi-environment, phase 5 (docs) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The user guide teaches concepts in C++ and groups everything else by environment and client, and the
architecture docs, the guide style, the root `README.md` and `CLAUDE.md` describe Oly as a C++ engine with
environments and clients.

**Architecture:** The numbered guide pages move with `git mv` to their new homes first, with every link rewritten, so
each later task only rewrites content. Then one task per concept turns `concepts/<concept>.md` into a C++ page (body
written against the context, registration labelled under it) and writes its mirror
`environments/web/typescript/<concept>.md` from the TS content the concept page loses. The environment pages, recipes,
index, cheat sheet, troubleshooting and guide style follow, then the architecture docs. A link checker kept in
`build/doc-check/` (not committed) proves that no link or anchor is broken.

**Tech Stack:** Markdown (GitHub rendering), Python 3 for the link checker (in the `dev` image), git.

**Spec:** `docs/superpowers/specs/2026-11-13-multi-environment-design.md` (§7, §8 phase 5). Base: `f296ec2`.

## Global Constraints

- Spec §7.3: the tree is exactly the one in the spec; pages are not numbered.
- Spec §7.4: "Concept pages carry the contract; client pages carry the API." A rule appears in a client page only when
  it exists only in that client (TS: `raw()` arrays, heap views invalidated by memory growth, `ContractError`).
- Spec §7.4: client pages mirror the concept pages: same file name, same section headings where the concept has a form
  in that client; a section with no client form is left out, not stubbed.
- Spec §7.4: the main example of each concept is the function body written against the context, valid in every
  environment; registration is short and labelled under it. Setup-only operations are shown on `Engine` with the same
  "where you own the `Engine`" label.
- Spec §7.4: C++ examples are checked against the headers by reading; compiling them is out of scope. Every call in a
  C++ example exists in `engine/runtime/*/include/oly/**` or `engine/environments/native/host/include/oly/native_host/`
  with that signature; every TS call exists in `@oly/core` (`api/public-exports.txt`) or `@oly/web-host`.
- Spec §7.4: no old paths kept; plans and old specs (`docs/superpowers/`) are history and are not edited.
- `docs/development/guide-style.md` rules that stay: English, glossary names, no filler, game-agnostic (generic names
  `Player`, `Enemy`, `Position`, `<game>`; no link to a game's file for code the reader needs), code style in examples
  (full names, braces on every `if`/`for`, blank lines between steps), comments only for the why, commands are
  `./spur` tasks.
- Toolchain only in Docker through `./spur`. The link checker runs with `./spur dev python3 -I build/doc-check/check_links.py`.
- Commits in English, prefix `docs:`, one short line on what changes; never cite the spec, the plan or task numbers.
- Do not touch `docs/benchmarks/` (`docs/benchmarks/bridge/` keeps its name, spec §7.5) nor internal TS code names
  (`Bridge`, `bridgeOf`).

**Decided here (not in the spec):**

- **Page headers.** A concept page starts with `# <Title>` and the line
  `[← <Previous>](<previous>.md) · [Guide index](../README.md) · Next: [<Next> →](<next>.md)`, in the order
  schema, world-and-entities, queries, engine-and-systems, events, input, schedule, states, performance (the first has
  no previous link, the last no next). A TS page starts with `# <Title> in TypeScript` and
  `[← <Previous>](<previous>.md) · [TypeScript client](README.md) · Concept: [<Title>](../../../concepts/<concept>.md) · Next: [<Next> →](<next>.md)`,
  in the same order, then `recipes.md`. Environment pages and references use
  `[Guide index](<relative>/README.md)` only.
- **Page endings.** Concept and TS pages end with `## Next` (one line: the link and what it teaches). A concept page's
  last line, after that section, is `In other clients: [TypeScript](../environments/web/typescript/<concept>.md).`
- **The registration label.** Under each main C++ body:

  ````markdown
  **Where you own the `Engine`** (native):

  ```cpp
  engine.addSystem(oly::app::Phase::Update, "integrate", integrate);
  ```

  **On the web**, the client registers it by name: [TypeScript](../environments/web/typescript/engine-and-systems.md#<anchor>).
  ````

  This web line is the anchor link from a concept section to its client section. A section whose examples are all
  setup-only drops the "On the web" line when the TS page has no section for it.
- **Bodies use the macros.** Systems, handlers and state hooks in concept pages are written with `OLY_SYSTEM`,
  `OLY_HANDLER` and `OLY_STATE_HOOK`, because that is the form both environments can register. On native,
  `OLY_SYSTEM` and `OLY_STATE_HOOK` define plain functions that `addSystem`, `onEnter` and `onExit` take directly. An
  `OLY_HANDLER(name, Event)` function takes `(HandlerContext&, std::span<const Event>)`, so its native registration
  is a captureless lambda (no new API):

  ```cpp
  engine.on<EnemyDied>("onEnemyDied", [](oly::app::HandlerContext& context) { onEnemyDied(context, context.events<EnemyDied>()); });
  ```

  The first time a page shows this lambda, one sentence says why: the macro's function takes the batch as a span, and
  `on<T>` takes a `HandlerFunction`.
- **Names in C++ examples** are the generated ones: components and events by their `.oly` name (`Position`,
  `EnemyDied`), actions as `actions::move`, states as `states::playing`, in a file that includes
  `"schema.generated.hpp"`. Native registration snippets assume `oly::app::declareSchema(engine)` was called.
- **The link checker** lives in `build/doc-check/check_links.py` (ignored by git; created in Task 1, used by every
  task). It is a verification tool for this phase, not a repository tool: nothing asked for a permanent link lint.
- **The TS page set** is exactly the spec's list: `README.md`, eight concept mirrors and `recipes.md`. There is no TS
  `performance.md`: the TS part of performance is the web boundary, so it lives in `environments/web/boundary.md`.
- **`concepts/` has no README**; `docs/guide/README.md` is its index.

## Review Focus

- **An anchor that a concept page links to does not exist in the TS page** (a heading renamed in one of the pair).
  The checker verifies anchors, not only files; every task runs it.
- **A C++ example calls something that does not exist** (`context.set`, `engine.world`, `context.actions` without
  parentheses: TS shapes leaking into C++). Each concept task ends with a step that checks every C++ call of its page
  against the header named in that step.
- **A contract rule written in both pages** (deferred vs. direct, event rounds, scope) and later edited in one. Each TS
  task has a step that greps its page for the rule words of its concept and keeps only TS-only rules.
- **A reader on the web lands on a C++ registration they cannot run** (an `Engine&` example with no label). Each
  concept task's check step looks for `engine.` in `cpp` blocks outside a labelled registration.
- **A link from outside `docs/guide/`** (`docs/architecture/*.md`, `games/*/README.md`, `README.md`) still points to a
  numbered page or `bridge.md`. The checker scans the whole repository except `docs/superpowers/`, `build/` and
  `node_modules/`.

---

### Task 1: Move the pages and rewrite every link

**Files:**
- Create: `build/doc-check/check_links.py` (not committed)
- Move (`git mv`): see the table in Step 3
- Modify: every `.md` file outside `docs/superpowers/` whose links point to a moved file

**Interfaces:**
- Produces: the new paths below, which every later task edits; `build/doc-check/check_links.py`, run as
  `./spur dev python3 -I build/doc-check/check_links.py`, exit 0 when every relative link and anchor resolves.

- [ ] **Step 1: Commit the revised spec and this plan**

```bash
git add docs/superpowers/specs/2026-11-13-multi-environment-design.md docs/superpowers/plans/2026-11-14-multi-environment-phase5-docs.md
git commit -m "docs: plan the guide reorganization"
```

- [ ] **Step 2: Write the link checker**

`check_links.py` walks every `*.md` under the repository root, skipping `docs/superpowers/`, `build/`, `node_modules/`
and `.git/`. For each inline link `[text](target)` outside fenced code blocks whose target is not a URL (`http`,
`https`, `mailto`), it resolves the path relative to the file and reports `file:line: missing <target>` when the file
does not exist, and `file:line: missing anchor <target>` when a `#anchor` is not among the target's heading slugs. A
bare `#anchor` refers to the same file. The slug follows GitHub: lowercase, drop every character that is not a letter,
digit, space, `-` or `_`, spaces become `-`, and a repeated slug in one file gets `-1`, `-2`... Exit code 1 when
anything is reported.

- [ ] **Step 3: Run it to record the baseline**

Run: `./spur dev python3 -I build/doc-check/check_links.py`
Expected: exit 0 (the guide's links are valid at `f296ec2`). If not, note the pre-existing failures in the task report
and fix them in Step 5.

- [ ] **Step 4: Move the files**

| From | To |
|---|---|
| `docs/guide/01-getting-started.md` | `docs/guide/environments/web/getting-started.md` |
| `docs/guide/02-schema.md` | `docs/guide/concepts/schema.md` |
| `docs/guide/03-world-and-entities.md` | `docs/guide/concepts/world-and-entities.md` |
| `docs/guide/04-queries.md` | `docs/guide/concepts/queries.md` |
| `docs/guide/05-engine-and-systems.md` | `docs/guide/concepts/engine-and-systems.md` |
| `docs/guide/06-events.md` | `docs/guide/concepts/events.md` |
| `docs/guide/07-input.md` | `docs/guide/concepts/input.md` |
| `docs/guide/08-schedule.md` | `docs/guide/concepts/schedule.md` |
| `docs/guide/09-states.md` | `docs/guide/concepts/states.md` |
| `docs/guide/11-testing.md` | `docs/guide/environments/web/testing.md` |
| `docs/guide/12-performance.md` | `docs/guide/concepts/performance.md` |
| `docs/guide/13-troubleshooting.md` | `docs/guide/troubleshooting.md` |
| `docs/guide/native-desktop.md` | `docs/guide/environments/native/getting-started.md` |
| `docs/architecture/bridge.md` | `docs/architecture/capi.md` |

`docs/guide/10-cpp-gameplay.md` stays where it is until Task 11 spreads its content and removes it.

- [ ] **Step 5: Rewrite the links**

Run the checker; for each reported link, point it to the new path of its target (relative to the linking file, anchor
kept), including the moved pages' own links to their neighbours and to `../architecture/`, the `README.md` index tables,
and `docs/architecture/README.md`'s index row for `bridge.md` (now `[capi.md](capi.md)`). Change only link targets;
link text and prose change in later tasks.

- [ ] **Step 6: Run the checker**

Run: `./spur dev python3 -I build/doc-check/check_links.py`
Expected: exit 0, no output.

- [ ] **Step 7: Commit**

```bash
git add -A docs README.md games
git commit -m "docs: move the guide pages to concepts and environments"
```

---

### Concept tasks (Tasks 2–9): the common steps

Each concept task rewrites `docs/guide/concepts/<concept>.md` and creates
`docs/guide/environments/web/typescript/<concept>.md`. Its own section lists the headings of both pages, in order, and
what each holds. The steps are the same for all of them:

1. **Write the TS page** from the TS content the concept page loses: its `ts` blocks, TS member tables and TS-only
   rules, under the headings the task lists, each section linking to the concept section for the rules. Header and
   ending as in Global Constraints.
2. **Rewrite the concept page** in C++: prose that states the contract, the main example as a body (macros, context),
   the labelled registration under it, setup-only operations on `Engine` with the same label. Header and ending as in
   Global Constraints. Rules move here from `10-cpp-gameplay.md` where the task says so; do not delete
   `10-cpp-gameplay.md` yet.
3. **Check the C++ calls**: for every call in the page's `cpp` blocks, find its declaration in the headers the task
   names, and correct the example when it differs. Then search the `cpp` blocks for `engine.`, `world.` and
   `oly::app::Engine` outside a "Where you own the `Engine`" block, and for TS shapes (`context.set`, `context.actions.`,
   `engine.world`, `eachBlock((block)`); expected: none.
4. **Check the rule split**: search the TS page for the rule words the task lists; each hit is either a link to the
   concept page or a TS-only rule. Expected: no contract rule restated.
5. **Run the checker**: `./spur dev python3 -I build/doc-check/check_links.py`, expected exit 0. Links into a TS page
   that a later task creates are allowed to fail only if they point to `typescript/<later-concept>.md`; list them in
   the task report. Every other link resolves.
6. **Commit** both pages: `git commit -m "docs: teach <concept> in C++ and move its TypeScript to the client pages"`.

### Task 2: Schema, and the TS client's README

**Files:**
- Modify: `docs/guide/concepts/schema.md`
- Create: `docs/guide/environments/web/typescript/schema.md`, `docs/guide/environments/web/typescript/README.md`

**Interfaces:**
- Consumes: Task 1's paths and checker.
- Produces: the TS client's `README.md` with the page list every TS page's header links to (`[TypeScript client](README.md)`).

- [ ] **Step 1: Write `typescript/README.md`**

Sections: what the TS client is (the TypeScript API over the C API, inside the `web` environment, no DOM); its
packages (`@oly/core`, the client; `@oly/core/generated`, for generated code only; `@oly/web-host`, the web host it
runs on; `@oly/testing` is private to the engine and not listed); the two error classes `ContractError` and
`SchemaMismatchError` with a link to [Troubleshooting](../../guide/troubleshooting.md); a table of the TS pages, each with
the concept it mirrors; a line saying that the C++ systems of a web game are registered from TS by name
(`addNativeSystem`, `addNativeHandler`, `addNativeOnEnter`/`addNativeOnExit`), with a link to
`typescript/engine-and-systems.md#c-systems-handlers-and-hooks` (the heading Task 5 creates).

- [ ] **Step 2: Common steps 1–6 with these headings**

| Concept page (C++) | TS page |
|---|---|
| The five kinds of entries (`.oly` block; the generated C++ names: structs, `actions::`, `states::`) | The five kinds of entries (the import from `schema.generated.js`) |
| Field types (`.oly` type → C++ type) | Field types (what each type reads as in TS) |
| Naming rules | — |
| Order matters | — |
| Memory layout (the generated `static_assert`s) | — |
| What the build generates (`schema.generated.hpp`, `schema_registry.generated.cpp`, `schema_declaration.generated.cpp` with `oly::app::declareSchema`) | What the build generates (`schema.generated.ts`, `@oly/core/generated` only in `*.generated.ts`, never committed) |
| — | The schema check at start-up (`loadRuntime`, `SchemaMismatchError`: TS-only rule) |
| Using the schema in several places | — |

Headers to check against: `engine/runtime/app/include/oly/app/schema.hpp`, `engine/schema/format/`, a generated
header in `build/` (`./spur build` if none exists). Rule words for step 4: `order`, `reserved`, `at most 32`, `hash`.

---

### Task 3: World and entities

**Files:**
- Modify: `docs/guide/concepts/world-and-entities.md`
- Create: `docs/guide/environments/web/typescript/world-and-entities.md`

- [ ] **Step 1: Common steps 1–6 with these headings**

| Concept page (C++) | TS page |
|---|---|
| Where the World comes from (the `Engine` does not own the `World`; the context gives access; native: `oly::ecs::World world;` before the `Engine`) | Where the World comes from (`engine.world`) |
| Create entities (`context.spawn(...)` in a body; `world.spawn` where you own the `Engine`) | Create entities |
| Read and write values (`get`, `tryGet`) | Read and write values (`get`, `set`) |
| Add and remove components | Add and remove components |
| Destroy and check | Destroy and check |
| Entity handles / Keep references between entities | Entity handles / Keep references between entities |
| What stays valid (references until the next structural change) | What stays valid (only the TS-only rule: heap views invalidated by memory growth) |
| Structural changes and the loop | — |
| Without the engine: `CommandBuffer` | — |
| Debugging the world (`oly::ecs` diagnostics) | Debugging the world |

Headers: `oly/app/context.hpp`, `oly/app/system.hpp`, `oly/ecs/world.hpp`, `oly/ecs/entity.hpp`,
`oly/ecs/command_buffer.hpp`, `oly/ecs/diagnostics.hpp`. Rule words: `deferred`, `end of the phase`, `isAlive`,
`generation`.

---

### Task 4: Queries

**Files:**
- Modify: `docs/guide/concepts/queries.md`
- Create: `docs/guide/environments/web/typescript/queries.md`

- [ ] **Step 1: Common steps 1–6 with these headings**

| Concept page (C++) | TS page |
|---|---|
| The query spec (`query<Ts...>(With<...>{}, Without<...>{})`; `const T` reads, `T` writes) | The query spec (`{ read, write, with, without }`) |
| Iterate (`each`, and `eachBlock` for whole columns) | Iterate (`eachBlock`) |
| Rules while iterating (changes go through the context; references valid inside the callback) | Rules while iterating (only TS-only rules) |
| Exactly one: `single` and `trySingle` | Exactly one: `single` and `trySingle` |
| Common query patterns: count, nearest, pairs (the nested-queries example of `10-cpp-gameplay.md`), several shapes | the same four, in TS |
| — | Raw arrays (`raw()`: TS-only rule; link to `../boundary.md` for when to use it) |

The old "C++ queries" section merges into the sections above and disappears. Headers: `oly/ecs/query.hpp`,
`oly/app/context.hpp`. Rule words: `deferred`, `valid`, `structural`.

---

### Task 5: Engine, loop and systems

**Files:**
- Modify: `docs/guide/concepts/engine-and-systems.md`
- Create: `docs/guide/environments/web/typescript/engine-and-systems.md`

- [ ] **Step 1: Common steps 1–6 with these headings**

| Concept page (C++) | TS page |
|---|---|
| Create the engine (`EngineConfig` fields; where you own the `Engine`) | Create the engine (`runtime.createEngine`, `dispose`) |
| The loop (who calls `frame`: the host of each environment, with links to both getting-started pages) | The loop (`runInBrowser`) |
| Phases | — |
| Register systems (`OLY_SYSTEM` body; `addSystem`, `setWhilePaused`, `setSystemStates`) | Register systems (`addSystem`, `.inState`, `.whilePaused`) |
| Systems in C++, registered by name: `OLY_SYSTEM`, `OLY_HANDLER`, `OLY_STATE_HOOK` (main section; the macro table of `10-cpp-gameplay.md` without its TS column, saying they back `olyEngineAddNative*` for every client; includes and `schema.generated.hpp`) | C++ systems, handlers and hooks (`addNativeSystem`, `addNativeHandler`, `addNativeOnEnter`/`addNativeOnExit`; a wrong name throws `ContractError` at registration) |
| The system context (the C++ context table of `10-cpp-gameplay.md`) | The system context (the TS member table) |
| Structural changes in systems | Structural changes in systems |
| Time and interpolation | Time and interpolation |
| Pause | Pause |
| Hidden (what `setHidden` does; each host decides when, with links) | — |
| Driving the loop yourself (`engine.frame` from a test or tool) | Driving the loop yourself |
| When a system throws | When a system throws (only the TS-only part: `ContractError` until the next `frame`) |

Headers: `oly/app/engine.hpp`, `oly/app/system.hpp`, `oly/app/context.hpp`, `oly/app/handler.hpp`,
`oly/app/state.hpp`, `oly/app/phase.hpp`. Rule words: `end of the phase`, `next phase`, `recovers`, `discards`,
`hidden`.

---

### Task 6: Events

**Files:**
- Modify: `docs/guide/concepts/events.md`
- Create: `docs/guide/environments/web/typescript/events.md`

- [ ] **Step 1: Common steps 1–6 with these headings**

| Concept page (C++) | TS page |
|---|---|
| Declare, emit, handle (`OLY_HANDLER` body; the lambda registration from Global Constraints, with its one-sentence reason) | Declare, emit, handle |
| Handlers receive batches (`std::span<const Event>`) | Handlers receive batches |
| Handlers change the world directly | Handlers change the world directly |
| The engine does not remove duplicates | The engine does not remove duplicates |
| Order / Put the destroying handler last | Order / Put the destroying handler last |
| Rounds: events that cause events | — |
| When events are dispatched | — |
| Restrict handlers to states (`setHandlerStates`) | Restrict handlers to states |
| Events vs. other tools | — |

The old "C++ handlers" section merges into the sections above. Headers: `oly/app/handler.hpp`, `oly/app/event.hpp`,
`oly/app/engine.hpp`. Rule words: `round`, `maxEventRounds`, `direct`, `duplicate`, `dispatched`.

---

### Task 7: Input

**Files:**
- Modify: `docs/guide/concepts/input.md`
- Create: `docs/guide/environments/web/typescript/input.md`

- [ ] **Step 1: Common steps 1–6 with these headings**

| Concept page (C++) | TS page |
|---|---|
| Three kinds of actions | — |
| Bind buttons (`engine.actions().bind(actions::jump, oly::input::Key::Space)`, labelled setup-only) | Bind buttons |
| Read actions in systems (`context.actions().button(...)`, `.axis2d`, `.pointer`) / The 2D axis | Read actions in systems / The 2D axis |
| Where input comes from (the host forwards `buttonDown`, `buttonUp`, `pointerMove`, `releaseAll`; links to both getting-started pages) | Connect the browser (`attachBrowserInput`) |
| Rebind at run time | Rebind at run time |
| Deliver input yourself (the `Engine` input functions, for tests and tools) | Deliver input yourself |

The old "C++" section merges into the sections above. Headers: `oly/input/action_map.hpp`,
`oly/input/physical_button.hpp`, `oly/app/engine.hpp`, `oly/app/context.hpp`. Rule words: `pressed`, `released`,
`edge`, `releaseAll`, `hidden`.

---

### Task 8: Schedule

**Files:**
- Modify: `docs/guide/concepts/schedule.md`
- Create: `docs/guide/environments/web/typescript/schedule.md`

- [ ] **Step 1: Common steps 1–6 with these headings**

| Concept page (C++) | TS page |
|---|---|
| `after` and `every` (function + payload, from the old "C++ tasks and payloads") | `after` and `every` |
| The task context | The task context |
| Cancel and check | Cancel and check |
| Payloads (C++ form of captured values; the `PlayerTarget` example of `10-cpp-gameplay.md`) | Captured values: closures |
| Discard callbacks | — |
| Pause, states and recovery | — |

Headers: `oly/app/scheduler.hpp`, `oly/app/task.hpp`, `oly/app/task_context.hpp`. Rule words: `game clock`, `pause`,
`scoped`, `discard`, `recovery`.

---

### Task 9: States

**Files:**
- Modify: `docs/guide/concepts/states.md`
- Create: `docs/guide/environments/web/typescript/states.md`

- [ ] **Step 1: Common steps 1–6 with these headings**

| Concept page (C++) | TS page |
|---|---|
| Declare and start (`requestState`, labelled) | Declare and start |
| Change state (`context.requestState(states::gameOver)`) | Change state |
| Run only in some states (`setSystemStates`) | Run only in some states |
| Enter and exit hooks (`OLY_STATE_HOOK` body; `onEnter`/`onExit`) | Enter and exit hooks |
| Scope: automatic clean-up / Keep something across states: `persist` | Scope: automatic clean-up / Keep something across states: `persist` |
| Complete pattern: menu → playing → game over (bodies, then one labelled block with the registrations) | Complete pattern: menu → playing → game over |

The old "C++" section merges into the sections above. Headers: `oly/app/state.hpp`, `oly/app/state_context.hpp`,
`oly/app/engine.hpp`. Rule words: `scoped`, `end of the step`, `at most 32`, `persist`.

---

### Task 10: Performance and the web boundary

**Files:**
- Modify: `docs/guide/concepts/performance.md`
- Create: `docs/guide/environments/web/boundary.md`

**Interfaces:**
- Consumes: `typescript/queries.md#raw-arrays` (Task 4).
- Produces: `environments/web/boundary.md`, linked from the web getting-started page (Task 11) and the README (Task 13).

- [ ] **Step 1: Write `environments/web/boundary.md`**

Title `# The WASM↔JS boundary`, header with `[Guide index](../../README.md)`. Sections, from the old rules 1–4:
`eachBlock` vs. per-entity `get`; move per-entity work to C++ (link to
`../../concepts/engine-and-systems.md#systems-in-c-registered-by-name-oly_system-oly_handler-oly_state_hook`); `raw()`
in hot loops (link to `typescript/queries.md#raw-arrays`); batching events across the boundary; measuring
(the `./spur wasm node ... boundary.js` command of `docs/development/commands.md`). Examples in TS.

- [ ] **Step 2: Rewrite `concepts/performance.md` in C++**

Sections: Avoid archetype churn; Split components by who reads them; Reuse queries; Watch the fragmentation; Keep
render cheap; Measuring (native benchmarks, link to `docs/development/commands.md`); Debug vs. release (presets,
`OLY_ASSERT` only in debug). Concept header (no next link) and the ending line
`In other clients: [TypeScript, across the WASM boundary](../environments/web/boundary.md).`

- [ ] **Step 3: Common steps 3, 5 and 6** (headers: `oly/ecs/query.hpp`, `oly/ecs/world.hpp`); commit message
`docs: split performance into the concept and the web boundary`.

---

### Task 11: The environment pages, and `10-cpp-gameplay.md` removed

**Files:**
- Modify: `docs/guide/environments/web/getting-started.md`, `docs/guide/environments/native/getting-started.md`,
  `docs/guide/environments/web/testing.md`
- Delete (`git rm`): `docs/guide/10-cpp-gameplay.md`

**Interfaces:**
- Consumes: the concept pages (Tasks 2–10).
- Produces: anchors `environments/web/getting-started.md#how-to-split-work-between-c-and-a-client` and
  `environments/native/getting-started.md#test-your-game`, used by the README (Task 13).

- [ ] **Step 1: Web getting started**

Title `# Getting started on the web`. Keep the old page's sections (what you need, run an example game, how a game is
built, create your own game, build and run). Add, from `10-cpp-gameplay.md`: "How to split work between C++ and a
client" (its table, with "TS" read as the client), the shared-constants and deterministic-randomness notes, and
"Advanced: your own exported C functions" as the last section. "Three ways to split the work" merges into the split
section. The `## Next` line points to the TS client README and to `../../concepts/schema.md`.

- [ ] **Step 2: Native getting started**

Title `# Getting started on the desktop`. Keep the old `native-desktop.md`. Bring the C++-only `main` example of
`10-cpp-gameplay.md` only where it adds something the existing smallest game lacks (`userData`, `setSystemStates`);
do not keep two full `main`s. Grow "Headless and fixed-time runs" into `## Test your game`: run a fixed number of frames
(`RunOptions::maxFrames` and `frameSeconds` with `SDL_VIDEO_DRIVER=dummy`, or `engine.frame` with a fake time and no
window), check the `World`, register the test with ctest in the game's `CMakeLists.txt` (`add_test`), run it with
`./spur native-games-test`. Move the old web testing page's "Testing C++ systems alone" here, rewritten for a game
with no WASM.

- [ ] **Step 3: Web testing**

Title `# Testing a web game`. Remove "Testing C++ systems alone" (now on the native page; leave one line linking to
it). Everything else stays.

- [ ] **Step 4: Remove `10-cpp-gameplay.md`**

Check first that each of its sections is in a new home: macros and context table (Task 5), examples (Tasks 4, 7, 8),
split, constants, exported C functions (Step 1), C++-only `main` (Step 2). Then `git rm docs/guide/10-cpp-gameplay.md`
and point every link to it at the new home of the anchor it used.

- [ ] **Step 5: Common steps 3 and 5** (headers: `oly/native_host/run.hpp`, `oly/native_host/window.hpp`,
`oly/app/engine.hpp`, `cmake/OlyGame.cmake`). Expected: the checker exits 0 except links to the TS pages of Tasks
12–13 not yet written.

- [ ] **Step 6: Commit**

```bash
git add -A docs/guide
git commit -m "docs: rewrite the web and desktop getting-started pages and remove the C++ gameplay page"
```

---

### Task 12: Recipes

**Files:**
- Modify: `docs/guide/recipes.md`
- Create: `docs/guide/environments/web/typescript/recipes.md`

- [ ] **Step 1: Write `typescript/recipes.md`**

Every recipe of the old page, in TS, under its same heading, with the table at the top. The browser-only recipes (High
score saved in the browser, Score and HUD, Debug overlay) are marked in the table as browser-only. TS page header (its
previous page is `states.md`, no next).

- [ ] **Step 2: Rewrite `recipes.md` in C++**

Every recipe that is not browser-only, in C++ (bodies with the macros, registration labelled as in concept pages),
each section ending with `TypeScript: [<heading>](environments/web/typescript/recipes.md#<anchor>).` The table at the
top lists every recipe; the browser-only ones link straight to the TS page. "Keep inside the screen or wrap around" and
"Health, damage and death" already have C++ to start from.

- [ ] **Step 3: Common steps 3, 5 and 6** (headers: those of Tasks 3–9); commit message
`docs: write the recipes in C++ and move their TypeScript to the client pages`.

---

### Task 13: README, cheat sheet and troubleshooting

**Files:**
- Modify: `docs/guide/README.md`, `docs/guide/cheat-sheet.md`, `docs/guide/troubleshooting.md`

- [ ] **Step 1: README**

Sections: what you can build (Oly as a C++ engine; environments `web` and `native`; the TS client; what it does not
do, linking to `troubleshooting.md#what-oly-does-not-do`); **The map**: core, environments, hosts, clients, and "C++
is not a client", in a few lines and a table of the two environments (host, clients, getting-started page); **Choose
your path**: three learning paths exactly as spec §7.4 lists them (TS on the web; C++ on the desktop; C++ inside a web
game), each a numbered list of links; **Concepts**: table of the nine concept pages with "you learn to"; **References**:
recipes, cheat sheet, troubleshooting, glossary; the big picture example rewritten as `.oly` + one C++ body + both
registrations (native `main` fragment and a link to the TS version in the web getting-started page); example games;
conventions (`cpp` blocks use `oly::ecs`, `oly::app`, `oly::input`; `ts` blocks only in client pages; "Contract
error": C++ `OLY_ASSERT` in debug builds, TS `ContractError`).

- [ ] **Step 2: Cheat sheet**

Order: C++ (by topic: start-up with `declareSchema`, schema names, world, queries, engine, contexts, events, input,
schedule, scope, macros, native host and `oly_add_game ... ENVIRONMENT`); C API (one table of the `oly*` groups as
`docs/architecture/capi.md` lists them, plus `OlyScriptInvoker` and `olyEngineSetScriptInvoker`, with a link to
`../architecture/capi.md` for the contract); TypeScript (the existing TS blocks); errors. Each topic links to its
concept page.

- [ ] **Step 3: Troubleshooting**

Title `# Troubleshooting`. "Common messages" stays shared. Replace "The two error classes (TS)" with two short
sections: `## C++: OLY_ASSERT and debug presets` and `## TypeScript: ContractError and SchemaMismatchError`. Keep
"It does nothing", "Rebuild checklist" (commands for both environments) and "What Oly does not do".

- [ ] **Step 4: Run the checker**

Run: `./spur dev python3 -I build/doc-check/check_links.py`
Expected: exit 0, no output. From this task on, nothing may fail.

- [ ] **Step 5: Commit**

```bash
git add docs/guide
git commit -m "docs: map the guide by environment and client"
```

---

### Task 14: Guide style

**Files:**
- Modify: `docs/development/guide-style.md`

- [ ] **Step 1: Update the style**

- "When the guide must change": the troubleshooting link becomes `../guide/troubleshooting.md`. The "What to update"
  table: a new or changed function updates the concept page and the client page of its topic and the cheat sheet; a C
  API change updates the cheat sheet and `docs/architecture/capi.md`; a new page updates the README tables and the
  navigation lines of its folder neighbours.
- "Audience and tone": the reader is a game developer in any supported language; C++ is the language of the concepts.
- "Files and navigation": the tree of spec §7.3; the page header and ending rules of this plan's Global Constraints;
  the mirroring rule (same file name, same headings, no stubs, `In other clients` line); the rule that a contract
  appears only in the concept page.
- "Examples": C++ first in concepts, the client's language in client pages; the body/registration split and the
  "Where you own the `Engine`" label; the `OLY_HANDLER` lambda.
- The game-agnostic rule stays as it is.

- [ ] **Step 2: Run the checker** (expected exit 0) **and commit**

```bash
git add docs/development/guide-style.md
git commit -m "docs: describe the guide by concepts, environments and clients"
```

---

### Task 15: Architecture docs, root README and CLAUDE.md

**Files:**
- Modify: `docs/architecture/README.md`, `docs/architecture/decisions.md`, `docs/architecture/repository.md`,
  `docs/architecture/modules-and-boundaries.md`, `docs/architecture/schema-and-codegen.md`,
  `docs/architecture/ts-packages.md`, `docs/architecture/limits.md`, `docs/architecture/glossary.md`,
  `docs/architecture/app.md`, `docs/architecture/input.md`, `docs/architecture/games.md`,
  `docs/architecture/performance.md`, `README.md`, `CLAUDE.md`, `docs/development/commands.md`

- [ ] **Step 1: `docs/architecture/README.md`**

Overview: core, environments, hosts and clients as spec §3.1 defines them ("C++ is not a client"). Rule 1: "The engine
is complete in C++. A client has API conveniences, never engine behaviour." Rule 2: "Each client crosses its boundary
per block, per batch or per system, never per entity, on hot paths." Rule 3 stays. The diagram shows the core, the C
API, the `web` environment (WASM, web host, TS client) and the `native` environment (native host, the game's `main`).
The index row reads `[capi.md](capi.md)`. The reference table gains
`docs/superpowers/specs/2026-11-13-multi-environment-design.md` (C++ first, environments and clients, `.oly`, native)
and `docs/superpowers/specs/2026-11-13-spur-task-runner-design.md` (the `./spur` task runner).

- [ ] **Step 2: `decisions.md`**

Add three rows (decision | reason | accepted cost): C++ first, clients grouped by environment, C++ not a client; the
native host as a library the game calls (spec §6.1: `main` visible, explicit order, headless test with no second entry
point | some repeated code in each game's `main`); C API headers as valid C (`.h`) (a C, C#, Rust or Lua client
consumes the real ABI | handles need `typedef`s and `struct` tags). The `@oly/core`/`@oly/web-host` row stays.

- [ ] **Step 3: The other architecture files**

For each file in the list, search for `bridge`, `thin TS layer`, `TS layer`, `browser engine`, `numbered`,
`native-desktop`, `10-cpp`, `0[1-9]-` and `1[0-3]-`, and correct leftovers of the old structure. `bridge` stays only
where it names the boundary benchmark suite or `docs/benchmarks/bridge/`. `games.md` describes every game in `games/`
(`arena` on `web`, `drift` on `native`), not only `arena`.

- [ ] **Step 4: Root `README.md`, `CLAUDE.md`, `commands.md`**

`README.md`: Oly as a C++ engine with environments (`web`, `native`) and clients (TS today); the docs table row for
`docs/architecture/` says `capi` instead of "bridge". `CLAUDE.md`'s first paragraph: "Oly: 2D game engine in C++20 on
top of a single-threaded archetype ECS (study and benchmark project). The C++ core runs in environments (`web`: WASM
with a TS client; `native`: a desktop host), with a C API `oly*` for clients in other languages." `commands.md`:
check every path and link.

- [ ] **Step 5: Run the checker and search for old names**

Run: `./spur dev python3 -I build/doc-check/check_links.py`
Expected: exit 0.

Run: `git grep -nE '0[1-9]-[a-z-]+\.md|1[0-3]-[a-z-]+\.md|native-desktop\.md|architecture/bridge\.md|\(bridge\.md\)' -- ':!docs/superpowers'`
Expected: no output.

- [ ] **Step 6: Commit**

```bash
git add docs README.md CLAUDE.md
git commit -m "docs: describe Oly as a C++ engine with environments and clients"
```

---

### Task 16: Whole-guide verification

**Files:** none changed unless a check fails.

- [ ] **Step 1: The tree**

Run: `find docs/guide -name '*.md' | sort`
Expected: exactly the files of spec §7.3 (README, recipes, cheat-sheet, troubleshooting, nine concept pages, web
getting-started/testing/boundary, ten TS pages, native getting-started).

- [ ] **Step 2: The mirrors**

For each concept with a TS page, list the `##`/`###` headings of the TS page that are not in the concept page. Each one
must be a TS-only section named in its task's table (`The schema check at start-up`, `Raw arrays`, `C++ systems,
handlers and hooks`, `Connect the browser`, `Captured values: closures`). Anything else is a renamed heading: fix it.

- [ ] **Step 3: No TS in concepts or in the C++ recipes**

Run: `grep -c '^```ts' docs/guide/concepts/*.md docs/guide/recipes.md`
Expected: 0 for every file.

- [ ] **Step 4: Links and builds**

Run: `./spur dev python3 -I build/doc-check/check_links.py` → exit 0.
Run: `./spur test` and `./spur games-test` → pass (nothing in code changed; this confirms it).

- [ ] **Step 5: Commit any fix**

```bash
git add docs
git commit -m "docs: fix the guide's mirrors and links"
```
