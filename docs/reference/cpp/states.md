# States in C++

[Reference](../../README.md#reference) · Concept: [States](../../explanation/concepts/states.md) · TypeScript: [States in TypeScript](../typescript/states.md)

Headers: `oly/app/state.hpp` for `StateId`, `StateMask`, `NoState` and `MaxStates`, `oly/app/state_context.hpp` for `StateContext`, and `oly/app/engine.hpp` and `oly/app/context.hpp` for the calls. When a state changes, what the scope cleans and in what order hooks run: the [concept page](../../explanation/concepts/states.md). A menu, a round and a game over screen, put together: [Menu → playing → game over](../../how-to/gameplay-recipes.md#menu--playing--game-over).

## Declare and start

### `Engine::declareState`

```cpp
StateId declareState(std::string_view name);
uint32_t stateCount() const noexcept;
```

Declares a state and returns its `StateId`, a dense index in declaration order. `setUpGame` declares the states of the schema, so a game does not call it, and the generated `states::menu`, `states::playing` and `states::gameOver` are the ids. `stateCount()` is the number of declared states. The call stops with `OLY_ASSERT` when it happens during `frame` (`declareState: state "<name>" declared during frame`), repeats a name (`declareState: state "<name>" already declared`) or goes past the maximum (`declareState: state "<name>" exceeds the maximum of 32`).

### `NoState`

```cpp
using StateId = uint32_t;
inline constexpr StateId NoState = 0xFFFFFFFFu;
inline constexpr uint32_t MaxStates = 32;
```

`NoState` is what `engine.state()` returns before the first change. `MaxStates` is 32, one bit of a `StateMask` per state.

The engine starts with `NoState`. Request the first state before the first frame:

**Where you own the `Engine`** (native):

```cpp
engine.requestState(states::menu);
```

**On the web**, the client requests it: [TypeScript](../typescript/states.md#declare-and-start).

## Change state

### `Engine::requestState`

```cpp
void requestState(StateId state);
StateId state() const noexcept;
```

`requestState` asks for a change. The change happens at step 5 of the fixed step, paused or not, and the last request before step 5 wins. It is valid inside and outside `frame`. A state that is not declared stops with `OLY_ASSERT` (`requestState: state=<id> out of range (<n> states)`). `state()` is `NoState` until the first change, and it changes only at step 5.

`context.requestState(state)` and `context.state()` do the same in every context: a system, handler, task or hook. A request made in a hook takes effect in the next step.

```cpp
OLY_SYSTEM(startOnConfirm) {
  if (context.actions().button(actions::confirm).pressed) {
    context.requestState(states::playing);
  }
}
```

From outside the loop, call `engine.requestState(states::gameOver)`. Read the current state and compare it with the generated id:

```cpp
if (context.state() == states::playing) {
  // ...
}
```

## Run only in some states

[`Engine::setSystemStates`](engine-and-systems.md#enginesetsystemstates) restricts a system, and [`Engine::setHandlerStates`](events.md#enginesethandlerstates) restricts a handler. Each also has a form that takes a `StateMask`.

### `StateMask`

```cpp
using StateMask = uint32_t;
```

One bit per state: bit `n` is the state with `StateId` `n`, and `0` removes the restriction. A mask that sets a bit of a state that is not declared stops with `OLY_ASSERT` (`<call>: mask 0x<mask> with undeclared state (<n> states)`). The forms: [`Engine::setSystemStateMask`](engine-and-systems.md#enginesetsystemstatemask) and [`Engine::setHandlerStateMask`](events.md#enginesethandlerstatemask).

```cpp
const oly::app::StateMask menuOrGameOver = (oly::app::StateMask{1} << states::menu) | (oly::app::StateMask{1} << states::gameOver);
```

**Where you own the `Engine`** (native):

```cpp
engine.setSystemStates(engine.addSystem(oly::app::Phase::UpdateBefore, "startOnConfirm", startOnConfirm), {states::menu, states::gameOver});
engine.setSystemStates(engine.addSystem(oly::app::Phase::Update, "chasePlayer", chasePlayer), {states::playing});
engine.setHandlerStates(engine.on<PlayerDied>("endRound", [](oly::app::HandlerContext& context) { endRound(context, context.events<PlayerDied>()); }), {states::playing});
```

**On the web**, the client uses `.inState(...)`: [TypeScript](../typescript/states.md#run-only-in-some-states).

## Enter and exit hooks

### `Engine::onEnter`

```cpp
StateHookId onEnter(StateId state, std::string_view name, StateHookFunction function, void* userData = nullptr);
StateHookId onExit(StateId state, std::string_view name, StateHookFunction function, void* userData = nullptr);
```

Registers `function` to run when the engine enters or leaves `state`, and returns its `StateHookId`. `StateHookFunction` is `void (*)(StateContext& context)`. Hooks of one state and one moment run in registration order. The ids count `onEnter` and `onExit` hooks together. Register everything at start-up: a call during `frame` stops with `OLY_ASSERT` (`onEnter: hook "<name>" registered during frame`, or `onExit: ...`), and a state that is not declared stops with `onEnter: state=<id> out of range (<n> states)`.

### `OLY_STATE_HOOK`

```cpp
OLY_STATE_HOOK(name)
```

Defines `void name(oly::app::StateContext& context)` and registers it by name, so every environment can find it ([C++ systems, handlers and hooks](engine-and-systems.md#c-systems-handlers-and-hooks)). The function is already the plain function that `onEnter` and `onExit` take.

```cpp
OLY_STATE_HOOK(startRound) {
  context.spawn(Player{}, Position{480.0f, 270.0f}, Health{3});
  context.schedule().every(5.0, spawnWave);
}

OLY_STATE_HOOK(saveBest) {
  Score& score = std::get<1>(context.query<Score>().single());
  score.best = std::max(score.best, score.score);
}
```

**Where you own the `Engine`** (native):

```cpp
engine.onEnter(states::playing, "startRound", startRound);
engine.onExit(states::playing, "saveBest", saveBest);
```

**On the web**, the client registers hooks by name or as closures: [TypeScript](../typescript/states.md#enter-and-exit-hooks).

### `StateContext::hookId`

```cpp
StateHookId hookId() const noexcept;
std::string_view name() const noexcept;
void* userData() const noexcept;
StateId fromState() const noexcept;
StateId toState() const noexcept;
bool entering() const noexcept;
```

A hook receives a `StateContext`, a direct context ([Direct and deferred changes](../../explanation/concepts/world-and-entities.md#direct-and-deferred-changes)), valid only during the call.

| Member | Value |
|---|---|
| `hookId()` | the id that `onEnter` or `onExit` returned for the running hook |
| `name()` | the name the hook was registered with |
| `userData()` | the pointer given at registration |
| `fromState()` | the state being left (`oly::app::NoState` on the first change) |
| `toState()` | the state being entered |
| `entering()` | `true` in an `onEnter`; a hook registered for both moments uses it to know which side it is on |

## Scope: automatic clean-up

`context.spawn` and the scheduler scope what they create to the active state ([Scope: automatic clean-up](../../explanation/concepts/states.md#scope-automatic-clean-up)). `world.spawn` never does.

### Keep something across states: `persist`

```cpp
void persist(ecs::Entity entity);
bool Scheduler::persist(TaskHandle task);
```

`context.persist(entity)` removes the scope of an entity, and `context.schedule().persist(handle)` removes the scope of a pending task ([`Scheduler::cancel`](schedule.md#schedulercancel)). The entity must be alive: otherwise the call stops with `OLY_ASSERT`. An entity that is not scoped is left as it is. In a system `persist` is deferred to the end of the phase, like `remove`. In hooks, handlers and tasks it is direct.

```cpp
OLY_STATE_HOOK(startRound) {
  oly::ecs::Entity camera = context.spawn(Camera{}, Position{});
  context.persist(camera);                     // survives every state change

  oly::app::TaskHandle music = context.schedule().every(30.0, changeTrack);
  context.schedule().persist(music);
}
```

For things that exist from the start (a camera, a background), create them with `engine.world().spawn` before the first frame: they are never scoped.
