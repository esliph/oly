# Plugins

[← Performance](performance.md) · [Guide index](../README.md)

A plugin is a unit of engine functionality that games reuse: its own components and events, the systems that act on them and the state those systems keep. A game turns a plugin on with one line of CMake. No line of game code installs it.

## Turn a plugin on

List the plugin in `PLUGINS` of the game's `oly_add_game`:

```cmake
oly_add_game(<game>_game
  ENVIRONMENT native
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/<game>.oly
  SOURCES ${gameSources}
  DEPENDS ecs app native_host
  PLUGINS lifetime)
```

`DIMENSION` is not a plugin you list: it turns the `essentials` plugin of that dimension on, installed before the ones in `PLUGINS` ([Transforms](transforms.md)). Listing `essentials2d` or `essentials3d` in `PLUGINS` is a configure error.

The build then:

| Step | Result |
|---|---|
| resolves the plugins the listed ones need | each plugin installs once, after the plugins it needs |
| merges the plugins' `.oly` into the game's schema | the plugins' types come first ([Types that come from plugins](schema.md#types-that-come-from-plugins)) |
| links the plugin targets | the game includes the plugins' headers |
| generates `oly::app::setUpGame` | declares the schema, then installs each plugin |

`setUpGame` is the call that sets up an `Engine` for the game. A native `main` calls it once, right after it creates the `Engine` ([Getting started on the desktop](../environments/native/getting-started.md)); on the web, the host calls it. Among plugins that do not need each other, the install order is the order of the `PLUGINS` list.

From then on the game uses the plugin through data: it adds the plugin's components to its entities, and the plugin's systems act on them.

## Reach an installed plugin

`engine.plugin<P>()` returns the installed instance, to configure it or to read its state:

```cpp
oly::app::setUpGame(engine);

const uint32_t expired = engine.plugin<oly::lifetime::Plugin>().expiredCount();
```

A plugin that is not installed is an `OLY_ASSERT` naming its type.

## Write a plugin

A plugin is a folder with its own CMake target:

```
lifetime/
  CMakeLists.txt
  lifetime.oly                     components, tags and events (optional)
  include/oly/lifetime/plugin.hpp  class oly::lifetime::Plugin
  src/plugin.cpp                   install and the systems
```

`CMakeLists.txt` declares the plugin, the modules it uses and the plugins it needs:

```cmake
oly_add_plugin(NAME lifetime SCHEMA lifetime.oly DEPENDS ecs app)
```

| Argument | Meaning |
|---|---|
| `NAME` | the plugin name: target `oly_<name>`, namespace `oly::<name>`, headers in `include/oly/<name>/` |
| `SCHEMA` | the plugin's `.oly`; the build generates `oly/<name>/schema.generated.hpp` from it |
| `DEPENDS` | the modules the plugin includes |
| `PLUGINS` | the plugins this one needs; they install before it |

The sources are every `src/*.cpp`. A plugin `.oly` declares only components, tags and events. An action or a state in it stops the build, because their ids are fixed in each game's header and would depend on the game's other plugins:

```
component Lifetime { seconds: f32 }
```

The class derives from `oly::app::Plugin`, has a default constructor and implements `install`:

```cpp
#pragma once

#include <cstdint>

#include "oly/app/plugin.hpp"

namespace oly::lifetime {
  class Plugin : public app::Plugin {
   private:
    uint32_t expiredCount_ = 0;

   public:
    void install(app::Engine& engine) override;

    uint32_t expiredCount() const noexcept {
      return expiredCount_;
    }
  };
}
```

`install` registers the plugin's systems, handlers, hooks and observers. The plugin's state lives in the instance and reaches its systems through `userData`:

```cpp
#include "oly/lifetime/plugin.hpp"

#include "oly/app/context.hpp"
#include "oly/app/engine.hpp"
#include "oly/lifetime/schema.generated.hpp"

namespace oly::lifetime {
  namespace {
    void expire(app::SystemContext& context) {
      uint32_t& expiredCount = *static_cast<uint32_t*>(context.userData());
      const float step = context.fixedDeltaTime();

      context.query<Lifetime>().each([&](ecs::Entity entity, Lifetime& lifetime) {
        lifetime.seconds -= step;
        if (lifetime.seconds <= 0) {
          context.destroy(entity);
          ++expiredCount;
        }
      });
    }
  }

  void Plugin::install(app::Engine& engine) {
    engine.addSystem(app::Phase::UpdateAfter, "expire", expire, &expiredCount_);
  }
}
```

Inside `install`, `engine.plugin<Q>()` reaches a plugin that installed earlier, for example one listed in `PLUGINS`.

## Lifetime and ownership

| Rule | Why |
|---|---|
| A plugin installs once per `Engine`, before the first `frame` | installing twice or after the first `frame` is an `OLY_ASSERT` naming the type |
| The `Engine` owns the instance and destroys it in reverse install order | a plugin outlives the plugins installed after it, which may use it |
| Systems, handlers and hooks end with the `Engine` | they belong to the `Engine` |
| Pending tasks are discarded before any plugin is destroyed | a task's discard function can still use the plugin it points to |
| The destructor removes the plugin's observers (`world.removeObserver(id)`) | observers belong to the `World`, which outlives the `Engine`; an observer left behind would call into a destroyed plugin |

## Run after the game's systems

Plugins install before the game registers its systems, so a plugin system in the default band runs **before** the game's systems of the same phase. A plugin that must see the game's work of the step, such as positions after movement, registers in the `Last` band ([Order bands](engine-and-systems.md#order-bands)):

```cpp
engine.addSystem(app::Phase::Update, "followTargets", followTargets, nullptr, app::OrderBand::Last);
```

## Rules for plugin code

- Include only the public headers of the modules in `DEPENDS` and the plugins in `PLUGINS`; never another module's `detail/`.
- Never name a game: a plugin serves every game.
- Use no path relative to the repository root: the plugin is its own target, and its `.oly` reaches `oly-schema` as an argument.

The `oly.lint.boundaries` test checks these rules, and that nothing in `engine/` includes a plugin.

## Next

[Troubleshooting](../troubleshooting.md): what each error means.

In other clients: a client uses a plugin through its components, which its generated schema lists ([TypeScript](../../reference/typescript/schema.md)).
