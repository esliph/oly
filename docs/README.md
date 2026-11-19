# Oly documentation

## Tutorials

| Page | What it answers |
|---|---|
| [Your first web game](tutorials/first-web-game.md) | How do I make a square that moves in the browser, with C++ movement and TypeScript drawing? |
| [Your first desktop game](tutorials/first-desktop-game.md) | How do I make a square that moves in a window, in C++ only? |

### Reading paths

Pages are not numbered. Each path lists every page of its environment in the order to read. A concept page is followed by its API page.

**TypeScript on the web**

1. [Your first web game](tutorials/first-web-game.md)
2. [Environments and hosts](explanation/concepts/environments-and-hosts.md)
3. [Schema](explanation/concepts/schema.md) and [in TypeScript](reference/typescript/schema.md)
4. [World and entities](explanation/concepts/world-and-entities.md) and [in TypeScript](reference/typescript/world-and-entities.md)
5. [Queries](explanation/concepts/queries.md) and [in TypeScript](reference/typescript/queries.md)
6. [Engine, loop and systems](explanation/concepts/engine-and-systems.md) and [in TypeScript](reference/typescript/engine-and-systems.md)
7. [Events](explanation/concepts/events.md) and [in TypeScript](reference/typescript/events.md)
8. [Input](explanation/concepts/input.md) and [in TypeScript](reference/typescript/input.md)
9. [Schedule](explanation/concepts/schedule.md) and [in TypeScript](reference/typescript/schedule.md)
10. [States](explanation/concepts/states.md) and [in TypeScript](reference/typescript/states.md)
11. [Performance](explanation/concepts/performance.md) and [Keep the hot path fast](how-to/keep-the-hot-path-fast.md)
12. [Plugins](explanation/concepts/plugins.md) and [Use a plugin](how-to/use-a-plugin.md)
13. [Transforms](explanation/concepts/transforms.md)
14. [Web boundary](explanation/concepts/web-boundary.md)
15. [Test a web game](how-to/test-a-web-game.md) and [Gameplay recipes in TypeScript](how-to/typescript/gameplay-recipes.md)

**C++ on the desktop**

1. [Your first desktop game](tutorials/first-desktop-game.md)
2. [Environments and hosts](explanation/concepts/environments-and-hosts.md) and [Native host](reference/native-host.md)
3. [Schema](explanation/concepts/schema.md) and [in C++](reference/cpp/schema.md)
4. [World and entities](explanation/concepts/world-and-entities.md) and [in C++](reference/cpp/world-and-entities.md)
5. [Queries](explanation/concepts/queries.md) and [in C++](reference/cpp/queries.md)
6. [Engine, loop and systems](explanation/concepts/engine-and-systems.md) and [in C++](reference/cpp/engine-and-systems.md)
7. [Events](explanation/concepts/events.md) and [in C++](reference/cpp/events.md)
8. [Input](explanation/concepts/input.md) and [in C++](reference/cpp/input.md)
9. [Schedule](explanation/concepts/schedule.md) and [in C++](reference/cpp/schedule.md)
10. [States](explanation/concepts/states.md) and [in C++](reference/cpp/states.md)
11. [Performance](explanation/concepts/performance.md) and [Keep the hot path fast](how-to/keep-the-hot-path-fast.md)
12. [Plugins](explanation/concepts/plugins.md) and [in C++](reference/cpp/plugins.md)
13. [Transforms](explanation/concepts/transforms.md) and [in C++](reference/cpp/transforms.md)
14. [Test a desktop game](how-to/test-a-desktop-game.md) and [Gameplay recipes](how-to/gameplay-recipes.md)

**C++ inside a web game**

1. [Your first web game](tutorials/first-web-game.md)
2. [Environments and hosts](explanation/concepts/environments-and-hosts.md)
3. [Schema](explanation/concepts/schema.md) and [in C++](reference/cpp/schema.md)
4. [World and entities](explanation/concepts/world-and-entities.md) and [in C++](reference/cpp/world-and-entities.md)
5. [Queries](explanation/concepts/queries.md) and [in C++](reference/cpp/queries.md)
6. [Engine, loop and systems](explanation/concepts/engine-and-systems.md) and [in C++](reference/cpp/engine-and-systems.md), then [C++ systems, handlers and hooks in TypeScript](reference/typescript/engine-and-systems.md#c-systems-handlers-and-hooks)
7. [Events](explanation/concepts/events.md) and [in C++](reference/cpp/events.md)
8. [Input](explanation/concepts/input.md) and [in C++](reference/cpp/input.md)
9. [Schedule](explanation/concepts/schedule.md) and [in C++](reference/cpp/schedule.md)
10. [States](explanation/concepts/states.md) and [in C++](reference/cpp/states.md)
11. [Performance](explanation/concepts/performance.md) and [Keep the hot path fast](how-to/keep-the-hot-path-fast.md)
12. [Plugins](explanation/concepts/plugins.md) and [in C++](reference/cpp/plugins.md)
13. [Transforms](explanation/concepts/transforms.md) and [in C++](reference/cpp/transforms.md)
14. [Web boundary](explanation/concepts/web-boundary.md) and [Export your own C functions](how-to/export-your-own-c-functions.md)
15. [Test a web game](how-to/test-a-web-game.md)

## How-to guides

| Page | What it answers |
|---|---|
| [Gameplay recipes](how-to/gameplay-recipes.md) | How do I write a projectile, a cooldown, a spawner, a pickup, a key rebinding, a game flow or a query pattern in C++? |
| [Gameplay recipes in TypeScript](how-to/typescript/gameplay-recipes.md) | How do I write the same patterns in TypeScript? |
| [Test a web game](how-to/test-a-web-game.md) | How do I run the real game in Node, press keys and read the world? |
| [Test a desktop game](how-to/test-a-desktop-game.md) | How do I run a desktop game with no display and check what its gameplay did? |
| [Export your own C functions](how-to/export-your-own-c-functions.md) | How do I export an extra `extern "C"` function from the WASM module of a web game? |
| [Debug the world](how-to/debug-the-world.md) | How do I check that a `World` is consistent and look at its shape? |
| [Drive the loop yourself](how-to/drive-the-loop-yourself.md) | How do I call `frame` from a test or a tool, one step at a time, and press buttons myself? |
| [Keep the hot path fast](how-to/keep-the-hot-path-fast.md) | How do I keep a system that runs on many entities every step cheap? |
| [Use a plugin](how-to/use-a-plugin.md) | How do I turn on a plugin that already exists in my game? |
| [Write a plugin](how-to/write-a-plugin.md) | How do I make a plugin with its own components, events and systems? |
| [Fix common problems](how-to/fix-common-problems.md) | Why does my game run but not do what I expect, and what do I rebuild after a schema change? |
| [Upgrade the engine](how-to/upgrade-the-engine.md) | How do I move a game to another engine release? |
| [Set up the CI of a game](how-to/set-up-game-ci.md) | How do I run the tests of a game on every push? |
| [Build with an engine checkout](how-to/build-with-an-engine-checkout.md) | How do I change the engine and a game together? |

## Reference

| Page | What it answers |
|---|---|
| [The `oly` command](reference/cli.md) | How do I install `oly`, and which commands, options, Docker modes and variables does it have? |
| [Errors](reference/errors.md) | What does this message mean, in C++, in TypeScript or from `oly`, and how do I fix it? |
| [TypeScript client](reference/typescript/README.md) | What does the TypeScript API offer, and where is each part? |
| [The web host in TypeScript](reference/typescript/web-host.md) | Which calls run the loop in a browser tab, deliver keyboard and mouse input, and load the WASM module? |
| [Schema in C++](reference/cpp/schema.md) | Which generated files and functions does the build write for a schema, and what do they declare? |
| [Queries in C++](reference/cpp/queries.md) | Which calls build a query, visit its rows and read its only row? |
| [Engine, loop and systems in C++](reference/cpp/engine-and-systems.md) | Which calls create the engine, register systems and states, emit from the host and pause or hide the game? |
| [Events in C++](reference/cpp/events.md) | Which calls declare an event, emit it, handle it and restrict a handler to states? |
| [Input in C++](reference/cpp/input.md) | Which calls bind buttons to actions, read buttons, the axis and the pointer, and list the declared actions? |
| [Schedule in C++](reference/cpp/schedule.md) | Which calls run a task after a delay or every interval, cancel it, and carry a payload into it? |
| [States in C++](reference/cpp/states.md) | Which calls declare states, request a change, restrict systems to states and run hooks on entry and exit? |
| [Plugins in C++](reference/cpp/plugins.md) | Which calls write a plugin, install it and reach an installed plugin? |
| [Transforms in C++](reference/cpp/transforms.md) | Which CMake argument turns the transform components on, and which fields does each component have? |
| [World and entities in C++](reference/cpp/world-and-entities.md) | Which calls create, read, change and destroy entities, record commands and observe changes? |
| [The game template](reference/game-template.md) | Which files does `oly new` write for a web game and for a desktop game, and where does each one go? |
| [CMake](reference/cmake.md) | Which arguments do `oly_add_game` and `oly_add_plugin` take, and which build options does a game set? |
| [C API](reference/c-api.md) | Which `oly*` functions, types and constants can a client in another language use, and what does each do? |
| [Native host](reference/native-host.md) | Which calls open the window and run the loop, and what does the host do with each window event? |
| [Glossary](reference/glossary.md) | What does each name mean in C++, the C API and TypeScript? |
| [Known limitations](reference/limits.md) | What does the engine not do? |
| [Benchmark results](reference/benchmarks/README.md) | How fast is the engine, and which targets does it meet? |

## Explanation

| Page | What it answers |
|---|---|
| [Architecture overview](explanation/architecture/README.md) | What are the pieces of the engine, and which rules shape its design? |
| [Modules and boundaries](explanation/architecture/modules-and-boundaries.md) | Which module depends on which, and how are the boundaries enforced? |
| [`app`: the `Engine`](explanation/architecture/app.md) | How does the `Engine` run the loop, the systems and the events? |
| [`ecs`: the archetype ECS](explanation/architecture/ecs.md) | How does the ECS store and query data? |
| [`input`: actions](explanation/architecture/input.md) | How does the action map decide what is pressed, held and released? |
| [Schema, `oly-schema` and the WASM module](explanation/architecture/schema-and-codegen.md) | How does a `.oly` file become code in each language? |
| [Main architectural decisions](explanation/architecture/decisions.md) | Why is the engine built this way, and what does it cost? |
| [Environments and hosts](explanation/concepts/environments-and-hosts.md) | What are the core, an environment, a host and a client, and how is a game assembled? |
| [Concepts](explanation/concepts/README.md) | In which order do I read the rules a game relies on, and what does each page explain? |
| [Plugins in the engine](explanation/architecture/plugins.md) | How does a plugin install, own its parts and merge its schema? |
| [`capi`: the C API](explanation/architecture/capi.md) | How is the C API designed, and how does the per-engine script invoker work? |
| [TypeScript packages](explanation/architecture/ts-packages.md) | How are `@oly/core`, `@oly/web-host` and `@oly/testing` built? |
| [Publishing](explanation/architecture/publishing.md) | How does a game outside this repository get the engine, and why is a release built this way? |
| [Build, tests and tools](explanation/architecture/build-and-tests.md) | How are the build, the tests and the tools organized? |

## Working on the engine

| Page | What it answers |
|---|---|
| [Set up the toolchain](how-to/contributor/set-up-the-toolchain.md) | How do I build the engine and run its tests? |
| [Run the benchmarks](how-to/contributor/run-benchmarks.md) | How do I measure the engine and record the result? |
| [Cut a release](how-to/contributor/cut-a-release.md) | How do I publish a version of the engine? |
| [Commands](reference/contributor/commands.md) | Which `./spur` tasks, services and presets does the repository have? |
| [Boundary rules](reference/contributor/boundary-rules.md) | Which includes and imports does the boundary lint reject? |
| [Repository map](reference/contributor/repository.md) | Where is each part of the repository? |
| [Code style](reference/contributor/code-style.md) | Which rules does the code follow? |
| [Specs and plans](superpowers/README.md) | Where is the record of how the engine was designed and built? |

## Conventions

| Page | What it answers |
|---|---|

- `cpp` blocks use the `oly::ecs`, `oly::app` and `oly::input` namespaces. `ts` blocks appear only in the TypeScript pages and in the pages of the web environment.
- Commands are `oly` commands, run from Git Bash on Windows ([The `oly` command](reference/cli.md)). Inside the engine repository they are `./spur` tasks, which run inside Docker ([Commands](reference/contributor/commands.md)).
- "Contract error" means the engine refuses the call: C++ stops on `OLY_ASSERT` in debug builds, and TypeScript throws `ContractError` ([Errors](reference/errors.md)).
