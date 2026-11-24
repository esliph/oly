# Write a plugin

[Docs](../README.md#how-to-guides) · Concept: [Plugins](../explanation/concepts/plugins.md)

Use this to make engine-level functionality that several games reuse: components, events and the systems that act on them. It assumes you know how a game turns a plugin on ([Use a plugin](use-a-plugin.md)) and how systems and observers work ([Engine, loop and systems](../explanation/concepts/engine-and-systems.md)).

## 1. Make the folder

A plugin is a folder with its own CMake target:

```
lifetime/
  CMakeLists.txt
  lifetime.oly                     components, tags and events (optional)
  include/oly/lifetime/plugin.hpp  class oly::lifetime::Plugin
  src/plugin.cpp                   install and the systems
```

## 2. Declare it in CMake

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

The sources are every `src/*.cpp`.

## 3. Declare its types

A plugin `.oly` declares only components, tags and events. An action or a state in it stops the build, because their ids are fixed in each game's header and would depend on the game's other plugins:

```
component Lifetime { seconds: f32 }
```

## 4. Write the class

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

#include "oly/app/engine.hpp"
#include "oly/app/system.hpp"
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

Inside `install`, `engine.plugin<Q>()` reaches a plugin that installed earlier, for example one listed in `PLUGINS` ([Plugins in C++](../reference/cpp/plugins.md#engineplugin)).

A plugin that registers observers removes them in its destructor, and a plugin system that must see the game's work of the step registers in the `Last` band. Both rules are in [Plugins](../explanation/concepts/plugins.md).

## 5. Follow the rules for plugin code

- Include only the public headers of the modules in `DEPENDS` and the plugins in `PLUGINS`; never another module's `detail/`.
- Never name a game: a plugin serves every game.
- Use no path relative to the repository root: the plugin is its own target, and its `.oly` reaches `oly-schema` as an argument.

The `oly.lint.boundaries` test checks these rules, and that nothing in `engine/` includes a plugin.

## See also

- [Plugins](../explanation/concepts/plugins.md): lifetime, ownership and order.
- [Plugins in C++](../reference/cpp/plugins.md): `Plugin`, `installPlugin` and `plugin`.
- [Use a plugin](use-a-plugin.md): turn it on in a game.
- [Schema](../explanation/concepts/schema.md#types-that-come-from-plugins): how the build merges the plugin's types.
