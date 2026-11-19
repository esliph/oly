# Engine, loop and systems in C++

[Reference](../../README.md#reference) · Concept: [Engine, loop and systems](../../explanation/concepts/engine-and-systems.md) · TypeScript: [Engine, loop and systems in TypeScript](../typescript/engine-and-systems.md)

Headers: `oly/app/engine.hpp`, `oly/app/system.hpp`, `oly/app/context.hpp`, `oly/app/phase.hpp`, `oly/app/handler.hpp`, `oly/app/state_context.hpp`, `oly/app/task_context.hpp` and `oly/app/game.hpp`. Which phase runs when, and what an order band means: [Phases](../../explanation/concepts/engine-and-systems.md#phases).

## Create the engine

**Where you own the `Engine`** (native):

### `Engine`

```cpp
struct EngineConfig {
  double fixedHz = 60.0;
  uint32_t maxStepsPerFrame = 5;
  uint32_t maxEventRounds = 16;
};

explicit Engine(ecs::World& world, EngineConfig config = {});
```

`fixedHz` is the number of gameplay steps per second, `maxStepsPerFrame` the catch-up limit per frame and `maxEventRounds` the event round limit per step. All three must be positive: the constructor stops with `OLY_ASSERT` (`Engine: fixedHz=<value> must be positive`, `Engine: maxStepsPerFrame must be positive` or `Engine: maxEventRounds must be positive`). An `Engine` is not copyable. Two `Engine`s over the same `World` share the scope tag: a state change in one destroys the scoped entities of the other.

```cpp
oly::ecs::World world;
oly::app::Engine engine(world, oly::app::EngineConfig{
  .fixedHz = 60.0,
  .maxStepsPerFrame = 5,
  .maxEventRounds = 16,
});

oly::app::setUpGame(engine);
```

`setUpGame` comes from the build: it declares the schema, then installs the game's [plugins](../../guide/concepts/plugins.md) ([Schema in C++](schema.md#olyappsetupgame)). Call it once, first. Register systems, handlers and hooks next, before the first `frame`.

**On the web**, the host creates the engine: [TypeScript](../typescript/engine-and-systems.md#create-the-engine).

## The loop

### `Engine::frame`

```cpp
void frame(double nowSeconds);
```

Runs up to `maxStepsPerFrame` fixed steps and then the Render phase once. The whole steps that did not fit are discarded, and only the fraction stays accumulated. The first frame only records the time. `nowSeconds` cannot go backwards: `frame` stops with `OLY_ASSERT` (`frame: time <now> less than the previous <previous>`). If the previous frame ended with an exception, this one discards the pending commands, events and state request (including those emitted after the exception) and also only records the time; scheduled tasks stay. How a frame runs: [The loop](../../explanation/concepts/engine-and-systems.md#the-loop). A host calls it for you; to call it yourself: [Drive the loop yourself](../../how-to/drive-the-loop-yourself.md).

### `Engine::setHidden`

```cpp
void setHidden(bool hidden);
```

While hidden, `frame` does nothing. When you show the engine again, the next `frame` only records the time. Hiding releases every held button. It does not touch the game pause.

### `Engine::inFrame`

```cpp
bool inFrame() const noexcept;
```

`true` while `frame` runs. Registering systems, handlers and hooks is not allowed then.

## Register systems

A system is a function of the context. Write it with `OLY_SYSTEM`:

```cpp
OLY_SYSTEM(spin) {
  const float step = 3.14159f * context.fixedDeltaTime();

  context.query<Rotation>().each([step](Rotation& rotation) {
    rotation.angle += step;
  });
}
```

**Where you own the `Engine`** (native):

```cpp
oly::app::SystemId spinId = engine.addSystem(oly::app::Phase::Update, "spin", spin);

engine.setSystemStates(spinId, {states::playing});
engine.setWhilePaused(engine.addSystem(oly::app::Phase::UpdateBefore, "resumeOnPress", resumeOnPress), true);
```

**On the web**, the client registers it by name: [TypeScript](../typescript/engine-and-systems.md#register-systems).

### `Engine::addSystem`

```cpp
SystemId addSystem(Phase phase, std::string_view name, SystemFunction function, void* userData = nullptr, OrderBand band = OrderBand::Default);
```

Registers `function` in `phase` and returns its `SystemId`. `name` appears in the error messages. The system reads `userData` with `context.userData()`. Within a phase, the `Default` band runs before the `Last` band, and within a band systems run in registration order. The phase applies its commands once, after the `Last` band. Register everything at start-up: a call during `frame` stops with `OLY_ASSERT` (`addSystem: system "<name>" registered during frame`).

```cpp
engine.addSystem(oly::app::Phase::Update, "followTargets", followTargets, nullptr, oly::app::OrderBand::Last);
```

### `Engine::setWhilePaused`

```cpp
void setWhilePaused(SystemId system, bool whilePaused);
```

In the gameplay phases, a `whilePaused` system runs only in paused steps and the others only in unpaused ones. A Render system cannot be `whilePaused`: the call stops with `OLY_ASSERT` (`setWhilePaused: render system "<name>" cannot be whilePaused`).

### `Engine::setSystemStates`

```cpp
void setSystemStates(SystemId system, std::initializer_list<StateId> states);
```

Restricts the system to the given states ([States](../../guide/concepts/states.md)), in every phase, including Render. An empty list removes the restriction. With no active state, a restricted system does not run. Cannot be called during `frame`.

```cpp
engine.setSystemStates(spinId, {states::playing});
```

### `Engine::setSystemStateMask`

```cpp
void setSystemStateMask(SystemId system, StateMask mask);
```

The same restriction, with the states as a `StateMask`: bit `n` is the state with `StateId` `n`, and `0` removes the restriction. Stops with `OLY_ASSERT` (`setSystemStates: mask 0x<mask> with undeclared state (<n> states)`) when the mask sets a bit of a state that is not declared, and with `setSystemStates: state restriction during frame` during `frame`.

```cpp
engine.setSystemStateMask(spinId, (oly::app::StateMask{1} << states::menu) | (oly::app::StateMask{1} << states::playing));
```

## C++ systems, handlers and hooks

Three macros define the functions a game registers. Each one registers its function **by name** when the module loads, so an environment can find it from a string. That is what the C API's `olyEngineAddNative*` functions use, for every client.

| Macro | Defines | Native registration |
|---|---|---|
| `OLY_SYSTEM(name)` | `void name(oly::app::SystemContext& context)` | `engine.addSystem(phase, "name", name)` |
| `OLY_HANDLER(name, Event)` | `void name(HandlerContext& context, std::span<const Event> events)` | `engine.on<Event>("name", lambda)` ([Events](events.md#engineon)) |
| `OLY_STATE_HOOK(name)` | `void name(oly::app::StateContext& context)` | `engine.onEnter(state, "name", name)` / `onExit` |

A system or a state hook is already the plain function that `addSystem`, `onEnter` and `onExit` take. A handler takes its batch as a span, so its native registration is a lambda.

Include the engine headers you use and the generated schema:

```cpp
#include "oly/app/handler.hpp"        // OLY_HANDLER
#include "oly/app/state_context.hpp"  // OLY_STATE_HOOK (StateContext)
#include "oly/app/system.hpp"         // OLY_SYSTEM
#include "oly/app/task_context.hpp"   // TaskContext, payloads
#include "oly/ecs/query.hpp"          // With, Without
#include "schema.generated.hpp"       // your components, events, actions:: and states::
```

## The system context

A system receives a `SystemContext`. It is valid only during that call.

| Need | Call |
|---|---|
| query | `context.query<Position, const Velocity>(oly::ecs::With<Enemy>{})` |
| read / write one component | `context.get<Health>(entity)` (reference), `context.tryGet<Health>(entity)` (pointer or `nullptr`) |
| check | `context.has<Frozen>(entity)`, `context.isAlive(entity)` |
| create | `context.spawn(Bullet{}, Position{x, y}, Velocity{vx, vy})` |
| change structure | `context.add(entity, Frozen{})`, `context.remove<Frozen>(entity)`, `context.destroy(entity)`, `context.persist(entity)` |
| events | `context.emit(EnemyDied{entity, x, y})` ([Events](events.md#contextemit)) |
| input | `context.actions().button(actions::shoot).held` ([Input](input.md)) |
| time | `context.fixedDeltaTime()`, `context.time()`, `context.interpolation()` |
| schedule | `context.schedule().after(seconds, function, payload)` ([Schedule](../../guide/concepts/schedule.md)) |
| state | `context.state() == states::playing`, `context.requestState(states::gameOver)` ([States](../../guide/concepts/states.md)) |
| pause | `context.pause()`, `context.resume()`, `context.paused()` |
| where it runs | `context.phase()`, `context.systemId()`, `context.name()`, `context.userData()` |

### `SystemContext::systemId`

```cpp
SystemId systemId() const noexcept;
```

Returns the `SystemId` that `addSystem` returned for the running system. Use it with a system that serves several registrations, to know which one is running.

## Emit from the host

### `Engine::emit`

```cpp
template <class T>
void emit(const T& event);
```

Emits an event from the code that owns the `Engine`, such as the game's `main`, a host or a test. It is valid inside and outside `frame`. Outside, the event is dispatched in the next fixed step. `T` must be declared (`declareSchema` does it for the events of the schema): otherwise the call stops with `OLY_ASSERT` (`emit: event "<T>" not declared`). Inside a system, handler or task, use `context.emit`: it dispatches in the next round of the current step.

```cpp
engine.emit(EnemyDied{oly::ecs::NullEntity, 0.0f, 0.0f});
```

## Time and interpolation

### `Engine::time`

```cpp
double time() const noexcept;
double fixedDeltaTime() const noexcept;
double interpolation() const noexcept;
```

`time()` is `steps × fixedDeltaTime()`. Inside a system it is the start of the current step, and it does not advance in paused steps. `interpolation()` is how far the real time is between the last step and the next; read it in Render. Every context has `fixedDeltaTime()` (a `float`) and `time()`; `SystemContext` also has `interpolation()` (a `float`). What they mean: [Time and interpolation](../../explanation/concepts/engine-and-systems.md#time-and-interpolation).

## Pause

### `Engine::pause`

```cpp
void pause() noexcept;
void resume() noexcept;
bool paused() const noexcept;
```

A request made in the middle of a step takes effect from the next fixed step. In a system, `context.pause()`, `context.resume()` and `context.paused()` do the same.

```cpp
engine.pause();
engine.resume();
engine.paused();
```

Register a system that resumes the game with `setWhilePaused`: [Engine::setWhilePaused](#enginesetwhilepaused). Which parts keep running: [Pause](../../explanation/concepts/engine-and-systems.md#pause).

```cpp
engine.addSystem(oly::app::Phase::UpdateBefore, "pauseOnPress", pauseOnPress);
engine.setWhilePaused(engine.addSystem(oly::app::Phase::UpdateBefore, "resumeOnPress", resumeOnPress), true);
```
