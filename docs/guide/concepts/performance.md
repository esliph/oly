# Performance

[← States](../../explanation/concepts/states.md) · [Guide index](../README.md) · Next: [Plugins →](plugins.md)

Oly is built around one cost model: **a query walks columns of components that sit next to each other in memory**. A system that touches a few components of many entities is cheap. A system that allocates, moves entities between tables or reads data it does not need is not.

The measured numbers are in [performance](../../architecture/performance.md) and [benchmarks](../../reference/benchmarks/README.md). This page turns them into rules.

## Avoid archetype churn

Every `add` or `remove` moves the entity to another table: it copies its components. Doing it every step to many entities costs more than a field write.

| Need | Cheap | Expensive if it changes every step |
|---|---|---|
| a state that toggles often (`active`, `ready`) | a `u8` field | a tag added and removed |
| a state that changes rarely (`Frozen`, `Invulnerable` for 1 s) | a tag (queries can filter it for free) | — |

For example, a `Dash` component keeps `ready` and `active` as `u8` fields, because they flip on every use; `Invulnerable` can be a tag, because it changes once per hit.

The first move to a new combination of components creates an archetype and a cached edge. After that, the same move costs O(components) with no allocation.

## Split components by who reads them

A system that reads `Position` loads only the `Position` column into the cache. If `Position` also carried `health` and `name`, every movement step would drag them along. Small components read together go together; data read by different systems goes apart.

## Reuse queries

`context.query<...>()` is cached by its shape. Calling it inside a system every step is a map lookup, not a new query. Still, in a very hot system you can take the reference once (`auto& enemies = context.query<...>()`) and use it in a nested loop.

Iterate with `each` or `eachBlock`, and use `get` for **one** entity: the player, the boss, the camera. A `get` per entity in a loop repeats a lookup that the iteration already did.

```cpp
OLY_SYSTEM(drift) {
  context.query<Position, const Velocity>(oly::ecs::With<Enemy>{}).eachBlock([](std::span<Position> positions, std::span<const Velocity> velocities) {
    for (size_t row = 0; row < positions.size(); ++row) {
      positions[row].x += velocities[row].x;
    }
  });
}
```

`eachBlock` gives the compiler whole columns, which it can vectorize. Register it as shown in [Engine, loop and systems](../../reference/cpp/engine-and-systems.md#register-systems).

## Watch the fragmentation

Many small tables (one entity per combination) make `eachBlock` call your function many times with a span of 1. Prefer a few fixed combinations per kind of entity. `oly::ecs::stats(world)` shows the archetype count and a histogram of table sizes.

## Keep render cheap

Render runs once per display frame, which may be 144 times per second. Draw with queries and avoid allocating per entity there.

## Measuring

- Compare against a baseline in the **same run**: the environment is noisy (WSL2). The engine's benchmarks report ratios, not absolute times.
- For the engine's own benchmarks, see [commands](../../development/commands.md#ecs-benchmarks).
- A native game can be profiled with any native profiler on the `native-games-release` build.

## Debug vs. release

| Preset | Asserts | Use |
|---|---|---|
| `debug-asan`, `native-games`, `wasm-games` | on | development: contract violations (`OLY_ASSERT`) stop with a message |
| `release`, `native-games-release`, `wasm-games-release` | off | shipping and measuring |

`OLY_ASSERT` only runs in debug. In release, a contract violation in your C++ code is not checked and is undefined behaviour. Run your tests in a debug preset.

## Next

[Plugins →](plugins.md): reuse engine functionality across games.

In other clients: [TypeScript, across the WASM boundary](../environments/web/boundary.md).
