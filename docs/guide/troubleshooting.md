# Troubleshooting

[Guide index](README.md) · References: [Recipes](recipes.md) · [Cheat sheet](cheat-sheet.md)

Oly checks how you use it and fails **at the wrong call**, with a message that names the operation, the entity (`index:generation`) and the type. This page explains the messages you will meet most.

## C++: OLY_ASSERT and debug presets

In C++, a contract violation stops on `OLY_ASSERT` in debug builds (`debug-asan`, `native-games`, `wasm-games`), with a message that names the operation, the entity and the type. In release builds (`release`, `native-games-release`, `wasm-games-release`) it is **not checked** and is undefined behaviour. Develop and test with asserts on: `PRESET=native-games ./spur native-games-test`, or `PRESET=wasm-games ./spur game <game>` for a web game.

## TypeScript: ContractError and SchemaMismatchError

| Error | Meaning | What to do |
|---|---|---|
| `ContractError` | you called the API in a way it does not allow | read the message: it says the operation and why |
| `SchemaMismatchError` | the WASM was built from a different schema | rebuild: `./spur ts` then the game build |

The TS client validates everything that goes through it, in release builds too ([TypeScript client](environments/web/typescript/README.md)).

## Common messages

| Message contains | Cause | Fix |
|---|---|---|
| `Parent ... is dead`, `own parent`, `descendant` | a `Parent` that points to a dead entity, to the entity itself or to one of its descendants ([Transforms](concepts/transforms.md#parent-and-children)) | parent only live entities, and never in a loop |
| `oly_add_game needs DIMENSION 2d\|3d`, `DIMENSION is 2d or 3d` | the game's CMake has no `DIMENSION` or an unknown one | add `DIMENSION 2d` or `DIMENSION 3d` |
| `oly-schema not found at ...: pass -DOLY_SCHEMA_TOOL=<path to oly-schema>` | a web game that fetches the engine as a subproject has no `oly-schema` for the machine (WASM cannot run the one it builds) | build `oly-schema` with the engine's `schema-tool` preset and pass its path: `-DOLY_SCHEMA_TOOL=<path to oly-schema>` |
| `Emscripten X is not the tested version Y (toolchain.env)` | the configure found an Emscripten other than the one the engine is tested with | install emsdk Y, or configure with `-DOLY_ALLOW_UNTESTED_TOOLCHAIN=ON` to turn the error into a warning |
| `GCC X is older than the minimum Y (toolchain.env)`, `MinGW GCC X ...` | the host or MinGW compiler is older than the minimum in `toolchain.env` | install GCC (or MinGW GCC) Y or newer; if `build/schema-tool` reports GCC 11, it is a cache of the old image: delete `build/schema-tool` |
| `CMake X is older than the minimum Y (toolchain.env)` | the CMake that configures is older than the minimum | install CMake Y or newer |
| `Ninja X is older than the minimum Y (toolchain.env)` | the Ninja generator is older than the minimum | install Ninja Y or newer |
| `comes from DIMENSION; do not list it in PLUGINS` | `essentials2d` or `essentials3d` is in `PLUGINS` | remove it: `DIMENSION` turns it on |
| `repeated name "Position"` | the game's `.oly` declares a component that `essentials` already brings | delete it from the `.oly` ([Transforms](concepts/transforms.md)) |
| `dead entity` | the entity was destroyed (maybe by another handler, or its state ended) | check `isAlive` before using a stored handle |
| `entity does not have the component` | `get`/`set` on a component the entity lacks | check with `has`, or add the component at spawn |
| `direct structural change during frame; use context.spawn/destroy/add/remove` | you used `engine.world.spawn/destroy/add/remove` inside a system | use the `context` methods |
| `structural change during eachBlock` | a direct change inside a query loop (handler, task, hook or outside the loop) | collect entities in an array and change them after the loop |
| `block used outside the eachBlock callback` | you kept a `Block` and used it later | read what you need inside the callback |
| `EventBatch used outside the handler call` | you kept a batch view | copy the values inside the handler |
| `SystemContext used outside the system call` (or handler/task/hook) | a closure kept the context and ran later | use the context passed to the new call (a task gets its own `TaskContext`) |
| `query with N rows; expected 1` | `single()` found 0 or several entities | use `trySingle()` where 0 is valid; check you did not spawn two players |
| `a tag cannot go in read or write; use with/without` | `read: [Enemy]` with a tag | move it to `with` |
| `type is not in the query's write` | `block.write(T)` for a type in `read` | put the type in `write` |
| `add e=...`, `remove e=...`, `destroy e=...` or `spawn during iteration` from inside an observer | a direct structural change in an observer | record it with `context.commands()` |
| `onAdd type=... during iteration` / `onRemove ...` / `observeById ...` / `removeObserver id=... during iteration` | an observer registered or removed inside a query loop or another observer | register observers at start-up |
| `removeObserver id=...: unknown or already removed` | the `ObserverId` was removed before, or never returned by this `World` | keep each id once; remove it once |
| `observeById: unknown component id=...` | the `ComponentId` was never registered in this process | get it from `componentId<T>()` or from the schema registry |
| `imply type=... during iteration` | `imply<T, Implied>` called inside a query loop or an observer | register implications at start-up |
| `imply type=... after an archetype contains it` | `imply<T, Implied>` called after an entity already had `T` | register implications at start-up, before spawning |
| `remove e=... type=... implied by ...` | removing an implied component while its implier stays | remove the implying component instead |
| `registration during frame` | `addSystem`/`on`/`onEnter` called from inside a system or handler | register everything at start-up |
| `installPlugin: plugin "..." installed after the first frame call` | `installPlugin` after `frame` ran once | turn the plugin on with `PLUGINS` in CMake; `setUpGame` installs it before the first `frame` |
| `installPlugin: plugin "..." already installed` | the plugin was installed by hand and by `setUpGame`, or twice | install it once; with `PLUGINS`, never by hand |
| `plugin: plugin "..." not installed` | `engine.plugin<P>()` for a plugin the game does not turn on, or before `setUpGame` | add it to `PLUGINS`; call `setUpGame` first |
| `the last frame was interrupted by an exception; call frame before registering` | a system threw; the engine has not recovered yet | call `frame` once, then register |
| `frame is not reentrant` | `engine.frame` called from inside a system | never call `frame` from game code |
| `time=... less than the previous frame's` | the time passed to `frame` went backwards | use a monotonic clock (`performance.now()`, rAF timestamps) |
| `no native system with that name (declare it with OLY_SYSTEM)` | typo, or the `.cpp` is not in `native/`, or the WASM is old | check the name and rebuild |
| `the native handler is for another event type` | `addNativeHandler(EventA, "x")` for an `OLY_HANDLER(x, EventB)` | pass the matching event |
| `action of kind button, expected axis2d` | `axis2d(shoot)` on a button action | read with the method of the action's kind |
| `button outside the W3C table` | a key name that does not exist (`"W"` instead of `"KeyW"`) | use `KeyboardEvent.code` names; check with `isButtonName` |
| `does not belong to this runtime's schema` | an entry from another generated schema module | import the entries from the game's `schema.generated.js` |
| `invalid type name` / `invalid field` | schema naming rules | `PascalCase` types, `camelCase` fields, no C++ keywords |
| `<game>.oly:<line>:<column>: ...` when building | a schema rule is broken | fix that line; the rules are in [Schema](concepts/schema.md#naming-rules) |
| `<game> is a web game, but this build is native` (or the reverse) when configuring | the game's `ENVIRONMENT` is not the environment of the preset | wrap the game's `CMakeLists.txt` in `if(OLY_ENVIRONMENT STREQUAL "...")`, see [Getting started on the desktop](environments/native/getting-started.md#build-it) |
| `declareSchema: the Engine already declares events, actions or states` | `setUpGame` called after a manual `declareEvent`, `declareState` or action declaration, or called twice | call `setUpGame` once, first, and declare nothing by hand |
| `<file>.oly:<line>:<column>: repeated name "X"; first declared at <file>.oly:<line>:<column>` when building | the game and a plugin it turns on (or two plugins) declare the same name | rename the game's type; names are flat across the merged schema ([Types that come from plugins](concepts/schema.md#types-that-come-from-plugins)) |
| `a plugin declares only components, tags and events; action "x" is not allowed` (or `state "x"`) when building | an action or a state in a plugin's `.oly` | declare actions and states in the game's `.oly` |
| `unknown plugin <name>` when configuring | a name in `PLUGINS` with no `oly_add_plugin` | check the name; add the plugin's folder to the build before the games |
| `plugin cycle: a -> b -> a` when configuring | two plugins list each other in `PLUGINS` | remove one of the dependencies |
| `SDL_InitSubSystem(video): ...` or `SDL window: ...` (`std::runtime_error` from `Window`) | no display or video driver on the machine | run on a desktop session, or set `SDL_VIDEO_DRIVER=dummy` for a headless run |
| `Engine already disposed` / `World already disposed` | use after `dispose()` | keep the engine alive while the game runs |
| `World belongs to the Engine; use engine.dispose()` | `engine.world.dispose()` | dispose the engine instead |

## "It does nothing" — silent problems

| Symptom | Likely cause |
|---|---|
| a system never runs | it has `.inState(...)` and no state is active (did you call `requestState` before the first frame?) or it is `.whilePaused()` and the game is not paused |
| nothing runs after pausing | no `.whilePaused()` system calls `resume()` |
| the next state is frozen | the game was paused when the state changed; resume in an `onExit` hook |
| a spawned entity is not found in the same phase | system changes are deferred to the end of the phase |
| entities disappear on state change | they were created through a context while a state was active: they are scoped. Use `persist`, or create them with `engine.world.spawn` before the first frame |
| a timer stopped firing | it was scoped to a state that ended, or the game is paused (game clock stopped) |
| a handler sees the same entity twice | two emitters reported it; deduplicate in the handler |
| events from Render are late | events emitted in Render are dispatched in the next step |
| a key does not respond | it is not bound, or another element has focus; check `engine.actions.isBound("KeyX")` |
| `pressed` never true in a test | input is captured at the start of a step: deliver the input, then call `frame` |
| the game jumps after alt-tab | not using `runInBrowser` (or not calling `setHidden`) |
| the right-click menu opens | `MouseRight` is not bound; prevent `contextmenu` on the canvas yourself |

## Rebuild checklist

After changing the schema:

**Web**

1. `./spur ts` — builds `oly-schema`, generates the TS schemas and compiles the TS;
2. build the game (`./spur game <game>` or `./spur games-test`) — regenerates `schema.generated.hpp` and the WASM;
3. reload the page (or rerun the tests).

**Desktop**

1. build the game (`./spur desktop <game>` or `./spur native-games-test`) — `oly-schema` regenerates `schema.generated.hpp`;
2. run it again.

## What Oly does not do

Out of scope by design: rendering, physics, audio, camera, UI, assets, networking, serialization, editor, threads and gamepad. Render and collision belong to the game: draw in a `Render` system ([Engine, loop and systems](concepts/engine-and-systems.md#phases)) and test overlaps in `UpdateAfter` ([Recipes](recipes.md#pickups)). More in [limits](../architecture/limits.md).

ECS v1 also has no fixed-size chunks, no change detection, no enable/disable flag per component and no sparse storage.
