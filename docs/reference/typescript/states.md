# States in TypeScript

[TypeScript client](README.md) · Concept: [States](../../explanation/concepts/states.md) · C++: [States in C++](../cpp/states.md)

When a state changes, what the scope cleans and in what order hooks run are in the [concept page](../../explanation/concepts/states.md). This page shows the TS calls. A menu, a round and a game over screen, put together: [Menu → playing → game over](../../how-to/typescript/gameplay-recipes.md#menu--playing--game-over).

## Declare and start

The engine starts with **no state** (`engine.state === null`). Request the first one before the first frame:

```ts
engine.requestState(menu);
```

## Change state

```ts
context.requestState(playing);   // from a system, handler, task or hook
engine.requestState(gameOver);   // from outside
```

Read the current state with `engine.state` or `context.state`. It returns the same object you declared, so compare with `===`:

```ts
if (context.state === playing) {
  // ...
}
```

## Run only in some states

```ts
engine.addSystem(Phase.UpdateBefore, "startOnConfirm", startOnConfirm).inState(menu, gameOver);
engine.addNativeSystem(Phase.Update, "chasePlayer").inState(playing);
engine.on(PlayerDied, "endRound", endRound).inState(playing);
```

## Enter and exit hooks

```ts
engine.onEnter(playing, "startRound", (context) => {
  context.spawn([Player, Position, Health], { Position: { x: 480, y: 270 }, Health: { value: 3 } });
  context.schedule.every(5, (task) => spawnWave(task));
});

engine.onExit(playing, "saveBest", (context) => {
  best = Math.max(best, score.score);
});
```

A hook has the type `StateHookFunction`, `(context: StateContext) => void`, and receives a `StateContext`. `fromState` is the state being left (`null` on the first change) and `toState` the one being entered.

A hook written in C++ is registered with `engine.addNativeOnEnter(playing, "name")` and `addNativeOnExit` ([C++ systems, handlers and hooks](engine-and-systems.md#c-systems-handlers-and-hooks)). The same C++ hook can serve both; `context.entering()` tells which side it is on.

## Scope: automatic clean-up

`engine.world.spawn(...)` is never scoped ([the scope table](../../explanation/concepts/states.md#scope-automatic-clean-up)).

### Keep something across states: `persist`

```ts
engine.onEnter(playing, "startRound", (context) => {
  const camera = context.spawn([Camera, Position]);
  context.persist(camera);                     // survives every state change

  const music = context.schedule.every(30, (task) => changeTrack());
  context.schedule.persist(music);
});
```

For things that exist from the start, create them with `engine.world.spawn` before the first frame: they are never scoped.
