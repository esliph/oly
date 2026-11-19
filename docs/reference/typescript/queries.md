# Queries in TypeScript

[TypeScript client](README.md) · Concept: [Queries](../../explanation/concepts/queries.md) · C++: [Queries in C++](../cpp/queries.md)

What a query visits and the rules while iterating are in the [concept page](../../explanation/concepts/queries.md). This page shows the TS calls. Ready patterns: [Recipes in TypeScript](../../how-to/typescript/gameplay-recipes.md#count-entities).

## The query spec

```ts
world.query({
  read: [Velocity],          // components you read
  write: [Position],         // components you read and write
  with: [Enemy],             // must have (data not needed: tags or components)
  without: [Frozen],         // must not have
});
```

| Key | Gives you | Accepts tags? |
|---|---|---|
| `read` | read-only views | no |
| `write` | read-write views | no |
| `with` | only filtering | yes |
| `without` | only filtering | yes |

All keys are optional. `world.query({ with: [Enemy] })` visits every enemy without reading any data.

## Iterate

`eachBlock` calls your function once per table, with all its rows.

```ts
world.query({ read: [Velocity], write: [Position] }).eachBlock((block) => {
  const velocity = block.read(Velocity);
  const position = block.write(Position);

  for (let row = 0; row < block.count; ++row) {
    position.x.set(row, position.x.get(row) + velocity.x.get(row));
    position.y.set(row, position.y.get(row) + velocity.y.get(row));
  }
});
```

| Block member | What it is |
|---|---|
| `block.count` | number of rows in this table |
| `block.entity(row)` | the entity of that row |
| `block.read(Type)` | view with `field.get(row)` |
| `block.write(Type)` | view with `field.get(row)` and `field.set(row, value)`; only for types in `write` |

The views read and write the WASM memory directly. There is **no copy** and no boundary crossing per entity.

## Rules while iterating

The block and its views are valid only inside the callback ([the contract](../../explanation/concepts/queries.md#rules-while-iterating)). A direct structural change on the world (`world.spawn/destroy/add/remove`) throws `ContractError`. Outside the loop, collect the entities first and change them after it:

```ts
const dead: Entity[] = [];

world.query({ read: [Health] }).eachBlock((block) => {
  const health = block.read(Health);
  for (let row = 0; row < block.count; ++row) {
    if (health.value.get(row) <= 0) {
      dead.push(block.entity(row));
    }
  }
});

for (const entity of dead) {
  world.destroy(entity);
}
```

## Exactly one: `single` and `trySingle`

```ts
const player = world.query({ read: [Position, Health], with: [Player] }).single();

player.entity;                  // the Entity
player.get(Position);           // a copy: { x, y }
player.get(Health).value;
```

| Method | 0 rows | 1 row | 2+ rows |
|---|---|---|---|
| `single()` | `ContractError` | the row | `ContractError` |
| `trySingle()` | `null` | the row | `ContractError` |

The row is a **copy** taken at the moment of the call. To change it, write through the world or the context: `context.set(player.entity, Health, { value: 0 })`.

Use `trySingle` where the entity may not exist yet, for example in a HUD drawn while in the menu:

```ts
const player = context.query({ read: [Health], with: [Player] }).trySingle();
if (player === null) {
  return;
}

drawer.text(16, 16, `Health ${player.get(Health).value}`);
```

## Raw arrays

`field.raw()` returns the typed array under a field, plus the base index and stride. Use it in tight loops to avoid one method call per element. When it pays off is in [The WASM↔JS boundary](../../guide/environments/web/boundary.md).

```ts
world.query({ read: [Velocity], write: [Position] }).eachBlock((block) => {
  const positionX = block.write(Position).x.raw();
  const velocityX = block.read(Velocity).x.raw();

  for (let row = 0; row < block.count; ++row) {
    positionX.array[positionX.base + row * positionX.stride] += velocityX.array[velocityX.base + row * velocityX.stride];
  }
});
```

A raw array is valid only until the next call that can allocate in the WASM (any spawn, add or engine call can grow the memory). Take it inside the block and use it right away. For `entity` fields, the array is `u32`: `base` points to the index and `base + 1` to the generation.
