# Transforms in C++

[Reference](../../README.md#reference) · Concept: [Transforms](../../explanation/concepts/transforms.md)

The components come from the `essentials2d` or `essentials3d` plugin, so a game's `.oly` does not declare them. When `WorldTransform` is up to date, how a parent and its children behave and what a `Parent` error means: the [concept page](../../explanation/concepts/transforms.md).

## Turn it on

### `DIMENSION`

```cmake
oly_add_game(<game>_game
  ENVIRONMENT native
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/<game>.oly
  SOURCES ${gameSources}
  DEPENDS ecs app input native_host)
```

`oly_add_game` needs `DIMENSION`: without it, configure stops with `<target>: oly_add_game needs DIMENSION 2d|3d`. `2d` turns `essentials2d` on and `3d` turns `essentials3d` on, installed before the plugins in `PLUGINS`. Listing `essentials2d` or `essentials3d` in `PLUGINS` stops configure with `<target>: <plugin> comes from DIMENSION; do not list it in PLUGINS`. A game that declares one of the components below stops `oly-schema` with `repeated name "<name>"; first declared at <file>:<line>:<column>`.

## The components

### `Position`

```
component Position { x: f32, y: f32 }
component Position { x: f32, y: f32, z: f32 }
```

The first line is the 2D form and the second is the 3D form. The local position, relative to the parent, or to the world when there is no `Parent`. Adding it also adds `WorldTransform`.

### `Rotation`

```
component Rotation { angle: f32 }
component Rotation { x: f32, y: f32, z: f32, w: f32 }
```

The local rotation. In 2D `angle` is in radians; in 3D the four fields are a unit quaternion. A missing `Rotation` is the identity.

### `Scale`

```
component Scale { x: f32, y: f32 }
component Scale { x: f32, y: f32, z: f32 }
```

The local scale. A missing `Scale` is 1.

### `Parent`

```
component Parent { entity: entity }
```

The entity this one nests under. The same in 2D and 3D. Add it with `add(entity, Parent{other})` and remove it with `remove<Parent>(entity)`. A `Parent` that points to a dead entity, to the entity itself or to one of its descendants stops with `OLY_ASSERT`. Change it with `add`, not by writing `Parent::entity` through `get` or a query.

```cpp
oly::ecs::Entity ship = context.spawn(Position{100.0f, 50.0f});
oly::ecs::Entity turret = context.spawn(Position{10.0f, 0.0f}, Parent{ship});
```

### `WorldTransform`

```
component WorldTransform { x: f32, y: f32, angle: f32, scaleX: f32, scaleY: f32 }
component WorldTransform { x: f32, y: f32, z: f32, rotationX: f32, rotationY: f32, rotationZ: f32, rotationW: f32, scaleX: f32, scaleY: f32, scaleZ: f32 }
```

The position, rotation and scale in the world, derived from the locals and the `Parent` chain. Read it, never write it.
