# Cheat sheet

[Guide index](README.md) · [Recipes](recipes.md) · [Glossary (C++ / C API / TS)](../architecture/glossary.md)

One page with every public name: C++ first, then the C API, then the TypeScript client. Each topic links to its concept page.

# C++

## Start-up

[Schema](concepts/schema.md) · [Engine, loop and systems](concepts/engine-and-systems.md#create-the-engine)

```cpp
#include "schema.generated.hpp"                           // structs, actions::, states::

oly::ecs::World world;                                    // before the Engine
oly::app::Engine engine(world, oly::app::EngineConfig{.fixedHz = 60.0, .maxStepsPerFrame = 5, .maxEventRounds = 16});
oly::app::declareSchema(engine);                          // events, actions and states of the schema, in schema order; once, on a fresh Engine
```

## Schema names

| In the `.oly` file | In C++ |
|---|---|
| `component Name { field: f32 }` | `struct Name { float field; }` (types: `f32 f64 i8 u8 i16 u16 i32 u32 entity`) |
| `tag Name` | `struct Name {}` |
| `event Name { field: u32 }` | `struct Name { uint32_t field; }`, at least one field; order = dispatch order |
| `action name: button \| axis2d \| pointer` | `actions::name` |
| `state name` | `states::name` (at most 32) |

## World

[World and entities](concepts/world-and-entities.md)

```cpp
oly::ecs::Entity entity = context.spawn(A{...}, Tag{});    // world.spawn(...) where you own the Engine
context.get<A>(entity);   context.tryGet<A>(entity);   context.has<A>(entity);   context.isAlive(entity);
context.add(entity, A{...});   context.remove<A>(entity);   context.destroy(entity);
oly::ecs::NullEntity;
oly::ecs::checkInvariants(world, &why);   oly::ecs::stats(world);    // oly/ecs/diagnostics.hpp
auto commands = world.commands();   world.flush(commands);          // CommandBuffer, bare World
oly::ecs::ObserverId id = world.onAdd<A>(function, userData);   world.onRemove<A>(function);   world.removeObserver(id);
world.observeById(oly::ecs::ObserverKind::OnAdd, componentId, function, userData);
void function(oly::ecs::ObserverContext& context);   // world(), entity(), componentId(), userData(), commands()
world.imply<A, B>();                                              // adding A adds B; before any entity has A
```

## Queries

[Queries](concepts/queries.md)

```cpp
context.query<A, const B>(oly::ecs::With<Tag>{}, oly::ecs::Without<Other>{}).each([](A& a, const B& b) {});
context.query<A>().each([](oly::ecs::Entity entity, A& a) {});
context.query<A, const B>().eachBlock([](std::span<A> as, std::span<const B> bs) {});
auto [entity, a] = context.query<A>(oly::ecs::With<Player>{}).single();
auto row = context.query<A>().trySingle();               // std::optional<std::tuple<Entity, A&>>
```

## Engine

[Engine, loop and systems](concepts/engine-and-systems.md)

| Call | Notes |
|---|---|
| `frame(nowSeconds)` | time cannot go back |
| `setHidden(bool)` | the host calls it |
| `pause()`, `resume()`, `paused()` | effect from the next step |
| `time()`, `fixedDeltaTime()`, `interpolation()` | seconds |
| `world()`, `actions()`, `schedule()` | |
| `addSystem(phase, name, function, userData)` | returns `SystemId`; `setWhilePaused(id, true)`, `setSystemStates(id, {states::a})` |
| `declareEvent<T>(name)`, `on<T>(name, function, userData)` | `on` returns `HandlerId`; `setHandlerStates(id, {...})` |
| `requestState(state)`, `state()` | `oly::app::NoState` until the first change |
| `onEnter(state, name, function)`, `onExit(state, name, function)` | |

Phases: `oly::app::Phase::UpdateBefore`, `Update`, `UpdateAfter`, `Render`.

Fixed step: capture → tasks → UpdateBefore → Update → UpdateAfter → events → state change. Render once per frame.

## Contexts

| | `SystemContext` | `HandlerContext` | `TaskContext` | `StateContext` |
|---|---|---|---|---|
| structural changes | deferred to end of phase | direct | direct | direct |
| extra members | `phase()`, `name()`, `userData()`, `interpolation()` | `events<T>()`, `eventCount()`, `name()`, `userData()` | `handle()`, `lastRun()`, `payload<P>()` | `fromState()`, `toState()`, `entering()` |

All contexts: `time()`, `fixedDeltaTime()`, `paused()`, `pause()`, `resume()`, `state()`, `requestState()`, `actions()`, `schedule()`, `query`, `get`, `tryGet`, `has`, `isAlive`, `emit`. The ones that change the world also have `spawn`, `destroy`, `add`, `remove`, `persist`.

## Events

[Events](concepts/events.md)

```cpp
context.emit(Event{...});
OLY_HANDLER(name, Event) { for (const Event& event : events) {} }
engine.on<Event>("name", [](oly::app::HandlerContext& context) { name(context, context.events<Event>()); });
```

## Input

[Input](concepts/input.md)

```cpp
engine.actions().bind(actions::jump, oly::input::Key::Space);
engine.actions().bind(actions::move, oly::input::Axis2dBinding{Key::W, Key::S, Key::A, Key::D});
engine.actions().clearBindings(actions::jump.id);   engine.actions().isBound(oly::input::Key::W);

context.actions().button(actions::shoot);    // { pressed, held, released }
context.actions().axis2d(actions::move);     // { x, y }, y down, diagonal normalized
context.actions().pointer(actions::aim);     // { x, y, valid }, logical pixels

engine.actions().buttonDown(Key::Space);   engine.actions().buttonUp(Key::Space);
engine.actions().pointerMove(x, y);        engine.actions().releaseAll();
```

## Schedule

[Schedule](concepts/schedule.md)

```cpp
oly::app::TaskHandle handle = context.schedule().after(seconds, function, Payload{...});   // payload: trivially copyable, ≤ 32 B
context.schedule().every(seconds, function);
context.schedule().cancel(handle);      // bool
context.schedule().isPending(handle);   // bool
context.schedule().persist(handle);     // survives state changes
// function: void (*)(oly::app::TaskContext&); context.payload<Payload>(), context.lastRun()
```

## Scope

[States](concepts/states.md#scope-automatic-clean-up)

| Created with | Scoped to the active state? |
|---|---|
| `context.spawn` | yes |
| `schedule().after/every` | yes |
| `world.spawn` | no |

`context.persist(entity)` and `schedule().persist(handle)` remove the scope.

## Macros

[Engine, loop and systems](concepts/engine-and-systems.md#systems-in-c-registered-by-name-oly_system-oly_handler-oly_state_hook)

```cpp
OLY_SYSTEM(name) { /* context: oly::app::SystemContext& */ }
OLY_HANDLER(name, Event) { /* context, events: std::span<const Event> */ }
OLY_STATE_HOOK(name) { /* context: oly::app::StateContext& */ }
```

## Native host and build

[Getting started on the desktop](environments/native/getting-started.md)

```cpp
oly::input::isKey(usage);                                 // true if the HID usage is one of Key

oly::native_host::Window window({"Title", 640, 480});     // throws std::runtime_error if SDL cannot open it; the size is the logical size
window.window();  window.renderer();                      // SDL_Window*, SDL_Renderer*
oly::native_host::run(engine, window, {.maxFrames = 600, .frameSeconds = 1.0 / 60});   // exit code; maxFrames 0 = until closed
oly::native_host::keyButtonOf(SDL_SCANCODE_W);            // std::optional<PhysicalButton>
```

```cmake
oly_add_game(<game>_game ENVIRONMENT web|native SCHEMA <game>.oly SOURCES <files> DEPENDS <modules>)
```

# C API

`#include "oly/capi/c_api.h"` and `#include "oly/capi/engine_api.h"` (valid C11 and C++20). A client in another language calls these. The contract (`kind`, pointer validity, errors) is in [capi.md](../architecture/capi.md).

| Group | Functions |
|---|---|
| World and schema | `olyWorldCreate/Destroy/CheckInvariants`, `olySchemaHash`, `olySchemaComponentCount/ComponentId`, `olySchemaEvent*`, `olySchemaAction*`, `olySchemaStateCount`, `olyComponentSize/Alignment` |
| Entities and components | `olyEntitySpawn/Destroy/IsAlive`, `olyComponentAdd/Remove/Has/Get` |
| Queries | `olyQueryCreate`, `olyQueryBegin`, `olyQueryEnd` |
| Engine and loop | `olyEngineCreate/Destroy/World/SetScriptInvoker`, `olyEngineFrame/SetHidden`, `olyEnginePause/Resume/IsPaused`, `olyEngineTime/FixedDeltaTime/Interpolation` |
| Systems | `olyEngineAddNativeSystem` (by name), `olyEngineAddScriptSystem`, `olyEngineSetWhilePaused`, `olySystemSpawn/Destroy/Add/Remove` |
| Events | `olyEngineEmit`, `olyEngineAddNativeHandler`, `olyEngineAddScriptHandler` |
| Input | `olyEngineBindButton/BindAxis2d/ClearBindings/IsBound`, `olyEngineButtonDown/Up`, `olyEnginePointerMove`, `olyEngineReleaseAll`, `olyEngineActionSnapshot` |
| Schedule | `olyEngineScheduleScriptTask`, `olyEngineCancelTask`, `olyEngineIsTaskPending`, `olyEnginePersistTask` |
| States | `olyEngineRequestState`, `olyEngineState`, `olyEngineSetSystemStates/SetHandlerStates`, `olyEngineAddNativeStateHook/AddScriptStateHook`, `olyDirectSpawn`, `olyContextPersist` |
| Script code | `OlyScriptInvoker` and `olyEngineSetScriptInvoker(engine, invoker, userData)`: the one function that runs every script system, handler, task and hook of an engine |

# TypeScript

Each topic has a page in the [TypeScript client](environments/web/typescript/README.md).

## Start-up

[Schema](environments/web/typescript/schema.md) · [Engine, loop and systems](environments/web/typescript/engine-and-systems.md)

```ts
import { loadRuntime, Phase } from "@oly/core";
import { attachBrowserInput, moduleFactoryFromUrl, runInBrowser } from "@oly/web-host";

const runtime = await loadRuntime(moduleFactoryFromUrl("./game_game.mjs"), schema);   // SchemaMismatchError
const engine = runtime.createEngine({ fixedHz: 60, maxStepsPerFrame: 5, maxEventRounds: 16 });
const world = runtime.createWorld();          // a World without engine; world.dispose()
attachBrowserInput(engine, canvas);           // .detach()
runInBrowser(engine);                         // .stop()
engine.dispose();
```

| Generated | Notes |
|---|---|
| `import schema from "../schema.generated.js"` | pass it to `loadRuntime` |

## World (and contexts)

[World and entities](environments/web/typescript/world-and-entities.md)

| Call | Returns / effect |
|---|---|
| `spawn([Types...], { Type: { field: value } })` | `Entity` |
| `destroy(entity)` | |
| `isAlive(entity)` | `boolean` |
| `has(entity, Type)` | `boolean` |
| `add(entity, Type, values?)` | replaces the value if present |
| `remove(entity, Type)` | no effect if absent |
| `get(entity, Type)` | a **copy** `{ field: number }` |
| `set(entity, Type, partialValues)` | immediate, not structural |
| `query(spec)` | cached `Query` |
| `checkInvariants()` | `string \| null` (World only) |

Helpers: `NullEntity`, `formatEntity(entity)`, `entityIndex(entity)`, `entityGeneration(entity)`.

## Queries

[Queries](environments/web/typescript/queries.md)

```ts
const query = context.query({ read: [A], write: [B], with: [TagOrType], without: [TagOrType] });

query.eachBlock((block) => {
  block.count;
  block.entity(row);
  block.read(A).field.get(row);
  block.write(B).field.set(row, value);
  block.read(A).field.raw();             // { array, base, stride }
});

query.single();      // SingleRow: .entity, .get(Type) — ContractError unless exactly 1
query.trySingle();   // SingleRow | null — ContractError if 2+
```

## Engine

[Engine, loop and systems](environments/web/typescript/engine-and-systems.md)

| Member | Notes |
|---|---|
| `frame(nowSeconds)` | not reentrant, time cannot go back |
| `setHidden(boolean)` | `runInBrowser` calls it |
| `pause()`, `resume()`, `paused` | effect from the next step |
| `time`, `fixedDeltaTime` | seconds |
| `world`, `actions`, `schedule` | |
| `addSystem(phase, name, fn)` | → `.whilePaused()`, `.inState(...states)` |
| `addNativeSystem(phase, name)` | `OLY_SYSTEM(name)` in C++ |
| `on(Event, name, (context, batch) => {})` | → `.inState(...)` |
| `addNativeHandler(Event, name)` | `OLY_HANDLER(name, Event)` in C++ |
| `requestState(state)`, `state` | `state` is `null` until the first change |
| `onEnter(state, name, fn)`, `onExit(state, name, fn)` | |
| `addNativeOnEnter(state, name)`, `addNativeOnExit(state, name)` | `OLY_STATE_HOOK(name)` in C++ |

Phases: `Phase.UpdateBefore`, `Phase.Update`, `Phase.UpdateAfter`, `Phase.Render`.

## Contexts

| | System | Handler | Task | State hook |
|---|---|---|---|---|
| structural changes | deferred to end of phase | direct | direct | direct |
| extra members | `phase`, `name`, `interpolation` | `eventType`, `name` (+ the batch argument) | `handle`, `lastRun` | `fromState`, `toState` |

All contexts: `time`, `fixedDeltaTime`, `paused`, `pause()`, `resume()`, `state`, `requestState()`, `actions`, `schedule`, `query`, `get`, `set`, `has`, `isAlive`, `emit`, `spawn`, `destroy`, `add`, `remove`, `persist`.

## Events

[Events](environments/web/typescript/events.md)

```ts
context.emit(Event, { field: value });                     // missing fields = 0
engine.on(Event, "name", (context, batch) => {
  const view = batch.read();
  for (let index = 0; index < batch.count; ++index) {
    view.field.get(index);
  }
});
```

## Input

[Input](environments/web/typescript/input.md)

```ts
engine.actions.bind(buttonAction, "KeyW");
engine.actions.bind(axisAction, { up: "KeyW", down: "KeyS", left: "KeyA", right: "KeyD" });
engine.actions.clearBindings(action);
engine.actions.isBound("KeyW");

context.actions.button(action);    // { pressed, held, released }
context.actions.axis2d(action);    // { x, y }, y down, diagonal normalized
context.actions.pointer(action);   // { x, y, valid }, canvas pixels

engine.actions.buttonDown("Space"); engine.actions.buttonUp("Space");
engine.actions.pointerMove(x, y); engine.actions.releaseAll();
```

## Schedule

[Schedule](environments/web/typescript/schedule.md)

```ts
const handle = context.schedule.after(seconds, (task) => {});
context.schedule.every(seconds, (task) => {});
context.schedule.cancel(handle);      // boolean
context.schedule.isPending(handle);   // boolean
context.schedule.persist(handle);     // survives state changes
```

## Scope

[States](environments/web/typescript/states.md)

| Created with | Scoped to the active state? |
|---|---|
| `context.spawn` | yes |
| `schedule.after/every` | yes |
| `engine.world.spawn` | no |

`context.persist(entity)` and `schedule.persist(handle)` remove the scope.

# Errors

`OLY_ASSERT` (C++ debug builds) · `ContractError` (wrong use in TS) · `SchemaMismatchError` (rebuild the WASM). See [Troubleshooting](troubleshooting.md).
