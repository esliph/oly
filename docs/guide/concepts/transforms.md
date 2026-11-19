# Transforms

[← Plugins](../../explanation/concepts/plugins.md) · [Guide index](../README.md)

Every game names its dimension in CMake, and the engine gives it the components to place entities in space and to nest them. You write `Position` on an entity; the engine keeps its world position up to date, including through parents.

## Turn it on

`oly_add_game` needs `DIMENSION`:

```cmake
oly_add_game(<game>_game
  ENVIRONMENT native
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/<game>.oly
  SOURCES ${gameSources}
  DEPENDS ecs app input native_host)
```

`2d` turns `essentials2d` on and `3d` turns `essentials3d` on. The game does not list the plugin and does not declare these components in its `.oly`: they come from the plugin ([Types that come from plugins](../../explanation/concepts/schema.md#types-that-come-from-plugins)). A game that declares its own `Position` stops compiling with a duplicate name.

## The components

| Component | 2D | 3D |
|---|---|---|
| `Position` | `x, y` | `x, y, z` |
| `Rotation` | `angle`, in radians | `x, y, z, w`, a unit quaternion |
| `Scale` | `x, y` | `x, y, z` |
| `Parent` | `entity` | `entity` |
| `WorldTransform` | `x, y, angle, scaleX, scaleY` | `x, y, z, rotationX, rotationY, rotationZ, rotationW, scaleX, scaleY, scaleZ` |

`Position`, `Rotation` and `Scale` are **local**: relative to the parent, or to the world when there is none. A missing `Rotation` is the identity and a missing `Scale` is 1. `WorldTransform` is **derived**: read it, never write it. An entity that has `Position` gets a `WorldTransform` in the same spawn.

## When `WorldTransform` is up to date

| Moment | Value |
|---|---|
| right after you add `Position` or `Parent` | computed at once from the locals |
| after the `Default` systems of `Update` | recomputed once per fixed step, in the `Last` band |
| in `UpdateAfter` and `Render` | reflects the movement of this step |

A change you make to a local component in `UpdateAfter` reaches `WorldTransform` in the next step. An entity spawned by a system during `Update` already has the right `WorldTransform` in `UpdateAfter`.

The world transform composes translate, rotate and scale, with no shear: the world scale is the parent's scale times the local scale, component by component; the world rotation is the parent's rotation followed by the local one; the world position is the parent's position plus the parent's rotation applied to the local position scaled by the parent's scale.

A parent with no `Position` (a group) counts as the identity, and so does the chain above it: its own `Parent` does not add to its children.

## Parent and children

```cpp
oly::ecs::Entity ship = context.spawn(Position{100.0f, 50.0f});
oly::ecs::Entity turret = context.spawn(Position{10.0f, 0.0f}, Parent{ship});
```

| You do | Result |
|---|---|
| `add(entity, Parent{other})` on an entity that has a parent | re-parents it; the local values stay, so the entity moves in the world |
| `remove<Parent>(entity)` | the entity becomes a root with its local values |
| destroy an entity | its children, grandchildren and so on are destroyed too, whether you destroy it directly, with a command buffer, or through a state change |

A `Parent` that points to a dead entity, to the entity itself or to one of its own descendants is a contract error (`OLY_ASSERT`). A parent spawned in the same command buffer as its child is valid. Writing `Parent::entity` through `get` or a query skips the bookkeeping: change it with `add`.

## Cost

Moving a parent costs one pass per fixed step over the roots and one lookup per child. The numbers are in [the plugins architecture page](../../architecture/plugins.md#essentials).
