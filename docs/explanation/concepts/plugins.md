# Plugins

[← Performance](performance.md) · [Concepts](README.md) · Next: [Transforms →](transforms.md)

A plugin is a unit of engine functionality that games reuse: its own components and events, the systems that act on them and the state those systems keep. A game turns a plugin on with one line of CMake. No line of game code installs it.

From then on the game uses the plugin through data: it adds the plugin's components to its entities, and the plugin's systems act on them. To turn one on, see [Use a plugin](../../how-to/use-a-plugin.md). To make one, see [Write a plugin](../../how-to/write-a-plugin.md).

## Lifetime and ownership

| Rule | Why |
|---|---|
| A plugin installs once per `Engine`, before the first `frame` | installing twice or after the first `frame` is an `OLY_ASSERT` naming the type |
| The `Engine` owns the instance and destroys it in reverse install order | a plugin outlives the plugins installed after it, which may use it |
| Systems, handlers and hooks end with the `Engine` | they belong to the `Engine` |
| Pending tasks are discarded before any plugin is destroyed | a task's discard function can still use the plugin it points to |
| The destructor removes the plugin's observers (`world.removeObserver(id)`) | observers belong to the `World`, which outlives the `Engine`; an observer left behind would call into a destroyed plugin |

## Run after the game's systems

The order of systems never depends on when a plugin installed ([Order by constraints](engine-and-systems.md#order-by-constraints)). A plugin that must see the game's work of the step, such as positions after movement, declares a phase after `Update` and registers its system there:

```cpp
engine.addPhase<Follow>(app::FixedStep{}).after<app::Update>().before<app::UpdateAfter>();
engine.addSystem<&followTargets>(Follow{}, "followTargets");
```

## In other clients

A client uses a plugin through its components, which its generated schema lists ([TypeScript](../../reference/typescript/schema.md)). A plugin has no TypeScript form of its own.

## API

- C++: [Plugins in C++](../../reference/cpp/plugins.md)

## Next

[Transforms](transforms.md): the plugin that every game turns on, which places entities and nests them.
