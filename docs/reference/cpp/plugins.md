# Plugins in C++

[Reference](../../README.md#reference) · Concept: [Plugins](../../explanation/concepts/plugins.md)

Headers: `oly/app/plugin.hpp` for `Plugin` and `oly/app/engine.hpp` for `installPlugin` and `plugin`. When a plugin installs, who destroys it and in what order: the [concept page](../../explanation/concepts/plugins.md#lifetime-and-ownership). Turn one on: [Use a plugin](../../how-to/use-a-plugin.md). Write one: [Write a plugin](../../how-to/write-a-plugin.md).

## Write the class

### `oly::app::Plugin`

```cpp
class Plugin {
 public:
  virtual ~Plugin() = default;

  virtual void install(Engine& engine) = 0;
};
```

The base class of every plugin. `install` registers the plugin's systems, handlers, hooks and observers, and the plugin keeps its own state in the instance, which reaches its systems through `userData`. A plugin class needs a default constructor. The destructor removes the observers that `install` registered with `World::removeObserver`: observers belong to the `World`, which outlives the `Engine`.

A plugin is a class named `Plugin` in the namespace `oly::<plugin>`, and it declares `install` in its public section:

```cpp
namespace oly::<plugin> {
  class Plugin : public oly::app::Plugin {
   public:
    void install(oly::app::Engine& engine) override;
  };
}
```

## Install and reach

### `Engine::installPlugin`

```cpp
template <class P>
P& installPlugin();
```

Constructs `P`, keeps it and runs its `install` once. `setUpGame` calls it for each plugin of the game, so a game does not. It stops with `OLY_ASSERT` when it happens after the first `frame` (`installPlugin: plugin "<type>" installed after the first frame call`) or when `P` is already installed (`installPlugin: plugin "<type>" already installed`). `P` must derive from `oly::app::Plugin` and have a default constructor, or the build stops.

### `Engine::plugin`

```cpp
template <class P>
P& plugin();
```

Returns the installed instance of `P`, to configure it or to read its state. It stops with `OLY_ASSERT` when `P` is not installed (`plugin: plugin "<type>" not installed`). Inside `install`, it reaches a plugin that installed earlier, for example one the plugin needs.

```cpp
oly::app::setUpGame(engine);

const uint32_t expired = engine.plugin<oly::lifetime::Plugin>().expiredCount();
```
