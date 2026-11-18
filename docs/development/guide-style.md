# Guide style

How to write and maintain the user guide in `docs/guide/`. The guide teaches how to **use** the engine to make a game. The architecture docs (`docs/architecture/`) explain how the engine is **built** and why. Keep the two apart: the guide links to the architecture for the "why" and does not repeat it.

## When the guide must change

Every change that adds, changes or removes something in the **public API** updates the guide in the same change (same commit or same PR). A public API change without a guide update is incomplete.

The public API is:

| Surface | Where it is defined |
|---|---|
| `@oly/core` exports | `engine/environments/web/clients/typescript/core/api/public-exports.txt` |
| `@oly/web-host` exports | `engine/environments/web/host/js/src/index.ts` |
| C++ public headers | `engine/runtime/<module>/include/oly/<module>/*.hpp` and `engine/environments/native/host/include/oly/native_host/*.hpp`, except `detail/` |
| registration macros | `OLY_SYSTEM`, `OLY_HANDLER`, `OLY_STATE_HOOK` |
| the C API | `engine/runtime/capi/include/oly/capi/c_api.h` and `engine_api.h` |
| the schema language and the generated code | `engine/schema/format/`, `engine/schema/codegen/`, `engine/environments/web/clients/typescript/codegen/`, `@oly/core/generated` |
| how a game is built | the `oly` commands (`oly new`, `build`, `test`, `run`, `doctor`, `upgrade`, `clean`, `--docker`, local mode), `templates/game/` and `tools/oly/`, `toolchain.env`, `cmake/OlyGame.cmake` (`oly_add_game`, with `ENVIRONMENT`, `DIMENSION`, `PLUGINS` and `TYPESCRIPT`), `cmake/OlyPlugin.cmake` (`oly_add_plugin`), the `schema-tool`, `ts`, `games-test`, `native-games-test` and `windows-test` tasks of the `Spurfile` |
| behaviour a game relies on | loop order, deferred vs. direct changes, event rounds, scope rules, validity of views, error messages |

What to update:

| The change | Update |
|---|---|
| new function, method, option or macro | the concept page and the client page of its topic, the [cheat sheet](../guide/cheat-sheet.md) and, if it enables a new pattern, a [recipe](../guide/recipes.md) |
| renamed or changed signature | every example that uses it (search the whole `docs/guide/`) and the cheat sheet |
| removed | every mention; replace examples that depended on it |
| new or changed error | the table in [Troubleshooting](../guide/troubleshooting.md) |
| changed behaviour (order, timing, scope) | the concept page that describes it and any recipe that relied on it |
| a C API change | the cheat sheet and [capi.md](../architecture/capi.md) |
| a change to the `oly` commands, the template or `toolchain.env` | [The `oly` command](../guide/cli.md), the cheat sheet and the messages in [Troubleshooting](../guide/troubleshooting.md) |
| new page | the index tables in `docs/guide/README.md` and the navigation lines of the neighbouring pages in its folder |

Before finishing, search `docs/guide/` for the old name and check the links you touched.

## Audience and tone

- The reader is a game developer in any supported language. C++ is the language of the concepts; a client page speaks the language of its client.
- Start each page and each section with what the reader can do, then how.
- Explain the contract the reader depends on: what is deferred or direct, what stays valid, what throws. Leave internal mechanisms to the architecture docs.

## Language and writing

- English, like the rest of the repository.
- Simple words, active voice, one idea per sentence, short paragraphs.
- Use the glossary names ([glossary](../architecture/glossary.md)). Never an alias or an old name.
- Prefer a table to a paragraph when comparing two or more options.
- No filler: no "simply", "just", "easily", no restating the heading.

## Game-agnostic

The guide does not reference any specific game. Games live in their own repositories, outside the engine.

- Write examples inline. Do not link to a game's file for code the reader needs.
- Use generic names: `Player`, `Enemy`, `Position`, `WorldWidth`, `<game>`. Not names that belong to one game.

## Files and navigation

```
docs/guide/
  README.md                    the map: core, environments, clients, how to choose, a learning path per environment
  concepts/                    valid in every environment; examples in C++
    schema.md  world-and-entities.md  queries.md  engine-and-systems.md
    events.md  input.md  schedule.md  states.md  performance.md  plugins.md
  environments/
    web/
      getting-started.md  testing.md  boundary.md
      typescript/
        README.md  schema.md  world-and-entities.md  queries.md  engine-and-systems.md
        events.md  input.md  schedule.md  states.md  recipes.md
    native/
      getting-started.md
  recipes.md                   recipes in C++, each linking to its TS version
  cli.md                       the oly command: install, new, doctor, build, test, run, upgrade, local mode, --docker
  cheat-sheet.md               C++, C API and per-client names
  troubleshooting.md
```

- Pages are not numbered. File names are lowercase `kebab-case`.
- **A concept page carries the contract; a client page carries the API.** The rules a game relies on (deferred vs. direct, event rounds, scope, validity) are written only in the concept page. A rule appears in a client page only when it exists only in that client (TS: `raw()` arrays, heap views invalidated by memory growth, `ContractError`).
- **Client pages mirror the concept pages**: the same file name and the same section headings where the concept has a form in that client. A section with no client form is left out, not stubbed. A concept section links to its client section by anchor.
- A concept page starts with `# Title` and a navigation line, in the order schema, world-and-entities, queries, engine-and-systems, events, input, schedule, states, performance, plugins (the first has no previous link, the last no next):

  ```markdown
  [← Previous title](previous.md) · [Guide index](../README.md) · Next: [Next title →](next.md)
  ```

- A TS page starts with `# Title in TypeScript` and its own navigation line, in the same order, then `recipes.md`:

  ```markdown
  [← Previous title](previous.md) · [TypeScript client](README.md) · Concept: [Title](../../../concepts/concept.md) · Next: [Next title →](next.md)
  ```

- Environment pages and references use `[Guide index](<relative>/README.md)` only.
- Concept and TS pages end with a `## Next` section: one line with the link and what the next page teaches. A concept page's last line is `In other clients: [TypeScript](../environments/web/typescript/<concept>.md).`
- Links are relative. Anchors follow GitHub's rule: lowercase, punctuation removed, spaces become `-` (`## Systems in C++` → `#systems-in-c`).

## Page structure

1. Title and navigation line.
2. One or two sentences: what this is and why the reader needs it.
3. The smallest working example.
4. Sections from the common case to the rare one.
5. Rules and pitfalls next to the feature they belong to, not in a separate list at the end.
6. `## Next`.

## Examples

- **C++ first in concept pages**, the client's language in client pages.
- **The main example of a concept is the function body written against the context**, with `OLY_SYSTEM`, `OLY_HANDLER` and `OLY_STATE_HOOK`: it is valid in every environment. Under it, the registration is short and labelled:

  ````markdown
  **Where you own the `Engine`** (native):

  ```cpp
  engine.addSystem(oly::app::Phase::Update, "integrate", integrate);
  ```

  **On the web**, the client registers it by name: [TypeScript](../environments/web/typescript/engine-and-systems.md#anchor).
  ````

  Setup-only operations (declaring, binding, pause from outside a system) are shown on `Engine` with the same "Where you own the `Engine`" label. Names are the generated ones: `Position`, `actions::move`, `states::playing`, in a file that includes `"schema.generated.hpp"`.
- **An `OLY_HANDLER` is registered natively with a captureless lambda**, because the macro's function takes the batch as a span and `on<T>` takes a function of the context alone. The first time a page shows it, one sentence says why:

  ```cpp
  engine.on<EnemyDied>("onEnemyDied", [](oly::app::HandlerContext& context) { onEnemyDied(context, context.events<EnemyDied>()); });
  ```

- **Real signatures only.** Check each C++ call against the headers and each TS call against `@oly/core` or `@oly/web-host` before writing it. Compiling the examples is out of scope; an example that does not exist in the API is worse than none.
- **Complete enough to copy**: imports when they are not obvious, the schema entries an example needs, no `...` inside the logic that matters.
- **Follow the code style** ([code-style.md](code-style.md)): full names, braces on every `if`/`for`, blank lines between steps.
- **Comments only for the why**, the same rule as in the code. No comments that narrate the example step by step: put that in the prose before the block.
- **Commands** are `oly` commands (`oly new`, `oly test`, `oly run`), as in [cli.md](../guide/cli.md). Inside the engine repository they are `./spur` tasks (`./spur games-test`, `./spur wasm <command>`, `./spur dev <command>`), as in [commands.md](commands.md).
- Show the wrong way only when the mistake is common, and always next to the right way.

## Recipes

- One problem per section, named by what the reader wants (`## Ability with a cooldown`), listed in the table at the top. The recipe is in C++ in `recipes.md` and in TS under the same heading in `environments/web/typescript/recipes.md`; a recipe that only makes sense in the browser lives only in the TS page.
- Each recipe shows the schema entries it needs, then the code, then one or two lines on why it is built that way or when to choose another tool.

## Cheat sheet

- Every public name, grouped by topic (C++, then the C API, then each client), with the shortest correct signature.
- No explanations beyond a few words: link to the page instead.
