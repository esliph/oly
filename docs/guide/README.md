# Oly user guide

This guide shows how to **make games with Oly**: what you write, where it goes and what the engine does with it. The concepts are taught in C++, the language of the engine. Everything that depends on where the game runs, or on the language of a client, is grouped by environment and client.

If you want to know *why* the engine is built this way, read [the architecture docs](../architecture/README.md). This guide only covers *how to use it*.

## What you can build

Oly is a 2D engine in C++. It runs in a window on Linux and Windows, and in the browser (and Node) as WebAssembly with a TypeScript client. With it you can:

- keep thousands of entities in an archetype ECS and iterate over them without copies;
- write gameplay in **C++**, and in **TypeScript** on the web, or mix both in the same phase;
- run a **fixed-step loop** with pause, hidden-window handling and render interpolation;
- send **events** between systems, in batches;
- map the keyboard and mouse to **actions** (button, 2D axis, pointer) and rebind them at run time;
- **schedule** work on the game clock (`after`, `every`) that pauses with the game;
- organize the game in **states** (menu → playing → game over) that clean up after themselves;
- **test the game** with a fake clock and fake input, in Node or in a native test.

Oly does **not** draw, play audio or do physics. You draw with whatever you want (Canvas 2D, SDL, a test recorder) inside a render system. See [Limits](troubleshooting.md#what-oly-does-not-do).

## The map

- The **core** is the engine in C++: the ECS, the `Engine`, input, schedule and states. It knows nothing about where it runs.
- An **environment** is a place where the core runs, with a **host** that drives the loop and forwards input.
- A **client** is a language that talks to the core through the C API `oly*`. C++ is not a client: it is the language of the core.

| Environment | Host | Clients | Start here |
|---|---|---|---|
| `web` | the web host: WASM, `runInBrowser`, `attachBrowserInput` | [TypeScript](../reference/typescript/README.md) | [Your first web game](../tutorials/first-web-game.md) |
| `native` | the native host: a window, SDL, `oly::native_host::run` | none: the game is C++ | [Your first desktop game](../tutorials/first-desktop-game.md) |

## Choose your path

Pages are not numbered. Each path lists them in the order to read.

**TypeScript on the web**

1. [Your first web game](../tutorials/first-web-game.md)
2. Each concept, followed by its TS page: [Schema](../explanation/concepts/schema.md) and [in TS](../reference/typescript/schema.md), [World and entities](../explanation/concepts/world-and-entities.md) and [in TS](../reference/typescript/world-and-entities.md), [Queries](../explanation/concepts/queries.md) and [in TS](../reference/typescript/queries.md), [Engine, loop and systems](../explanation/concepts/engine-and-systems.md) and [in TS](../reference/typescript/engine-and-systems.md), [Events](../explanation/concepts/events.md) and [in TS](../reference/typescript/events.md), [Input](../explanation/concepts/input.md) and [in TS](../reference/typescript/input.md), [Schedule](../explanation/concepts/schedule.md) and [in TS](../reference/typescript/schedule.md), [States](../explanation/concepts/states.md) and [in TS](../reference/typescript/states.md)
3. [Testing a web game](../how-to/test-a-web-game.md)
4. [The WASM↔JS boundary](../explanation/concepts/web-boundary.md)

**C++ on the desktop**

1. [Your first desktop game](../tutorials/first-desktop-game.md)
2. The [concepts](#concepts), in order
3. [Test a desktop game](../how-to/test-a-desktop-game.md)

**C++ inside a web game**

1. [Your first web game](../tutorials/first-web-game.md), then [Web boundary](../explanation/concepts/web-boundary.md)
2. The [concepts](#concepts), in order
3. [Systems in C++, registered by name](../reference/cpp/engine-and-systems.md#c-systems-handlers-and-hooks) and [C++ systems, handlers and hooks](../reference/typescript/engine-and-systems.md#c-systems-handlers-and-hooks)

## Concepts

Read in order the first time. Each page builds on the one before.

| Page | You learn to |
|---|---|
| [Schema](../explanation/concepts/schema.md) | declare components, tags, events, actions and states |
| [World and entities](../explanation/concepts/world-and-entities.md) | create, change, read and destroy entities |
| [Queries](../explanation/concepts/queries.md) | iterate over entities fast, filter them, find "the only one" |
| [Engine, loop and systems](../explanation/concepts/engine-and-systems.md) | run the game loop, register systems, pause, render |
| [Events](../explanation/concepts/events.md) | let systems talk without knowing each other |
| [Input](../explanation/concepts/input.md) | bind keys and mouse to actions and read them |
| [Schedule](../explanation/concepts/schedule.md) | run code later or every N seconds on the game clock |
| [States](../explanation/concepts/states.md) | build menu, playing and game over screens that reset cleanly |
| [Performance](../explanation/concepts/performance.md) | keep the hot path fast |
| [Plugins](../explanation/concepts/plugins.md) | turn reusable engine functionality on, and write your own |
| [Transforms](../explanation/concepts/transforms.md) | place entities with `Position`, nest them with `Parent` and read `WorldTransform` |

## References

| Page | Use it when |
|---|---|
| [The `oly` command](cli.md) | you want to install `oly`, make a game, build, test, run or upgrade it, or build with an engine checkout |
| [Recipes](../how-to/gameplay-recipes.md) | you want a ready pattern: projectile, cooldown, spawner, pickup, health, timer, nearest target, key rebinding and more |
| [Cheat sheet](cheat-sheet.md) | you remember the idea but not the exact name or signature |
| [Troubleshooting](troubleshooting.md) | you need to understand an error or fix a common mistake |
| [Glossary](../reference/glossary.md) | you need the same operation in C++, C API and TS |

## The big picture in one example

A complete game is three things: a **schema** (the data), **systems** (the behaviour) and a **host** that drives the loop.

```
# hello.oly — the data, shared by every language
component Velocity { x: f32, y: f32 }
tag Player
action move: axis2d
```

```cpp
// the behaviour: a body written against the context, valid in every environment
OLY_SYSTEM(steer) {
  const oly::input::Vector2 direction = context.actions().axis2d(actions::move);

  context.query<Velocity>(oly::ecs::With<Player>{}).each([direction](Velocity& velocity) {
    velocity = Velocity{direction.x * 200.0f, direction.y * 200.0f};
  });
}

OLY_SYSTEM(integrate) {
  const float deltaTime = context.fixedDeltaTime();

  context.query<Position, const Velocity>().each([deltaTime](Position& position, const Velocity& velocity) {
    position.x += velocity.x * deltaTime;
    position.y += velocity.y * deltaTime;
  });
}
```

**Where you own the `Engine`** (native), the registration is a fragment of `main`:

```cpp
oly::ecs::World world;
oly::app::Engine engine(world);
oly::app::setUpGame(engine);

engine.actions().bind(actions::move, oly::input::Axis2dBinding{oly::input::Key::W, oly::input::Key::S, oly::input::Key::A, oly::input::Key::D});
world.spawn(Player{}, Position{100.0f, 100.0f}, Velocity{});

engine.addSystem(oly::app::Phase::UpdateBefore, "steer", steer);
engine.addSystem(oly::app::Phase::Update, "integrate", integrate);

oly::native_host::Window window({"Hello", 640, 360});
return oly::native_host::run(engine, window);
```

**On the web**, the TS client registers the same functions by name and drives the loop from the browser: see the `game.ts` and `main.ts` of [The game template](../reference/game-template.md#web).

## Conventions used in this guide

- `cpp` blocks use the `oly::ecs`, `oly::app` and `oly::input` namespaces. `ts` blocks appear only in the client pages (`environments/web/typescript/`) and in the pages of the web environment.
- Commands are `oly` commands, run from Git Bash on Windows ([The `oly` command](cli.md)). Inside the engine repository they are `./spur` tasks, which run inside Docker ([commands](../development/commands.md)).
- "Contract error" means the engine refuses the call: C++ stops on `OLY_ASSERT` in debug builds; TS throws `ContractError`.
