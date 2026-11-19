# Oly — documentation reorganization by Diátaxis type — design

Date: 2026-11-18. Written on `develop` at `1d6ee51`.
Input: `2026-11-18-docs-audit.md`, cited as **A1–A10** (prioritized summary), **D1–D18** (duplications), **§3.2** (public
API without docs) and **§3.3** (names public only because they sit outside `detail/`).

Marking: **[user decision]** was decided in the conversation; **[author decision]** was my choice during the design,
which the user may review; the rest is a direct consequence of the two.

## 1. Intent

The audit found the documentation correct in names and signatures but wrong in structure and drifting in behaviour:
pages mix tutorial, reference, explanation and how-to; the two getting-started examples and the web test harness no
longer match what `oly new` creates (A1, A6); the C++ API lives inside the concept pages, so a contract fix and an API
fix touch the same paragraph (A2); the same facts live in up to five places (D1–D18); about 118 C++ names, 17 C API
constants and types and 33 TS names have no docs (§3).

The user asked for `docs/` **separated by Diátaxis type** and for a README that serves someone arriving for the first
time: what Oly is, how to install it, a minimal example, and links into `docs/` **[user decision]**.

Today a concept page carries contract, API and patterns together, by the explicit rule of `guide-style.md:81` ("a
concept page carries the contract; a client page carries the API"). TS already follows the second half of that rule; C++
is the only language whose API sits inside the concept page. This design extends the rule to C++ and to the rest of
`docs/`: each concept page splits into **explanation** (the contract: deferred × direct, rounds, scope, validity),
**reference per language** (the API) and **how-to** (the patterns). Files then follow the axis of change: a new
signature changes only `reference/`; a change of order or deadline changes only `explanation/`. That last claim is an
**[inference]** about the change history, not a measurement; §10 sets the criterion that tests it.

## 2. Goals, non-goals, anti-goals

**Goals**

- Every page under `docs/` is one Diátaxis type. Redoing the audit's §2 classification on the new tree marks no page
  "Mixed".
- Every **verified** audit finding is closed, or deferred with a written reason (§10).
- Each fact has one home; every other mention is one line and a link (D1–D18).
- The README answers, in order: what Oly is, what it does, how to install, how to create a game, what the code looks
  like, where to go next.
- A link checker in the repository keeps relative links and anchors valid on every pull request (§8).

**Non-goals**

- Code changes. Moving helpers into `detail/` (A10) and fixing the header comment `engine_api.h:53-55` are code
  decisions, outside this project.
- Publishing the docs as a site.
- Rewriting the content of `docs/superpowers/` specs and plans: they stay as the historical record, with one status
  README and the two broken links fixed.

**Anti-goals**

- A public API change ends up touching more doc files than it does today. That is the abandon criterion of §10.
- The README example drifts from the template again: A1 and A6 are exactly that failure.
- A migration step leaves a broken link or an old path behind, even for one commit.
- Game names in the docs (the game-agnostic rule of `guide-style.md:51-56` keeps holding).

## 3. Constraints

- English, simple words, active voice, glossary names only (`CLAUDE.md`). New terms enter `reference/glossary.md` first.
- Game-agnostic examples: `<game>`, `Player`, `Position` (`guide-style.md:51-56`).
- Every command, message and signature quoted in the docs is copied from the code, not paraphrased.
- Each migration step is one commit (or one commit per theme in step 2) that leaves the link checker at 0 broken and a
  grep for the old paths at 0 results outside `docs/superpowers/` (whose specs and plans name the old paths as plain
  text, about 380 times, and stay unchanged).
- The toolchain runs only through `./spur` (`CLAUDE.md`).
- `docs/superpowers/` stays at its path: it is the default path of the spec and plan skills, and two agents and one
  skill cite it (`.claude/agents/perf-reviewer.md:19`, `.claude/agents/spec-conformance-reviewer.md:21-26`,
  `.claude/skills/bench-compare/SKILL.md:32`).

## 4. Approaches

| Criterion | **Type at the top** (chosen) | Audience at the top (`guide/` and `engine/`, Diátaxis inside) |
|---|---|---|
| Matches "separated by type" | yes | partially |
| User × contributor separation | by subfolder (`contributor/`, `explanation/architecture/`) | by root folder, stronger |
| Pages to keep per public API change | C++ reference + TS reference (+ how-to if the pattern changes) | the same split, one level down |
| Links to rewrite in the migration | all | all except those in `docs/architecture/` |
| Finding "how do I X" without knowing the language | one `how-to/` | two `how-to/` |

**Chosen: type at the top, with the audience as a subfolder only where both audiences meet** **[user decision]**.

**Rejected: audience at the top.** It is the strongest form of the alternative: it keeps today's wall between guide and
architecture (`guide-style.md:3`), and a game user never meets a contributor page. Rejected because the request is to
separate by type, and the audience subfolders solve the mixing without a second tree level.

**Fallback: concept + client page per theme** (today's split of each concept page, with only the audit's fixes). It is
what step 2 falls back to if the abandon criterion of §10 fires; the type-based tree of step 1 stays (§10).

**Accepted cost.** A feature now has three homes instead of two (explanation, C++ reference, TS reference), and
navigation has to link across types.

**When to revisit.** If contributor docs grow to dominate `reference/` and `how-to/` (more than one active maintainer,
for example), move to audience at the top. That change only renames folders: the mapping of §6 still holds.

## 5. Decisions closed in the conversation

| # | Decision | Outcome |
|---|---|---|
| 1 | What is the C++ public surface (A10)? Today it is everything outside `detail/`. | **Narrow the rule to what a game uses** **[user decision]**. `docs-style.md` names the public surface as the names `reference/cpp/` documents, and lists the §3.3 groups by name as "internal, pending a code decision (move to `detail/`)". A reviewer can then tell an omission from an exclusion. `core.md` becomes one line in `explanation/architecture/README.md`; there is no `reference/cpp/core.md`. |
| 2 | Cheat sheet: delete, or keep as an index? | **Delete** **[user decision]**. Signatures move to `reference/`; lookup across languages stays in `reference/glossary.md`. |
| 3 | Does `docs/superpowers/` stay? | **Stays** **[user decision]**, with a new `docs/superpowers/README.md` (§7). |
| 4 | Language of the README example | **C++ from `templates/game/common/`** **[user decision]**: it holds in both environments. TS would hold only on the web and needs more setup (`loadRuntime`, host). |
| 5 | Do benchmarks move to `reference/benchmarks/`? | **Yes** **[user decision]**, with the data files. The cost is four paths in tools: `tools/run_ladder.sh:23`, `.claude/skills/bench-compare/`, `.claude/agents/perf-reviewer.md`. |
| 6 | Does the link checker enter the repository? | **Yes, once the files have moved** (the source plan's step 2, step 1 in §10) **[user decision]**. How: §8. |
| 7 | Do `explanation/concepts/` pages show code, and in which language? | **Prose, with a short C++ excerpt only where the contract has to be seen** (for example, the deferred `destroy`), each followed by a link to the TS equivalent in `reference/typescript/` **[user decision]**. It keeps today's "C++ first" convention. A signature change may still touch those few excerpts; the abandon criterion of §10 counts them. |
| 8 | Does the narrow public surface of decision 1 hold beyond C++? | **Yes, on every surface** **[user decision]**. The §3.3 groups of TS (layout and view types, the `@oly/web-host` test hooks and injection parameters), CMake and `toolchain.env` (the engine-level options) and `oly` (the internal variables and `version-at-least`) join the exclusion list of `docs-style.md`. The engine-level CMake options go to `reference/contributor/commands.md`, not to `reference/cmake.md`, which keeps only what a game sets. |

## 6. The new tree

```
docs/
  README.md                         index: type × audience table, reading paths, conventions
  tutorials/                        learn by doing (game user)
    first-web-game.md
    first-desktop-game.md
  how-to/                           solve a known task
    gameplay-recipes.md             gameplay recipes in C++
    typescript/
      gameplay-recipes.md           the same recipes in TS
    test-a-web-game.md
    test-a-desktop-game.md
    drive-the-loop-yourself.md      own host: frame, time and input delivered by the game
    use-a-plugin.md
    write-a-plugin.md
    debug-the-world.md
    keep-the-hot-path-fast.md       C++ and the WASM↔TS boundary
    fix-common-problems.md          silent problems + rebuild checklist
    export-your-own-c-functions.md
    upgrade-the-engine.md
    build-with-an-engine-checkout.md
    set-up-game-ci.md
    contributor/
      set-up-the-toolchain.md       Git Bash + Docker, ./spur images, build and tests
      run-benchmarks.md
      cut-a-release.md
  reference/                        look up a fact
    cli.md                          the oly command: install, commands, options, Docker, local mode, variables
    game-template.md                what `oly new` creates, per environment
    errors.md                       messages by origin: C++, TS, oly
    cmake.md                        oly_add_game, oly_add_plugin, OLY_* options
    c-api.md                        functions with signatures, types and constants
    native-host.md                  Window, WindowConfig, run, what the host does with the window
    glossary.md                     the same concept in C++, C API and TS
    limits.md                       out of scope and limitations (single source)
    cpp/                            C++ API by theme
      schema.md  world-and-entities.md  queries.md  engine-and-systems.md
      events.md  input.md  schedule.md  states.md  plugins.md  transforms.md
    typescript/                     TS API by theme, mirror of cpp/
      README.md                     packages, errors, map
      schema.md  world-and-entities.md  queries.md  engine-and-systems.md
      events.md  input.md  schedule.md  states.md
      web-host.md                   runInBrowser, attachBrowserInput, moduleFactoryFromUrl
    contributor/
      commands.md                   ./spur tasks and presets
      code-style.md
      docs-style.md                 replaces guide-style.md, covers all of docs/
      repository.md
      boundary-rules.md             the boundary lint rules
    benchmarks/                     results and targets (pages + JSON/TXT data)
      README.md
      bridge/README.md
      ecs/  wasm/  bridge/*.json
  explanation/                      understand why
    concepts/                       the contract a game relies on (game user)
      README.md                     reading order
      environments-and-hosts.md     core, environment, host, client, how a game is assembled
      schema.md  world-and-entities.md  queries.md  engine-and-systems.md
      events.md  input.md  schedule.md  states.md  performance.md
      plugins.md  transforms.md
      web-boundary.md               why cross by block; how to split work C++ × TS
    architecture/                   how the engine is built (contributor)
      README.md                     overview
      modules-and-boundaries.md  ecs.md  app.md  input.md  capi.md
      schema-and-codegen.md  ts-packages.md  plugins.md  publishing.md
      build-and-tests.md  decisions.md
  superpowers/                      process history, outside Diátaxis (unchanged + a status README)
    README.md
    specs/  plans/
```

Count: 53 pages today (outside `superpowers/`), 82 after. The growth comes from splitting the 11 concept pages and from
6 references that do not exist today. All names in the tree are **[author decision]**.

**Navigation rules** (they go into `docs-style.md`):
- `explanation/concepts/` keeps today's order: schema → … → plugins → transforms.
- `reference/cpp/` and `reference/typescript/` use the same file names and the same section titles.
- Each explanation page ends with links to the theme's reference in each language; each reference page links back to
  its explanation.
- Explanation pages are prose; a C++ excerpt appears only where the contract has to be seen, with a link to the TS
  equivalent (decision 7).

## 7. Mapping: current file → destination

Actions: **move** (`git mv`, content unchanged), **split** (the sections go to more than one destination), **merge**
(goes into another file), **delete** (removed after the merge, or because it is covered elsewhere).

### 7.1 `docs/guide/` — concept pages (all: **split**)

| Current | → `explanation/concepts/` | → `reference/cpp/` | → `how-to/` | Fix on the way |
|---|---|---|---|---|
| `concepts/schema.md` | the five kinds of entry (`:7`), order and memory layout (`:83-113`), types from plugins (`:154`), use in several places (`:166`) | what the build generates (`:114-153`) | — | A7. Field types and name rules (`:45-82`) are **deleted** and become a link to `engine/schema/format/README.md` (D10). |
| `concepts/world-and-entities.md` | where the `World` comes from (`:7`), handles (`:94`), what stays valid (`:119`), structural changes and the loop (`:123`), implied components (`:207`), when observers fire | create, read, write, add, remove and destroy (`:26-93`), `CommandBuffer` (`:136`), observer API (`:162`) | `debug-the-world.md` (`:222`) | A2: separate direct from deferred behaviour. Add `CommandBuffer::empty/clear/commandCount` and `Entity.index/.generation` (§3.2). |
| `concepts/queries.md` | rules during iteration (`:65`) | the query spec (`:7`), iterate (`:41`), `single`/`trySingle` (`:80`) | patterns (`:117-218`) → `gameplay-recipes.md` | — |
| `concepts/engine-and-systems.md` | the loop (`:26`), phases and bands (`:50`), structural changes (`:152`), time and interpolation (`:179`), pause (`:218`), hidden window (`:266`), a system that throws (`:292`) | create the engine (`:7`), register (`:76`), by-name macros (`:110`), system context (`:133`) | `drive-the-loop-yourself.md` (`:272`) | §3.2: `Engine::emit` from the host, `systemId()`, `setSystemStateMask` with a mask. |
| `concepts/events.md` | batches, direct change, duplicates, order, rounds, when it dispatches (`:50-141`), events × other tools (`:164`) | declare, emit, handle (`:9`), restrict to states (`:142`) | — | §3.2: `eventType()`, `handlerId()`, `eventData()`. |
| `concepts/input.md` | the three action kinds (`:7`), where input comes from (`:112`) | bind buttons (`:24`), read actions and the 2D axis (`:48-111`) | rebind (`:121`) → merge into the "key rebinding" recipe; deliver input yourself (`:144`) → `drive-the-loop-yourself.md` | §3.2: `ActionKind`, `ActionMap::actionCount/kind/name`. |
| `concepts/schedule.md` | pause, states and recovery (`:128`) | `after`/`every`, task context, cancel, payloads, discard (`:7-127`) | — | §3.2: `Scheduler::schedule(...)`, `pendingCount`, payload alignment. |
| `concepts/states.md` | scope and cleanup (`:109`), `persist` (`:127`) | declare, switch, run only in some states, hooks (`:7-108`) | menu → playing → game over pattern (`:143`) → `gameplay-recipes.md` | §3.2: `declareState`, `stateCount`, `hookId()`. |
| `concepts/performance.md` | cost model: archetype churn (`:9`), fragmentation (`:44`), debug × release (`:58`) and `architecture/performance.md:23` | — | split components, reuse queries, cheap render, measure (`:22-57`) → `keep-the-hot-path-fast.md` | — |
| `concepts/plugins.md` | lifetime and ownership (`:136`), run after the game's systems (`:146`) | reach an installed plugin (`:36`) | turn on (`:7`) → `use-a-plugin.md`; write and rules (`:48-135`, `:154`) → `write-a-plugin.md` | Close the navigation (audit §5.1). Include `oly/app/system.hpp`, not `context.hpp` (`:108`). |
| `concepts/transforms.md` | when `WorldTransform` is up to date (`:34`), parent and children (`:48`), cost (`:63`) | turn on (`:7`), the components (`:22`) | — | Wording of `:20` (audit §4). The implication kept in a `World` that outlives the `Engine` (audit §3 note). |

### 7.2 `docs/guide/` — the other pages

| Current | Action | Destination(s) | Fix on the way |
|---|---|---|---|
| `README.md` | split | "What you can build" (`:7-20`) → root README. The map (`:22-31`) → `explanation/concepts/environments-and-hosts.md`. Paths and tables (`:33-82`) and conventions (`:134`) → `docs/README.md`. "The big picture" (`:84-132`) → **delete** (replaced by the README example and the tutorials). | Complete the reading paths (audit §5.1). |
| `cli.md` | split | "What you install", commands, Docker, local mode and variables (`:7-30`, `:48-99`, `:146` table, `:168`) → `reference/cli.md`. "Make a game" (`:31`) → both tutorials. The game repository (`:100`) → `reference/game-template.md`. Upgrade (`:134`) → `how-to/upgrade-the-engine.md`. CI (`:142`) → `how-to/set-up-game-ci.md`. Engine checkout (`:146`) → `how-to/build-with-an-engine-checkout.md`. Errors (`:178`) → `reference/errors.md`. | §3.2: `oly sync`, `oly platform`. D7: the local mode table lives only here. D16. |
| `cheat-sheet.md` | merge + delete | C++ sections → `reference/cpp/<theme>.md`. C API → `reference/c-api.md`. TS → `reference/typescript/<theme>.md`. | `Key::W` unqualified (`:106,113`), incomplete contexts (`:86`), `install` in the `private` section (`:188`). D11, D12. |
| `recipes.md` | move | `how-to/gameplay-recipes.md`; receives the query and state patterns. | `${nativeSources}` (`:692`). |
| `troubleshooting.md` | split | Message tables (`:7-97`) → `reference/errors.md`, split into C++, TS and `oly`. Silent problems and checklist (`:98-120`) → `how-to/fix-common-problems.md`. "What Oly does not do" (`:121`) → **delete** (link to `reference/limits.md`, D6). | A3, A8. |
| `environments/web/getting-started.md` | split | `:7-34` → `tutorials/first-web-game.md`. How a game is assembled (`:35`) → `explanation/concepts/environments-and-hosts.md`. File by file (`:48-178`) → `reference/game-template.md` (web). C++ × client split (`:180`) → `explanation/concepts/web-boundary.md`. Own C functions (`:201`) → `how-to/export-your-own-c-functions.md`. Inside the engine repository (`:205`) → **delete** (covered by `reference/contributor/commands.md`). | Align with the template. |
| `environments/native/getting-started.md` | split | `:9-34` and the smallest game (`:71-121`) → `tutorials/first-desktop-game.md`. What `oly new` created (`:35`) → `reference/game-template.md` (native). Data for systems and states (`:122`) → `reference/cpp/engine-and-systems.md`. Drawing (`:150`) → `explanation/concepts/environments-and-hosts.md`. The host and the window (`:154`) → `reference/native-host.md`. Tests (`:169`) → `how-to/test-a-desktop-game.md`. `:254` → **delete**. | A6: the smallest game becomes the template's `main.cpp`. §3.2: `WindowConfig`. |
| `environments/web/testing.md` | move | `how-to/test-a-web-game.md`; the opening explanation (`:7-24`) stays as short context. | A1: the template's `installGame` signature. |
| `environments/web/boundary.md` | split | The why (intro, `:29`) → `explanation/concepts/web-boundary.md`. `eachBlock`, `raw()`, batches, measure, release (`:9`, `:37-61`) → `how-to/keep-the-hot-path-fast.md` (TS section). | A9: anchor `#wasm-and-typescript`. |
| `environments/web/typescript/README.md` | move | `reference/typescript/README.md`. | D17. |
| `environments/web/typescript/{schema,world-and-entities,queries,engine-and-systems,events,input,schedule,states}.md` | move | `reference/typescript/<same name>.md`. Repeated contract rules become a link to the explanation. | `world-and-entities.md:23,101`; `states.md:78`. §3.2 TS. |
| `environments/web/typescript/recipes.md` | move | `how-to/typescript/gameplay-recipes.md`. | `:334` (`installGame`). |

### 7.3 `docs/architecture/`

| Current | Action | Destination(s) | Fix on the way |
|---|---|---|---|
| `README.md` | split | Overview (`:1-38`) → `explanation/architecture/README.md`. Index (`:40-63`) → `docs/README.md`. "Reference documents" (`:65-80`) → `docs/superpowers/README.md`. | Drop "source of truth"; add the publishing spec and this design. |
| `app.md`, `ecs.md`, `input.md`, `schema-and-codegen.md`, `decisions.md` | move | `explanation/architecture/<same name>.md` | D9: `ecs.md` becomes the single source of "how the ECS works". |
| `capi.md` | split | Function table (`:5-15`) → `reference/c-api.md` (with signatures and constants). Design points and the invoker → `explanation/architecture/capi.md`. | D12; §3.2 C API. |
| `core.md` | merge | One line in `explanation/architecture/README.md` (decision 1). | — |
| `glossary.md` | move | `reference/glossary.md` | `:62` ("create" does not exist as a name): restrict to entities. |
| `limits.md` | move | `reference/limits.md` (single source of D6) | — |
| `modules-and-boundaries.md` | split | Lint rules (`:49-68`) → `reference/contributor/boundary-rules.md`. The rest → `explanation/architecture/`. Receives the boundary rules (D14). | `:58`: the `games/` folder no longer exists. |
| `performance.md` | merge | Targets and status → `reference/benchmarks/README.md`. The consequence for a game (`:23`) → `explanation/concepts/performance.md`. | — |
| `plugins.md` | split | "Build" (`:67-118`) → `reference/cmake.md`. Boundaries (`:155-168`) → `explanation/architecture/modules-and-boundaries.md`. The rest → `explanation/architecture/plugins.md`. | D14, D18. |
| `publishing.md` | split | Versioning, tag last, launcher, when Docker is used, `Debug` builds, subproject, consumer test → `explanation/architecture/publishing.md`. Local mode table (`:89-95`) → **delete** (link to `reference/cli.md`, D7). Release steps → `how-to/contributor/cut-a-release.md`. | — |
| `repository.md` | move | `reference/contributor/repository.md` | — |
| `build-and-tests.md` | split | Service and preset tables → `reference/contributor/commands.md`. Organization and why → `explanation/architecture/build-and-tests.md` (home of the `clang-format` hook, D4). | D1. |
| `ts-packages.md` | split | What each package exports → `reference/typescript/README.md`. Internal rules → `explanation/architecture/ts-packages.md`. | `:3` esbuild; `:10` `createEngine`. |

### 7.4 `docs/development/`, `docs/benchmarks/`, `docs/superpowers/`

| Current | Action | Destination(s) | Fix on the way |
|---|---|---|---|
| `development/commands.md` | split | Tasks → `reference/contributor/commands.md`. "Cutting a release" (`:82-90`) → `how-to/contributor/cut-a-release.md`. Toolchain intro (`:3-10`) → `how-to/contributor/set-up-the-toolchain.md`. Benchmarks (`:92`) → `how-to/contributor/run-benchmarks.md`. | Audit §3.1: the `configure` task. D1, D2. |
| `development/code-style.md` | move | `reference/contributor/code-style.md` | D3: single source of the code rules. |
| `development/guide-style.md` | split + rewrite | `reference/contributor/docs-style.md`, rewritten for the new tree: types, navigation, the cpp/ts mirror, what to update per kind of change, and the public surface of decision 1. | A10. |
| `benchmarks/README.md`, `benchmarks/bridge/README.md` | move + split | `reference/benchmarks/` (with the data). The commands → `how-to/contributor/run-benchmarks.md`. | — |
| `benchmarks/**/*.json`, `*.txt` | move | `reference/benchmarks/` (same structure) | Update `tools/run_ladder.sh:23`, `bench-compare`, `perf-reviewer`. |
| `superpowers/specs/*`, `superpowers/plans/*` | stay | Unchanged, plus a new `superpowers/README.md`: historical record; the current state is in `explanation/architecture/`; the ECS spec (`R-*`, `I-*`, `NF-*`) remains the requirement in force. | The two broken links in the plans (`phase3-schema.md:1953`, `phase5-docs.md:221`): one line each. |

### 7.5 Outside `docs/`

| Current | Action | Notes |
|---|---|---|
| `README.md` | rewrite | Sections in §9. What leaves: "What already exists" → `reference/contributor/repository.md`; "How the ECS works" → `explanation/architecture/ecs.md`; the ECS-only and TS examples → `reference/cpp/` and `tutorials/first-web-game.md`; "Build and test" and "Benchmarks" → `how-to/contributor/`. |
| `CONTRIBUTING.md` | reduce | Stays as the entry door: what is welcome, workflow, commits, one link per theme. The duplication of D1, D2, D3, D5, D13, D14 and D15 leaves. The PR checklist lives only in the PR template. |
| `CLAUDE.md` | update | New paths. `@docs/reference/contributor/code-style.md`. The rule "updates `docs/guide/`" points to `docs-style.md`. The D3 duplication with the imported file leaves. |
| `.github/pull_request_template.md` | update | Unified checklist (D5); `docs/` path. |
| `.github/ISSUE_TEMPLATE/feature_request.yml`, `config.yml`, `bug_report.yml` | update | Paths of `limits` and `decisions`. Contact link to SUPPORT. A neutral `render:` for the reproduction. |
| `engine/schema/format/README.md` | stays | Normative reference next to the parser; `docs/README.md` and `explanation/concepts/schema.md` link to it (D10). |
| `.claude/skills/add-test-case/SKILL.md` | stays | Linked from `how-to/contributor/set-up-the-toolchain.md` as the how-to for "add a test". Paths updated (`:130,143,161`). |
| `.claude/agents/perf-reviewer.md`, `spec-conformance-reviewer.md`, `.claude/skills/bench-compare/SKILL.md`, `tools/run_ladder.sh` | update paths | `docs/architecture/ecs.md`, `docs/development/code-style.md`, `docs/benchmarks/`. References to `docs/superpowers/` do not change. |

### 7.6 New files (no current source)

| File | Content source |
|---|---|
| `docs/README.md` | the guide's and the architecture's indexes + the type × audience table |
| `reference/cmake.md` | `architecture/plugins.md:67-118`, `cmake/OlyGame.cmake`, `cmake/OlyOptions.cmake` (§3.2: `OLY_BUILD_NATIVE_HOST`, `OLY_BUILD_TESTS`, `OLY_ENABLE_ASSERTS`); only what a game sets (decision 8) |
| `reference/c-api.md` | `architecture/capi.md:5-15` + the cheat sheet + `c_api.h`/`engine_api.h` (the 17 undocumented constants and types) |
| `reference/native-host.md` | `native/getting-started.md:154` + `native_host/*.hpp` |
| `reference/typescript/web-host.md` | `@oly/web-host/src/index.ts` + the host part of `web/getting-started.md` |
| `reference/game-template.md` | both "What `oly new` created" sections + `cli.md:100` |
| `reference/errors.md` | `troubleshooting.md` + `cli.md:178`, with the missing C++ messages (A3). **[author decision]** Scope: the messages the docs list today, each labelled by origin (C++, TS, `oly`) and given its text in every language that has it, plus the A3 messages. Not every `OLY_ASSERT` (about 95 outside tests): a new message gets a row only when a game reaches it through ordinary misuse of the public API. |
| `explanation/concepts/README.md`, `environments-and-hosts.md`, `web-boundary.md` | see §7.1 and §7.2 |
| `how-to/contributor/set-up-the-toolchain.md`, `run-benchmarks.md`, `cut-a-release.md` | see §7.4 |
| `docs/superpowers/README.md` | status banner and the table of specs |
| `SUPPORT.md` (root) | where to ask: issues, Discussions if enabled, what is not support |
| `CHANGELOG.md` (root) | one section per `0.x` version, with "Breaking" and migration (A4) |
| `.github/CODEOWNERS` | optional (single maintainer) |
| `tools/check_links.py`, `tools/test_check_links.py` | §8 |

## 8. The link checker

**[author decision]** Port the audit's Node checker to Python as `tools/check_links.py`, with a self-test
`tools/test_check_links.py`, and register both as ctest tests next to the boundary lint (`CMakeLists.txt:103-111`):
`oly.lint.links` and `oly.lint.links_selftest`. Why Python and ctest instead of a new `./spur` task and CI job: the
boundary lint already runs this way, in the `dev` image, inside `./spur test`, which the `native` CI job runs on every
pull request (`.github/workflows/ci.yml`). The checker then needs no new image, task or job.

Behaviour (the audit's §5 method):
- Walks every `.md` under the root except `.git`, `node_modules`, `build*` and `.superpowers`.
- Ignores fenced code blocks and inline code spans.
- Resolves relative paths; for a `.md` target with an anchor, builds the target's anchors with GitHub's rule
  (lowercase, punctuation removed, spaces → `-`, a `-N` suffix for repeats, `<a name|id>` too).
- Skips URLs with a scheme; does not fetch external links.
- Prints `file:line | target | reason` per broken link and exits non-zero when any exists.

It lands at the end of step 1 (§10), after the two broken links in `docs/superpowers/plans/` are fixed, so it covers the
whole repository with no exclusion list.

**[author decision]** The README example check: a test `oly.lint.readme_example` compares each fenced block of the
README's "Minimal example" section with the template file it names, byte for byte after trimming. It is the guard
against the A1/A6 failure for the one example a newcomer copies first.

## 9. The new README

Audience: someone arriving for the first time. Each section answers one question; the rest lives in `docs/`.

| # | Section | Answers | Source | Size |
|---|---|---|---|---|
| 1 | `# Oly` + opening paragraph | What is it? A 2D engine in C++20 on an archetype ECS, running in the browser (WASM + TS) and on the desktop. A study project with measured costs, linked to the benchmarks. A status line: `0.x`, the API may change, packages only in releases. | `README.md:3-7`, D8 | 3-5 lines |
| 2 | `## Features` | What can it do? 6-8 items. One line "Oly does not draw, play audio or do physics" with a link to `reference/limits.md`. | `guide/README.md:9-20` | 8-10 lines |
| 3 | `## Install` | What do I need and how do I install? Requirements per OS (Linux; Windows with Git Bash), one line per environment, the `curl … install.sh \| sh` and `oly doctor`. The full table stays in `reference/cli.md`. | `cli.md:7-29` | 10-15 lines |
| 4 | `## Create a game` | How do I see it work? `oly new <game> --environment web\|native`, `cd`, `oly test`, `oly run`. | `cli.md:31-39` | 5-8 lines |
| 5 | `## Minimal example` | What does the code look like? The `game.oly` and the `movement.cpp` system of `templates/game/common/`: they hold in both environments and are exactly what `oly new` created. One sentence says where each file lives. A link to the TS equivalent in the web tutorial. | `templates/game/common/game.oly`, `common/native/movement.cpp` | ~25 lines |
| 6 | `## Documentation` | Where do I go next? A table with 4 rows, one per type, each with a sentence and a link: Tutorials, How-to guides, Reference, Explanation. Plus a row "Working on the engine" → `how-to/contributor/` and `explanation/architecture/`. | `docs/README.md` | 7 lines |
| 7 | `## Contributing` | How do I help? One sentence + links to CONTRIBUTING, CODE_OF_CONDUCT, SECURITY and SUPPORT. | `README.md:147-149` | 2-3 lines |
| 8 | `## License` | MIT. | — | 1 line |

The minimal example is a literal copy of the template, with no game-specific names (`guide-style.md:51-56`), checked by
the test of §8.

## 10. Migration order

Each step is a commit (step 2: one per theme) that leaves the links working: the checker at 0 broken (a ctest from the end
of step 1; before that, the audit's script) and a grep for the old paths at 0 results outside `docs/superpowers/` (§3).

1. **Skeleton + `git mv` of the files that only move** (§7.2–§7.4, action "move") + every reference of §7.5. No text
   changes except links. The two broken links in `docs/superpowers/plans/` are fixed. The link checker and its
   self-test enter the repository (§8).
2. **Split of the concept pages**, one theme per commit (§7.1), with the fixes of the "Fix on the way" column (A2, A7).
   TS reference section titles mirror the C++ reference.
3. **Tutorials + `reference/game-template.md`**, aligned with the template (A1, A6).
4. **`cli.md`, `troubleshooting.md`, cheat sheet**: split and deleted; `docs/guide/` stops existing.
5. **New references** (§7.6) with the audit's §3.2 items.
6. **`docs-style.md`** (replaces `guide-style.md`), `CLAUDE.md`, `CONTRIBUTING.md`, the GitHub templates.
7. **New README** (§9) and the README example test (§8).
8. **SUPPORT, CHANGELOG, CODEOWNERS.**

(The source plan numbered "close the open decisions" as step 1; §5 closed them, so the numbering here starts at the
first change to the tree.)

**Success criterion:**
- No page is marked "Mixed" when the audit's §2 classification is redone on the new tree.
- Every verified audit finding is closed, or deferred with a reason in the final step's commit message or in
  `docs/superpowers/README.md`.

**Abandon criterion** **[author decision on how to measure it]**. Step 2 starts with one theme as a pilot. With the pilot
split, replay two past public API changes of that theme from the git history on both trees and count the doc files a
correct update has to touch:
- The baseline is today's tree, following the table of `guide-style.md` for that kind of change (concept page, client
  page, cheat sheet, `capi.md`).
- The glossary counts on both sides only when the change adds a name.
- If the new tree needs more files than the baseline for either change, stop.

The fallback keeps step 1: the tree stays type-based. The remaining concept pages move whole into
`explanation/concepts/` with their C++ API, as they carry it today; `reference/cpp/` holds only what step 5 adds; the TS
reference stays as moved in step 1. The pilot theme is merged back the same way.

## 11. What is left to the builder

- How to group the steps into plans and commits beyond the rule "each commit leaves the links working".
- The exact wording of every page, within `docs-style.md` and the constraints of §3.
- The internal design of `check_links.py` and of the README example test, within the behaviour of §8.
- Which audit **suspected** items to confirm and fix on the way, and which to record as deferred.
- Whether `CODEOWNERS` is worth adding (optional).
