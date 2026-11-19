# Recipes

[Guide index](../guide/README.md) · [Cheat sheet](../guide/cheat-sheet.md)

Ready patterns for common game features, in C++. Each recipe shows the schema entries it needs and the code: the bodies are written against the context, so they run in every environment, and the registration is labelled under them. Every recipe with a TS form ends with a link to its TypeScript version, which registers the same pieces from the TS client; order bands and plugins are C++ only.

| Recipe | Uses |
|---|---|
| [Projectile with a lifetime](#projectile-with-a-lifetime) | component timer, deferred destroy |
| [Spawner every N seconds](#spawner-every-n-seconds) | `schedule().every`, scope |
| [Difficulty that ramps up](#difficulty-that-ramps-up) | component counter |
| [Ability with a cooldown](#ability-with-a-cooldown) | `u8` flags, `schedule().after`, payload |
| [Health, damage and death](#health-damage-and-death) | events, handler order |
| [Temporary status effect](#temporary-status-effect) | tag + `after` |
| [Pickups](#pickups) | overlap test, event, `isAlive` |
| [Homing projectile](#homing-projectile) | `entity` field, `tryGet` on another entity |
| [Attach one entity to another](#attach-one-entity-to-another) | parent handle, offset |
| [Keep inside the screen or wrap around](#keep-inside-the-screen-or-wrap-around) | clamping, modulo |
| [Score and HUD](typescript/gameplay-recipes.md#score-and-hud) | browser-only |
| [Round timer](#round-timer) | `context.time()`, `after` |
| [Pause screen](#pause-screen) | `setWhilePaused`, `paused` |
| [Restart the game](#restart-the-game) | states and scope |
| [High score saved in the browser](typescript/gameplay-recipes.md#high-score-saved-in-the-browser) | browser-only |
| [A sequence of timed steps](#a-sequence-of-timed-steps) | chained `after` |
| [Random spawn positions](#random-spawn-positions) | `std::minstd_rand` |
| [Debug overlay](typescript/gameplay-recipes.md#debug-overlay) | browser-only |
| [Camera that follows after movement](#camera-that-follows-after-movement) | `OrderBand::Last` |
| [Share systems between games](#share-systems-between-games) | plugin, `PLUGINS` |
| [Count entities](#count-entities) | `eachBlock`, rows per table |
| [Find the nearest entity](#find-the-nearest-entity) | `each` with the `Entity`, squared distance |
| [Two queries together (pairs)](#two-queries-together-pairs) | `single` into a local value, nested `each` |
| [Several shapes with one function](#several-shapes-with-one-function) | filters as parameters |

---

## Projectile with a lifetime

```
# game.oly
component Lifetime { seconds: f32 }
component Weapon { interval: f32, cooldown: f32 }
```

```cpp
OLY_SYSTEM(expire) {
  const float deltaTime = context.fixedDeltaTime();

  context.query<Lifetime>().each([&](oly::ecs::Entity entity, Lifetime& lifetime) {
    lifetime.seconds -= deltaTime;

    if (lifetime.seconds <= 0.0f) {
      context.destroy(entity);
    }
  });
}

OLY_SYSTEM(fireWeapon) {
  auto [player, position, weapon] = context.query<const Position, Weapon>(oly::ecs::With<Player>{}).single();
  weapon.cooldown = std::max(0.0f, weapon.cooldown - context.fixedDeltaTime());

  if (!context.actions().button(actions::shoot).held || weapon.cooldown > 0.0f) {
    return;
  }

  const oly::input::PointerState aim = context.actions().pointer(actions::aim);
  const float deltaX = aim.x - position.x;
  const float deltaY = aim.y - position.y;
  const float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);

  if (!aim.valid || length == 0.0f) {
    return;
  }

  weapon.cooldown = weapon.interval;
  context.spawn(Bullet{}, Position{position.x, position.y}, Velocity{deltaX / length * 600.0f, deltaY / length * 600.0f}, Lifetime{1.5f});
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "expire", expire);
engine.addSystem(oly::app::Phase::Update, "fireWeapon", fireWeapon);
```

A component timer is cheaper than one scheduled task per bullet when you have hundreds of bullets. For a few long-lived things, `schedule().after` is simpler.

TypeScript: [Projectile with a lifetime](typescript/gameplay-recipes.md#projectile-with-a-lifetime).

## Spawner every N seconds

```cpp
void spawnEnemy(oly::app::TaskContext& context) {
  context.spawn(Enemy{}, Position{0.0f, 0.0f}, Health{1});
}

OLY_STATE_HOOK(startSpawner) {
  context.schedule().every(2.0, spawnEnemy);
}
```

**Where you own the `Engine`** (native):

```cpp
engine.onEnter(states::playing, "startSpawner", startSpawner);
```

The task is created while `playing` is active, so it is cancelled when the round ends. No clean-up code.

TypeScript: [Spawner every N seconds](typescript/gameplay-recipes.md#spawner-every-n-seconds).

## Difficulty that ramps up

Keep the counter in a component on a scoped entity. A new round creates a new entity, so the counter restarts.

```
# game.oly
component Difficulty { wave: u32 }
```

```cpp
void spawnWave(oly::app::TaskContext& context) {
  Difficulty& difficulty = std::get<1>(context.query<Difficulty>().single());
  difficulty.wave += 1;

  const uint32_t enemyCount = 2 + difficulty.wave;
  const float speed = 80.0f + static_cast<float>(difficulty.wave) * 10.0f;

  for (uint32_t index = 0; index < enemyCount; ++index) {
    context.spawn(Enemy{}, Position{static_cast<float>(index) * 40.0f, 0.0f}, Velocity{}, Chaser{speed});
  }
}

OLY_STATE_HOOK(startWaves) {
  context.spawn(Difficulty{0});
  context.schedule().every(5.0, spawnWave);
}
```

**Where you own the `Engine`** (native):

```cpp
engine.onEnter(states::playing, "startWaves", startWaves);
```

TypeScript: [Difficulty that ramps up](typescript/gameplay-recipes.md#difficulty-that-ramps-up).

## Ability with a cooldown

Use `u8` fields for flags that change often; adding and removing tags every use would move the entity between tables.

```
# game.oly
component Dash { ready: u8, active: u8, duration: f32, recharge: f32, speed: f32 }
```

```cpp
struct PlayerTarget {
  oly::ecs::Entity player;
};

void endDash(oly::app::TaskContext& context) {
  const oly::ecs::Entity player = context.payload<PlayerTarget>().player;

  if (context.isAlive(player)) {
    context.get<Dash>(player).active = 0;
  }
}

void rechargeDash(oly::app::TaskContext& context) {
  const oly::ecs::Entity player = context.payload<PlayerTarget>().player;

  if (context.isAlive(player)) {
    context.get<Dash>(player).ready = 1;
  }
}

OLY_SYSTEM(dash) {
  if (!context.actions().button(actions::dash).pressed) {
    return;
  }

  auto player = context.query<Dash>(oly::ecs::With<Player>{}).trySingle();
  if (!player || std::get<1>(*player).ready == 0) {
    return;
  }

  auto& [entity, dashState] = *player;
  dashState.ready = 0;
  dashState.active = 1;

  context.schedule().after(dashState.duration, endDash, PlayerTarget{entity});
  context.schedule().after(dashState.recharge, rechargeDash, PlayerTarget{entity});
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::UpdateBefore, "dash", dash);
```

The payload carries the entity to the task ([Schedule](../guide/concepts/schedule.md#payloads)).

TypeScript: [Ability with a cooldown](typescript/gameplay-recipes.md#ability-with-a-cooldown).

## Health, damage and death

Damage is applied where the hit is detected. Death is an event, so several reactions can hang off it.

```
# game.oly
component Health { value: i32 }
event EnemyDied { entity: entity, x: f32, y: f32 }
```

```cpp
// in the collision system, for each hit:
health.value -= 1;

if (health.value <= 0) {
  context.emit(EnemyDied{enemy, position.x, position.y});
}
```

```cpp
OLY_HANDLER(dropCoin, EnemyDied) {                       // reacts: reads the position
  for (const EnemyDied& died : events) {
    context.spawn(Coin{}, Position{died.x, died.y});
  }
}

OLY_HANDLER(countKill, EnemyDied) {
  std::get<1>(context.query<Score>().single()).kills += static_cast<int32_t>(events.size());
}

OLY_HANDLER(destroyDead, EnemyDied) {                    // destroys: registered last
  for (const EnemyDied& died : events) {
    if (context.isAlive(died.entity)) {
      context.destroy(died.entity);
    }
  }
}
```

**Where you own the `Engine`** (native):

```cpp
engine.on<EnemyDied>("dropCoin", [](oly::app::HandlerContext& context) { dropCoin(context, context.events<EnemyDied>()); });
engine.on<EnemyDied>("countKill", [](oly::app::HandlerContext& context) { countKill(context, context.events<EnemyDied>()); });
engine.on<EnemyDied>("destroyDead", [](oly::app::HandlerContext& context) { destroyDead(context, context.events<EnemyDied>()); });
```

The lambda passes the batch the macro's function expects ([Events](../guide/concepts/events.md#declare-emit-handle)).

TypeScript: [Health, damage and death](typescript/gameplay-recipes.md#health-damage-and-death).

## Temporary status effect

A tag that lasts a few seconds. Filtering with `Without` makes affected entities skip systems for free.

```
# game.oly
tag Stunned
```

```cpp
struct StunTarget {
  oly::ecs::Entity enemy;
};

void endStun(oly::app::TaskContext& context) {
  const oly::ecs::Entity enemy = context.payload<StunTarget>().enemy;

  if (context.isAlive(enemy)) {
    context.remove<Stunned>(enemy);
  }
}

void stun(oly::app::HandlerContext& context, oly::ecs::Entity enemy, double seconds) {
  if (!context.isAlive(enemy) || context.has<Stunned>(enemy)) {
    return;
  }

  context.add(enemy, Stunned{});
  context.schedule().after(seconds, endStun, StunTarget{enemy});
}

OLY_SYSTEM(chase) {
  // stunned enemies do not chase
  context.query<const Position, const Chaser, Velocity>(oly::ecs::With<Enemy>{}, oly::ecs::Without<Stunned>{}).each([](const Position&, const Chaser&, Velocity&) {
    // steer toward the target
  });
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "chase", chase);
```

`stun` is a helper that a handler calls with its own context.

TypeScript: [Temporary status effect](typescript/gameplay-recipes.md#temporary-status-effect).

## Pickups

Detect the overlap in a system, emit an event, collect in a handler.

```
# game.oly
tag Coin
event CoinCollected { coin: entity }
```

```cpp
OLY_SYSTEM(touchCoins) {
  auto player = context.query<const Position, const Collider>(oly::ecs::With<Player>{}).trySingle();
  if (!player) {
    return;
  }

  const Position origin = std::get<1>(*player);
  const float reach = std::get<2>(*player).radius;

  context.query<const Position, const Collider>(oly::ecs::With<Coin>{}).each([&](oly::ecs::Entity coin, const Position& position, const Collider& collider) {
    const float deltaX = position.x - origin.x;
    const float deltaY = position.y - origin.y;
    const float radii = reach + collider.radius;

    if (deltaX * deltaX + deltaY * deltaY <= radii * radii) {
      context.emit(CoinCollected{coin});
    }
  });
}

OLY_HANDLER(collect, CoinCollected) {
  Score& score = std::get<1>(context.query<Score>().single());

  for (const CoinCollected& collected : events) {
    if (context.isAlive(collected.coin)) {
      context.destroy(collected.coin);
      score.coins += 1;
    }
  }
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::UpdateAfter, "touchCoins", touchCoins);
engine.on<CoinCollected>("collect", [](oly::app::HandlerContext& context) { collect(context, context.events<CoinCollected>()); });
```

TypeScript: [Pickups](typescript/gameplay-recipes.md#pickups).

## Homing projectile

```
# game.oly
component Homing { target: entity, speed: f32 }
```

```cpp
OLY_SYSTEM(home) {
  context.query<const Position, const Homing, Velocity>().each([&](oly::ecs::Entity entity, const Position& position, const Homing& homing, Velocity& velocity) {
    const Position* goal = context.tryGet<Position>(homing.target);

    if (goal == nullptr) {
      context.destroy(entity);
      return;
    }

    const float deltaX = goal->x - position.x;
    const float deltaY = goal->y - position.y;
    const float length = std::max(std::sqrt(deltaX * deltaX + deltaY * deltaY), 1e-6f);

    velocity = Velocity{deltaX / length * homing.speed, deltaY / length * homing.speed};
  });
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "home", home);
```

`tryGet` is `nullptr` if the target died or lost its `Position`, so one call replaces `isAlive` plus `get`.

TypeScript: [Homing projectile](typescript/gameplay-recipes.md#homing-projectile).

## Attach one entity to another

A shield, a health bar or a turret that follows its owner.

```
# game.oly
component Attached { parent: entity, offsetX: f32, offsetY: f32 }
```

```cpp
// in UpdateAfter, after everything moved
OLY_SYSTEM(followParent) {
  context.query<const Attached, Position>().each([&](oly::ecs::Entity entity, const Attached& attached, Position& position) {
    const Position* parent = context.tryGet<Position>(attached.parent);

    if (parent == nullptr) {
      context.destroy(entity);   // the owner is gone: so is the attachment
      return;
    }

    position = Position{parent->x + attached.offsetX, parent->y + attached.offsetY};
  });
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::UpdateAfter, "followParent", followParent);
```

TypeScript: [Attach one entity to another](typescript/gameplay-recipes.md#attach-one-entity-to-another).

## Keep inside the screen or wrap around

```cpp
OLY_SYSTEM(confinePlayer) {
  auto [player, position, collider] = context.query<Position, const Collider>(oly::ecs::With<Player>{}).single();
  position.x = std::clamp(position.x, collider.radius, WorldWidth - collider.radius);
  position.y = std::clamp(position.y, collider.radius, WorldHeight - collider.radius);
}

OLY_SYSTEM(wrapAsteroids) {
  context.query<Position>(oly::ecs::With<Asteroid>{}).each([](Position& position) {
    position.x = std::fmod(position.x + WorldWidth, WorldWidth);
    position.y = std::fmod(position.y + WorldHeight, WorldHeight);
  });
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::UpdateAfter, "confinePlayer", confinePlayer);
engine.addSystem(oly::app::Phase::UpdateAfter, "wrapAsteroids", wrapAsteroids);
```

TypeScript: [Keep inside the screen or wrap around](typescript/gameplay-recipes.md#keep-inside-the-screen-or-wrap-around).

## Round timer

```
# game.oly
component Round { endTime: f64 }
```

```cpp
constexpr double RoundSeconds = 60.0;

void endRound(oly::app::TaskContext& context) {
  context.requestState(states::gameOver);
}

OLY_STATE_HOOK(startClock) {
  context.spawn(Round{context.time() + RoundSeconds});
  context.schedule().after(RoundSeconds, endRound);
}

OLY_SYSTEM(readClock) {
  if (context.state() != states::playing) {
    return;
  }

  const Round& round = std::get<1>(context.query<const Round>().single());
  const double remaining = std::max(0.0, round.endTime - context.time());

  // draw `remaining` here
}
```

**Where you own the `Engine`** (native):

```cpp
engine.onEnter(states::playing, "startClock", startClock);
engine.addSystem(oly::app::Phase::Render, "readClock", readClock);
```

Both use the game clock, so the timer stops while paused and the task is cancelled if the round ends earlier.

TypeScript: [Round timer](typescript/gameplay-recipes.md#round-timer).

## Pause screen

```cpp
OLY_SYSTEM(pauseOnPress) {
  if (context.actions().button(actions::pause).pressed) {
    context.pause();
  }
}

OLY_SYSTEM(resumeOnPress) {
  if (context.actions().button(actions::pause).pressed) {
    context.resume();
  }
}

OLY_STATE_HOOK(resumeOnExit) {
  context.resume();
}

OLY_SYSTEM(pauseOverlay) {
  if (context.paused()) {
    // draw "Paused — Esc to continue"
  }
}
```

**Where you own the `Engine`** (native):

```cpp
engine.setSystemStates(engine.addSystem(oly::app::Phase::UpdateBefore, "pauseOnPress", pauseOnPress), {states::playing});

oly::app::SystemId resumeId = engine.addSystem(oly::app::Phase::UpdateBefore, "resumeOnPress", resumeOnPress);
engine.setWhilePaused(resumeId, true);
engine.setSystemStates(resumeId, {states::playing});

engine.onExit(states::playing, "resumeOnExit", resumeOnExit);
engine.addSystem(oly::app::Phase::Render, "pauseOverlay", pauseOverlay);
```

TypeScript: [Pause screen](typescript/gameplay-recipes.md#pause-screen).

## Restart the game

With states, restarting is only a state change. Everything the round created was scoped to `playing`:

```cpp
OLY_SYSTEM(playAgain) {
  if (context.actions().button(actions::confirm).pressed) {
    context.requestState(states::playing);
  }
}

OLY_STATE_HOOK(startRound) {
  std::get<1>(context.query<Score>().single()).kills = 0;
  context.spawn(Player{}, Position{480.0f, 270.0f}, Health{3});
  context.schedule().every(5.0, spawnWave);
}

OLY_SYSTEM(retry) {
  if (context.actions().button(actions::retry).pressed) {
    context.requestState(states::playing);
  }
}
```

**Where you own the `Engine`** (native):

```cpp
engine.setSystemStates(engine.addSystem(oly::app::Phase::UpdateBefore, "playAgain", playAgain), {states::gameOver});
engine.onEnter(states::playing, "startRound", startRound);
engine.setSystemStates(engine.addSystem(oly::app::Phase::UpdateBefore, "retry", retry), {states::playing});
```

To restart from inside `playing` (a "retry" key), request `playing` again. A request for the active state is a full change: `onExit`, scope clean-up, then `onEnter`.

TypeScript: [Restart the game](typescript/gameplay-recipes.md#restart-the-game).

## A sequence of timed steps

A countdown "3, 2, 1, go!" before the round. Each task schedules the next one:

```
# game.oly
component Banner { number: u8 }
```

```cpp
void showGo(oly::app::TaskContext& context) {
  std::get<1>(context.query<Banner>().single()).number = 0;
  context.requestState(states::playing);
}

void showOne(oly::app::TaskContext& context) {
  std::get<1>(context.query<Banner>().single()).number = 1;
  context.schedule().after(1.0, showGo);
}

void showTwo(oly::app::TaskContext& context) {
  std::get<1>(context.query<Banner>().single()).number = 2;
  context.schedule().after(1.0, showOne);
}

OLY_STATE_HOOK(startCountdown) {
  context.spawn(Banner{3});
  context.schedule().after(1.0, showTwo);
}
```

**Where you own the `Engine`** (native):

```cpp
engine.onEnter(states::countdown, "startCountdown", startCountdown);
```

A task created inside another task runs from the next step on, so the chain cannot loop within one step.

TypeScript: [A sequence of timed steps](typescript/gameplay-recipes.md#a-sequence-of-timed-steps).

## Random spawn positions

Keep one `std::minstd_rand` with a fixed seed where tests can set it, and pass it to the code that needs it:

```cpp
oly::ecs::Entity spawnAtEdge(oly::app::SystemContext& context, std::minstd_rand& random) {
  const uint32_t side = random() % 4;
  const float along = static_cast<float>(random() % 10000) / 10000.0f;

  switch (side) {
    case 0:
      return context.spawn(Enemy{}, Position{along * Width, 0.0f});
    case 1:
      return context.spawn(Enemy{}, Position{Width, along * Height});
    case 2:
      return context.spawn(Enemy{}, Position{along * Width, Height});
    default:
      return context.spawn(Enemy{}, Position{0.0f, along * Height});
  }
}

OLY_SYSTEM(spawnEnemies) {
  std::minstd_rand& random = *static_cast<std::minstd_rand*>(context.userData());

  spawnAtEdge(context, random);
}
```

**Where you own the `Engine`** (native):

```cpp
std::minstd_rand random(42);

engine.addSystem(oly::app::Phase::Update, "spawnEnemies", spawnEnemies, &random);
```

Its sequence is the same in every standard library; the `std::*_distribution` classes are not, so use `random() % n` or your own mapping ([Deterministic randomness](../guide/environments/web/getting-started.md#deterministic-randomness)).

TypeScript: [Random spawn positions](typescript/gameplay-recipes.md#random-spawn-positions).

## Camera that follows after movement

The camera must read the player's position of this step, after every system that moves the player, whatever order the systems were registered in.

```
# game.oly
component Camera { x: f32, y: f32 }
tag Player
```

```cpp
OLY_SYSTEM(followPlayer) {
  auto player = context.query<const Position>(oly::ecs::With<Player>{}).trySingle();
  if (!player) {
    return;
  }

  const Position target = std::get<1>(*player);
  context.query<Camera>().each([&target](Camera& camera) {
    camera.x = target.x;
    camera.y = target.y;
  });
}
```

**Where you own the `Engine`** (native):

```cpp
engine.addSystem(oly::app::Phase::Update, "followPlayer", followPlayer, nullptr, oly::app::OrderBand::Last);
```

The `Last` band runs after the whole default band of `Update`, so movement registered later still runs first ([Order bands](../guide/concepts/engine-and-systems.md#order-bands)). The clients register in the default band: in TS, register the camera system after the movement systems.

## Share systems between games

Two games need the same components and systems. Move them into a plugin: the `.oly` entries go into the plugin's `.oly`, the systems into its `install`, and each game lists the plugin in `PLUGINS`.

```cmake
oly_add_plugin(NAME lifetime SCHEMA lifetime.oly DEPENDS ecs app)
```

```cmake
oly_add_game(<game>_game ENVIRONMENT web DIMENSION 2d SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/<game>.oly SOURCES ${nativeSources} DEPENDS ecs app capi PLUGINS lifetime)
```

The game's code changes in one place: the types come from the plugin's header, which the game's `schema.generated.hpp` includes. The full walk-through is in [Plugins](../guide/concepts/plugins.md#write-a-plugin). A TS game uses the plugin's components through its generated schema and writes no install code.

## Count entities

Count rows per table, not entity by entity:

```cpp
size_t countEnemies(oly::app::SystemContext& context) {
  size_t total = 0;

  context.query<const Health>(oly::ecs::With<Enemy>{}).eachBlock([&total](std::span<const Health> healths) {
    total += healths.size();
  });

  return total;
}
```

TypeScript: [Count entities](typescript/gameplay-recipes.md#count-entities).

## Find the nearest entity

```cpp
oly::ecs::Entity nearestEnemy(oly::app::SystemContext& context, float x, float y) {
  oly::ecs::Entity nearest = oly::ecs::NullEntity;
  float nearestDistance = std::numeric_limits<float>::infinity();

  context.query<const Position>(oly::ecs::With<Enemy>{}).each([&](oly::ecs::Entity entity, const Position& position) {
    const float deltaX = position.x - x;
    const float deltaY = position.y - y;
    const float distance = deltaX * deltaX + deltaY * deltaY;

    if (distance < nearestDistance) {
      nearestDistance = distance;
      nearest = entity;
    }
  });

  return nearest;
}
```

TypeScript: [Find the nearest entity](typescript/gameplay-recipes.md#find-the-nearest-entity).

## Two queries together (pairs)

Read the small set into a local value first, then iterate the large set. Here, enemies chase the player:

```cpp
OLY_SYSTEM(chasePlayer) {
  const Position target = std::get<1>(context.query<const Position>(oly::ecs::With<Player>{}).single());

  context.query<const Position, const Chaser, Velocity>(oly::ecs::With<Enemy>{}).each([target](const Position& position, const Chaser& chaser, Velocity& velocity) {
    const float deltaX = target.x - position.x;
    const float deltaY = target.y - position.y;
    const float length = std::max(std::sqrt(deltaX * deltaX + deltaY * deltaY), 1e-6f);

    velocity = Velocity{deltaX / length * chaser.speed, deltaY / length * chaser.speed};
  });
}
```

For "each against each" (bullets × enemies), take the inner query reference once, then nest the loops. Changes go through the context, so they are safe inside both loops.

```cpp
OLY_SYSTEM(hitEnemies) {
  auto& enemies = context.query<const Position, const Collider, Health>(oly::ecs::With<Enemy>{});

  context.query<const Position, const Collider>(oly::ecs::With<Bullet>{}).each([&](oly::ecs::Entity bullet, const Position& bulletPosition, const Collider& bulletCollider) {
    bool hit = false;

    enemies.each([&](oly::ecs::Entity enemy, const Position& enemyPosition, const Collider& enemyCollider, Health& health) {
      if (hit || !touches(bulletPosition, bulletCollider, enemyPosition, enemyCollider)) {
        return;
      }

      hit = true;
      health.value -= 1;
      context.destroy(bullet);

      if (health.value <= 0) {
        context.emit(EnemyDied{enemy, enemyPosition.x, enemyPosition.y});
      }
    });
  });
}
```

This is a brute-force O(bullets × enemies) check. It is fine for hundreds of entities. For thousands, add a spatial grid.

TypeScript: [Two queries together (pairs)](typescript/gameplay-recipes.md#two-queries-together-pairs).

## Several shapes with one function

Pass the filter as a parameter. One function draws coins, bullets, enemies and the player:

```cpp
template <class... Filters>
void drawCircles(oly::app::SystemContext& context, Drawer& drawer, Color color, Filters... filters) {
  context.query<const Position, const Collider>(filters...).each([&](const Position& position, const Collider& collider) {
    drawer.circle(position.x, position.y, collider.radius, color);
  });
}

drawCircles(context, drawer, red, oly::ecs::With<Enemy>{});
drawCircles(context, drawer, blue, oly::ecs::With<Player>{}, oly::ecs::Without<Invulnerable>{});
```

TypeScript: [Several shapes with one function](typescript/gameplay-recipes.md#several-shapes-with-one-function).
