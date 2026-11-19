# Glossary

The same verb stands for the same concept in C++, in the C API and in TS.

| Concept | C++ (`oly::ecs`, `oly::app`, `oly::input`) | C API | TS (`@oly/core`) |
|---|---|---|---|
| create entity | `World::spawn()`, `spawn(components...)`, `CommandBuffer::spawn()` | `olyEntitySpawn`, `olyDirectSpawn` (TS handler, task and hook, with the state scope) | `World.spawn` |
| destroy / alive | `destroy`, `isAlive` | `olyEntityDestroy`, `olyEntityIsAlive` | `destroy`, `isAlive` |
| add / remove / has | `add`, `remove`, `has` | `olyComponentAdd/Remove/Has` | `add`, `remove`, `has` |
| read (exists by contract) / if it exists | `get` -> `T&` / `tryGet` -> `T*` | `olyComponentGet` | `get` -> copy |
| query / iterate | `query<Ts...>`, `each`, `eachBlock` | `olyQueryCreate`, `olyQueryBegin/End` | `query`, `eachBlock` |
| read the only row of a query (exactly one / at most one) | `Query::single()` -> `tuple<Entity, Ts&...>` / `trySingle()` -> `optional` | (`olyQueryBegin/End`) | `query.single()` / `trySingle()` -> `SingleRow` (copy) |
| game (fixed) step / hidden tab | `Engine::frame(nowSeconds)`, `setHidden` | `olyEngineFrame`, `olyEngineSetHidden` | `Engine.frame`, `setHidden` |
| pause / resume | `Engine::pause`, `resume`, `paused`; `SystemContext::pause/resume` | `olyEnginePause/Resume/IsPaused` | `pause`, `resume`, `paused` |
| register system | `Engine::addSystem(phase, name, function, userData, band)`, `OLY_SYSTEM(name)` (by name) | `olyEngineAddNativeSystem` (by name), `olyEngineAddScriptSystem` | `addSystem`, `addNativeSystem` |
| order band: where a system runs inside its phase (`Default` first, then `Last`; registration order inside a band) | `OrderBand::Default`, `OrderBand::Last` (last parameter of `addSystem`) | — (always `Default`) | — (always `Default`) |
| system runs while paused | `Engine::setWhilePaused` | `olyEngineSetWhilePaused` | `SystemRegistration.whilePaused()` |
| deferred structural change (inside a system) | `SystemContext::spawn/destroy/add/remove` | `olySystemSpawn/Destroy/Add/Remove` | `context.spawn/destroy/add/remove` |
| observer: run code when an entity gains (**on add**) or loses (**on remove**) a component | `World::onAdd<T>`, `onRemove<T>`, `observeById(kind, componentId, ...)`, `removeObserver`; `ObserverContext` | — | — |
| implied component: added with the component that implies it, removed with its last implier | `World::imply<T, Implied>()` | — | — |
| declare component / tag (order = ComponentId) | the generated `struct Name { ... }` / `struct Name {}` | (schema types, when the `OlyEngine` is created) | `component Name { ... }` / `tag Name` in the `.oly` schema |
| declare event (order = dispatch order) | `Engine::declareEvent<T>(name)` | (schema events, when the `OlyEngine` is created) | `event Name { ... }` in the `.oly` schema |
| emit event | `Engine::emit`, `SystemContext/HandlerContext::emit` | `olyEngineEmit` | `context.emit` |
| declare action | `ActionMap::declareButton/declareAxis2d/declarePointer` | (schema actions, when the `OlyEngine` is created) | `action name: kind` in the `.oly` schema |
| bind action to physical button | `ActionMap::bind`, `clearBindings`, `isBound` | `olyEngineBindButton/BindAxis2d`, `olyEngineClearBindings`, `olyEngineIsBound` | `engine.actions.bind/clearBindings/isBound` |
| deliver host input | `buttonDown`, `buttonUp`, `pointerMove`, `releaseAll` | `olyEngineButtonDown/Up`, `olyEnginePointerMove`, `olyEngineReleaseAll` | `engine.actions.buttonDown/buttonUp/pointerMove/releaseAll` |
| read action in the step | `ActionMap::button/axis2d/pointer` (`context.actions()`) | `olyEngineActionSnapshot` (read without crossing) | `context.actions.button/axis2d/pointer` |
| register handler (batch per round) | `Engine::on<T>(name, function)` | `olyEngineAddScriptHandler` | `Engine.on` |
| handler by name | `OLY_HANDLER(name, EventType)` | `olyEngineAddNativeHandler` | `addNativeHandler` |
| schedule task (game clock) | `Scheduler::after/every` (`engine.schedule()`, `context.schedule()`) | `olyEngineScheduleScriptTask` | `schedule.after/every` |
| cancel / query task | `Scheduler::cancel/isPending` | `olyEngineCancelTask`, `olyEngineIsTaskPending` | `schedule.cancel/isPending` |
| declare state (order = StateId) | `Engine::declareState(name)` | (schema states, when the `OlyEngine` is created) | `state name` in the `.oly` schema |
| request / read state | `Engine::requestState`, `state`; `EngineContext::requestState/state` | `olyEngineRequestState`, `olyEngineState` | `engine.requestState`, `engine.state`, `context.requestState/state` |
| state hook | `Engine::onEnter/onExit(state, name, function)`, `OLY_STATE_HOOK(name)` (by name) | `olyEngineAddNativeStateHook` (by name), `olyEngineAddScriptStateHook` | `onEnter`, `onExit`, `addNativeOnEnter/addNativeOnExit` |
| call script code | — (C++ code is called directly) | `olyEngineSetScriptInvoker` (per engine, dispatch by `kind`) | installed by `Runtime.createEngine` |
| restrict to states | `Engine::setSystemStates/setHandlerStates` (and `*StateMask`) | `olyEngineSetSystemStates/SetHandlerStates` | `registration.inState(...states)` |
| persist (remove the state scope) | `SystemContext/DirectContext::persist`, `Scheduler::persist` | `olyContextPersist`, `olyEnginePersistTask` | `context.persist`, `schedule.persist` |
| set up the game: declare the schema (events, actions, states), then install the plugins | `oly::app::setUpGame(engine)` (generated, in `game_setup.generated.cpp`); it calls `oly::app::declareSchema(engine)` (generated, in `schema_declaration.generated.cpp`) | (done by `olyEngineCreate`) | (done by `Runtime.createEngine`) |
| plugin: engine-level functionality a game turns on in its build | `oly::app::Plugin` (`install(engine)`); `oly_add_plugin(NAME <p> ...)` in CMake, turned on with `oly_add_game(... PLUGINS ...)` | — | — |
| place an entity and nest it | `Position`, `Rotation`, `Scale`, `Parent`, `WorldTransform` (`essentials2d` or `essentials3d`, turned on by `oly_add_game(... DIMENSION 2d\|3d ...)`) | the same names, in the generated schema | the same names |
| install a plugin / reach an installed plugin | `Engine::installPlugin<P>()` / `Engine::plugin<P>()` | — | — |
| desktop window and loop | `oly::native_host::Window` (`WindowConfig`), `oly::native_host::run(engine, window, options)` | — | — |
| environment: a place where the core runs, with a host that drives the loop and forwards input | `oly_add_game(... ENVIRONMENT web\|native ...)` | — | `runInBrowser`, `attachBrowserInput` (`@oly/web-host`) |
| client: a language that calls the core through the C API (C++ is not a client) | — | `oly*` | `@oly/core` |
| game template: the files `oly new` copies into a new game | `templates/game/` | — | — |
| variant by ID | `ById` suffix: `addById`, `removeById`, `hasById`, `getById`, `tryGetById`, `queryById` | (all by ID) | — |

## Publishing

These terms have no C API or TS form. Details: [Publishing](../explanation/architecture/publishing.md).

| Term | Meaning |
|---|---|
| `oly-tools` | the archive of one platform and one version that holds `spur`, the game `Spurfile`, `toolchain.env`, `bin/oly-schema`, `template/` and `docker-compose.yaml` |
| launcher | the `oly` script (POSIX sh) in `~/.oly/bin`: finds the game, fetches the `oly-tools` of its version and runs that version's `Spurfile` |
| pinned version | the `OLY_VERSION` of a game's `oly.env`: the engine release the game builds with |
| local mode | `OLY_ENGINE_DIR` names an engine checkout, and the launcher takes the tools, the C++ source, `oly-schema` and the npm packages from it instead of from a release |
| `toolchain.env` | the file at the engine root with the toolchain versions; the Dockerfiles, `oly doctor` and the configure check read it |
| consumer test | the test that creates a game with `oly new` and builds and tests it, as a user of `oly` would |
| staged release | the release files written to a local folder (`./spur stage`) with GitHub's layout, which `OLY_RELEASE_URL` points the launcher at before anything is published |

## Naming rules

- The `oly` prefix appears only in the C API (`oly*`, `OlyWorld`, `OLY_API`) and in what is internal to `capi` (`OlyModule`). The C++ and TS APIs do not repeat `oly` in names. The reason is in [Modules and boundaries](../explanation/architecture/modules-and-boundaries.md#vocabulary-rules).
- `create` does not exist as a name for an entity: an entity is born through `spawn`. `create` stays for the other objects (`olyWorldCreate`, `olyEngineCreate`, `Runtime.createEngine`).
- "Raw" does not exist as a name: the variant by `ComponentId` uses the `ById` suffix.
- There are no aliases or compatibility `using` declarations for old names.
