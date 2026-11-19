# Queries in TypeScript

[← World and entities](world-and-entities.md) · [TypeScript client](README.md) · Concept: [Queries](../../guide/concepts/queries.md) · Next: [Engine, loop and systems →](engine-and-systems.md)

What a query visits and the rules while iterating are in the [concept page](../../guide/concepts/queries.md). This page shows the TS calls.

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

The block and its views are valid only inside the callback ([the contract](../../guide/concepts/queries.md#rules-while-iterating)). A direct structural change on the world (`world.spawn/destroy/add/remove`) throws `ContractError`. Outside the loop, collect the entities first and change them after it:

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

## Common query patterns

### Count entities

```ts
function count(world: World, spec: QuerySpec): number {
  let total = 0;
  world.query(spec).eachBlock((block) => {
    total += block.count;
  });

  return total;
}

count(world, { with: [Enemy] });
count(world, { with: [Enemy], without: [Frozen] });
```

### Find the nearest entity

```ts
function nearestEnemy(context: SystemContext, x: number, y: number): Entity | null {
  let nearest: Entity | null = null;
  let nearestDistance = Infinity;

  context.query({ read: [Position], with: [Enemy] }).eachBlock((block) => {
    const position = block.read(Position);
    for (let row = 0; row < block.count; ++row) {
      const deltaX = position.x.get(row) - x;
      const deltaY = position.y.get(row) - y;
      const distance = deltaX * deltaX + deltaY * deltaY;
      if (distance < nearestDistance) {
        nearestDistance = distance;
        nearest = block.entity(row);
      }
    }
  });

  return nearest;
}
```

### Two queries together (pairs)

Read the small set into local values first, then iterate the large set. Here, enemies chase the player:

```ts
const target = context.query({ read: [Position], with: [Player] }).single().get(Position);

context.query({ read: [Position, Chaser], write: [Velocity], with: [Enemy] }).eachBlock((block) => {
  const position = block.read(Position);
  const chaser = block.read(Chaser);
  const velocity = block.write(Velocity);
  for (let row = 0; row < block.count; ++row) {
    const deltaX = target.x - position.x.get(row);
    const deltaY = target.y - position.y.get(row);
    const length = Math.hypot(deltaX, deltaY) || 1;
    velocity.x.set(row, (deltaX / length) * chaser.speed.get(row));
    velocity.y.set(row, (deltaY / length) * chaser.speed.get(row));
  }
});
```

For "each against each" (bullets × enemies), prefer a C++ system: see [Nested queries](../../guide/concepts/queries.md#two-queries-together-pairs).

### Several shapes with one function

```ts
type Filter = Pick<QuerySpec, "with" | "without">;

function drawCircles(context: SystemContext, drawer: Drawer, filter: Filter, color: string): void {
  context.query({ read: [Position, Collider], ...filter }).eachBlock((block) => {
    const position = block.read(Position);
    const collider = block.read(Collider);
    for (let row = 0; row < block.count; ++row) {
      drawer.circle(position.x.get(row), position.y.get(row), collider.radius.get(row), color);
    }
  });
}

drawCircles(context, drawer, { with: [Enemy] }, "red");
drawCircles(context, drawer, { with: [Player], without: [Invulnerable] }, "blue");
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

## Next

[Engine, loop and systems →](engine-and-systems.md): run queries every step inside systems.
