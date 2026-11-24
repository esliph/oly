# Recipes in TypeScript

[Docs](../../README.md#how-to-guides) · Reference: [TypeScript client](../../reference/typescript/README.md) · C++: [Recipes](../gameplay-recipes.md)

Ready patterns for common game features, in TypeScript. Each recipe shows the schema entries it needs and the code. Every recipe also has a C++ version in the [recipes page](../gameplay-recipes.md), except the ones marked browser-only, which need the DOM or browser storage.

| Recipe | Uses |
|---|---|
| [Projectile with a lifetime](#projectile-with-a-lifetime) | component timer, deferred destroy |
| [Spawner every N seconds](#spawner-every-n-seconds) | `schedule.every`, scope |
| [Difficulty that ramps up](#difficulty-that-ramps-up) | closure counter |
| [Ability with a cooldown](#ability-with-a-cooldown) | `u8` flags, `schedule.after` |
| [Health, damage and death](#health-damage-and-death) | events, handler order |
| [Temporary status effect](#temporary-status-effect) | tag + `after` |
| [Pickups](#pickups) | overlap test, event, `isAlive` |
| [Homing projectile](#homing-projectile) | `entity` field, `get` on another entity |
| [Attach one entity to another](#attach-one-entity-to-another) | parent handle, offset |
| [Keep inside the screen or wrap around](#keep-inside-the-screen-or-wrap-around) | clamping, modulo |
| [Score and HUD](#score-and-hud) | TS state, render system, `trySingle` (browser-only) |
| [Round timer](#round-timer) | `context.time`, `after` |
| [Rebind a key](#rebind-a-key) | `clearBindings`, `bind`, `isBound` |
| [Pause screen](#pause-screen) | `whilePaused`, `paused` |
| [Restart the game](#restart-the-game) | states and scope |
| [Menu → playing → game over](#menu--playing--game-over) | `requestState`, hooks, `inState`, scope |
| [High score saved in the browser](#high-score-saved-in-the-browser) | injected storage, `onExit` (browser-only) |
| [A sequence of timed steps](#a-sequence-of-timed-steps) | chained `after` |
| [Random spawn positions](#random-spawn-positions) | injected `random` |
| [Debug overlay](#debug-overlay) | counting with queries (browser-only) |
| [Count entities](#count-entities) | `eachBlock`, `block.count` |
| [Find the nearest entity](#find-the-nearest-entity) | `eachBlock`, squared distance |
| [Two queries together (pairs)](#two-queries-together-pairs) | `single` into a local value, `eachBlock` |
| [Several shapes with one function](#several-shapes-with-one-function) | filters as parameters |

---

## Projectile with a lifetime

```
# game.oly
component Lifetime { seconds: f32 }
```

```ts
engine.addSystem(Phase.Update, "expire", (context) => {
  const deltaTime = context.fixedDeltaTime;

  context.query({ write: [Lifetime] }).eachBlock((block) => {
    const lifetime = block.write(Lifetime);

    for (let row = 0; row < block.count; ++row) {
      const remaining = lifetime.seconds.get(row) - deltaTime;
      lifetime.seconds.set(row, remaining);

      if (remaining <= 0) {
        context.destroy(block.entity(row));
      }
    }
  });
});

// spawning
context.spawn([Bullet, Position, Velocity, Lifetime], {
  Position: { x, y },
  Velocity: { x: directionX * 600, y: directionY * 600 },
  Lifetime: { seconds: 1.5 },
});
```

A component timer is cheaper than one scheduled task per bullet when you have hundreds of bullets. For a few long-lived things, `schedule.after` is simpler.

## Spawner every N seconds

```ts
engine.onEnter(playing, "startSpawner", (context) => {
  context.schedule.every(2, (task) => {
    task.spawn([Enemy, Position, Health], { Position: { x: 0, y: 0 }, Health: { value: 1 } });
  });
});
```

The task is created while `playing` is active, so it is cancelled when the round ends. No clean-up code.

## Difficulty that ramps up

Keep the counter in the closure of the hook. A new round creates a new closure, so the counter restarts.

```ts
engine.onEnter(playing, "startWaves", (context) => {
  let wave = 0;

  context.schedule.every(5, (task) => {
    wave += 1;
    const enemyCount = 2 + wave;
    const speed = 80 + wave * 10;

    for (let index = 0; index < enemyCount; ++index) {
      task.spawn([Enemy, Position, Velocity, Chaser], { Position: edgePoint(random), Chaser: { speed } });
    }
  });
});
```

## Ability with a cooldown

Use `u8` fields for flags that change often; adding and removing tags every use would move the entity between tables.

```
# game.oly
component Dash { ready: u8, active: u8, duration: f32, recharge: f32, speed: f32 }
```

```ts
engine.addSystem(Phase.UpdateBefore, "dash", (context) => {
  if (!context.actions.button(dash).pressed) {
    return;
  }

  const player = context.query({ read: [Dash], with: [Player] }).trySingle();

  if (player === null || player.get(Dash).ready === 0) {
    return;
  }

  const entity = player.entity;
  const { duration, recharge } = player.get(Dash);
  context.set(entity, Dash, { ready: 0, active: 1 });
  context.schedule.after(duration, (task) => {
    if (task.isAlive(entity)) {
      task.set(entity, Dash, { active: 0 });
    }
  });
  context.schedule.after(recharge, (task) => {
    if (task.isAlive(entity)) {
      task.set(entity, Dash, { ready: 1 });
    }
  });
});
```

C++: [Ability with a cooldown](../gameplay-recipes.md#ability-with-a-cooldown).

## Health, damage and death

Damage is applied where the hit is detected. Death is an event, so several reactions can hang off it.

```
# game.oly
component Health { value: i32 }
event EnemyDied { entity: entity, x: f32, y: f32 }
```

```ts
// in the collision system, for each hit:
const remaining = context.get(enemy, Health).value - 1;
context.set(enemy, Health, { value: remaining });

if (remaining <= 0) {
  const at = context.get(enemy, Position);
  context.emit(EnemyDied, { entity: enemy, x: at.x, y: at.y });
}
```

```ts
engine.addNativeHandler(EnemyDied, "dropCoin");                       // reacts: reads the position
engine.on(EnemyDied, "countKill", (_context, batch) => countKill(score, batch));
engine.on(EnemyDied, "destroyDead", (context, batch) => {             // destroys: registered last
  const enemies = batch.read().entity;

  for (let index = 0; index < batch.count; ++index) {
    const enemy = enemies.get(index);

    if (context.isAlive(enemy)) {
      context.destroy(enemy);
    }
  }
});
```

## Temporary status effect

A tag that lasts a few seconds. Filtering with `without` makes affected entities skip systems for free.

```
# game.oly
tag Stunned
```

```ts
function stun(context: HandlerContext, enemy: Entity, seconds: number): void {
  if (!context.isAlive(enemy) || context.has(enemy, Stunned)) {
    return;
  }

  context.add(enemy, Stunned);
  context.schedule.after(seconds, (task) => {
    if (task.isAlive(enemy)) {
      task.remove(enemy, Stunned);
    }
  });
}

// stunned enemies do not chase
context.query({ read: [Position, Chaser], write: [Velocity], with: [Enemy], without: [Stunned] });
```

## Pickups

Detect the overlap in a system, emit an event, collect in a handler.

```
# game.oly
tag Coin
event CoinCollected { coin: entity }
```

```ts
engine.addSystem(Phase.UpdateAfter, "touchCoins", (context) => {
  const player = context.query({ read: [Position, Collider], with: [Player] }).trySingle();

  if (player === null) {
    return;
  }

  const { x, y } = player.get(Position);
  const reach = player.get(Collider).radius;

  context.query({ read: [Position, Collider], with: [Coin] }).eachBlock((block) => {
    const position = block.read(Position);
    const collider = block.read(Collider);

    for (let row = 0; row < block.count; ++row) {
      const deltaX = position.x.get(row) - x;
      const deltaY = position.y.get(row) - y;
      const radii = reach + collider.radius.get(row);

      if (deltaX * deltaX + deltaY * deltaY <= radii * radii) {
        context.emit(CoinCollected, { coin: block.entity(row) });
      }
    }
  });
});

engine.on(CoinCollected, "collect", (context, batch) => {
  const coins = batch.read().coin;

  for (let index = 0; index < batch.count; ++index) {
    const coin = coins.get(index);

    if (context.isAlive(coin)) {
      context.destroy(coin);
      score.coins += 1;
    }
  }
});
```

## Homing projectile

```
# game.oly
component Homing { target: entity, speed: f32 }
```

```ts
engine.addSystem(Phase.Update, "home", (context) => {
  context.query({ read: [Position, Homing], write: [Velocity] }).eachBlock((block) => {
    const position = block.read(Position);
    const homing = block.read(Homing);
    const velocity = block.write(Velocity);

    for (let row = 0; row < block.count; ++row) {
      const target = homing.target.get(row);

      if (!context.isAlive(target)) {
        context.destroy(block.entity(row));
        continue;
      }

      const goal = context.get(target, Position);
      const deltaX = goal.x - position.x.get(row);
      const deltaY = goal.y - position.y.get(row);
      const length = Math.hypot(deltaX, deltaY) || 1;
      velocity.x.set(row, (deltaX / length) * homing.speed.get(row));
      velocity.y.set(row, (deltaY / length) * homing.speed.get(row));
    }
  });
});
```

`context.get` crosses into WASM once per missile. For a few dozen missiles this is fine; for thousands, write the system in C++ ([C++ version](../gameplay-recipes.md#homing-projectile)).

## Attach one entity to another

A shield, a health bar or a turret that follows its owner.

```
# game.oly
component Attached { parent: entity, offsetX: f32, offsetY: f32 }
```

```ts
// in UpdateAfter, after everything moved
engine.addSystem(Phase.UpdateAfter, "followParent", (context) => {
  context.query({ read: [Attached], write: [Position] }).eachBlock((block) => {
    const attached = block.read(Attached);
    const position = block.write(Position);

    for (let row = 0; row < block.count; ++row) {
      const parent = attached.parent.get(row);

      if (!context.isAlive(parent)) {
        context.destroy(block.entity(row));     // the owner is gone: so is the attachment
        continue;
      }

      const parentPosition = context.get(parent, Position);
      position.x.set(row, parentPosition.x + attached.offsetX.get(row));
      position.y.set(row, parentPosition.y + attached.offsetY.get(row));
    }
  });
});
```

## Keep inside the screen or wrap around

```ts
engine.addSystem(Phase.UpdateAfter, "confinePlayer", (context) => {
  context.query({ read: [Collider], write: [Position], with: [Player] }).eachBlock((block) => {
    const collider = block.read(Collider);
    const position = block.write(Position);

    for (let row = 0; row < block.count; ++row) {
      const radius = collider.radius.get(row);
      position.x.set(row, Math.min(Math.max(position.x.get(row), radius), WorldWidth - radius));
      position.y.set(row, Math.min(Math.max(position.y.get(row), radius), WorldHeight - radius));
    }
  });
});

engine.addSystem(Phase.UpdateAfter, "wrapAsteroids", (context) => {
  context.query({ write: [Position], with: [Asteroid] }).eachBlock((block) => {
    const position = block.write(Position);

    for (let row = 0; row < block.count; ++row) {
      position.x.set(row, (position.x.get(row) + WorldWidth) % WorldWidth);
      position.y.set(row, (position.y.get(row) + WorldHeight) % WorldHeight);
    }
  });
});
```

## Score and HUD

Game rules that are not per entity (score, wave number, combo) can live in plain TS objects. The render system reads them. This example is from a bigger game: its `Drawer` also has `text`, and `startGameplay` returns the `score`. The template's `Drawer` has only `clear` and `square` ([Your first web game](../../tutorials/first-web-game.md#7-read-the-typescript-system)).

```ts
export function startGameplay(engine: Engine, drawer: Drawer): Score {   // a bigger game: the template returns void
  const score = { kills: 0, coins: 0 };

  engine.on(EnemyDied, "countKill", (_context, batch) => {
    score.kills += batch.count;
  });

  engine.addSystem(Phase.Render, "hud", (context) => {
    const player = context.query({ read: [Health], with: [Player] }).trySingle();

    if (player === null) {
      return;
    }

    drawer.text(16, 24, `Health ${player.get(Health).value}`);
    drawer.text(16, 48, `Score ${score.kills * 10 + score.coins * 5}`);
  });

  return score;
}
```

Returning the score object lets tests read it directly.

## Round timer

```ts
const RoundSeconds = 60;
let roundEnd = 0;

engine.onEnter(playing, "startClock", (context) => {
  roundEnd = context.time + RoundSeconds;
  context.schedule.after(RoundSeconds, (task) => task.requestState(gameOver));
});

engine.addSystem(Phase.Render, "drawClock", (context) => {
  if (context.state !== playing) {
    return;
  }

  const remaining = Math.max(0, roundEnd - context.time);
  drawer.text(900, 24, `${Math.ceil(remaining)}`);
});
```

Both use the game clock, so the timer stops while paused and the task is cancelled if the round ends earlier.

## Rebind a key

```ts
function rebind(engine: Engine, action: ButtonAction, button: ButtonName): void {
  engine.actions.clearBindings(action);
  engine.actions.bind(action, button);
}

rebind(engine, jump, "KeyK");
```

A "press a key to rebind" screen takes the next key the player presses:

```ts
window.addEventListener("keydown", function capture(event) {
  if (!isButtonName(event.code)) {
    return;
  }

  rebind(engine, jump, event.code);
  window.removeEventListener("keydown", capture);
});
```

Use `engine.actions.isBound(button)` to warn when a key is already in use.

## Pause screen

```ts
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

engine.onExit(playing, "resumeOnExit", (context) => context.resume());

engine.addSystem(Phase.Render, "pauseOverlay", (context) => {
  if (context.paused) {
    drawer.text(400, 270, "Paused — Esc to continue");
  }
});
```

## Restart the game

With states, restarting is only a state change. Everything the round created was scoped to `playing`:

```ts
engine.addSystem(Phase.UpdateBefore, "playAgain", (context) => {
  if (context.actions.button(confirm).pressed) {
    context.requestState(playing);
  }
}).inState(gameOver);

engine.onEnter(playing, "startRound", (context) => {
  score.kills = 0;
  spawnPlayer(context);
  startWaves(context);
});
```

To restart from inside `playing` (a "retry" key), request `playing` again. A request for the active state is a full change: `onExit`, scope clean-up, then `onEnter`.

```ts
engine.addSystem(Phase.UpdateBefore, "retry", (context) => {
  if (context.actions.button(retry).pressed) {
    context.requestState(playing);
  }
}).inState(playing);
```

## Menu → playing → game over

```
# game.oly
action confirm: button
action pause: button
tag Player
component Health { value: i32 }
component Score { kills: i32 }
event PlayerDied { entity: entity }
state menu
state playing
state gameOver
```

The registrations, reduced to the state logic:

```ts
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

engine.onExit(playing, "resumeOnExit", (context) => context.resume());

engine.requestState(menu);
```

Notice:

- No code destroys the enemies or cancels the wave timer: the scope does it ([Scope: automatic clean-up](../../explanation/concepts/states.md#scope-automatic-clean-up)).
- The pause belongs to the whole engine, and only `resumeOnPress`, limited to `playing`, lifts it. Leaving `playing` while paused would freeze the next state, so `resumeOnExit` resumes on the way out.

## High score saved in the browser

Keep `localStorage` out of the game logic: inject it.

```ts
export interface Storage {
  loadBest(): number;
  saveBest(value: number): void;
}

engine.onExit(playing, "saveBest", () => {
  if (score.total > options.storage.loadBest()) {
    options.storage.saveBest(score.total);
  }
});
```

```ts
// game.ts
const storage = {
  loadBest: () => Number(localStorage.getItem("best") ?? 0),
  saveBest: (value: number) => localStorage.setItem("best", String(value)),
};
```

In tests, pass an object that keeps the value in memory.

## A sequence of timed steps

A countdown "3, 2, 1, go!" before the round:

```ts
engine.onEnter(countdown, "startCountdown", (context) => {
  banner.text = "3";
  context.schedule.after(1, (first) => {
    banner.text = "2";
    first.schedule.after(1, (second) => {
      banner.text = "1";
      second.schedule.after(1, (third) => {
        banner.text = "";
        third.requestState(playing);
      });
    });
  });
});
```

A task created inside another task runs from the next step on, so the chain cannot loop within one step.

## Random spawn positions

Inject the random source so tests can fix it:

```ts
function edgePoint(random: () => number): { x: number; y: number } {
  const side = Math.floor(random() * 4);
  const along = random();

  if (side === 0) {
    return { x: along * Width, y: 0 };
  }
  if (side === 1) {
    return { x: Width, y: along * Height };
  }
  if (side === 2) {
    return { x: along * Width, y: Height };
  }

  return { x: 0, y: along * Height };
}
```

C++: [Random spawn positions](../gameplay-recipes.md#random-spawn-positions).

## Debug overlay

```ts
engine.addSystem(Phase.Render, "debug", (context) => {
  let enemies = 0;
  context.query({ with: [Enemy] }).eachBlock((block) => {
    enemies += block.count;
  });

  drawer.text(16, 520, `enemies ${enemies}  time ${context.time.toFixed(1)}s  ${context.paused ? "paused" : ""}`);
});
```

In tests, call `engine.world.checkInvariants()` after each step ([Testing a web game](../test-a-web-game.md)).

## Count entities

```ts
function count(world: World, spec: QuerySpec): number {
  let total = 0;
  world.query(spec).eachBlock((block) => {
    total += block.count;
  });

  return total;
}

count(world, { with: [Enemy] });
count(world, { with: [Enemy], without: [Frozen] });
```

C++: [Count entities](../gameplay-recipes.md#count-entities).

## Find the nearest entity

```ts
function nearestEnemy(context: SystemContext, x: number, y: number): Entity | null {
  let nearest: Entity | null = null;
  let nearestDistance = Infinity;

  context.query({ read: [Position], with: [Enemy] }).eachBlock((block) => {
    const position = block.read(Position);

    for (let row = 0; row < block.count; ++row) {
      const deltaX = position.x.get(row) - x;
      const deltaY = position.y.get(row) - y;
      const distance = deltaX * deltaX + deltaY * deltaY;

      if (distance < nearestDistance) {
        nearestDistance = distance;
        nearest = block.entity(row);
      }
    }
  });

  return nearest;
}
```

C++: [Find the nearest entity](../gameplay-recipes.md#find-the-nearest-entity).

## Two queries together (pairs)

Read the small set into local values first, then iterate the large set. Here, enemies chase the player:

```ts
const target = context.query({ read: [Position], with: [Player] }).single().get(Position);

context.query({ read: [Position, Chaser], write: [Velocity], with: [Enemy] }).eachBlock((block) => {
  const position = block.read(Position);
  const chaser = block.read(Chaser);
  const velocity = block.write(Velocity);

  for (let row = 0; row < block.count; ++row) {
    const deltaX = target.x - position.x.get(row);
    const deltaY = target.y - position.y.get(row);
    const length = Math.hypot(deltaX, deltaY) || 1;
    velocity.x.set(row, (deltaX / length) * chaser.speed.get(row));
    velocity.y.set(row, (deltaY / length) * chaser.speed.get(row));
  }
});
```

For "each against each" (bullets × enemies), prefer a C++ system: see [Nested queries](../gameplay-recipes.md#two-queries-together-pairs).

C++: [Two queries together (pairs)](../gameplay-recipes.md#two-queries-together-pairs).

## Several shapes with one function

```ts
type Filter = Pick<QuerySpec, "with" | "without">;

function drawCircles(context: SystemContext, drawer: Drawer, filter: Filter, color: string): void {
  context.query({ read: [Position, Collider], ...filter }).eachBlock((block) => {
    const position = block.read(Position);
    const collider = block.read(Collider);

    for (let row = 0; row < block.count; ++row) {
      drawer.circle(position.x.get(row), position.y.get(row), collider.radius.get(row), color);
    }
  });
}

drawCircles(context, drawer, { with: [Enemy] }, "red");
drawCircles(context, drawer, { with: [Player], without: [Invulnerable] }, "blue");
```

C++: [Several shapes with one function](../gameplay-recipes.md#several-shapes-with-one-function).
