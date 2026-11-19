# Docs style

[Reference](../../README.md#reference) · Related: [Code style](code-style.md)

How to write and maintain everything in `docs/`. Read it before you add, move or change a page.

## The four types

Every page has one type, and the type decides what the page may hold. The type is the folder.

| Type | Folder | The reader wants to | The page holds | The page does not hold |
|---|---|---|---|---|
| Tutorial | `tutorials/` | learn by doing | one path from nothing to a running result, in numbered steps | choices, options, the why |
| How-to | `how-to/` | solve a task they already know | the goal, what it assumes, the steps | teaching, every option |
| Reference | `reference/` | look up a fact | signatures, arguments, messages, tables; each fact once | explanation, steps |
| Explanation | `explanation/` | understand why | the contract and the reasons for the design, in prose | the full API, step lists |

`docs/superpowers/` is the record of how the engine was designed and built. It is outside the four types, and its specs and plans are not rewritten.

The audience is the second axis. A game developer reads `tutorials/`, `how-to/` (outside `contributor/`), `reference/` (outside `contributor/`) and `explanation/concepts/`. A contributor reads the `contributor/` folders and `explanation/architecture/`.

## The tree

```
docs/
  README.md                  the index, the reading paths and the conventions
  tutorials/                 first-web-game.md  first-desktop-game.md
  how-to/                    one task per page
    typescript/              the TypeScript form of a how-to
    contributor/             tasks of someone who works on the engine
  reference/
    cpp/                     the C++ API by theme
    typescript/              the TypeScript API by theme, the mirror of cpp/
    contributor/             commands, code style, docs style, repository map, boundary rules
    benchmarks/              results and targets, with the data files
    cli.md  cmake.md  c-api.md  native-host.md  game-template.md  errors.md  glossary.md  limits.md
  explanation/
    concepts/                the contract a game relies on
    architecture/            how the engine is built
  superpowers/               specs and plans
```

- Pages are not numbered. File names are lowercase `kebab-case`.
- A fact has one home. Every other page links to it and does not copy it. The rules a game relies on (deferred or direct, event rounds, scope, validity) are written only in `explanation/concepts/`.
- A rule appears in a language page only when it exists only in that language (TypeScript: `raw()` arrays, views invalidated by memory growth, `ContractError`).
- Links are relative. Anchors follow GitHub's rule: lowercase, punctuation removed, spaces become `-` (`## Systems in C++` is `#systems-in-c`).
- The link checker (`tools/check_links.py`) runs with the lint tests. It reports a broken file, a broken anchor and a link whose case differs from the file.

## Page contracts

### `docs/README.md`

The index has these sections, in this order. Other pages link to them by anchor, so a heading does not change.

| Section | Anchor |
|---|---|
| `## Tutorials` | `#tutorials` |
| `## How-to guides` | `#how-to-guides` |
| `## Reference` | `#reference` |
| `## Explanation` | `#explanation` |
| `## Working on the engine` | `#working-on-the-engine` |
| `## Conventions` | `#conventions` |

- Each section is a table `| Page | What it answers |`, and every page of the docs has a row. A task that creates, moves or deletes a page updates its row.
- The reading paths are `### Reading paths`, a subsection of `## Tutorials`, after the tutorial table. Each path lists every page of one environment, in order, and a concept page is followed by its API page.
- The convention bullets (the language of each code block, the commands, the contract error) go in `## Conventions`, after its table.

### Explanation concept page

`explanation/concepts/<theme>.md` is prose. A short C++ excerpt appears only where the contract has to be seen, followed by one line `TypeScript: [<section>](../../reference/typescript/<theme>.md#<anchor>)`.

The pages follow one chain: environments-and-hosts, schema, world-and-entities, queries, engine-and-systems, events, input, schedule, states, performance, plugins, transforms, web-boundary. The first page has no previous link and the last has no next link.

~~~markdown
# World and entities

[← Schema](schema.md) · [Concepts](README.md) · Next: [Queries →](queries.md)

...

## API

- C++: [World and entities in C++](../../reference/cpp/world-and-entities.md)
- TypeScript: [World and entities in TypeScript](../../reference/typescript/world-and-entities.md)

## Next

[Queries](queries.md): what the next page teaches, in one line.
~~~

A theme with no TypeScript form drops the TypeScript line. `performance` replaces `## API` with a link to `how-to/keep-the-hot-path-fast.md`.

### Explanation architecture page

`explanation/architecture/<topic>.md` describes the engine as it is today, not its history. The overview, `README.md`, links every page of the folder. A page has no previous and next links.

### Reference page per language

`reference/cpp/<theme>.md` and `reference/typescript/<theme>.md` have the same file names and the same `##` section titles. A section with no form in one language is left out, not stubbed. There are no previous or next links.

~~~markdown
# World and entities in C++

[Reference](../../README.md#reference) · Concept: [World and entities](../../explanation/concepts/world-and-entities.md) · TypeScript: [World and entities in TypeScript](../typescript/world-and-entities.md)
~~~

The line of a TypeScript page is `[TypeScript client](README.md) · Concept: [...](../../explanation/concepts/<theme>.md) · C++: [... in C++](../cpp/<theme>.md)`. The title is `# <Theme> in C++` or `# <Theme> in TypeScript`.

Each entry is `### \`name\``, then the signature in a code block, then one paragraph: what it does, the preconditions, the error and its message. An example follows only when correct use is not obvious. A contract rule is not repeated: one line links to its section in the concept page.

### Other reference pages

A page that is not per language (`cli.md`, `cmake.md`, `c-api.md`, `native-host.md`, `game-template.md`, `errors.md`, `glossary.md`, `limits.md`, `contributor/`) starts with `[Reference](<relative>/README.md#reference)` and, when it has one, the link to its concept or explanation page. `reference/typescript/web-host.md` uses the TypeScript line.

### How-to page

`# <task, as a verb phrase>`, then the line `[Docs](<relative>/README.md#how-to-guides)`. The first paragraph says the goal and what the page assumes. The steps follow. The page ends with `## See also`: links to the explanation and the reference.

### Tutorial

`# Your first <web|desktop> game`, then the line `[Docs](../README.md#tutorials)`. The sections are "What you will build", "Before you start", then numbered `## N. <step>` sections, each ending with what the reader sees. The page ends with `## Next steps`. Every code block is a file of the template, or a marked excerpt of it.

## The `cpp` and `ts` mirror

- `reference/cpp/` and `reference/typescript/` mirror each other: same file names, same section titles.
- A concept page links to the section of each language under `## API` and, where it shows C++, to the TypeScript line under the excerpt. Each reference page links back to its concept page.
- `cpp` blocks use the `oly::ecs`, `oly::app` and `oly::input` namespaces. `ts` blocks appear only in TypeScript pages and in pages of the web environment.

## What to update

A change to the public surface updates the docs in the same change (same commit or same pull request). A change without its docs is incomplete.

| The change | Update |
|---|---|
| a new function, method, option or macro | the signature in `reference/cpp/<theme>.md` and `reference/typescript/<theme>.md`; the concept page only if the contract changes; a [recipe](../../how-to/gameplay-recipes.md) if it enables a new pattern |
| a renamed name or a changed signature | every example that uses it: search the whole `docs/` |
| a removed name | every mention; replace the examples that depended on it |
| a changed behaviour (order, timing, scope) | the concept page that describes it and the recipes that relied on it |
| a new or changed error | the table in [Errors](../errors.md) |
| a C API change | [C API](../c-api.md) and, for the design, [The C API](../../explanation/architecture/capi.md) |
| a change to the `oly` commands | [The `oly` command](../cli.md) and the messages in [Errors](../errors.md) |
| a change to the template or `toolchain.env` | [The game template](../game-template.md), [The `oly` command](../cli.md), the messages in [Errors](../errors.md), the two tutorials ([web](../../tutorials/first-web-game.md), [desktop](../../tutorials/first-desktop-game.md)) and the [README](../../../README.md) |
| a change to `oly_add_game` or `oly_add_plugin` | [CMake](../cmake.md) |
| a change to the native host | [Native host](../native-host.md) |
| a change to the web host | [The web host in TypeScript](../typescript/web-host.md) |
| a change to how a part of the engine works | its page in `explanation/architecture/` |
| a change to a `./spur` task, a preset or an engine option | [Commands](commands.md) |
| a new page | its row in [docs/README.md](../../README.md) and the navigation lines of its neighbours |

The `oly.lint.readme_example` test checks the template blocks of the README. Nothing checks the tutorial blocks: copy them by hand from the template file.

Before you finish, search `docs/` for the old name and run the link checker.

## Public surface

The public surface is what a game uses. The names that `reference/` documents are the public surface. A name that a game does not use, and that no reference page documents, is excluded.

| Surface | Defined in | Documented in |
|---|---|---|
| `@oly/core` exports | `engine/environments/web/clients/typescript/core/api/public-exports.txt` | `reference/typescript/` |
| `@oly/web-host` exports | `engine/environments/web/host/js/src/index.ts` | `reference/typescript/web-host.md` |
| C++ public headers | `engine/runtime/<module>/include/oly/<module>/*.hpp`, except `detail/` | `reference/cpp/` |
| the native host | `engine/environments/native/host/include/oly/native_host/*.hpp`, except `detail/` | `reference/native-host.md` |
| registration macros | `OLY_SYSTEM`, `OLY_HANDLER`, `OLY_STATE_HOOK` | `reference/cpp/engine-and-systems.md`, `events.md` and `states.md` |
| the C API | `engine/runtime/capi/include/oly/capi/c_api.h` and `engine_api.h` | `reference/c-api.md` |
| the schema language and the generated code | `engine/schema/format/`, `engine/schema/codegen/`, `engine/environments/web/clients/typescript/codegen/`, `@oly/core/generated` | `engine/schema/format/README.md` (the language), `reference/cpp/schema.md`, `reference/typescript/schema.md` |
| the `oly` commands | `tools/oly/`, `templates/game/`, `toolchain.env` | `reference/cli.md`, `reference/game-template.md` |
| the build of a game | `cmake/OlyGame.cmake`, `cmake/OlyPlugin.cmake` | `reference/cmake.md` |
| behaviour a game relies on: loop order, deferred or direct changes, event rounds, scope rules, validity of views, error messages | the code | `explanation/concepts/`, `reference/errors.md` |

The core module (`oly/core/`) has no reference page. [Oly architecture](../../explanation/architecture/README.md) names it in one line.

### Excluded: internal, pending a code decision

These groups are in a public header or export, outside `detail/`, and no reference page documents them. They are internal, pending a code decision: move them to `detail/` or document them. A reviewer who meets one of these names without a reference entry has found an exclusion, not an omission.

A name of a group that a reference page already documents is documented, not excluded. Examples are `TaskPayload`, `TaskFunction`, `SystemFunction`, `StateHookFunction`, `EventTypeId`, `ArchetypeStats`, `ReadField`, `WriteField`, `FrameTarget`, `BrowserLoop`, `InputTarget`, `InputCanvas` and `BrowserInput`.

| Surface | Group and names |
|---|---|
| C++ | The `core` module: `alignUp`, `alignedAllocate`, `alignedFree`, `AssertHandler`, `setAssertHandler`, `SmallMap`, `typeName<T>` |
| C++ | Named registrations: `NamedHandler`, `findNamedHandler`, `NamedHandlerRegistration`, `NamedSystemRegistration`, `findNamedSystem`, `NamedStateHookRegistration`, `findNamedStateHook` |
| C++ | Event identifiers: `EventKey`, `InvalidEventType`, `eventKey<T>`, `Engine::eventTypeOf`, `eventTypeCount`, `onById`, `emitById`, `context.emitById` |
| C++ | Plugin and phase internals: `PluginKey`, `pluginKey<P>`, `PhaseCount`, `OrderBandCount`, `isGameplayPhase` |
| C++ | Task internals: `PayloadKey`, `payloadKey<P>`, `NoPayloadKey`, `InvalidTaskSlot`, `TaskHandle.slot` and `.generation` |
| C++ | The query `ById` variants: `queryById`; `QueryByIdSpec`, `BlockById`, `QueryIteration`, `QueryById` |
| C++ | Query and type diagnostics: `Query::matchedCount`, `Query::writes`; `TypeInfo`, `typeInfo()`, `registeredTypeCount`; `signatureLookupCount`, `archetypeCount()` |
| C++ | Action map internals: `ActionState`, `ActionPressed`, `ActionHeld`, `ActionReleased`, `ActionValid`, `ActionMap::declare*`, `capture`, `snapshot`, `setDeclarationsLocked`, `PhysicalButton(uint32_t)`, `.usage` |
| C++ | Schema registry: `capi::SchemaEvent`, `SchemaAction`, `SchemaState`, `capi::schemaHash`, `schemaComponentIds`, `schemaEvents`, `schemaActions`, `schemaStates` |
| C++ | Plugin classes and checks: `essentials2d::Plugin`, `essentials3d::Plugin`, `checkHierarchy` |
| TypeScript | Layout and view types: `ComponentLayout`, `FieldLayout`, `ScalarType`, `RawField`, `ReadView`, `WriteView`, `Fields` |
| TypeScript | `@oly/web-host` test hooks: `BrowserEnvironment`, `InputEnvironment` and the injection parameters (`runInBrowser(…, environment)`, `attachBrowserInput(…, environment)`, `moduleFactoryFromUrl(…, importModule)`) |
| `oly` | `oly version-at-least`, `OLY_TOOLS_DIR`, `OLY_GAME_DIR`, `OLY_IN_DOCKER`, `OLY_ENGINE_SOURCE_DIR` |
| CMake | Engine-level: `oly_typescript_schemas`, `oly_add_engine_test_game`, `OLY_WERROR`, `OLY_SANITIZE`, `OLY_BUILD_BENCH`, `OLY_VEC_REPORT`, `OLY_BUILD_GAMES`. [Commands](commands.md) documents the options; [CMake](../cmake.md) keeps only what a game sets |
| `toolchain.env` | `GCC_VERSION`, `CMAKE_VERSION`, `CMAKE_MINIMUM`, `NINJA_MINIMUM`, `MINGW_GCC_MINIMUM` |

## Audience and tone

- The reader of a tutorial, a how-to, a reference and a concept page is a game developer in any supported language. C++ is the language of the concepts. A language page speaks the language of its client.
- Start each page and each section with what the reader can do, then how.
- Explain the contract the reader depends on: what is deferred or direct, what stays valid, what throws. Leave the internal mechanisms to `explanation/architecture/`.

## Language and writing

- English, like the rest of the repository.
- Simple words, active voice, one idea per sentence, short paragraphs.
- Use the glossary names ([Glossary](../glossary.md)). Never an alias or an old name. A new term enters the glossary first.
- Prefer a table to a paragraph when you compare two or more options.
- No filler: no "simply", "just", "easily", no restating the heading.
- Copy every command, message and signature from the code. Open the header, `.ts`, `.h`, `.cmake`, `Spurfile` or template file before you write one.

## Game-agnostic

The docs do not reference any specific game. Games live in their own repositories, outside the engine.

- Write examples inline. Do not link to a game's file for code the reader needs.
- Use generic names: `Player`, `Enemy`, `Position`, `WorldWidth`, `<game>`. Not names that belong to one game.

## Examples

- **C++ first in a concept page**, the client's language in a client page.
- **The main example of a concept is the function body written against the context**, with `OLY_SYSTEM`, `OLY_HANDLER` and `OLY_STATE_HOOK`: it is valid in every environment. Under it, the registration is short and labelled:

  ````markdown
  **Where you own the `Engine`** (native):

  ```cpp
  engine.addSystem(oly::app::Phase::Update, "integrate", integrate);
  ```

  **On the web**, the client registers it by name: [TypeScript](../../reference/typescript/engine-and-systems.md#anchor).
  ````

  Setup-only operations (declaring, binding, pause from outside a system) are shown on `Engine` with the same "Where you own the `Engine`" label. Names are the generated ones: `Position`, `actions::move`, `states::playing`, in a file that includes `"schema.generated.hpp"`.
- **An `OLY_HANDLER` is registered natively with a captureless lambda**, because the macro's function takes the batch as a span and `on<T>` takes a function of the context alone. The first time a page shows it, one sentence says why:

  ```cpp
  engine.on<EnemyDied>("onEnemyDied", [](oly::app::HandlerContext& context) { onEnemyDied(context, context.events<EnemyDied>()); });
  ```

- **Real signatures only.** Check each C++ call against the headers and each TypeScript call against `@oly/core` or `@oly/web-host` before you write it. An example that does not exist in the API is worse than none.
- **Complete enough to copy**: imports when they are not obvious, the schema entries an example needs, no `...` inside the logic that matters.
- **Follow the [code style](code-style.md)**: full names, braces on every `if`/`for`, blank lines between steps.
- **Comments only for the why**, the same rule as in the code. A comment does not narrate the example step by step: that goes in the prose before the block.
- **Commands** are `oly` commands (`oly new`, `oly test`, `oly run`), as in [The `oly` command](../cli.md). Inside the engine repository they are `./spur` tasks (`./spur games-test`, `./spur wasm <command>`, `./spur dev <command>`), as in [Commands](commands.md).
- Show the wrong way only when the mistake is common, and always next to the right way.

## Recipes

- One problem per section, named by what the reader wants (`## Ability with a cooldown`), listed in the table at the top. The recipe is in C++ in `how-to/gameplay-recipes.md` and in TypeScript under the same heading in `how-to/typescript/gameplay-recipes.md`. A recipe that only makes sense in the browser lives only in the TypeScript page.
- Each recipe shows the schema entries it needs, then the code, then one or two lines on why it is built that way or when to choose another tool.
