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


Every other verified finding of [the audit](specs/2026-11-18-docs-audit.md) is closed. The open ones are below, each with its reason.

| Finding | Why it is open |
|---|---|
| A10, the code half: the surfaces that are public only because they sit outside `detail/` (the `core` module, the `Named*Registration` types, the `ById` variants, the `capi::schema*` functions and the other groups of audit section 3.3) | Moving them into `detail/` or narrowing the rule is a code decision, outside the documentation work. Until then [`docs-style.md`](../reference/contributor/docs-style.md#public-surface) lists each group as internal, so a reviewer tells an omission from an exclusion. |
| `engine_api.h:53-55` says that `olyEngineCreate` calls `declareSchema`; the code calls `setUpGame` (`engine_api.cpp:127`) | A comment in a header. The documentation is right ([C API](../reference/c-api.md), [Architecture of the C API](../explanation/architecture/capi.md)); the fix is a code change. |
| The message of `oly run --docker` on a desktop game names `build/cmake-docker/games/<game>` (`tools/oly/Spurfile:464`), while a host build writes to `build/cmake/games/` | Which path the message should print is a code decision. [Errors](../reference/errors.md) and [CLI](../reference/cli.md) quote the message with `<path>`. |
| In C++, `Engine::frame` called from inside a system takes the recovery path of an interrupted frame (`engine.cpp:125`), while the TypeScript client raises `frame is not reentrant` | The two languages differ in behavior, and which one is right is a code decision. [Errors](../reference/errors.md) lists the message in the TypeScript table only. |
| D9, D11, D17 and D18, the duplications the audit marked as suspected | The audit could not prove them. The reorganization gave each topic one home, but no line-by-line comparison confirmed that no copy is left. |
| `SECURITY.md`: an alternative channel in case private reporting is disabled in the repository, and `CODE_OF_CONDUCT.md`: a fixed contact for reports | Both need a maintainer decision and a setting of the GitHub repository that cannot be read from the checkout. |
| GitHub Discussions as a place to ask a question | `SUPPORT.md` names only issues. Whether the repository has Discussions enabled cannot be checked from the checkout. |
| `.github/CODEOWNERS` | Optional. With one maintainer it requests no review, so it was not added. |
