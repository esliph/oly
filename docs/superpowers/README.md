# Specs and plans

> **Status.** The specs and plans in this folder are the historical record of how Oly was designed and built. They are not rewritten when the code changes. The current state of the engine is in [`docs/explanation/architecture/`](../explanation/architecture/README.md). The ECS spec (`R-*`, `I-*`, `NF-*`) remains the requirement in force.

The plans record, phase by phase, how each part was implemented and the decisions the specs left open.

## Specs

| Document | Contents |
|---|---|
| [`specs/2026-10-16-ecs-archetype-spec.md`](specs/2026-10-16-ecs-archetype-spec.md) | ECS: requirements `R-*`, invariants `I-1..I-13`, targets `NF-*` |
| [`specs/2026-11-06-wasm-cost-study-design.md`](specs/2026-11-06-wasm-cost-study-design.md) | native × WASM cost study |
| [`specs/2026-11-09-game-engine-design.md`](specs/2026-11-09-game-engine-design.md) | the engine: loop, events, input, schedule, states, validation game |
| [`specs/2026-11-09-oly-modularization-design.md`](specs/2026-11-09-oly-modularization-design.md) | modules, boundaries, names and public surfaces |
| [`specs/2026-11-13-multi-environment-design.md`](specs/2026-11-13-multi-environment-design.md) | C++ first, environments and clients, `.oly`, native |
| [`specs/2026-11-13-spur-task-runner-design.md`](specs/2026-11-13-spur-task-runner-design.md) | the `./spur` task runner |
| [`specs/2026-11-16-plugins-design.md`](specs/2026-11-16-plugins-design.md) | plugins, order bands, schema merge, observers, `essentials` |
| [`specs/2026-11-17-publishing-design.md`](specs/2026-11-17-publishing-design.md) | publishing: releases, the `oly` launcher and the game tasks |
| [`specs/2026-11-18-docs-audit.md`](specs/2026-11-18-docs-audit.md) | the audit of the documentation |
| [`specs/2026-11-18-docs-reorganization-design.md`](specs/2026-11-18-docs-reorganization-design.md) | the reorganization of `docs/` by Diátaxis type |

## Plans

[`plans/`](plans/) holds the implementation plans, one per phase.

## Documentation audit

