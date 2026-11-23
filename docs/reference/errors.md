# Errors

[Reference](../README.md#reference) · Fix without an error: [Fix common problems](../how-to/fix-common-problems.md)

Oly checks how you use it and fails at the wrong call, with a message that names the operation, the entity (`<index>:<generation>`) and the type. Look up the text you see:

- [C++](#c): the messages of the build and the `OLY_ASSERT` messages of the C++ code.
- [TypeScript](#typescript): the `ContractError` and `SchemaMismatchError` messages of the TypeScript client.
- [oly](#oly): the messages of the `oly` command.

Every row names its origin and gives the text in each language that has it. `none` means that language has no check or message for it. In a message, `<name>` stands for text the engine fills in; search for the words around it.

This page lists the messages a game reaches through ordinary misuse of the public API, not every `OLY_ASSERT` of the engine.

## C++

### At build time

The CMake and `oly-schema` messages are the same for every environment: a web game's `oly build` shows them too.

| Message contains | Origin | In TypeScript | Cause | Fix |
|---|---|---|---|---|
| `<target>: oly_add_game needs DIMENSION 2d\|3d`, `<value>: DIMENSION is 2d or 3d` | CMake, `oly_add_game` | same | the game's CMake has no `DIMENSION` or an unknown one | add `DIMENSION 2d` or `DIMENSION 3d` |
| `<target>: <plugin> comes from DIMENSION; do not list it in PLUGINS` | CMake, `oly_add_game` | same | `essentials2d` or `essentials3d` is in `PLUGINS` | remove it: `DIMENSION` turns it on |
| `<target> is a <environment> game, but this build is <environment>` | CMake, `oly_add_game` | same | the game's `ENVIRONMENT` is not the environment of the build | build with a preset of the game's environment, or guard the `oly_add_game` call with `if(OLY_ENVIRONMENT STREQUAL "...")` |
| `unknown plugin <name>` | CMake, `oly_add_game` | same | a name in `PLUGINS` with no `oly_add_plugin` | check the name; add the plugin's folder to the build before the games |
| `plugin cycle: a -> b -> a` | CMake, `oly_add_game` | same | two plugins list each other in `PLUGINS` | remove one of the dependencies |
| `oly-schema not found at <path>: pass -DOLY_SCHEMA_TOOL=<path to oly-schema>` | CMake, `oly_add_game` | same | a web game that fetches the engine as a subproject has no `oly-schema` for the machine (WASM cannot run the one it builds) | pass the path of an `oly-schema` built for the machine: `-DOLY_SCHEMA_TOOL=<path to oly-schema>` |
| `Emscripten <found> is not the tested version <tested> (toolchain.env)` | CMake, toolchain check | same | the configure found an Emscripten other than the one the engine is tested with | install emsdk `<tested>`, or configure with `-DOLY_ALLOW_UNTESTED_TOOLCHAIN=ON` to turn the error into a warning |
| `GCC <found> is older than the minimum <minimum> (toolchain.env)`, `MinGW GCC <found> is older than ...` | CMake, toolchain check | same | the host or MinGW compiler is older than the minimum in `toolchain.env` | install GCC (or MinGW GCC) `<minimum>` or newer |
| `CMake <found> is older than the minimum <minimum> (toolchain.env)` | CMake, toolchain check | same | the CMake that configures is older than the minimum | install CMake `<minimum>` or newer |
| `Ninja <found> is older than the minimum <minimum> (toolchain.env)` | CMake, toolchain check | same | the Ninja generator is older than the minimum | install Ninja `<minimum>` or newer |
| `<game>.oly:<line>:<column>: ...` | `oly-schema` | same | a schema rule is broken | fix that line; the rules are in [Naming rules and limits](../../engine/schema/format/README.md#naming-rules-and-limits) |
| `invalid type name "<name>": use PascalCase ([A-Z][A-Za-z0-9]*)`, `invalid field ...` | `oly-schema` | same | schema naming rules | `PascalCase` types, `camelCase` fields, no C++ keywords |
| `repeated name "<name>"` | `oly-schema` | same | the game's `.oly` declares a name twice, or a component that `essentials` already brings (`Position`) | delete it from the `.oly` ([Transforms](../explanation/concepts/transforms.md)) |
| `repeated name "<name>"; first declared at <file>.oly:<line>:<column>` | `oly-schema`, merge | same | the game and a plugin it turns on (or two plugins) declare the same name | rename the game's type; names are flat across the merged schema ([Types that come from plugins](../explanation/concepts/schema.md#types-that-come-from-plugins)) |
| `a plugin declares only components, tags and events; action "<name>" is not allowed` (or `state "<name>"`) | `oly-schema` | same | an action or a state in a plugin's `.oly` | declare actions and states in the game's `.oly` |
| `tag in Query<Ts...> is not allowed; use With<Tag>/Without<Tag> (R-QY-2)` | `ecs`, compile error | `query type=<Type>: a tag cannot go in read or write; use with/without` | a tag type in the query's component list | move it to `With<Tag>` (TypeScript: `with`) |

### At run time

In debug builds, a contract violation stops on `OLY_ASSERT`. `oly build`, `oly test` and `oly run` build in `Debug`, so a game made by `oly new` has the asserts on. In release builds (`release`, `native-games-release`, `wasm-games-release`) the contract is **not checked** and a violation is undefined behaviour. The `debug-asan`, `native-games` and `wasm-games` presets keep the asserts.

| Message contains | Origin | In TypeScript | Cause | Fix |
|---|---|---|---|---|
| `get e=<index>:<generation> type=<Type>`, `getById e=...` | `ecs` | `<operation> e=<index>:<generation> type=<Type>: dead entity` or `...: entity does not have the component` | `get` on a dead entity or on one without the component | check `isAlive` and `has`, use `tryGet`, or add the component at spawn |
| `add e=<index>:<generation> type=<Type>`, `remove e=... type=...`, `destroy e=... during iteration=<n>`, `spawn during iteration=<n>` | `ecs` | `<operation> e=... type=...: dead entity` or `...: structural change during eachBlock` | a change to a dead entity, or a direct change inside a query loop or an observer | collect the entities and change them after the loop; in an observer record the change with `ObserverContext::commands()` |
| `remove e=<index>:<generation> type=<Type> implied by <Type>` | `ecs` | none | removing an implied component while its implier stays | remove the implying component instead |
| `onAdd type=<Type> during iteration=<n>`, `onRemove type=...`, `observeById type=... during iteration=<n>`, `removeObserver id=<id> during iteration=<n>` | `ecs` | none: no observers in TypeScript | an observer registered or removed inside a query loop or another observer | register observers at start-up |
| `removeObserver id=<id>: unknown or already removed` | `ecs` | none | the `ObserverId` was removed before, or never returned by this `World` | keep each id once; remove it once |
| `observeById: unknown component id=<id>` | `ecs` | none | the `ComponentId` was never registered in this process | get it from `componentId<T>()` or from the schema registry |
| `imply type=<Type> during iteration=<n>` | `ecs` | none | `imply<T, Implied>` called inside a query loop or an observer | register implications at start-up |
| `imply type=<Type> after an archetype contains it` | `ecs` | none | `imply<T, Implied>` called after an entity already had `T` | register implications at start-up, before spawning |
| `add e=<index>:<generation> type=Parent: parent e=<index>:<generation> is dead`, `...: an entity cannot be its own parent`, `...: parent e=... is a descendant of the entity` | `essentials2d`, `essentials3d` plugins | the same text: the plugin runs in C++ in a web game too | a `Parent` that points to a dead entity, to the entity itself or to one of its descendants ([Transforms](../explanation/concepts/transforms.md#parent-and-children)) | parent only live entities, and never in a loop |
| `single: <Query type> with <n> rows; expected 1`, `trySingle: <Query type> with <n> rows; expected 0 or 1` | `ecs` | `single: query with <n> rows; expected 1`, `trySingle: query with <n> rows; expected 0 or 1` | `single` found 0 or several entities, `trySingle` found several | use `trySingle` where 0 is valid; check you did not spawn two players |
| `addSystem: system "<name>" registered during frame`, `on: handler "<name>" registered during frame`, `declareEvent: event "<name>" declared during frame`, `declareState: state "<name>" declared during frame`, `<onEnter or onExit>: hook "<name>" registered during frame` | `app` | `<operation> system=<name>: registration during frame` (`handler=<name>` and `hook=<name>` for the others) | the call ran inside a system, handler or hook | register everything at start-up |
| `on: handler "<name>" for event "<Type>" not declared`, `emit: event "<Type>" not declared` | `app` | none | the event is not in the schema, or `declareSchema` did not run | declare the event in the `.oly` and call `setUpGame` first |
| `dispatchEvents: limit of <n> event rounds exceeded; pending: <events>` | `app` | none | handlers emit events in a chain longer than `maxEventRounds` | break the cycle, or raise `maxEventRounds` ([Events](../explanation/concepts/events.md)) |
| `installPlugin: plugin "<Type>" installed after the first frame call` | `app` | none: plugins are C++ only | `installPlugin` after `frame` ran once | turn the plugin on with `PLUGINS` in CMake; `setUpGame` installs it before the first `frame` |
| `installPlugin: plugin "<Type>" already installed` | `app` | none | the plugin was installed by hand and by `setUpGame`, or twice | install it once; with `PLUGINS`, never by hand |
| `plugin: plugin "<Type>" not installed` | `app` | none | `engine.plugin<P>()` for a plugin the game does not turn on, or before `setUpGame` | add it to `PLUGINS`; call `setUpGame` first |
| `declareSchema: the Engine already declares events, actions or states` | `app`, generated by `oly-schema` | none | `setUpGame` called after a manual `declareEvent`, `declareState` or action declaration, or called twice | call `setUpGame` once, first, and declare nothing by hand |
| `frame: time <now> less than the previous <previous>` | `app` | `frame: time=<now> less than the previous frame's (<previous>)` | the time passed to `frame` went backwards | use a monotonic clock |
| `Engine: fixedHz=<value> must be positive`, `Engine: maxStepsPerFrame must be positive`, `Engine: maxEventRounds must be positive` | `app` | `createEngine fixedHz=<value>: must be a positive number`, `createEngine maxStepsPerFrame=<value>: must be a positive integer`, `createEngine maxEventRounds=<value>: must be a positive integer` | the `EngineConfig` has a zero or negative field | pass positive values |
| `<operation>: action '<name>' has kind <kind>, expected <kind>` | `input` | `<operation> action=<name>: action of kind <kind>, expected <kind>` | `axis2d(shoot)` on a button action | read with the method of the action's kind |
| `bind: invalid usage 0 (action '<name>')`, `buttonDown: invalid usage 0`, `buttonUp: invalid usage 0` | `input` | `<operation> button=<name>: button outside the W3C table` | a button with usage `0` | use the `oly::input::Key` and `MouseButton` values (TypeScript: `KeyboardEvent.code` names; check with `isButtonName`) |
| `SDL_InitSubSystem(video): ...` or `SDL window: ...` (`std::runtime_error` from `Window`) | native host | none | no display or video driver on the machine | run on a desktop session, or set `SDL_VIDEO_DRIVER=dummy` for a headless run |
| `usage: <program> [--frames <n>]` (exit code 64) | native host | none | `runGame<G>` got an unknown argument, or a `--frames` value that is not a whole number above 0 | pass `--frames <n>` with `n` above 0, or no argument |

## TypeScript

`ContractError` means you called the API in a way it does not allow: the message says the operation and why. `SchemaMismatchError` means the WASM was built from a different schema. The TypeScript client validates everything that goes through it, in release builds too ([TypeScript client](typescript/README.md)). The CMake and `oly-schema` messages are in [At build time](#at-build-time).

| Message contains | Origin | In C++ | Cause | Fix |
|---|---|---|---|---|
| `SchemaMismatchError`: `WASM schema (<hash>) differs from the TS schema (<hash>); ...; recompile the WASM` | `loadRuntime` | none | the WASM was built from a different schema than the generated TypeScript | run `oly build`, then reload the page |
| `<operation> e=<index>:<generation> type=<Type>: dead entity` | `World`, contexts | `get e=<index>:<generation> type=<Type>`, `add e=... type=...` | the entity was destroyed (maybe by another handler, or its state ended) | check `isAlive` before using a stored handle |
| `<operation> e=<index>:<generation> type=<Type>: entity does not have the component` | `World`, contexts | `get e=<index>:<generation> type=<Type>` | `get` or `set` on a component the entity lacks | check with `has`, or add the component at spawn |
| `<operation> e=... type=...: unknown field "<name>"` | `World`, contexts | none: a C++ field name is checked at compile time | a field the component does not declare | use the names of the schema |
| `<operation> e=... type=...: direct structural change during frame; use context.spawn/destroy/add/remove` | `World` | none | `engine.world.spawn/destroy/add/remove` inside a system | use the `context` methods |
| `<operation> e=... type=...: structural change during eachBlock` | `World` | `add e=... type=...`, `destroy e=... during iteration=<n>` | a direct change inside a query loop (handler, task, hook or outside the loop) | collect entities in an array and change them after the loop |
| `<operation> type=<Type>: block used outside the eachBlock callback` | `Query` block | none: C++ passes spans | you kept a `Block` and used it later | read what you need inside the callback |
| `read type=<Type>: EventBatch used outside the handler call` | `EventBatch` | none | you kept a batch view | copy the values inside the handler |
| `<operation> system=<name>: SystemContext used outside the system call` (`handler=`/`HandlerContext`, `task=`/`TaskContext`, `hook=`/`StateContext` for the others) | contexts | none | a closure kept the context and ran later | use the context passed to the new call (a task gets its own `TaskContext`) |
| `<operation>: query with <n> rows; expected 1` (`single`), `expected 0 or 1` (`trySingle`) | `Query` | `single: <Query type> with <n> rows; expected 1` | `single` found 0 or several entities | use `trySingle()` where 0 is valid; check you did not spawn two players |
| `query type=<Type>: a tag cannot go in read or write; use with/without` | `World.query` | compile error `tag in Query<Ts...> is not allowed; ...` | `read: [Enemy]` with a tag | move it to `with` |
| `write type=<Type>: type is not in the query's write (only in read or outside it)` | `Query` block | none | `block.write(T)` for a type in `read` | put the type in `write` |
| `<operation> system=<name>: registration during frame` (`handler=<name>`, `hook=<name>`) | `Engine` | `addSystem: system "<name>" registered during frame` | `addSystem`, `on` or `onEnter` called from inside a system or handler | register everything at start-up |
| `<operation> <subject>: the last frame was interrupted by an exception; call frame before registering` | `Engine` | none | a system threw; the engine has not recovered yet | call `frame` once, then register |
| `frame: called from inside a system or handler (frame is not reentrant)` | `Engine` | none | `engine.frame` called from inside a system | never call `frame` from game code |
| `frame: time=<now> is not a finite number` | `Engine` | none | `NaN` or `Infinity` passed to `frame` | pass a finite time |
| `frame: time=<now> less than the previous frame's (<previous>)` | `Engine` | `frame: time <now> less than the previous <previous>` | the time passed to `frame` went backwards | use a monotonic clock (`performance.now()`, rAF timestamps) |
| `addNativeSystem name=<name>: no native system with that name (declare it with OLY_SYSTEM)` (also `addNativeHandler ... OLY_HANDLER`, `... OLY_STATE_HOOK`) | `Engine` | none: the C API returns `OLY_INVALID_SYSTEM`, `OLY_INVALID_HANDLER` or `OLY_INVALID_STATE_HOOK` | typo, or the `.cpp` is not in `native/`, or the WASM is old | check the name and rebuild |
| `addNativeHandler name=<name> event=<Type>: the native handler is for another event type` | `Engine` | none | `addNativeHandler(EventA, "x")` for an `OLY_HANDLER(x, EventB)` | pass the matching event |
| `<operation> action=<name>: action of kind <kind>, expected <kind>` | `ActionMap` | `<operation>: action '<name>' has kind <kind>, expected <kind>` | `axis2d(shoot)` on a button action | read with the method of the action's kind |
| `<operation> button=<name>: button outside the W3C table` | `ActionMap` | `bind: invalid usage 0 (action '<name>')` | a key name that does not exist (`"W"` instead of `"KeyW"`) | use `KeyboardEvent.code` names; check with `isButtonName` |
| `type=<Type>: type does not belong to this runtime's schema` (also `event=<Type>: type does not belong to this runtime's schema`, `action=<name>: action does not belong to this runtime's schema`, `state=<name>: state does not belong to this runtime's schema`) | `Runtime` | none | an entry from another generated schema module | import the entries from the game's `schema.generated.js` |
| `createEngine fixedHz=<value>: must be a positive number` (and `maxStepsPerFrame`, `maxEventRounds`: a positive integer) | `Runtime` | `Engine: fixedHz=<value> must be positive` | the config has a zero or negative field | pass positive values |
| `<operation>: Engine already disposed`, `<operation>: World already disposed` | `Engine`, `World` | none | use after `dispose()` | keep the engine alive while the game runs |
| `dispose: World belongs to the Engine; use engine.dispose()` | `World` | none | `engine.world.dispose()` | dispose the engine instead |
| `canvas with id "<id>" not found on the page` (`Error`, also written into the page as `Error starting the game: ...`) | `runGame` | none | the `canvas` option is an id that finds no element, or an element that is not a `<canvas>` | give the `<canvas>` that id in the HTML, or pass the element || `stop: game in frame; stop after the frame` | `BrowserGame` | none | `stop()` called from a system, handler, hook or task | set a flag and call `stop()` after the frame, from an event handler of the page |

## oly

The messages of [`oly`](cli.md) start with `oly:` (the launcher) or `oly <command>:` (the tasks of the game). Both exit with 64 for a wrong use and with 1 for a failure. A command `oly` does not know is refused by `spur`, which lists the commands. They have no C++ or TypeScript form.

| Message contains | Origin | Cause | Fix |
|---|---|---|---|
| `not inside an Oly game: no oly.env in <folder> or above` | launcher | the current folder is not inside a game | `cd` into the game, or make one with `oly new` |
| `oly.env has no OLY_VERSION`, `OLY_VERSION in ... is not a valid version` | launcher | `oly.env` lost the line, or the version has characters that are not letters, digits, `.` or `-` | write `OLY_VERSION=<version>`, or run `oly upgrade <version>` |
| `could not install oly-tools <version> from <url>` | launcher | the download failed: no network, a version that was never released, or a wrong `OLY_RELEASE_URL` | check the network and that the release `v<version>` exists; run again |
| `could not read the latest version from <url>` | launcher | `oly new` or `oly doctor` outside a game could not reach the latest release | check the network, or pass `oly new --version <version>` |
| `oly.env pins <version> but package.json has other @oly tarballs: run oly upgrade <version>` | `oly build` | the two files name different engine versions, after a hand edit or an interrupted upgrade | run `oly upgrade <version>` ([Upgrade the engine](../how-to/upgrade-the-engine.md)) |
| `EMSDK is not set: install emsdk <version> and source emsdk_env.sh` | `oly build` | `emcc` is on the `PATH` but `emsdk_env.sh` was not sourced in this shell (without `emcc`, `oly doctor` reports `missing emcc` first) | install emsdk at that version and run `source <emsdk>/emsdk_env.sh`, or build with `--docker` |
| `the host tools do not match toolchain.env (see above)` | `oly build` | a line of the check above this one is not `ok` | install what that line says, or build with `--docker` |
| `unsupported host <name>: oly runs on Linux and on Windows in Git Bash` | launcher | macOS or another system | use Linux, or Git Bash on Windows |
| `<game> already exists` | `oly new` | `oly new` does not write over a folder | pick another name, or remove the folder |
| `game name must match [a-z][a-z0-9-]*` | `oly new` | the name has capitals, spaces or `_`, or starts with a digit | rename the game |
| `is a CMake or engine target name: pick another game name` | `oly new` | `test`, `all`, `clean`, `help`, `install`, `package`, `oly` and `oly-*` are targets | pick another game name |
| `--platform windows is for native games` | `oly build`, `oly test`, `oly run` | `--platform windows` on a web game | build a web game without it |
| `a desktop window cannot open in the container: run <path> on the host` | `oly run` | `oly run --docker` on a desktop game | run it on the host: `oly run --host` |
| `OLY_ENGINE_DIR is not a directory` | launcher | the checkout path is wrong | fix the path ([local mode](cli.md#local-mode)) |
| `this launcher runs from a checkout (version dev)` | `oly self-update` | `oly self-update` on the launcher of an engine checkout | install a release with the `install.sh` line of [Install](cli.md#install) |

## See also

- [The `oly` command](cli.md): `oly doctor` prints `ok`, `old`, `untested`, `missing` or `wrong` for each tool.
- [Fix common problems](../how-to/fix-common-problems.md): problems that give no message.
- [TypeScript client](typescript/README.md): the error classes.
