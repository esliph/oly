# Oly — a `Late` order band between `Default` and `Last` — design

> **Status: suspended.** Phase 1 of the roadmap replaces the order bands with system sets: `2026-11-24-aaa-roadmap-design.md`.

Date: 2026-11-24. Written on `develop` at `e4f00d5`. Short design: gap M1 of the plugin gap list.

Marking: **[user decision]** was decided in the conversation; **[author decision]** was my choice during the design,
which the user may review; the rest is a direct consequence of the two.

## 1. Intent

The next plugin is `motion2d` **[user decision]**: `Velocity`, `AngularVelocity`, `Damping` and the integration of
`Position` and `Rotation`. Its integration must run after the game's systems of `Update` (which may write the
velocity) and before `propagateTransforms`, so `WorldTransform` reflects the movement in the same step.

Today no plugin can take that slot. The dimension plugin installs first and registers `propagateTransforms` in the
`Last` band of `Update`; a plugin installed after it registers later in the same band and runs after the
propagation. In the `Default` band, it runs before the game's systems, because plugins install before the game.

## 2. Goals, non-goals, anti-goals

Goals:

- A third band, `OrderBand::Late`, between `Default` and `Last`: inside a phase, `Default` → `Late` → `Last`;
  inside a band, registration order **[user decision: approach B]**.
- `motion2d` can register its integration in `Late` of `Update`, and `essentials2d`/`essentials3d` stay in `Last`.

Non-goals:

- The `motion2d` plugin itself: its own design, after this one.
- A band parameter in the C API and the TS client: they keep registering in `Default` **[author decision]**, as today.
- A general ordering mechanism (labels, before/after): rejected for now, see §3.
- A plugin that corrects positions in `UpdateAfter` (physics) and needs a second propagation: not covered here.

Anti-goals:

- A branch per system in `runPhase`, or a second flush per phase. The phase still applies its `CommandBuffer` once,
  after the `Last` band.

## 3. Approaches

| | Approach | Trade-off | Verdict |
|---|---|---|---|
| A | Convention only: `motion2d` in `Default` of `Update`, the game writes velocity in `UpdateBefore` | no engine change; a velocity written in `Update` or later moves the body one step late, a rule nothing checks | rejected: the user wants games free to write velocity in `Update` |
| B | `OrderBand::Late` between `Default` and `Last` | small public change (enum, docs, changelog); inside `Late`, order is install order | **chosen** |
| C | Labels with before/after, topologically sorted before the first frame | covers every case, physics included; the largest API (C++, C API, TS), cycle detection, no second consumer today | deferred until a physics plugin needs it |

Moving `propagateTransforms` to `Last` of `UpdateAfter` was dropped: it breaks the documented contract that
`UpdateAfter` (collision) reads the step's `WorldTransform`.

## 4. Design

- `oly/app/phase.hpp`: `enum class OrderBand : uint8_t { Default, Late, Last };` and its comment names the three
  bands. `OrderBandCount` stays derived from `Last`, so `phases_` gets one more vector per phase and `runPhase`,
  which already walks the bands in order, does not change.
- The band name `Late` **[author decision]**: reads as "after the default work, not the very end".
- Rule for plugins, in the docs: a plugin that must run after the game's systems of a phase and before the
  dimension plugin's propagation registers in `Late`; one that must read the propagated `WorldTransform` registers
  in `Last`. A `Late` system sees the field writes of `Default` but not its deferred structural changes, the same as
  `Last` today.
- Cost **[inference]**: one empty vector walked per phase per step; no per-system cost.

## 5. Documentation and changelog

Every page that names the two bands moves to three: `docs/reference/glossary.md` (order band row),
`docs/reference/cpp/engine-and-systems.md` (`Phase` and `OrderBand`, `addSystem`),
`docs/explanation/concepts/engine-and-systems.md` (band table),
`docs/explanation/architecture/app.md` and `docs/explanation/architecture/plugins.md` (order bands),
`docs/explanation/concepts/plugins.md`, `docs/explanation/architecture/decisions.md` (the row "only two bands").
`CHANGELOG.md`, `## Unreleased`, `### Migration`: the new `OrderBand::Late`, with the line that a game keeps working
without it. `### Breaking` stays `None.` **[author decision]**: adding an enumerator only breaks a game that switches
exhaustively over `OrderBand`, which no documented use does.

## 6. Verification

- `engine/runtime/app/tests/systems.cpp` (suite built when `OLY_BUILD_TESTS` is `ON`, as in the default
  `debug-asan` preset), via the `add-test-case` skill:
  - a `Late` system runs after every `Default` system and before every `Last` system of its phase, whatever the
    registration order;
  - `Late` systems keep their registration order;
  - a `Late` system sees a `Default` field write but not a `Default` spawn (the phase flushes once, after `Last`).
- Run: `./spur test`, then `./spur format-check`.

## 7. Left to the builder

- Exact test names and wording of the doc paragraphs.
- Whether `gameplay-recipes.md` gains a line about `Late` (today it only uses `Last` for the camera).
