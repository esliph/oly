# C API

[Reference](../README.md#reference) · Names in each language: [Glossary](glossary.md) · Design: [The C API](../explanation/architecture/capi.md)

The C API `oly*` is what a client in another language calls to reach the core. Include `oly/capi/c_api.h` for the world and the schema, and `oly/capi/engine_api.h` for the engine; both are valid C11 and C++20. This page lists every function, type and constant of the two headers.

A **contract violation** stops on `OLY_ASSERT` in a build with asserts on. In a release build the contract is not checked and a violation is undefined behaviour ([Errors](errors.md#c)).

Every function has the `OLY_API` export mark. It is left out of the signatures below.

- [World and schema](#world-and-schema)
- [Entities and components](#entities-and-components)
- [Queries](#queries)
- [Engine and loop](#engine-and-loop)
- [Systems](#systems)
- [Events](#events)
- [Input](#input)
- [Schedule](#schedule)
- [States](#states)
- [Types](#types)
- [Constants](#constants)

## World and schema

### `olyWorldCreate`

```c
OlyWorld* olyWorldCreate(void);
```

Creates a world with no engine on top of it, for a client that only needs the ECS. The caller owns it and ends it with `olyWorldDestroy`.

### `olyWorldDestroy`

```c
void olyWorldDestroy(OlyWorld* world);
```

Destroys a world made by `olyWorldCreate`. Never pass the world of an engine (`olyEngineWorld`): `olyEngineDestroy` ends that one.

### `olyWorldCheckInvariants`

```c
int32_t olyWorldCheckInvariants(OlyWorld* world, char* message, uint32_t capacity);
```

Checks the structural invariants of the world. Returns 1 when they hold. Returns 0 when one fails, with the reason truncated into `message` and NUL-terminated. With `capacity` 0, `message` is not written.

### `olySchemaHash`

```c
uint32_t olySchemaHash(void);
```

Returns the hash of the merged schema the module was built with. A client compares it with the hash of its own generated schema.

### `olySchemaComponentCount`

```c
uint32_t olySchemaComponentCount(void);
```

Returns the number of components and tags in the schema.

### `olySchemaComponentId`

```c
uint32_t olySchemaComponentId(uint32_t schemaIndex);
```

Returns the `ComponentId` of the component or tag at `schemaIndex`. `schemaIndex` must be below `olySchemaComponentCount()`; the call does not check it.

### `olyComponentSize`

```c
uint32_t olyComponentSize(uint32_t componentId);
```

Returns the size in bytes of a component, and 0 for a tag.

### `olyComponentAlignment`

```c
uint32_t olyComponentAlignment(uint32_t componentId);
```

Returns the alignment in bytes of a component.

### `olySchemaEventCount`

```c
uint32_t olySchemaEventCount(void);
```

Returns the number of events in the schema.

### `olySchemaEventSize`

```c
uint32_t olySchemaEventSize(uint32_t eventIndex);
```

Returns the size in bytes of the event at `eventIndex`. `eventIndex` must be below `olySchemaEventCount()`; the call does not check it. The index in the schema is also the event type in the engine (`olyEngineEmit`, `olyEngineAddNativeHandler`, `olyEngineAddScriptHandler`).

### `olySchemaEventAlignment`

```c
uint32_t olySchemaEventAlignment(uint32_t eventIndex);
```

Returns the alignment in bytes of the event at `eventIndex`, with the same precondition as `olySchemaEventSize`.

### `olySchemaActionCount`

```c
uint32_t olySchemaActionCount(void);
```

Returns the number of input actions in the schema.

### `olySchemaActionKind`

```c
uint32_t olySchemaActionKind(uint32_t actionIndex);
```

Returns the kind of the action at `actionIndex`: `OLY_ACTION_BUTTON`, `OLY_ACTION_AXIS2D` or `OLY_ACTION_POINTER`. `actionIndex` must be below `olySchemaActionCount()`; the call does not check it. The index in the schema is also the `ActionId` in the engine.

### `olySchemaStateCount`

```c
uint32_t olySchemaStateCount(void);
```

Returns the number of states in the schema. The index of a state in the schema is its `StateId` in the engine.

## Entities and components

A structural change (spawn, destroy, add, remove) needs no open iteration. `olyEntityDestroy`, `olyComponentAdd` and `olyComponentRemove` also need an alive entity. A structural change invalidates every pointer that `olyComponentGet` returned; only the pair `(index, generation)` stays valid as a handle.

### `olyEntitySpawn`

```c
void olyEntitySpawn(OlyWorld* world, OlyEntity* out);
```

Creates an entity with no component and writes its handle to `out`. Breaks the contract during an iteration.

### `olyEntityDestroy`

```c
void olyEntityDestroy(OlyWorld* world, uint32_t index, uint32_t generation);
```

Destroys the entity. Breaks the contract for a dead entity or during an iteration.

### `olyEntityIsAlive`

```c
int32_t olyEntityIsAlive(OlyWorld* world, uint32_t index, uint32_t generation);
```

Returns 1 if the handle names a living entity and 0 otherwise. It is the one call that accepts a dead handle.

### `olyComponentAdd`

```c
void olyComponentAdd(OlyWorld* world, uint32_t index, uint32_t generation, uint32_t componentId, void* value);
```

Moves the value from `value`, which holds `olyComponentSize(componentId)` bytes, into the entity. If the entity already has the component, the call replaces it. For a tag, `value` is ignored and may be `NULL`. Breaks the contract for a dead entity or during an iteration.

### `olyComponentRemove`

```c
void olyComponentRemove(OlyWorld* world, uint32_t index, uint32_t generation, uint32_t componentId);
```

Removes the component from the entity. Breaks the contract for a dead entity or during an iteration.

### `olyComponentHas`

```c
int32_t olyComponentHas(OlyWorld* world, uint32_t index, uint32_t generation, uint32_t componentId);
```

Returns 1 if the entity has the component and 0 otherwise.

### `olyComponentGet`

```c
void* olyComponentGet(OlyWorld* world, uint32_t index, uint32_t generation, uint32_t componentId);
```

Returns a pointer to the component's value, owned by the world. Returns `NULL` if the entity is dead, the component is absent or the component is a tag. The pointer is valid until the next structural change.

## Queries

### `olyQueryCreate`

```c
OlyQuery* olyQueryCreate(OlyWorld* world, const uint32_t* componentIds, uint32_t readCount, uint32_t writeCount, uint32_t withCount, uint32_t withoutCount);
```

Returns the query for a combination of components. `componentIds` holds the read, write, with and without ids, in that order, concatenated. The query belongs to the world and has no destroy call. The world caches it by spec: the same combination returns the same pointer. In the read and write lists, an unknown, repeated or tag component is a contract violation.

### `olyQueryBegin`

```c
OlyIteration* olyQueryBegin(OlyWorld* world, OlyQuery* query, uint32_t* outBlockCount, const uintptr_t** outDescriptors);
```

Starts an iteration. `*outBlockCount` receives the number of blocks (non-empty tables), and `*outDescriptors` points to `blocks × (2 + readCount + writeCount)` values of `uintptr_t`. Each block is `[count, address of the entities, address of each column (read, then write)...]`. The descriptors stay valid until the `olyQueryEnd` of that iteration, even with nested iterations. Every `olyQueryBegin` needs an `olyQueryEnd`; until then, a structural change in the world is a contract violation.

### `olyQueryEnd`

```c
void olyQueryEnd(OlyWorld* world, OlyIteration* iteration);
```

Ends the innermost open iteration. Any other order is a contract violation.

## Engine and loop

### `olyEngineCreate`

```c
OlyEngine* olyEngineCreate(double fixedHz, uint32_t maxStepsPerFrame, uint32_t maxEventRounds);
```

Creates an engine with its own world. `fixedHz`, `maxStepsPerFrame` and `maxEventRounds` must be positive. The call declares the schema with `oly::app::declareSchema`, so the index of an event in the schema is its event type, the index of an action is its `ActionId` and the index of a state is its `StateId`. It then installs the game's plugins. The caller owns the engine and ends it with `olyEngineDestroy`.

### `olyEngineDestroy`

```c
void olyEngineDestroy(OlyEngine* engine);
```

Destroys the engine and its world. It calls the invoker with `OLY_SCRIPT_TASK_DISCARD` for each pending script task, so the `userData` of the invoker must stay valid until then. During those calls the invoker must not call any `oly*` function on that engine.

### `olyEngineWorld`

```c
OlyWorld* olyEngineWorld(OlyEngine* engine);
```

Returns the world of the engine. The engine owns it: it stays valid until `olyEngineDestroy`, and it is never passed to `olyWorldDestroy`.

### `olyEngineSetScriptInvoker`

```c
void olyEngineSetScriptInvoker(OlyEngine* engine, OlyScriptInvoker invoker, void* userData);
```

Installs the one function that runs every script system, handler, task and hook of this engine, and the pointer it receives. `invoker` must not be null. The call is valid inside and outside `olyEngineFrame`, and the next script call uses the new invoker. A script call before the first `olyEngineSetScriptInvoker` is a contract violation that names the `kind` and the `id`. How the invoker uses `kind` is in [The C API](../explanation/architecture/capi.md#the-script-invoker).

### `olyEngineFrame`

```c
void olyEngineFrame(OlyEngine* engine, double nowSeconds);
```

Runs one frame at the real time `nowSeconds`: the fixed steps that are due, then the `Render` phase. The contract of the loop is that of `oly::app::Engine` ([Engine, loop and systems](../explanation/concepts/engine-and-systems.md#the-loop)).

### `olyEngineSetHidden`

```c
void olyEngineSetHidden(OlyEngine* engine, int32_t hidden);
```

Tells the engine that the tab is hidden (non-zero) or visible (0).

### `olyEnginePause`

```c
void olyEnginePause(OlyEngine* engine);
```

Pauses the game.

### `olyEngineResume`

```c
void olyEngineResume(OlyEngine* engine);
```

Resumes the game.

### `olyEngineIsPaused`

```c
int32_t olyEngineIsPaused(OlyEngine* engine);
```

Returns 1 while the game is paused and 0 otherwise.

### `olyEngineTime`

```c
double olyEngineTime(OlyEngine* engine);
```

Returns the game time in seconds. It stops while the game is paused.

### `olyEngineFixedDeltaTime`

```c
double olyEngineFixedDeltaTime(OlyEngine* engine);
```

Returns the seconds of one fixed step, `1 / fixedHz`.

### `olyEngineInterpolation`

```c
double olyEngineInterpolation(OlyEngine* engine);
```

Returns how far, from 0 to 1, the real time is between the last fixed step and the next one.

## Systems

### `olyEngineAddNativeSystem`

```c
uint32_t olyEngineAddNativeSystem(OlyEngine* engine, uint32_t phase, const char* name);
```

Registers the C++ system that `OLY_SYSTEM` defined under `name`, in `phase`, and returns its system id. Returns `OLY_INVALID_SYSTEM` if no native system has that name. `phase` is an `oly::app::Phase`; a value out of range is a contract violation. Registering during `olyEngineFrame` is a contract violation.

### `olyEngineAddScriptSystem`

```c
uint32_t olyEngineAddScriptSystem(OlyEngine* engine, uint32_t phase, const char* name);
```

Registers a script system and returns its system id. The invoker receives that id with `OLY_SCRIPT_SYSTEM` once per system per step. The same rules about `phase` and about registering during `olyEngineFrame` apply.

### `olyEngineSetWhilePaused`

```c
void olyEngineSetWhilePaused(OlyEngine* engine, uint32_t systemId, int32_t whilePaused);
```

Makes the system run only while the game is paused (non-zero), or removes that restriction (0).

### `olySystemSpawn`

```c
void olySystemSpawn(OlyEngine* engine, OlyEntity* out);
```

Records a spawn in the command buffer of the running system and writes the new entity's handle to `out`. The change applies at the end of the phase. Valid only inside a script system; outside it is a contract violation.

### `olySystemDestroy`

```c
void olySystemDestroy(OlyEngine* engine, uint32_t index, uint32_t generation);
```

Records a destroy for the end of the phase. Valid only inside a script system.

### `olySystemAdd`

```c
void olySystemAdd(OlyEngine* engine, uint32_t index, uint32_t generation, uint32_t componentId, void* value);
```

Records an add for the end of the phase. `value` holds `olyComponentSize(componentId)` bytes. Valid only inside a script system.

### `olySystemRemove`

```c
void olySystemRemove(OlyEngine* engine, uint32_t index, uint32_t generation, uint32_t componentId);
```

Records a remove for the end of the phase. Valid only inside a script system.

## Events

### `olyEngineEmit`

```c
void olyEngineEmit(OlyEngine* engine, uint32_t eventType, const void* value);
```

Copies the event, which has the size of its type, into the queue. Valid inside and outside `olyEngineFrame`. `eventType` must be below `olySchemaEventCount()`; otherwise it is a contract violation. The engine dispatches emitted events in step 4 of the fixed step, batched by type.

### `olyEngineAddNativeHandler`

```c
uint32_t olyEngineAddNativeHandler(OlyEngine* engine, uint32_t eventType, const char* name);
```

Registers the C++ handler that `OLY_HANDLER` defined under `name` and returns its handler id. Returns `OLY_INVALID_HANDLER` if no native handler has that name, and `OLY_HANDLER_TYPE_MISMATCH` if the named handler is for another event type. Registering during `olyEngineFrame` is an error.

### `olyEngineAddScriptHandler`

```c
uint32_t olyEngineAddScriptHandler(OlyEngine* engine, uint32_t eventType, const char* name);
```

Registers a script handler and returns its handler id. The invoker receives that id with `OLY_SCRIPT_HANDLER` once per batch.

## Input

An action id is the index of the action in the schema. `button` is a HID usage, with the usage page in the high 16 bits. All the calls below are valid inside and outside `olyEngineFrame`.

### `olyEngineBindButton`

```c
void olyEngineBindButton(OlyEngine* engine, uint32_t action, uint32_t button);
```

Binds a button to a button action. The call checks the range and the kind of the action; otherwise it is a contract violation.

### `olyEngineBindAxis2d`

```c
void olyEngineBindAxis2d(OlyEngine* engine, uint32_t action, uint32_t up, uint32_t down, uint32_t left, uint32_t right);
```

Binds four buttons to an axis action. It checks the range and the kind of the action like `olyEngineBindButton`.

### `olyEngineClearBindings`

```c
void olyEngineClearBindings(OlyEngine* engine, uint32_t action);
```

Removes every binding of the action. It checks only the range.

### `olyEngineIsBound`

```c
int32_t olyEngineIsBound(OlyEngine* engine, uint32_t button);
```

Returns 1 if some action binds the button and 0 otherwise.

### `olyEngineButtonDown`

```c
void olyEngineButtonDown(OlyEngine* engine, uint32_t button);
```

Reports a button press from the host. It enters the queue and becomes action state at the capture of the next step.

### `olyEngineButtonUp`

```c
void olyEngineButtonUp(OlyEngine* engine, uint32_t button);
```

Reports a button release from the host, with the same timing as `olyEngineButtonDown`.

### `olyEnginePointerMove`

```c
void olyEnginePointerMove(OlyEngine* engine, float x, float y);
```

Reports the pointer position from the host, with the same timing as `olyEngineButtonDown`.

### `olyEngineReleaseAll`

```c
void olyEngineReleaseAll(OlyEngine* engine);
```

Releases every held button, for a window that loses focus.

### `olyEngineActionSnapshot`

```c
const OlyActionState* olyEngineActionSnapshot(OlyEngine* engine);
```

Returns the address of one `OlyActionState` per action, in schema order. The engine owns the array. The address stays valid until `olyEngineDestroy`, because the engine declares every action at creation, so a client reads the actions straight from the heap.

## Schedule

### `olyEngineScheduleScriptTask`

```c
void olyEngineScheduleScriptTask(OlyEngine* engine, double delaySeconds, double intervalSeconds, uint32_t scriptId, OlyTaskHandle* out);
```

Schedules a script task on the game clock and writes its handle to `out`. With `intervalSeconds` 0 the task is an `after` that runs once after `delaySeconds`. With `intervalSeconds` above 0 it is an `every` whose first run is at `delaySeconds`. Valid inside and outside `olyEngineFrame`. The invoker receives `OLY_SCRIPT_TASK` on each run, with `eventCount` 1 on the last run of an `after`. It receives `OLY_SCRIPT_TASK_DISCARD` once if the task leaves the queue without its last run (cancellation or `olyEngineDestroy`). An invalid delay or interval is a contract violation of the `Scheduler`.

### `olyEngineCancelTask`

```c
int32_t olyEngineCancelTask(OlyEngine* engine, uint32_t slot, uint32_t generation);
```

Cancels the task. Returns 1 if the task was pending. Returns 0 if it already finished, was canceled or the handle belongs to an older task of the slot.

### `olyEngineIsTaskPending`

```c
int32_t olyEngineIsTaskPending(OlyEngine* engine, uint32_t slot, uint32_t generation);
```

Returns 1 if the task is pending and 0 otherwise.

### `olyEnginePersistTask`

```c
int32_t olyEnginePersistTask(OlyEngine* engine, uint32_t slot, uint32_t generation);
```

Takes the task out of the scope of the current state. Returns 1 if the task was pending and lost its scope, and 0 if it is not pending.

## States

### `olyEngineRequestState`

```c
void olyEngineRequestState(OlyEngine* engine, uint32_t state);
```

Requests a switch to `state`. The request is valid inside and outside `olyEngineFrame`, the last request wins and the switch happens in step 5. A state out of range is a contract violation.

### `olyEngineState`

```c
uint32_t olyEngineState(OlyEngine* engine);
```

Returns the current state, or `OLY_NO_STATE` until the first switch.

### `olyEngineSetSystemStates`

```c
void olyEngineSetSystemStates(OlyEngine* engine, uint32_t systemId, uint32_t stateMask);
```

Restricts the system to the states whose bit is set in `stateMask`, one bit per state. A `stateMask` of 0 removes the restriction. A bit of an undeclared state is a contract violation.

### `olyEngineSetHandlerStates`

```c
void olyEngineSetHandlerStates(OlyEngine* engine, uint32_t handlerId, uint32_t stateMask);
```

The same restriction for a handler.

### `olyEngineAddNativeStateHook`

```c
uint32_t olyEngineAddNativeStateHook(OlyEngine* engine, uint32_t when, uint32_t state, const char* name);
```

Registers the C++ hook that `OLY_STATE_HOOK` defined under `name`, to run when `state` is entered (`when` is `OLY_STATE_ENTER`) or left (`OLY_STATE_EXIT`). Returns the hook id, or `OLY_INVALID_STATE_HOOK` if no hook has that name. Any other value of `when` is a contract violation.

### `olyEngineAddScriptStateHook`

```c
uint32_t olyEngineAddScriptStateHook(OlyEngine* engine, uint32_t when, uint32_t state, const char* name);
```

Registers a script hook and returns its hook id. The invoker receives `OLY_SCRIPT_STATE_HOOK` with that id and the state on the other side of the switch. The `when` rule is the same.

### `olyDirectSpawn`

```c
void olyDirectSpawn(OlyEngine* engine, OlyEntity* out);
```

Spawns an entity at once and writes its handle to `out`. The entity takes the scope of the current state. Valid only inside a script handler, task or hook.

### `olyContextPersist`

```c
void olyContextPersist(OlyEngine* engine, uint32_t index, uint32_t generation);
```

Takes the entity out of the scope of the current state. Inside a script system the change is deferred; inside a script handler, task or hook it is direct, and the entity must be alive.

## Types

| Type | What it is |
|---|---|
| `OlyWorld` | opaque; a world and a stack of iteration slots. Made by `olyWorldCreate`, or owned by an `OlyEngine` |
| `OlyEngine` | opaque; an `OlyWorld` and the `Engine` on top of it. Made by `olyEngineCreate` |
| `OlyQuery` | opaque; a query that belongs to its world. Never freed |
| `OlyIteration` | opaque; the slot of an open iteration, from `olyQueryBegin` to `olyQueryEnd` |
| `OlyEntity` | `struct { uint32_t index; uint32_t generation; }`, the handle of an entity |
| `OlyTaskHandle` | `struct { uint32_t slot; uint32_t generation; }`, the handle of a script task |
| `OlyActionState` | `struct { float x; float y; uint32_t flags; }`, the state of one action, 12 bytes. It mirrors `oly::input::ActionState`. `flags` holds the `OLY_ACTION_*` bits below |
| `OlyScriptInvoker` | `void (*)(void* userData, OlyEngine* engine, uint32_t kind, uint32_t id, const void* events, uint32_t eventCount)`, the function that runs script code ([The C API](../explanation/architecture/capi.md#the-script-invoker)) |

## Constants

| Constant | Value | Use |
|---|---|---|
| `OLY_ACTION_BUTTON` | `0u` | kind of a button action (`olySchemaActionKind`) |
| `OLY_ACTION_AXIS2D` | `1u` | kind of a 2D axis action |
| `OLY_ACTION_POINTER` | `2u` | kind of a pointer action |
| `OLY_ACTION_PRESSED` | `1u` | bit of `OlyActionState.flags`: `pressed` of a button |
| `OLY_ACTION_HELD` | `2u` | bit: `held` of a button |
| `OLY_ACTION_RELEASED` | `4u` | bit: `released` of a button |
| `OLY_ACTION_VALID` | `8u` | bit: `valid` of a pointer, which has a position |
| `OLY_INVALID_SYSTEM` | `0xFFFFFFFFu` | returned by `olyEngineAddNativeSystem` for an unknown name |
| `OLY_INVALID_HANDLER` | `0xFFFFFFFFu` | returned by `olyEngineAddNativeHandler` for an unknown name |
| `OLY_HANDLER_TYPE_MISMATCH` | `0xFFFFFFFEu` | returned by `olyEngineAddNativeHandler` when the named handler is for another event type |
| `OLY_INVALID_STATE_HOOK` | `0xFFFFFFFFu` | returned by `olyEngineAddNativeStateHook` for an unknown name |
| `OLY_NO_STATE` | `0xFFFFFFFFu` | returned by `olyEngineState` until the first switch |
| `OLY_STATE_ENTER` | `0u` | `when` of a state hook: run on entering the state |
| `OLY_STATE_EXIT` | `1u` | `when` of a state hook: run on leaving the state |
| `OLY_SCRIPT_SYSTEM` | `0u` | `kind` of a script call: a system |
| `OLY_SCRIPT_HANDLER` | `1u` | `kind`: a handler batch |
| `OLY_SCRIPT_TASK` | `2u` | `kind`: a run of a task |
| `OLY_SCRIPT_TASK_DISCARD` | `3u` | `kind`: a task left the queue without its last run |
| `OLY_SCRIPT_STATE_HOOK` | `4u` | `kind`: a state hook |

## See also

- [The C API](../explanation/architecture/capi.md): how the C API is designed and how the invoker works.
- [Glossary](glossary.md): the C API function that matches each C++ call and each TypeScript call.
- [TypeScript client](typescript/README.md): the client that calls the C API in a web game.
- [Export your own C functions](../how-to/export-your-own-c-functions.md): add functions of your own next to these.
