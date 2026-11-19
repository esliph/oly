# Performance

[← States](states.md) · [Concepts](README.md) · Next: [Plugins →](../../guide/concepts/plugins.md)

Oly is built around one cost model: **a query walks columns of components that sit next to each other in memory**. A system that touches a few components of many entities is cheap. A system that allocates, moves entities between tables or reads data it does not need is not.

The measured numbers are in [performance](../../architecture/performance.md) and [benchmarks](../../reference/benchmarks/README.md). This page explains what the numbers mean for a game.

## Archetype churn

Every `add` or `remove` moves the entity to another table: it copies its components. Doing it every step to many entities costs more than a field write.

| Need | Cheap | Expensive if it changes every step |
|---|---|---|
| a state that toggles often (`active`, `ready`) | a `u8` field | a tag added and removed |
| a state that changes rarely (`Frozen`, `Invulnerable` for 1 s) | a tag (queries can filter it for free) | — |

For example, a `Dash` component keeps `ready` and `active` as `u8` fields, because they flip on every use; `Invulnerable` can be a tag, because it changes once per hit.

The first move to a new combination of components creates an archetype and a cached edge. After that, the same move costs O(components) with no allocation.

## Fragmentation

Many small tables (one entity per combination) make `eachBlock` call your function many times with a span of 1. Prefer a few fixed combinations per kind of entity. `oly::ecs::stats(world)` shows the archetype count and a histogram of table sizes.

## Debug and release

| Preset | Asserts | Use |
|---|---|---|
| `debug-asan`, `native-games`, `wasm-games` | on | development: contract violations (`OLY_ASSERT`) stop with a message |
| `release`, `native-games-release`, `wasm-games-release` | off | shipping and measuring |

`OLY_ASSERT` only runs in debug. In release, a contract violation in your C++ code is not checked and is undefined behaviour. Run your tests in a debug preset.

## What it means for a game

Hot per-entity loops stay in C++. TypeScript keeps what runs per system, per batch or a few times per step. The reason is the cost of crossing from TypeScript into WebAssembly: [Web boundary](../../guide/environments/web/boundary.md).

## Keep the hot path fast

The techniques, in C++ and in TypeScript: [Keep the hot path fast](../../how-to/keep-the-hot-path-fast.md).

## Next

[Plugins](../../guide/concepts/plugins.md): reuse engine functionality across games.
