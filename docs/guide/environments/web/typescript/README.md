# TypeScript client

[Guide index](../../../README.md)

The TypeScript client is the TS API over the [C API](../../../../architecture/capi.md), inside the `web` environment. It runs wherever the web host runs: a browser tab or Node. It has no DOM code of its own; drawing and sound are yours.

Oly is complete in C++. The client adds TS conveniences (typed accessors, closures, `raw()` arrays) and never engine behaviour. The rules of each concept are in the [concept pages](../../../concepts/schema.md); the pages here only show the TS form of the same calls.

## Packages

| Package | Use it for |
|---|---|
| `@oly/core` | the client: `loadRuntime`, `Engine`, the world, queries, contexts, events, input, schedule, states |
| `@oly/core/generated` | generated code only (`*.generated.ts`); never import it by hand |
| `@oly/web-host` | the web host the client runs on: `runInBrowser`, `attachBrowserInput`, the WASM loader |

## Errors

| Class | Thrown when |
|---|---|
| `ContractError` | you break a rule of the API (wrong entry, dead entity, call in the wrong place) |
| `SchemaMismatchError` | the WASM and the generated TS were built from different schemas |

See [Troubleshooting](../../../troubleshooting.md) for each message.

## Pages

Each page mirrors the concept page with the same file name.

| Page | Concept |
|---|---|
| [Schema](schema.md) | [Schema](../../../concepts/schema.md) |
| [World and entities](world-and-entities.md) | [World and entities](../../../concepts/world-and-entities.md) |
| [Queries](queries.md) | [Queries](../../../concepts/queries.md) |
| [Engine, loop and systems](engine-and-systems.md) | [Engine, loop and systems](../../../concepts/engine-and-systems.md) |
| [Events](events.md) | [Events](../../../concepts/events.md) |
| [Input](input.md) | [Input](../../../concepts/input.md) |
| [Schedule](schedule.md) | [Schedule](../../../concepts/schedule.md) |
| [States](states.md) | [States](../../../concepts/states.md) |
| [Recipes](recipes.md) | [Recipes](../../../recipes.md) |

## C++ systems in a web game

A web game can run part of its gameplay in C++. TS registers those C++ systems, handlers and hooks by name with `addNativeSystem`, `addNativeHandler` and `addNativeOnEnter`/`addNativeOnExit`. See [C++ systems, handlers and hooks](engine-and-systems.md#c-systems-handlers-and-hooks).
