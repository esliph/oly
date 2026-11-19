# Oly documentation

## Tutorials

| Page | What it answers |
|---|---|

## How-to guides

| Page | What it answers |
|---|---|
| [Gameplay recipes](how-to/gameplay-recipes.md) | How do I write a projectile, a cooldown, a spawner, a pickup, a key rebinding, a game flow or a query pattern in C++? |
| [Gameplay recipes in TypeScript](how-to/typescript/gameplay-recipes.md) | How do I write the same patterns in TypeScript? |
| [Test a web game](how-to/test-a-web-game.md) | How do I run the real game in Node, press keys and read the world? |
| [Debug the world](how-to/debug-the-world.md) | How do I check that a `World` is consistent and look at its shape? |
| [Drive the loop yourself](how-to/drive-the-loop-yourself.md) | How do I call `frame` from a test or a tool, one step at a time, and press buttons myself? |
| [User guide](guide/README.md) | Where are the other guides for the game user, until they move here? |

## Reference

| Page | What it answers |
|---|---|
| [TypeScript client](reference/typescript/README.md) | What does the TypeScript API offer, and where is each part? |
| [Schema in C++](reference/cpp/schema.md) | Which generated files and functions does the build write for a schema, and what do they declare? |
| [Queries in C++](reference/cpp/queries.md) | Which calls build a query, visit its rows and read its only row? |
| [Engine, loop and systems in C++](reference/cpp/engine-and-systems.md) | Which calls create the engine, register systems and states, emit from the host and pause or hide the game? |
| [Events in C++](reference/cpp/events.md) | Which calls declare an event, emit it, handle it and restrict a handler to states? |
| [Input in C++](reference/cpp/input.md) | Which calls bind buttons to actions, read buttons, the axis and the pointer, and list the declared actions? |
| [Schedule in C++](reference/cpp/schedule.md) | Which calls run a task after a delay or every interval, cancel it, and carry a payload into it? |
| [States in C++](reference/cpp/states.md) | Which calls declare states, request a change, restrict systems to states and run hooks on entry and exit? |
| [World and entities in C++](reference/cpp/world-and-entities.md) | Which calls create, read, change and destroy entities, record commands and observe changes? |
| [Glossary](reference/glossary.md) | What does each name mean in C++, the C API and TypeScript? |
| [Known limitations](reference/limits.md) | What does the engine not do? |
| [Repository map](reference/contributor/repository.md) | Where is each part of the repository? |
| [Code style](reference/contributor/code-style.md) | Which rules does the code follow? |
| [Benchmark results](reference/benchmarks/README.md) | How fast is the engine, and which targets does it meet? |
| [Architecture index](architecture/README.md) | Where are the other contributor pages, until they move here? |

## Explanation

| Page | What it answers |
|---|---|
| [`app`: the `Engine`](explanation/architecture/app.md) | How does the `Engine` run the loop, the systems and the events? |
| [`ecs`: the archetype ECS](explanation/architecture/ecs.md) | How does the ECS store and query data? |
| [`input`: actions](explanation/architecture/input.md) | How does the action map decide what is pressed, held and released? |
| [Schema, `oly-schema` and the WASM module](explanation/architecture/schema-and-codegen.md) | How does a `.oly` file become code in each language? |
| [Main architectural decisions](explanation/architecture/decisions.md) | Why is the engine built this way, and what does it cost? |
| [Concepts](explanation/concepts/README.md) | In which order do I read the rules a game relies on, and what does each page explain? |
| [Architecture index](architecture/README.md) | Where are the other contributor pages, until they move here? |

## Working on the engine

| Page | What it answers |
|---|---|

## Conventions

| Page | What it answers |
|---|---|
