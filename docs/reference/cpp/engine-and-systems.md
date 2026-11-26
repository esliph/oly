# Engine, loop and systems in C++

[Reference](../../README.md#reference) · Concept: [Engine, loop and systems](../../explanation/concepts/engine-and-systems.md) · TypeScript: [Engine, loop and systems in TypeScript](../typescript/engine-and-systems.md)

Headers: `oly/app/engine.hpp`, `oly/app/system.hpp`, `oly/app/phase.hpp`, `oly/app/system_parameters.hpp`, `oly/app/handler.hpp`, `oly/app/state_context.hpp`, `oly/app/task_context.hpp` and `oly/app/game.hpp`. Which phase runs when, and how systems are ordered: [Phases](../../explanation/concepts/engine-and-systems.md#phases).

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

`setUpGame` comes from the build: it declares the schema, then installs the game's [plugins](../../explanation/concepts/plugins.md) ([Schema in C++](schema.md#olyappsetupgame)). Call it once, first. Register systems, handlers and hooks next, before the first `frame`.

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

A system is a function whose parameters declare what it reads and writes ([System parameters](#system-parameters)):

```cpp
void spin(oly::app::Query<Rotation> rotations, const oly::app::StepInfo& step) {
  const float angle = 3.14159f * step.fixedDeltaTime;

  rotations.each([angle](Rotation& rotation) {
    rotation.angle += angle;
  });
}
```

**Where you own the `Engine`** (native):

```cpp
oly::app::SystemId spinId = engine.addSystem<&spin>(oly::app::Update{}, "spin").id();

engine.setSystemStates(spinId, {states::playing});
engine.addSystem<&resumeOnPress>(oly::app::UpdateBefore{}, "resumeOnPress").whilePaused();
```

**On the web**, the client registers it by name: [TypeScript](../typescript/engine-and-systems.md#register-systems). Which phase runs when, and how systems are ordered: [Phases](../../explanation/concepts/engine-and-systems.md#phases).

### Data for a system or a handler

A system keeps its state in a resource, and takes it as `Res<T>` ([System parameters](#system-parameters)). A handler or a state hook takes a `userData` pointer and reads the batch with `context.events<T>()`:

```cpp
struct Game {
  uint32_t hits = 0;
};

void steer(oly::app::Res<Game> game, oly::app::Res<const oly::input::ActionMap> actionMap) {
  // read actionMap->button(actions::shoot), use game->hits
}

void countHits(oly::app::HandlerContext& context) {
  static_cast<Game*>(context.userData())->hits += context.eventCount();
}

world.insertResource(Game{});
engine.addSystem<&steer>(oly::app::UpdateBefore{}, "steer").inStates({states::playing});
engine.on<Hit>("countHits", countHits, &world.resource<Game>());
engine.requestState(states::playing);
```

`OLY_SYSTEM`, `OLY_HANDLER` and `OLY_STATE_HOOK` are only needed when a client registers by name ([C++ systems, handlers and hooks](#c-systems-handlers-and-hooks)). A desktop game with no client can use plain functions, as above.

### `Engine::setWhilePaused`

```cpp
void setWhilePaused(SystemId system, bool whilePaused);
```

In the gameplay phases, a `whilePaused` system runs only in paused steps and the others only in unpaused ones. A Render system cannot be `whilePaused`: the call stops with `OLY_ASSERT` (`setWhilePaused: render system "<name>" cannot be whilePaused`).

### `Engine::setSystemStates`

```cpp
void setSystemStates(SystemId system, std::initializer_list<StateId> states);
```

Restricts the system to the given states ([States](states.md)), in every phase, including Render. An empty list removes the restriction. With no active state, a restricted system does not run. Cannot be called during `frame`.

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

## Phases, sets and constraints

A **phase** is a group of systems that runs together and ends with one flush of the deferred changes. A **schedule** says when a phase runs: `FixedStep` phases run in each fixed step, `Frame` phases run once per frame, after the steps. The built-in phases are tags in `oly/app/phase.hpp`:

```cpp
struct FixedStep {};
struct Frame {};
struct UpdateBefore {};  // FixedStep, id 0
struct Update {};        // FixedStep, id 1
struct UpdateAfter {};   // FixedStep, id 2
struct Render {};        // Frame, id 3
```

`UpdateBefore` runs before `Update`, and `Update` before `UpdateAfter`. A phase id is its declaration order, built-ins first.

```cpp
engine.addPhase<Physics>(oly::app::FixedStep{}).after<oly::app::Update>().before<oly::app::UpdateAfter>();
engine.set<Spatial>().before<&collide>();
engine.addSystem<&collide>(oly::app::UpdateAfter{}, "collide").inSet<Spatial>().after("moveAll");
```

### `Engine::addPhase` and `Engine::phase`

```cpp
template <class P, class Schedule> PhaseBuilder addPhase(Schedule schedule);  // FixedStep{} or Frame{}
template <class P> PhaseBuilder phase();
```

`addPhase` declares the phase `P`; declaring it twice stops with `OLY_ASSERT` (`addPhase: phase "<name>" already declared`). `phase<P>()` adds constraints to a phase declared now or later. `PhaseBuilder::before<Q>()` and `after<Q>()` each add one constraint between two phases. Two phases of one schedule need a path of constraints between them, and a constraint cannot join a `FixedStep` phase to a `Frame` phase.

### `Engine::set`

```cpp
template <class S> SetBuilder set();
```

A **system set** is a tag `S` that groups systems. `set<S>().before<Q>()` and `after<Q>()` order every member of `S`; the target is a set, a function (`before<&f>()`, every registration of `f`) or a system name (`before("name")`). A set orders its members in each phase they are in.

### `Engine::addSystem`

```cpp
template <auto Function, class P> SystemBuilder addSystem(P phase, std::string_view name);
SystemBuilder addSystem(ScheduleKey phase, std::string_view name, const SystemDescriptor& descriptor);
```

Registers `Function` in the phase `P` with the access its parameters declare ([System parameters](#system-parameters)). Names are unique in one `Engine`: a repeat stops with `OLY_ASSERT` (`addSystem: system "<name>" already registered`). The `SystemBuilder` returned adds, each call one constraint or setting:

- `inSet<S>()`: the system joins the set `S`.
- `before` / `after`, each as `<S>()` (a set), `<&f>()` (every registration of `f`) or `("name")` (one system).
- `whilePaused(bool = true)`: as [`setWhilePaused`](#enginesetwhilepaused). A system of a `Frame` phase cannot be `whilePaused` (`setWhilePaused: frame system "<name>" cannot be whilePaused`).
- `inStates({...})`: as [`setSystemStates`](#enginesetsystemstates).
- `id()`: the `SystemId`.

`Engine::orderSystems(first, second)` adds one constraint between two `SystemId`s.

### The order rule

Registration closes at the top of the first `frame`. Then the engine builds the order of each schedule:

1. Phases follow their constraints.
2. Inside a phase, systems follow their constraints, and a system with no constraint between two others runs in **name order**.

The order depends on names and constraints only, never on registration order or plugin install order. A constraint that makes a cycle, two phases with no order between them, and a system that needs a missing resource stop the first `frame` with one `OLY_ASSERT` that lists every problem, one per line, each starting with `frame: ` ([Errors](../errors.md#c)). After the first `frame`, `addSystem`, `addPhase`, `set`, `phase` and `orderSystems` stop with `OLY_ASSERT` (`<operation>: "<name>" registered after the first frame`).

`Engine::phaseId<P>()` returns the `PhaseId` of `P` (`OLY_ASSERT` when not declared) and `Engine::phaseName(PhaseId)` its name.

### Access conflicts

Two systems of one phase **conflict** when no path of constraints orders them, they touch the same component or resource, and at least one writes it: write–write and read–write. Two reads never conflict. An exclusive system (`ecs::World&`) conflicts with every system of its phase it is not ordered with. Filters (`With`, `Without`) never make two accesses disjoint, and a restriction (`inStates`, `whilePaused`) does not exempt a pair. Systems of different phases never conflict, because a phase ends before the next starts. Every conflict of the phase is listed in the error of the first `frame` ([Errors](../errors.md#c)); fix one by ordering the pair, moving one system to another phase, or reading instead of writing.

A constraint across two phases must agree with the order of the phases; it adds no order inside a phase.

### Engine controls as parameters

`Res<StateRequest>` requests a state (`request(StateId)`, as `Engine::requestState`); `Res<PauseRequest>` pauses and resumes (`pause()`, `resume()`, `requested()`); `Res<const ActionMap>` reads the actions of the step. They resolve to the `Engine` that runs the system, so two `Engine`s on one `World` do not share them. `StepInfo` holds the clock, the state and the pause as they were at the start of the phase; `SystemInfo` holds the system's name and id.

## C++ systems, handlers and hooks

Three macros define the functions a game registers. Each one registers its function **by name** when the module loads, so an environment can find it from a string. That is what the C API's `olyEngineAddNative*` functions use, for every client.

| Macro | Defines | Native registration |
|---|---|---|
| `OLY_SYSTEM(name);` | after the definition of the function `name` | `engine.addSystem<&name>(phase, "name")` |
| `OLY_HANDLER(name, Event)` | `void name(HandlerContext& context, std::span<const Event> events)` | `engine.on<Event>("name", lambda)` ([Events](events.md#engineon)) |
| `OLY_STATE_HOOK(name)` | `void name(oly::app::StateContext& context)` | `engine.onEnter(state, "name", name)` / `onExit` |

A system or a state hook is already the plain function that `addSystem`, `onEnter` and `onExit` take. A handler takes its batch as a span, so its native registration is a lambda.

Include the engine headers you use and the generated schema:

```cpp
#include "oly/app/handler.hpp"        // OLY_HANDLER
#include "oly/app/state_context.hpp"  // OLY_STATE_HOOK (StateContext)
#include "oly/app/system.hpp"         // OLY_SYSTEM, after system_descriptor.hpp
#include "oly/app/task_context.hpp"   // TaskContext, payloads
#include "oly/ecs/query.hpp"          // With, Without
#include "schema.generated.hpp"       // your components, events, actions:: and states::
```

## System parameters

A system function declares what it needs as parameters. The engine builds the declared access from the parameter types, which is what lets it order systems and refuse conflicts. A system is a function pointer with no `void*`: its state is a resource.

```cpp
#include "oly/app/system_descriptor.hpp"
#include "oly/app/system_parameters.hpp"

void move(oly::app::Query<Position, const Velocity> bodies, oly::app::Res<const Clock> clock) {
  bodies.each([&](Position& position, const Velocity& velocity) { position.x += velocity.x * clock->delta; });
}
```

| Parameter | Declares | Gives |
|---|---|---|
| `Query<Ts...>` | a read of each `const T`, a write of each `T`; `ecs::With<...>` and `ecs::Without<...>` declare nothing | `each`, `eachBlock`, `single`, `trySingle`, `matchedCount`, as `ecs::Query` |
| `Lookup<const T>`, `Lookup<T>` | a read, a write of component `T` | `get(Entity)`, `tryGet(Entity)`, `has(Entity)`; for a tag only `Lookup<const Tag>`, with `has` alone |
| `Res<const T>`, `Res<T>` | a read, a write of resource `T` | `get()`, `operator*`, `operator->` |
| `Commands` | nothing | `spawn`, `destroy`, `add`, `remove<T>`, `addById`, `removeById`, `persist`, `isAlive`; deferred to the phase's flush |
| `Events<E>` | nothing | `emit(const E&)` |
| `Tasks` | nothing | `schedule`, `after`, `every`; no `cancel` |
| `const StepInfo&` | nothing | `time`, `fixedDeltaTime`, `interpolation`, `state`, `paused` |
| `const SystemInfo&` | nothing | `id`, `name`, `phase`, `schedule` |
| `ecs::World&` | the system is exclusive | direct access to the world |

A write of a type replaces its read in the declared access. `StepInfo` is the same for every system of one phase run: `paused` is the pause request read when the phase starts. A parameter of any other type does not compile (`addSystem: this parameter type is not a system parameter`), and neither does a write through a `const` parameter. A system takes at most 16 parameters.

An exclusive system takes `ecs::World&` and may change structure directly; no other system runs beside it.

A `Res<T>` whose resource is not inserted, and an `Events<E>` whose event is not declared, are reported with the system's name when registration closes.

### `SystemInfo`

```cpp
struct SystemInfo {
  SystemId id;
  std::string_view name;
  PhaseId phase;
  ScheduleKind schedule;
};
```

`const SystemInfo&` gives the `SystemId` that `addSystem` returned for the running system, its name and its phase. Use it with a function that serves several registrations, to know which one is running. It is valid only during the call.

### Engine state as resources

State that a system keeps goes in a resource, read as `Res<T>` ([Resources](world-and-entities.md#resources)). A system does not take a `void*`.

| Need | Parameter |
|---|---|
| query | `oly::app::Query<Position, const Velocity, oly::ecs::With<Enemy>>` |
| read / write one component | `Lookup<Health>`: `get(entity)` (reference), `tryGet(entity)` (pointer or `nullptr`), `has(entity)` |
| create, change structure | `Commands`: `spawn`, `add`, `remove<T>`, `destroy`, `persist`, `isAlive` |
| events | `Events<EnemyDied>`: `emit(EnemyDied{entity, x, y})` ([Events](events.md#eventsemit-and-contextemit)) |
| input | `Res<const input::ActionMap>`: `button(actions::shoot).held` ([Input](input.md)) |
| time, state, pause | `const StepInfo&`: `fixedDeltaTime`, `time`, `interpolation`, `state`, `paused` |
| schedule | `Tasks`: `after(seconds, function, payload)` ([Schedule](schedule.md)) |
| request a state | `Res<StateRequest>`: `request(states::gameOver)` ([States](states.md)) |
| pause, resume | `Res<PauseRequest>`: `pause()`, `resume()`, `requested()` |
| where it runs | `const SystemInfo&` |

## The contexts

Three contexts reach a game's code besides the system parameters. All of them derive from the same base, so they share the calls in the first row. They change the world at once.

| | `HandlerContext` | `TaskContext` | `StateContext` |
|---|---|---|---|
| structural changes | direct | direct | direct |
| members only this context has | `eventType()`, `handlerId()`, `name()`, `userData()`, `events<T>()`, `eventCount()`, `eventData()` | `handle()`, `lastRun()`, `payload<P>()` | `hookId()`, `name()`, `userData()`, `fromState()`, `toState()`, `entering()` |

Every context has `time()`, `fixedDeltaTime()`, `paused()`, `pause()`, `resume()`, `state()`, `requestState()`, `actions()`, `schedule()`, `query`, `get`, `tryGet`, `has`, `isAlive`, `emit`, `spawn`, `destroy`, `add`, `remove` and `persist`. Each context is valid only during the call it was passed to. The members of each: [`HandlerContext`](events.md#handlercontextevents), [`TaskContext`](schedule.md#the-task-context) and [`StateContext`](states.md#statecontexthookid). A system does not receive a context: it declares [parameters](#system-parameters).

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

`time()` is `steps × fixedDeltaTime()`. Inside a system it is the start of the current step, and it does not advance in paused steps. `interpolation()` is how far the real time is between the last step and the next; read it in Render. Every context has `fixedDeltaTime()` (a `float`) and `time()`; a system reads `interpolation` from `StepInfo`. What they mean: [Time and interpolation](../../explanation/concepts/engine-and-systems.md#time-and-interpolation).

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
engine.addSystem<&pauseOnPress>(oly::app::UpdateBefore{}, "pauseOnPress");
engine.addSystem<&resumeOnPress>(oly::app::UpdateBefore{}, "resumeOnPress").whilePaused();
```

## Time per system

### `Engine::setTimingEnabled`

```cpp
void setTimingEnabled(bool enabled);
bool timingEnabled() const noexcept;
```

Timing is off by default. While it is on, each fixed step and each `Render` records the time of every system that runs and of the flush that ends each phase. Event handlers and scheduled tasks are not timed. The clock is `std::chrono::steady_clock`: it is coarse on MinGW under Wine and on WASM, so a system that ran may measure 0 ns. Off, a frame pays one branch per phase.

`setTimingEnabled` cannot be called during `frame`: it stops with `OLY_ASSERT` (`setTimingEnabled: called during frame`) ([Errors](../errors.md)).

```cpp
engine.setTimingEnabled(true);
engine.frame(nowSeconds);

for (const oly::app::SystemTiming& timing : engine.systemTimings()) {
  if (timing.ran) {
    std::printf("%.*s: %llu ns\n", static_cast<int>(timing.name.size()), timing.name.data(), static_cast<unsigned long long>(timing.nanoseconds));
  }
}
```

### `Engine::systemTimings`

```cpp
std::span<const SystemTiming> systemTimings() const noexcept;
std::span<const FlushTiming> flushTimings() const noexcept;
```

- Timing off: both spans are empty.
- Timing on: both spans are empty until registration closes at the first `frame`. After it, `systemTimings()` has one entry per system in dispatch order (fixed-step phases, then frame phases) and `flushTimings()` one entry per phase in the same order, each with `ran = false` and 0 ns until its phase runs.
- Each phase overwrites the entries of its phase. After a frame, the gameplay entries hold the last fixed step and the `Render` entries the last `Render`. A frame with no fixed step keeps the last step's entries.
- The spans, and the names in them, stay valid until the next `setTimingEnabled` call. Calling `setTimingEnabled` resets every entry to `ran = false` and 0 ns.

### `SystemTiming` and `FlushTiming`

```cpp
struct SystemTiming {
  SystemId system;
  std::string_view name;
  PhaseId phase;
  std::string_view phaseName;
  bool ran;
  uint64_t nanoseconds;
};

struct FlushTiming {
  PhaseId phase;
  std::string_view phaseName;
  uint64_t nanoseconds;
};
```

`ran` is false when the pause or the system's states skipped it. Both types are in `oly/app/timing.hpp`, which `oly/app/engine.hpp` includes.
