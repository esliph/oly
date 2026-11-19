# States

[← Schedule](schedule.md) · [Concepts](README.md) · Next: [Performance →](performance.md)

States split the game into screens or modes: `menu`, `playing`, `gameOver`. Each state can turn systems and handlers on or off, run code on entry and exit, and — the best part — **clean up after itself**.

## One state at a time

The schema declares the states (`state menu`, `state playing`, `state gameOver`, see [Schema](schema.md)). The declaration order gives each state its `StateId`.

States are flat: one active state at a time, no nesting, no stack. A game has at most 32 states. The engine starts with **no state**, so a game requests the first one before the first frame. The calls: [Declare and start](../../reference/cpp/states.md#declare-and-start).

TypeScript: [Declare and start](../../reference/typescript/states.md#declare-and-start).

## When a state changes

A system, handler, task or hook **requests** a state. The request does not change the state at once. The change happens at **step 5** of the fixed step, after the phases and events. If several requests arrive in one step, the **last one wins**. State changes happen even while paused.

Requesting the state that is already active is a full change too: it runs `onExit`, the clean-up and `onEnter` again. Use it to restart a round.

The change runs in this order:

1. `onExit` hooks of the old state, in registration order;
2. **scope clean-up** (below);
3. the state becomes the new one;
4. `onEnter` hooks of the new state, in registration order;
5. events emitted by the hooks are dispatched.

A request made inside a hook takes effect in the next step. A hook can emit events, schedule tasks and make direct structural changes, but cannot register anything. The calls: [Change state](../../reference/cpp/states.md#change-state) and [Enter and exit hooks](../../reference/cpp/states.md#enter-and-exit-hooks).

TypeScript: [Change state](../../reference/typescript/states.md#change-state) and [Enter and exit hooks](../../reference/typescript/states.md#enter-and-exit-hooks).

## Run only in some states

A system or a handler can be restricted to a list of states:

- No restriction: it runs in every state, and also with no state.
- With a restriction: it runs only in the listed states. With no active state, it does not run.
- It works in every phase, Render included.

The calls: [Run only in some states](../../reference/cpp/states.md#run-only-in-some-states).

TypeScript: [Run only in some states](../../reference/typescript/states.md#run-only-in-some-states).

## Scope: automatic clean-up

While a state is active, **everything created through a context is scoped to that state**:

- entities created with `context.spawn` (in systems, handlers, tasks and hooks);
- tasks created with `schedule().after` and `schedule().every` ([Schedule](schedule.md#pause-states-and-recovery)).

When the state changes, the engine destroys the scoped entities and cancels the scoped tasks. So "play again" needs **no reset code**: leaving `playing` removes the player, the enemies, the bullets and the wave timer.

| Created by | Scoped? |
|---|---|
| `context.spawn(...)` with an active state | yes |
| `schedule().after/every` with an active state | yes |
| `world.spawn(...)` (direct on the world) | **never** |
| anything with no active state | no |

What `onExit` creates is cleaned in the same change. What `onEnter` creates belongs to the new state.

TypeScript: [Scope: automatic clean-up](../../reference/typescript/states.md#scope-automatic-clean-up).

### Keep something across states: `persist`

`persist` removes the scope of one entity or one task, so it survives every state change. In a system, `persist` is deferred to the end of the phase, like `remove`. In hooks, handlers and tasks it is direct.

For things that exist from the start (a camera, a background), create them with `engine.world().spawn` before the first frame: they are never scoped. The calls: [Keep something across states: `persist`](../../reference/cpp/states.md#keep-something-across-states-persist).

TypeScript: [Keep something across states: `persist`](../../reference/typescript/states.md#keep-something-across-states-persist).

## API

- C++: [States in C++](../../reference/cpp/states.md)
- TypeScript: [States in TypeScript](../../reference/typescript/states.md)

A menu, a round and a game over screen, put together: [Menu → playing → game over](../../how-to/gameplay-recipes.md#menu--playing--game-over).

## Next

[Performance](performance.md): keep the hot path fast.
