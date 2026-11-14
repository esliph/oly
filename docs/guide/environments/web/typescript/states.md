# States in TypeScript

[← Schedule](schedule.md) · [TypeScript client](README.md) · Concept: [States](../../../concepts/states.md) · Next: [Recipes →](recipes.md)

When a state changes, what the scope cleans and in what order hooks run are in the [concept page](../../../concepts/states.md). This page shows the TS calls.

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

A hook receives a `StateContext`. `fromState` is the state being left (`null` on the first change) and `toState` the one being entered.

A hook written in C++ is registered with `engine.addNativeOnEnter(playing, "name")` and `addNativeOnExit` ([C++ systems, handlers and hooks](engine-and-systems.md#c-systems-handlers-and-hooks)). The same C++ hook can serve both; `context.entering()` tells which side it is on.

## Scope: automatic clean-up

`engine.world.spawn(...)` is never scoped ([the scope table](../../../concepts/states.md#scope-automatic-clean-up)).

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

## Complete pattern: menu → playing → game over

A typical `src/game.ts`, reduced to the state logic.

```ts
export function installGame(engine: Engine): { kills: number } {
  const score = { kills: 0 };

  engine.actions.bind(confirm, "Enter");
  engine.actions.bind(pause, "Escape");

  engine.addSystem(Phase.UpdateBefore, "startOnConfirm", (context) => {
    if (context.actions.button(confirm).pressed) {
      context.requestState(playing);
    }
  }).inState(menu, gameOver);

  engine.addSystem(Phase.UpdateBefore, "pauseOnPress", (context) => {
    if (context.actions.button(pause).pressed) {
      context.pause();
    }
  }).inState(playing);
  engine.addSystem(Phase.UpdateBefore, "resumeOnPress", (context) => {
    if (context.actions.button(pause).pressed) {
      context.resume();
    }
  }).whilePaused().inState(playing);

  engine.addNativeSystem(Phase.Update, "integrate").inState(playing);
  engine.addNativeSystem(Phase.UpdateAfter, "collide").inState(playing);

  engine.on(PlayerDied, "endRound", (context) => context.requestState(gameOver));

  engine.onEnter(playing, "startRound", (context) => {
    score.kills = 0;
    context.spawn([Player, Position, Health], { Position: { x: 480, y: 270 }, Health: { value: 3 } });
    context.schedule.every(5, (task) => spawnWave(task));
  });
  // The pause belongs to the whole engine and only resumeOnPress (limited to playing) lifts it:
  // leaving playing while paused would freeze the next state.
  engine.onExit(playing, "resumeOnExit", (context) => context.resume());

  engine.requestState(menu);

  return score;
}
```

## Next

[Recipes →](recipes.md): ready patterns for common gameplay.
