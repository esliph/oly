# `app`: the `Engine`

Code in `engine/runtime/app/`. Spec: `docs/superpowers/specs/2026-11-09-game-engine-design.md`.

`oly::app::Engine` holds the fixed-step clock, the systems of each phase, the pause, the hidden tab, the event queues, the `Scheduler`, the states, an `ActionMap` ([input.md](input.md)), a `CommandBuffer` and the installed plugins ([plugins.md](plugins.md)). It does **not** own the `World`: it receives `World&` in the constructor.

`EngineConfig`: `fixedHz = 60`, `maxStepsPerFrame = 5`, `maxEventRounds = 16`.

## The loop

The host calls `frame(nowSeconds)` once per frame and `setHidden(bool)` when the tab changes visibility. The core decides the rest.

```
frame(now):
  if the previous frame was interrupted by an exception → recover (see "Exception recovery")
  if the tab is hidden → return
  first frame (or first after hidden/recovery) → only record the time
  accumulator += now − previous
  while a step fits and steps < maxStepsPerFrame:
    fixed step
  excess beyond the limit → discarded (avoids the spiral of death)
  render (once), with interpolation = accumulator / step

fixed step:
  1. action capture                          (paused or not)
  2. due scheduled tasks                     (only when not paused)
  3. the FixedStep phases, in constraint order (updateBefore → update → physics → propagation → updateAfter; CommandBuffer applied at the end of each phase)
  4. event dispatch, in rounds
  5. pending state change                    (paused or not)
  clock advances (only when not paused)
```

- `time()` = completed unpaused steps × `fixedDeltaTime`. Inside a step, it is the start of the step; outside `frame` and in the `render` phase, it is the start of the next step. There is no accumulated `double` sum.
- The phases are types a plugin adds with `addPhase<P>(FixedStep{})` or `Frame{}`; `physics2d` declares `Physics` and `essentials2d` and `essentials3d` declare `Propagation`. The order of phases and of the systems inside a phase comes from constraints and names ([Phases, sets and order](#phases-sets-and-order)).

## Systems and contexts

A system is a function whose parameters declare what it reads and writes. A compile-time adapter (`oly/app/detail/system_adapter.hpp`) turns the function pointer into a `SystemDescriptor`: the declared access, the resolution of each parameter once, and one trampoline of a fixed signature. That way `app` does not know about TS: `capi` registers a TS system through a descriptor of its own ([capi.md](capi.md)).

Registration: `addSystem<&f>(phase, name)` or, by name, the `OLY_SYSTEM(name);` macro after the function, which TS adds with `addNativeSystem`. C++ and TS systems mix in the same phase, and a TS system is an exclusive system.

### Phases, sets and order

Plugins install before the game registers its systems, so registration order cannot be what orders them. The order comes from constraints and names only.

- A phase is a type (`oly/app/phase.hpp`) with a schedule, `FixedStep` or `Frame`. Phases and sets share one key space, `scheduleKey<T>()`. `before<Q>()` and `after<Q>()` on a phase, a set, a function or a system name add constraints.
- Registration closes on the first `frame` call. `detail::buildSchedule` (`oly/app/detail/schedule_graph.hpp`) is a pure pass over the registered phases, sets and systems: it orders the phases, orders each phase topologically with ties broken by name, and collects the errors (a cycle, two systems that conflict with no order between them) as strings. The `Engine` raises them with one `OLY_ASSERT`, then resolves every parameter once.
- The phase applies its `CommandBuffer` once, after its last system. `setWhilePaused` and `inStates` apply to a system like any other.
- A script system takes `ecs::World&`: it is exclusive, and runs alone.

Contexts and parameters:

```
EngineContext      time, pause, query/get/has/isAlive, emit, actions, schedule, state, requestState
└── DirectContext  direct changes to the World, direct persist
    ├── HandlerContext  + event batch
    ├── TaskContext     + handle, lastRun, payload<P>()
    └── StateContext    + fromState, toState
```

A system receives no context: it declares `Query`, `Lookup`, `Res`, `Commands`, `Events`, `Tasks`, `StepInfo` and `SystemInfo`. The engine controls it needs are resources the `Engine` owns: `StateRequest`, `PauseRequest`, `input::ActionMap` and `Scheduler`. A resource id resolves against those first, then against the `World`.

Systems do not make direct structural changes, because they may be inside an iteration: they record them through `Commands`. Handlers, tasks and hooks run outside iteration, so they change the `World` directly.

## Pause and hidden tab

- `pause()`/`resume()` take effect from the next fixed step.
- Paused: the clock and the schedule freeze; only systems marked `whilePaused` run. Capture, event dispatch, state change and `render` continue.
- Hidden tab: no step runs, `releaseAll()` is called and the accumulated time is discarded on return. The hidden tab is independent of the game's pause.

## Events

- `declareEvent<T>(name)` gives the `EventTypeId` in declaration order, which is the dispatch order. `T` is trivially copyable.
- Each type has two byte queues: `pending` (receives `emit`, at any time) and `dispatching` (the round's batch).
- **Round**: swaps the queues of every type with pending events and calls each handler of the type once with the whole batch, type by type. What handlers emit goes to the next round of the same step. The batch a handler reads never changes or moves while it runs.
- Rounds continue until the queues are empty or `maxEventRounds` is exceeded (`OLY_ASSERT`; without asserts, it discards everything and the step continues).
- The engine does not deduplicate events or destroy anything on its own. The game registers the destroying handler last and checks `isAlive`.
- An event with no handler disappears at the end of the step. An event emitted in `render` or outside `frame` is dispatched in the next step.

## Schedule

- `Scheduler::after/every/cancel/isPending`, on the game clock. Due time = `time()` + delay.
- The queue (`detail::TaskQueue`) is a min-heap by (due time, sequence) over slots with generation and lazy cancellation. It does not know the `Engine`.
- Due tasks run in step 2, only when not paused. A task created during step 2 only runs from the next step on, so `after(0)` inside a task does not become a loop.
- `every` does not drift and fires at most once per step.
- The payload is copied (trivially copyable, up to 32 B, alignment up to 8). `payload<P>()` checks the type.
- The optional discard runs exactly once when the task leaves the queue without its last run (cancellation, state change, destruction of the `Engine`). It never runs after the last firing of an `after` (`lastRun()`): a JS exception unwinds the WASM without running what comes after, and the TS closure would stay stuck.

## States

- Flat states, no nesting and no stack. `declareState` gives the `StateId` in declaration order; at most 32, so the restriction is a `uint32_t` mask. The engine starts in `NoState`.
- `requestState` is valid at any time; the last request before step 5 wins. There is no special initial state: the game requests the first state before running.
- **Change (step 5)**: `onExit` of the leaving state → scope cleanup → new state → `onEnter` of the entering one → new event dispatch, which counts toward the step's round limit. A request made in a hook takes effect in the next step.
- **`inState`**: systems and handlers can be restricted by mask, in every phase. With no active state, anything with a restriction does not run.
- **Scope**: with an active state, every context `spawn` adds the internal tag `detail::StateScoped`, and every scheduled task is marked as `scoped`. The change destroys the entities with the tag (through a `QueryById` created once) and cancels the scoped tasks. That way "play again" starts clean with no cleanup code in the game. A direct `World::spawn` is always persistent. `persist` removes the scope (deferred in `SystemContext`, direct in the other contexts). What `onExit` creates is cleaned up in the same change; what `onEnter` creates stays until the next change.
- Accepted cost of tag-based scope: twin archetypes (with and without the tag) when the game creates the same combination in both modes.

## Plugins and `setUpGame`

The `Engine` owns the plugin instances ([plugins.md](plugins.md)):

- `installPlugin<P>()` constructs `P`, keeps it and runs its `install` once; `plugin<P>()` returns it. Both assert with the type name: installing twice or after the first `frame` call, or reading a plugin that is not installed.
- `~Engine()` first discards the pending tasks, so a discard that reaches a plugin through its payload runs while the plugin lives, then destroys the plugins in reverse install order, while the `World` and the registrations still exist. A plugin's destructor removes the observers it registered, because the `World` outlives the `Engine`.

`oly::app::setUpGame(Engine&)` (`oly/app/game.hpp`) is the game's entry point, called once per `Engine` before the first `frame`: by `olyEngineCreate` ([capi.md](capi.md)) and, on native, by `runGame` (a `main` that drives the loop itself calls it: [Drive the loop yourself](../../how-to/drive-the-loop-yourself.md)). The build generates it in `game_setup.generated.cpp`: it calls `declareSchema` (`oly/app/schema.hpp`, generated by `oly-schema`), then installs the game's plugins in dependency order.

## Exception recovery

A TS system that throws unwinds the WASM without running destructors. The next `frame` sees `inFrame_` still set and recovers:

- discards the pending commands, the event queues and the state request;
- returns to the heap the tasks created during the interrupted step 2;
- resets the frame clock (the next `frame` only records the time).

What the earlier phases already applied stays. Scheduled tasks survive. An exception in an `onExit` leaves the state and the scope as they were; in an `onEnter`, the state is already the new one.
