# Web boundary

[← Transforms](transforms.md) · [Concepts](README.md)

On the web, the engine runs in WASM and the TS client calls into it. Data in WASM memory is cheap to reach from C++, and cheap from TS only in batches. Each call from TS into WASM (or back) has a fixed cost. Pay it per system, per table or per event batch, never per entity.

The measured numbers are in [performance](../../architecture/performance.md) and [benchmarks](../../reference/benchmarks/README.md). The cost model that does not depend on the boundary (archetype churn, fragmentation) is in [Performance](performance.md).

## Where each kind of work runs

A TS system costs at least one crossing per call and runs in the JS engine. A C++ system runs next to the data, and the compiler can inline and vectorize it. So the work splits by how often it touches an entity:

| Work | Where | Why |
|---|---|---|
| per entity, every step: movement, collision, AI, weapons | C++ | no crossing per entity, and the compiler sees whole columns |
| per system, per batch or a few times per step | TypeScript | the crossing is paid once for many entities |

A game can run only TS systems (a prototype or few entities) or only C++. How to assemble a game that splits its work: [Getting started on the web](../../guide/environments/web/getting-started.md#how-to-split-work-between-c-and-a-client).

## Keep the hot path fast

The techniques for the TS side of the boundary (`eachBlock`, `raw()`, event batches, measuring): [Keep the hot path fast](../../how-to/keep-the-hot-path-fast.md#typescript).
