# Use a plugin

Use this to turn on a plugin that already exists, from your game's CMake. It assumes a game that builds with `oly_add_game` ([Getting started on the desktop](../guide/environments/native/getting-started.md) or [on the web](../guide/environments/web/getting-started.md)).

## 1. List the plugin

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

`DIMENSION` is not a plugin you list: it turns the `essentials` plugin of that dimension on, installed before the ones in `PLUGINS` ([Transforms](../explanation/concepts/transforms.md)). Listing `essentials2d` or `essentials3d` in `PLUGINS` is a configure error.

## 2. Build

The build does four things:

| Step | Result |
|---|---|
| resolves the plugins the listed ones need | each plugin installs once, after the plugins it needs |
| merges the plugins' `.oly` into the game's schema | the plugins' types come first ([Types that come from plugins](../explanation/concepts/schema.md#types-that-come-from-plugins)) |
| links the plugin targets | the game includes the plugins' headers |
| generates `oly::app::setUpGame` | declares the schema, then installs each plugin |

`setUpGame` is the call that sets up an `Engine` for the game. A native `main` calls it once, right after it creates the `Engine`; on the web, the host calls it ([Schema in C++](../reference/cpp/schema.md#olyappsetupgame)). Among plugins that do not need each other, the install order is the order of the `PLUGINS` list.

## 3. Use it through data

Add the plugin's components to your entities. The plugin's systems act on them, and your game's code installs nothing.

To configure the plugin or to read its state, reach the installed instance with `engine.plugin<P>()` ([Plugins in C++](../reference/cpp/plugins.md#engineplugin)).

## See also

- [Plugins](../explanation/concepts/plugins.md): when a plugin installs and who owns it.
- [Plugins in C++](../reference/cpp/plugins.md): `installPlugin` and `plugin`.
- [Write a plugin](write-a-plugin.md): make your own.
- [Gameplay recipes](gameplay-recipes.md): a game that uses a plugin's components.
