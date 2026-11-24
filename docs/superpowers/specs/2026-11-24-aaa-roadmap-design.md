# Oly — roadmap from a study engine to a production engine — design

Date: 2026-11-24. Written on `feature/engine-AAA` at `4d4aa4b`. A roadmap: it fixes the phases, their order, their
gates and the rules every phase keeps. Each phase gets its own brainstorm, spec and plan, one at a time.

Marking: **[user decision]** was decided in the conversation; **[author decision]** was my choice while writing, which
the user may review; **[fact]** can be checked in the repository; **[hypothesis]** needs a measurement.

Inputs, kept outside the repository by the user: an architecture review of Oly at `ec433d7` ("o que falta
arquiteturalmente para uma engine AAA", called *the review* below) and the design of the validation game *Enxame*
("Enxame — jogo 2D para validar a Oly"). Their findings, the acceptance matrix and the scenarios are restated here
**[author decision]**, so no phase depends on a file outside the repository. The Enxame's mechanics (what an enemy,
a belt or an assembler does) are not: phase 0 defines them.

## 1. Intent

Oly stops being a study project. It becomes a production engine for indie 2D games, and 3D is expected later
**[user decision]**. "AAA" in this roadmap means production quality and a plugin model with no build-order rules, not
the scale of a large 3D studio.

C++ on the `native` environment is the first-class environment **[user decision]**. The `web` environment (WASM and
the TypeScript client) stays supported but may lag behind a phase.

Why now: the API is still `0.x` and no game outside the repository depends on it. The review found five decisions
in the core that block growth (single thread with undeclared access, unstable type identity with no reflection,
plugins resolved at compile time, no resources or assets, no render architecture). Each one costs more to change with
every system written against the current API.

## 2. Goals, non-goals, anti-goals

Goals:

- Every subsystem has an extension point a plugin can use, and no rule depends on install or registration order.
- The engine runs the Enxame at its target scale: up to 100 000 enemies, 10 000 projectiles and 50 000 belt items at
  60 Hz, with save, load, deterministic replay, an in-game inspector and a mod declared by the game.
- Each phase closes only when its rows of the acceptance matrix (§6) pass in the Enxame.

Non-goals:

- An own RHI over Vulkan, D3D12 and Metal; console SDKs; 3D render. Phase 6 draws through SDL3's GPU API, which the
  engine already links and which also serves a later 3D render **[author decision]**.
- Networking. Deterministic replay (phase 3) is its prerequisite and may become the next validation game.
- An editor beyond the in-game inspector, which validates reflection without building an editor.
- Shipping the Enxame. Its art is placeholder; it exists to stress the engine.

Anti-goals (a phase that does one of these fails even if it works):

- A type or rule that only makes sense in 2D enters the core (`core`, `ecs`, `input`, `app`, `capi`).
- Anything in the engine (CMake, `Spurfile`, presets, the release, the lint) depends on `games/`.
- The Enxame grows mechanics that point to no row of the acceptance matrix.

## 3. Constraints

- **[fact]** Toolchain only in Docker through `./spur` (`CLAUDE.md`); every phase's verification names a `./spur` task
  or an `oly` task that reaches the code it checks.
- **[fact]** Game builds are `Debug` (`tools/oly/Spurfile:135`, `:261`; `decisions.md`, row "Game builds are
  `Debug`"). The Enxame scenarios must be measured in a release build, so phase 0 needs a release path for a game
  (§5, phase 0).
- **[fact]** WASM threads need `SharedArrayBuffer`, which a browser only enables when the page is served with the
  COOP and COEP headers, and an Emscripten pthreads build.
- **[fact]** `docs/superpowers/` is history: specs and plans are not rewritten (`docs/superpowers/README.md`). The ECS
  spec (`R-*`, `I-*`, `NF-*`) is the requirement in force; a phase that changes one of them says so in its own spec.
- **[fact]** Changes to `table`, `archetype`, `query*`, `command_buffer` or `world` keep `checkInvariants` covering
  them and go through the `spec-conformance-reviewer` and `perf-reviewer` agents (`CLAUDE.md`). Phases 3 and 4 live
  there.
- A public API change updates `docs/` and `CHANGELOG.md` `## Unreleased` in the same change (`CLAUDE.md`).

## 4. Approaches

**Order of the phases.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| A | The review's §7 order: declared access first, then reflection and plugins, jobs, resources and assets, render | the system signature breaks once, before games exist; parallelism comes early | **chosen [user decision]**: the Enxame needs 100 000 entities, so threads matter |
| B | The review's §8 order for indie 2D: reflection and plugins first, jobs and render later | unblocks plugins sooner; the system signature breaks later, after more systems exist | rejected |

**Where the validation game lives.**

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| A | `games/enxame/` in this repository, isolated, built with `oly` in local mode | one checkout; an API break and the game's migration sit side by side | **chosen [user decision]** |
| B | Its own repository | proves the third-party path, but each API break needs two coordinated changes | rejected |

The game is disposable **[user decision]**: no CI runs it, no lint guards it and `decisions.md` gains no row for it.
It is committed while validation lasts and removed when it ends **[author decision]**.

**Shape of the roadmap.** One roadmap with one phase per Enxame milestone, and a brainstorm, spec and plan per phase.
A single spec for everything was rejected: the performance targets of later phases depend on the baseline phase 0
measures. Starting phase 0 with no roadmap was rejected: the rules of §5 would have no written home.

## 5. The design

### Rules every phase keeps

1. **3D stays open.** No 2D-only type or rule in the core; dimension-specific work (physics, render, transforms) lives
   in plugins per dimension. `essentials3d`'s tests stay green in every phase.
2. **Native first, web keeps working.** A phase may change the C++ API without changing the C API and the TS client
   in the same phase. The web keeps building and passing its tests; what it lacks is written in `limits.md`. Each
   phase's spec says what the C API and TS get in that phase.
3. **One phase at a time.** Brainstorm, spec, plan, implementation. A phase closes when its rows of §6 pass in the
   Enxame, run locally.
4. **Ratios against phase 0.** A performance gate is a ratio against the phase 0 commit, both built and run side by
   side in the same session, as `docs/reference/benchmarks/README.md` already requires **[user decision]**. Absolute
   numbers (the step p99) are recorded, never gated, until a dedicated Linux machine without WSL2 exists. A ratio
   becomes a criterion only after phase 0 measures the baseline; until then it is a **[hypothesis]**.
5. **Instruments before gates.** Every scenario a gate uses (§7) is created in phase 0, in the repository, before any
   gate uses it, and runs there. It is recorded as "blocked" only when the current engine cannot express it: S4 (no
   stable ids) and S7 (no assets) **[author decision]**. A blocked scenario gets its baseline in the phase that
   unblocks it, before that phase changes anything else.
6. **The game stays small.** Every Enxame mechanic points to a row of §6. The playable reduced game has 2 000 enemies;
   each scenario sets its own N.
7. **Frozen workloads.** Phase 0 defines each scenario's workload and freezes it. A later phase that moves work into a
   plugin (`physics2d` in phase 1) keeps the same work; a scenario whose workload has to change is re-measured on the
   phase 0 commit first **[author decision]**.

### The Enxame

- `games/enxame/` is a game like one in its own repository: `oly.env`, `CMakeLists.txt` and its own build, driven by
  `tools/oly/oly` with `OLY_ENGINE_DIR` set to the repository root. The engine never names `games/` **[user
  decision]**.
- Its plugins split in two **[author decision]**: engine-level ones (`physics2d`, `camera2d`, `render2d`,
  `inspector`) go to `plugins/` when their phase builds them; game-level ones (`waves`, `flowfield`, `logistics`,
  `combat`, the mod `railgun`) stay in the game. Until phase 2 lets a game declare plugins, the game-level ones are
  plain systems.
- The engine's existing checks (`./spur format-check`, `oly.lint.boundaries`, `oly.lint.links`) cover `games/` like
  any tracked file; "no CI" means no CI builds or runs the game **[author decision]**. A release cut while the game
  exists carries `games/` in its tag, an accepted cost **[user decision]**.
- Fixed steps per tick, once phase 1 exists: `UpdateBefore` (wave clock, flow field), `Update` (horde, belts,
  assemblers), `Physics` (registered by `physics2d`), `UpdateAfter` (combat, then `railgun`), `Render` (extraction).

### Phases

The Enxame document names seven milestones but their list did not reach this design; the phases below are derived
from the review's §7 and the acceptance matrix **[author decision]**. The numbers are the default order; a phase whose
dependencies are met may move earlier, decided when the previous phase closes.

| Phase | Content | Depends on | Gate (rows of §6) |
|---|---|---|---|
| 0 — identity and baseline | rewrite the project's identity (`CLAUDE.md`, `README.md`, `docs/explanation/architecture/README.md`, `publishing.md`, `limits.md`); create `games/enxame/` reduced (2 000 enemies, no save, no mods); a release build path for the game, for measurement; scenarios S1–S8, headless, with a fixed seed, recorded input and the time fed to `frame` by the harness, with time per system; record the baseline | — | baseline of S1–S8 recorded |
| 1 — declared access and system sets | systems declare reads, writes and filters; system sets with `before`/`after` replace the order bands; phases registered by plugins; a constraint cycle is an error at registration; `physics2d` registers `Physics` and takes over the game's integration and separation systems, with no new work (rule 7) | 0 | 1, 4 |
| 2 — reflection and plugin extensibility | stable ids (hash of the qualified name, collisions caught by `oly-schema`); a reflection table generated by `oly-schema`; schema grammar with fixed arrays, enums, nested structs, strings or asset handles and 64-bit integers; runtime handles for actions, states, events and components; namespaces per plugin; plugins declared by the game; plugins with no `install` | 1 | 5, 6, 7, 8 |
| 3 — jobs, determinism, change detection | a job system with work stealing; one `CommandBuffer` per thread with a deterministic merge; a `World` that takes several read queries at once; change detection; time per job in the harness | 1 | 2, 3, 11 |
| 4 — ECS v1 limits | sparse storage for tags that toggle often; native relations (and `essentials` hierarchy on them); archetype removal | 2, 3 | 12, 13, 14 |
| 5 — resources and assets | typed resources in the `World`, in the conflict graph like components; services a plugin can expose or replace; assets with ref-counted handles, async loading, dependencies and hot reload | 2, 3 | 9, 10 |
| 6 — extraction and `render2d` | simulation to render snapshot, incremental through change detection; `render2d` over SDL3's GPU API; simulation of step N+1 alongside the render of step N | 3, 5 | 15 |
| 7 — release and tooling | shipping builds for players (asserts off, packaging), beyond phase 0's measurement build; a profiler timeline over the per-system and per-job times; MSVC and macOS | 3 | 16 |

The pending `Late` order band (`specs/2026-11-24-order-band-late-design.md`, `plans/2026-11-24-order-band-late.md`) is
suspended: phase 1 replaces the bands **[user decision]**. Each of the two files gets one status line saying so; no
other line of them changes.

## 6. Acceptance matrix

Restated from the Enxame design. Functional criteria hold as written. Performance criteria are ratios against the
phase 0 commit (rule 4); an absolute number below is recorded next to its ratio, not gated.

| # | Improvement | Enxame mechanic | Criterion |
|---|---|---|---|
| 1 | declared access | every system declares reads and writes | registration refuses two parallel systems that write the same component; no behavior change against phase 0 |
| 2 | job system | horde, projectiles and belts in parallel | S1 at 100 000 enemies: step p99 ratio against phase 0, target set after the baseline; speedup from 1 to 8 threads recorded; absolute p99 recorded against the 8 ms budget |
| 3 | parallel determinism | replay on 1 and N threads | `World` hash after 10 000 steps equal on 1 thread, N threads, `native` and `web` |
| 4 | system sets and phases | `physics2d` adds the `Physics` phase; `combat` runs after `physics2d` | order from `before`/`after`, independent of install order; a cycle is an error at registration |
| 5 | actions and states in plugins | `camera2d` declares `pan` and `zoom`; `waves` declares `intermission` and `assault` | ids resolved at run time; two games with different plugin orders work with the same plugin binary |
| 6 | namespaces and game plugins | mod `railgun`, declared by the game, with its own `Body` | `physics2d::Body` and `railgun::Body` coexist; the mod needs no engine change |
| 7 | stable ids and reflection | save, load, inspector | a save made by one build loads in a build with another plugin installed; the inspector edits any field with no per-component code |
| 8 | wider schema | inventory, recipes, seed | `Inventory { slots: [ItemStack; 16] }`, `enum Recipe`, `seed: u64` in the `.oly`, same layout in C++ and TS |
| 9 | resources | `FlowField`, `SpatialHash`, `WaveClock` | resources take part in the conflict graph like components |
| 10 | assets | atlas, tilemaps, chunks on demand | chunks load asynchronously without a step spike; an atlas hot-reloads without a restart |
| 11 | change detection | belts, tiles, inspector | "changed since the last step" without scanning the whole table |
| 12 | sparse storage | burning, slowed, stunned | S3: toggling a status on 10% of enemies per step moves no entity between archetypes |
| 13 | relations | turret → target, item → belt, vehicle → turrets | destroying the target clears the relation with no observer written by the game |
| 14 | archetype removal | random enemy modifier combinations | stable memory after 50 waves (S8) |
| 15 | extraction and render | `render2d` draws sprites and chunks | step N+1 simulates while step N renders; only changed chunks are re-sent |
| 16 | release and profiler | release build, timeline per system | every scenario runs in release; the profiler shows time per system and per job |

## 7. Scenarios

All headless, with a fixed seed, recorded input and the time fed to `frame` by the harness, as Oly's tests already do.
Measured in a release build, the phase's commit and the phase 0 commit side by side in the same session, median of 10
repetitions, each result recorded next to the engine commit, as `docs/reference/benchmarks/` does today.

| Scenario | Builds | Measures | Rows |
|---|---|---|---|
| S1 Horde | N enemies on a flow field, N = 1 000, 10 000, 100 000 | step p50 and p99; speedup 1 → 8 threads | 1, 2 |
| S2 Factory | 50 000 belt items, 500 assemblers | step time; allocations per step | 2, 13 |
| S3 Status | 10% of enemies change status each step | archetype transitions per step; flush time | 12 |
| S4 Save | the worlds of S1 and S2 saved and loaded | time and size; identical world after load | 7 |
| S5 Replay | 10 000 steps of recorded input | `World` hash on 1 thread, N threads, `native`, `web` | 3 |
| S6 Hierarchy | 1 000 vehicles with 4 turrets each | propagation per child, against 26–36 ns today | 13 |
| S7 Streaming | camera across the 2048 × 2048 map | worst step while chunks load; resident memory | 10 |
| S8 Waves | 50 waves of random modifier combinations | live archetypes and memory at the end | 14 |

**[hypothesis]** The 8 ms budget for the step leaves half of the 16.6 ms at 60 Hz to render and the OS. It is recorded,
not gated (rule 4); S1 in phase 0 says how much the single thread already delivers.

## 8. Risks, each with the phase that owns it

| Risk | Effect | Owner |
|---|---|---|
| Same `World` hash on `native` and `web` | GCC native code and Emscripten differ in FMA contraction and in libm (`sinf`, `cosf`), so a bit-equal hash needs at least `-ffp-contract=off` and engine-owned math functions | 3 |
| Threads on `web` | needs COOP/COEP and a pthreads build; until then the web runs the same scheduler on one thread | 3 |
| 64-bit integers in TS | `u64` needs `BigInt`, which Oly avoided on purpose; limit `u64` to fields off the hot path or expose two `u32` | 2 |
| Game builds are `Debug` | the baseline would measure assert-heavy code | 0 |
| Optimistic or pessimistic baseline | wrong targets | 0 (rule 4) |
| WSL2 and Docker noise | an absolute gate would pass or fail on noise (`BM_DirectAdds` has swung from 60 to 120 ns) | 0: gates are ratios (rule 4) |
| Rewriting the Enxame in phase 1 | the new system signature touches every system | 0: the reduced game stays small |

## 9. Left to the builder of each phase

- Phase 0: the mechanics behind each scenario (enemy movement and separation, belts, assemblers, vehicles, wave
  modifiers, the map and the flow field), kept to what the scenarios measure; how the harness builds and runs the
  phase 0 commit next to the current one; the release path for a game (a task of the game `Spurfile` or an option of the existing ones); where the
  baseline results live (`docs/reference/benchmarks/` or the game); which environments the reduced Enxame builds in
  (`native` first; `web` is needed by S5 only from phase 3); the exact wording of the identity rewrite.
- Every phase: the exact API, names and test cases, decided in its own brainstorm.
