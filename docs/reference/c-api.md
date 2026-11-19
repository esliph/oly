# C API

[Reference](../README.md#reference) · Names in each language: [Glossary](glossary.md) · Contract: [capi.md](../architecture/capi.md)

The C API `oly*` is what a client in another language calls to reach the core. Include `oly/capi/c_api.h` and `oly/capi/engine_api.h`; both are valid C11 and C++20. The contract (`kind`, pointer validity, errors) is in [capi.md](../architecture/capi.md).

## Functions by group

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

## See also

- [Glossary](glossary.md): the C API function that matches each C++ call and each TypeScript call.
- [capi.md](../architecture/capi.md): the contract of the C API.
- [TypeScript client](typescript/README.md): the client that calls the C API in a web game.
