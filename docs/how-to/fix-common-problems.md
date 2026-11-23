# Fix common problems

[Docs](../README.md#how-to-guides) · Reference: [Errors](../reference/errors.md)

Use this when the game runs but does not do what you expect, with no error, or when you changed the schema and the game does not show it. It assumes a game made by `oly new`. For a message you can read on screen, look it up in [Errors](../reference/errors.md).

## 1. Find your symptom

| Symptom | Likely cause |
|---|---|
| a system never runs | it is restricted to states and no state is active (C++ `setSystemStates`, TypeScript `.inState(...)`): did you call `requestState` before the first frame? Or it is `whilePaused` and the game is not paused |
| nothing runs after pausing | no `whilePaused` system calls `resume()` |
| the next state is frozen | the game was paused when the state changed; resume in an `onExit` hook |
| a spawned entity is not found in the same phase | system changes are deferred to the end of the phase |
| entities disappear on state change | they were created through a context while a state was active: they are scoped. Use `persist`, or create them with `world.spawn` before the first frame |
| a timer stopped firing | it was scoped to a state that ended, or the game is paused (game clock stopped) |
| a handler sees the same entity twice | two emitters reported it; deduplicate in the handler |
| events from Render are late | events emitted in Render are dispatched in the next step |
| a key does not respond | it is not bound, or another element has focus; check `engine.actions().isBound(...)` in C++ or `engine.actions.isBound("KeyX")` in TypeScript |
| `pressed` never true in a test | input is captured at the start of a step: deliver the input, then call `frame` |
| the game jumps after alt-tab | not using `runInBrowser` (or not calling `setHidden`) |
| the right-click menu opens | `MouseRight` is not bound; prevent `contextmenu` on the canvas yourself |

The rules behind these rows are in [Engine, loop and systems](../explanation/concepts/engine-and-systems.md), [States](../explanation/concepts/states.md) and [Input](../explanation/concepts/input.md).

## 2. Rebuild after a schema change

After you change `<game>.oly`, run `./oly build` (or `./oly test`) in your game. It regenerates the C++ and TypeScript schemas, builds the WASM or the executable and compiles the TypeScript. Then reload the page or run the game again. A web game that skips this stops at start-up with `SchemaMismatchError` ([Errors](../reference/errors.md#typescript)).

## 3. Turn the asserts on

In C++, a contract violation stops on `OLY_ASSERT` in debug builds only. `oly build`, `oly test` and `oly run` build in `Debug`, so a game made by `oly new` has them on. A release build does not check and has undefined behaviour ([Errors](../reference/errors.md#at-run-time)).

## 4. Check for something the engine does not do

The engine does not draw, play audio, simulate physics or handle networking. If the symptom is one of those, it is not a bug: [Known limitations](../reference/limits.md).

## See also

- [Errors](../reference/errors.md): every message, its cause and its fix.
- [Debug the world](debug-the-world.md): check that a `World` is consistent.
- [Drive the loop yourself](drive-the-loop-yourself.md): step the game from a test.
