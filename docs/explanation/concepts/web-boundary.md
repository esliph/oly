# Web boundary

[← Transforms](transforms.md) · [Concepts](README.md)

On the web, the engine runs in WASM and the TS client calls into it. Data in WASM memory is cheap to reach from C++, and cheap from TS only in batches. Each call from TS into WASM (or back) has a fixed cost. Pay it per system, per table or per event batch, never per entity.

The measured numbers are in [Benchmark results](../../reference/benchmarks/README.md). The cost model that does not depend on the boundary (archetype churn, fragmentation) is in [Performance](performance.md).

## Where each kind of work runs

A TS system costs at least one crossing per call and runs in the JS engine. A C++ system runs next to the data, and the compiler can inline and vectorize it. So the work splits by how often it touches an entity:

| Work | Where | Why |
|---|---|---|
| per entity, every step: movement, collision, AI, weapons, tight math | C++ | no crossing per entity, and the compiler sees whole columns |
| per system, per batch or a few times per step: render, HUD, menus | TypeScript | the crossing is paid once for many entities |
| rules that change often: score, waves | TypeScript | they are not per entity, and a TS change needs no C++ build |
| bindings and state hooks that talk to the page | TypeScript | they need the browser |

Both kinds of system can live in the same phase once each pair that conflicts is ordered: a C++ system declares the access of its parameters, and a TS system conflicts with every other system of its phase ([Registration options](../../reference/typescript/engine-and-systems.md)). The template uses both: the C++ system `movePlayer` and the TS system `draw` ([Your first web game](../../tutorials/first-web-game.md#7-read-the-typescript-system)).

A game can run only TS systems (a prototype or few entities: the `native/` folder can be empty), or only C++ with no client at all: that is a [desktop game](../../tutorials/first-desktop-game.md) or a tool, with an `oly::app::Engine` over an `oly::ecs::World`.

## Values both languages use

A value that both languages need (world size, bullet speed) lives in a C++ header (`native/constants.hpp`) and a TS module (`src/constants.ts`). Keep them side by side and say in each file that the other one must change with it.

## Keep the hot path fast

The techniques for the TS side of the boundary (`eachBlock`, `raw()`, event batches, measuring): [Keep the hot path fast](../../how-to/keep-the-hot-path-fast.md#typescript).
