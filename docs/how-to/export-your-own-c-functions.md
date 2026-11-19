# Export your own C functions

[Docs](../README.md#how-to-guides) · Concept: [Web boundary](../explanation/concepts/web-boundary.md)

Use this to export an extra `extern "C"` function from the WASM module of a web game, next to the engine's own C API. It assumes a web game made by `oly new <game> --environment web` ([Your first web game](../tutorials/first-web-game.md)). In game code, prefer `OLY_SYSTEM`, `OLY_HANDLER` and `OLY_STATE_HOOK`: they register by name and need no export ([C++ systems, handlers and hooks](../reference/cpp/engine-and-systems.md#c-systems-handlers-and-hooks)). Export a function only when none of them fits.

1. In a `.cpp` file of `native/`, include `oly/capi/c_api.h` and write the function inside `extern "C"`, marked with `OLY_API`.
2. From an `OlyWorld*` (the C API handle), call `oly::capi::worldOf(world)` (`oly/capi/world.hpp`) to get the `oly::ecs::World&` behind it. From an `OlyEngine*`, call `oly::capi::engineOf(engine)` (`oly/capi/engine.hpp`) to get the `Engine&`.
3. Build the game. The function is now in the module.

The engine's test fixture, `engine/testing/fixture/systems.cpp`, shows the calls.

Calling such a function from TypeScript needs the module internals (`@oly/core/internal`), which are reserved for the engine. A client that stays inside the public API cannot call it.

## See also

- [C API](../reference/c-api.md): the functions the engine exports.
- [Web boundary](../explanation/concepts/web-boundary.md): why work crosses between C++ and TypeScript by block.
