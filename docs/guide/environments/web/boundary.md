# The WASM↔JS boundary

[Guide index](../../README.md)

On the web, the engine runs in WASM and the TS client calls into it. Data in WASM memory is cheap to reach from C++, and cheap from TS only in batches. Each call from TS into WASM (or back) has a fixed cost. Pay it per system, per table or per event batch, never per entity.

The measured numbers are in [performance](../../../architecture/performance.md) and [benchmarks](../../../benchmarks/README.md). The rules that do not depend on the boundary (archetype churn, splitting components, fragmentation) are in [Performance](../../concepts/performance.md).

## Iterate with `eachBlock`, not with `get` per entity

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

## Move per-entity work to C++

A TS system costs at least one crossing per call and runs in the JS engine. A C++ system runs next to the data, can be inlined and vectorized by the compiler. Movement, collision, AI and weapons belong in C++ once the entity count grows. [Register them by name](../../concepts/engine-and-systems.md#systems-in-c-registered-by-name-oly_system-oly_handler-oly_state_hook) from TS:

```ts
engine.addNativeSystem(Phase.Update, "integrate");
```

## Use `raw()` in hot loops

`field.raw()` gives the typed array, base and stride. One array access per element, no method call ([Raw arrays](typescript/queries.md#raw-arrays)):

```ts
const x = block.write(Position).x.raw();
for (let row = 0; row < block.count; ++row) {
  x.array[x.base + row * x.stride] += 1;
}
```

Take raw arrays inside the block and do not call anything that can allocate in WASM (spawn, add, emit) while you use them: if WASM memory grows, the old array is detached.

## Handle events in batches

Handlers already receive a whole batch per call. Do the loop inside the handler. Emitting thousands of events is fine; the queue is a byte buffer.

## Measuring

- In the browser, use the Chromium profiler on the running game.
- The boundary benchmark runs with `./spur wasm node engine/testing/dist/bench/boundary.js ...`; see [commands](../../../development/commands.md#wasm-and-typescript) for the full command.

## Release builds

In release (`wasm-games-release`), asserts are off: a violation inside your **C++** code is not checked and is undefined behaviour. The TS client still validates everything that goes through it and throws `ContractError`. Run your tests in the debug preset ([Debug vs. release](../../concepts/performance.md#debug-vs-release)).
