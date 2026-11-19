# Keep the hot path fast

Use this when a system runs on many entities every step and you want it cheap. Steps 1 to 4 apply to C++; [TypeScript](#typescript) has its own section. It assumes a game with queries and systems ([Queries](../explanation/concepts/queries.md), [Engine, loop and systems](../explanation/concepts/engine-and-systems.md)) and the cost model of [Performance](../explanation/concepts/performance.md).

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
- For the engine's own benchmarks, see [Run the benchmarks](contributor/run-benchmarks.md).
- Profile a native game with any native profiler on the `native-games-release` build.

## TypeScript

On the web, each call from TypeScript into WASM (or back) has a fixed cost: pay it per system, per table or per event batch, never per entity ([Web boundary](../explanation/concepts/web-boundary.md)).

### Iterate with `eachBlock`, not with `get` per entity

```ts
// slow: two boundary crossings and one object per entity
for (const enemy of enemies) {
  const position = world.get(enemy, Position);
  world.set(enemy, Position, { x: position.x + 1 });
}

// fast: one crossing per table, no objects
world.query({ write: [Position], with: [Enemy] }).eachBlock((block) => {
  const position = block.write(Position);
  for (let row = 0; row < block.count; ++row) {
    position.x.set(row, position.x.get(row) + 1);
  }
});
```

`get`, `set`, `single` and `trySingle` are for **one** entity: the player, the boss, the camera.

### Move per-entity work to C++

Movement, collision, AI and weapons belong in C++ once the entity count grows. [Register them by name](../reference/typescript/engine-and-systems.md#c-systems-handlers-and-hooks) from TS:

```ts
engine.addNativeSystem(Phase.Update, "integrate");
```

The C++ side of that registration: [Systems in C++, registered by name](../reference/cpp/engine-and-systems.md#c-systems-handlers-and-hooks).

### Use `raw()` in hot loops

`field.raw()` gives the typed array, base and stride. One array access per element, no method call ([Raw arrays](../reference/typescript/queries.md#raw-arrays)):

```ts
const x = block.write(Position).x.raw();
for (let row = 0; row < block.count; ++row) {
  x.array[x.base + row * x.stride] += 1;
}
```

Take raw arrays inside the block and do not call anything that can allocate in WASM (spawn, add, emit) while you use them: if WASM memory grows, the old array is detached.

### Handle events in batches

Handlers already receive a whole batch per call. Do the loop inside the handler. Emitting thousands of events is fine; the queue is a byte buffer.

### Measure in the browser

- Use the Chromium profiler on the running game.
- The boundary benchmark runs with `./spur wasm node engine/testing/dist/bench/boundary.js ...`; see [Run the benchmarks](contributor/run-benchmarks.md#3-run-a-wasm-boundary-benchmark) for the full command.

### Test in debug, ship in release

In release (`wasm-games-release`), asserts are off: a violation inside your **C++** code is not checked and is undefined behaviour. The TS client still validates everything that goes through it and throws `ContractError`. Run your tests in the debug preset ([Debug and release](../explanation/concepts/performance.md#debug-and-release)).

## See also

- [Web boundary](../explanation/concepts/web-boundary.md): why the crossing has a cost.
- [Performance](../explanation/concepts/performance.md): the cost model behind these steps.
- [Benchmark results](../reference/benchmarks/README.md): the measured numbers.
- [Queries](../explanation/concepts/queries.md) and [Queries in C++](../reference/cpp/queries.md): `each`, `eachBlock` and `get`.
