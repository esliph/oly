# Keep the hot path fast

Use this when a system runs on many entities every step and you want it cheap. It assumes a game with queries and systems ([Queries](../explanation/concepts/queries.md), [Engine, loop and systems](../explanation/concepts/engine-and-systems.md)) and the cost model of [Performance](../explanation/concepts/performance.md).

## 1. Split components by who reads them

A system that reads `Position` loads only the `Position` column into the cache. If `Position` also carried `health` and `name`, every movement step would drag them along. Keep small components that are read together in one component, and put data that different systems read in different components.

## 2. Reuse queries

`context.query<...>()` is cached by its shape. Calling it inside a system every step is a map lookup, not a new query. In a very hot system you can take the reference once (`auto& enemies = context.query<...>()`) and use it in a nested loop.

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

`eachBlock` gives the compiler whole columns, which it can vectorize. Register the system as shown in [Engine, loop and systems](../reference/cpp/engine-and-systems.md#register-systems).

## 3. Keep render cheap

Render runs once per display frame, which may be 144 times per second. Draw with queries and avoid allocating per entity there.

## 4. Measure

- Compare against a baseline in the **same run**: the environment is noisy (WSL2). The engine's benchmarks report ratios, not absolute times.
- For the engine's own benchmarks, see [commands](../development/commands.md#ecs-benchmarks).
- Profile a native game with any native profiler on the `native-games-release` build.

## See also

- [Performance](../explanation/concepts/performance.md): the cost model behind these steps.
- [Benchmark results](../reference/benchmarks/README.md): the measured numbers.
- [Queries](../explanation/concepts/queries.md) and [Queries in C++](../reference/cpp/queries.md): `each`, `eachBlock` and `get`.
