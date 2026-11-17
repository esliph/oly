# Schema

[Guide index](../README.md) · Next: [World and entities →](world-and-entities.md)

The schema is the **single source** of your game's data. You write it once, in a `<game>.oly` file. The build generates the matching C++ (and the code of each client) from it, so every language agrees on the memory layout.

## The five kinds of entries

| Declaration | Declares | Example |
|---|---|---|
| `component Name { fields }` | data attached to an entity | `component Health { value: i32 }` |
| `tag Name` | a marker with no data | `tag Enemy` |
| `event Name { fields }` | a message sent between systems | `event EnemyDied { entity: entity, x: f32, y: f32 }` |
| `action name: kind` | an input action (`button`, `axis2d`, `pointer`) | `action shoot: button` |
| `state name` | a game state (menu, playing...) | `state playing` |

```
# game.oly
component Health { value: i32 }
component Weapon { interval: f32, cooldown: f32 }
component Target { entity: entity }
tag Enemy
tag Frozen

event EnemyDied { entity: entity, x: f32, y: f32 }

action move: axis2d
action aim: pointer
action shoot: button

state menu
state playing
```

Whitespace is free and `#` starts a comment. A field list can span several lines and may end with a comma.

C++ gets every entry from the generated header:

```cpp
#include "schema.generated.hpp"
```

Components, tags and events are structs with the name you wrote (`Health`, `Enemy`, `EnemyDied`). Actions are `actions::shoot` and states are `states::playing`. You pass them to every API call: `context.get<Health>(entity)`, `context.emit(EnemyDied{...})`, `context.actions().button(actions::shoot)`. There are no string lookups at run time, and a typo such as `actions::shoto` is a compile error.

## Field types

| Type | C++ | Range / use |
|---|---|---|
| `f32` | `float` | positions, speeds, timers |
| `f64` | `double` | values that need precision (accumulated time, money) |
| `i8` `u8` | `int8_t` `uint8_t` | small counters, booleans (`0`/`1`) |
| `i16` `u16` | `int16_t` `uint16_t` | medium counters |
| `i32` `u32` | `int32_t` `uint32_t` | health, score, ids |
| `entity` | `oly::ecs::Entity` | a reference to another entity |

There are no 64-bit integers (a client in JavaScript could not read them) and no strings. Keep text, sprites and other assets outside the schema, indexed by a number in a component.

Booleans are a `u8` field with `0` or `1`:

```
component Dash { speed: f32, duration: f32, ready: u8, active: u8 }
```

## Naming rules

The names become C++ identifiers, so the build checks them. A broken rule stops the build with `<game>.oly:<line>:<column>: <message>`, and no file is written.

| Entry | Rule | Good | Bad |
|---|---|---|---|
| component, tag, event | `PascalCase` | `EnemyDied` | `enemy_died` |
| field | `camelCase`, not a C++ keyword | `cooldown` | `Cooldown`, `class`, `int` |
| action, state | `camelCase`, not a C++ keyword | `gameOver` | `GameOver`, `default` |

Other rules:

- A component needs at least one field. With no data, use `tag`.
- An event needs at least one field.
- A field name appears once per component or event.
- A name can appear only once in the file (components, tags, events, actions and states share one namespace).
- An action or state cannot be a reserved word of a client language such as `function`, because that client gets it as a constant.
- At most 32 states.

## Order matters

Each entry gets an index inside its group, in the order you write them:

- **Events**: the index is the dispatch order. Earlier events are handled first in each round ([Events](events.md#order)).
- **Actions** and **states**: the index is the id used in C++ (`actions::shoot`, `states::playing`).

Changing the order, adding or removing an entry changes the **schema hash**. Every build that embeds the schema must be rebuilt.

## Memory layout

Each component becomes a plain C++ struct with natural alignment, in field order:

```
component Mixed { a: u8, b: f64, c: i16 }
```

```cpp
// Generated: do not edit.
struct Mixed {
  uint8_t a;   // offset 0
  double b;    // offset 8 (7 bytes of padding before it)
  int16_t c;   // offset 16
};
static_assert(sizeof(Mixed) == 24 && alignof(Mixed) == 8, "schema: layout of Mixed diverges from oly-schema");
```

Tip: order fields from the largest to the smallest type to avoid padding. `{ b: f64, c: i16, a: u8 }` takes 16 bytes instead of 24.

Each archetype stores one column per component (SoA across components). Inside a column, each element is the whole struct (AoS inside the component). So split data that is read by different systems into different components: a system that reads only `Position` does not pull `Health` into the cache.

## What the build generates

On every build, `oly-schema` reads the `.oly` file and writes the generated files of the game. They are never committed and never edited. For C++ they are:

| File | Holds |
|---|---|
| `schema.generated.hpp` | the structs, `actions::` and `states::` |
| `schema_registry.generated.cpp` | the schema hash and the component ids, which the C API reads |
| `schema_declaration.generated.cpp` | `oly::app::declareSchema(Engine&)` |
| `game_setup.generated.cpp` | `oly::app::setUpGame(Engine&)`: `declareSchema`, then the install of each plugin |

From the schema above, `schema.generated.hpp` contains:

```cpp
struct Position { float x; float y; };
struct Health { int32_t value; };
struct Target { oly::ecs::Entity entity; };
struct Enemy {};
struct EnemyDied { oly::ecs::Entity entity; float x; float y; };

namespace actions {
  inline constexpr oly::input::Axis2dAction move{0};
  inline constexpr oly::input::PointerAction aim{1};
  inline constexpr oly::input::ButtonAction shoot{2};
}

namespace states {
  inline constexpr oly::app::StateId menu{0};
  inline constexpr oly::app::StateId playing{1};
}
```

`oly::app::setUpGame(engine)` declares the events, actions and states in schema order, so the index in the schema is the id, and then installs the game's [plugins](plugins.md). Call it once, on an `Engine` that declares nothing yet (`OLY_ASSERT` otherwise):

```cpp
oly::app::setUpGame(engine);
```

Where you do not own the `Engine`, the host does this for you ([the web host](../environments/web/getting-started.md), [the desktop host](../environments/native/getting-started.md)).

## Types that come from plugins

A plugin can bring its own `.oly` with components, tags and events. When a game turns the plugin on (`PLUGINS` in its CMake, [Plugins](plugins.md)), the build merges the plugin's types into the game's schema:

- the plugins' types come first, in install order, then the game's. That order gives the component ids and the event order;
- names are flat: a name that a plugin and the game both declare stops the build, and the error names both places (`game.oly:3:11: repeated name "Counter"; first declared at counting.oly:1:11`);
- the hash covers the merged schema, so a client built with another set of plugins fails to load with a schema mismatch.

`DIMENSION` is the one plugin you never list: it brings `Position`, `Rotation`, `Scale`, `Parent` and `WorldTransform` ([Transforms](transforms.md)), and the game's `.oly` does not declare them.

The game uses a plugin's types like its own: the game's `schema.generated.hpp` includes the plugin's header (`oly/<plugin>/schema.generated.hpp`), and the TS schema lists them.

## Using the schema in several places

Include the generated header everywhere: from the game, from the shell and from tests. The runtime only accepts the entries of its own schema.

## Next

[World and entities →](world-and-entities.md): create entities from these types.

In other clients: [TypeScript](../environments/web/typescript/schema.md).
