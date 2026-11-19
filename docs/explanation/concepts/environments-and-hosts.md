# Environments and hosts

[Concepts](README.md) · Next: [Schema →](schema.md)

A game is made of three layers. This page names them, shows how a game is assembled from your files and says where the drawing happens.

## The map

- The **core** is the engine in C++: the ECS, the `Engine`, input, schedule and states. It knows nothing about where it runs.
- An **environment** is a place where the core runs, with a **host** that drives the loop and forwards input.
- A **client** is a language that talks to the core through the C API `oly*`. C++ is not a client: it is the language of the core.

| Environment | Host | Clients | Start here |
|---|---|---|---|
| `web` | the web host: WASM, `runInBrowser`, `attachBrowserInput` | [TypeScript](../../reference/typescript/README.md) | [Your first web game](../../tutorials/first-web-game.md) |
| `native` | the native host: a window, SDL, `oly::native_host::run` | none: the game is C++ | [Your first desktop game](../../tutorials/first-desktop-game.md) |

The engine, the schema and the gameplay code are the same in both environments. Only the shell and the drawing change. Choose `native` when the game does not need a browser, and `web` when it does. Both live in the game's own repository, which pins one engine version ([The `oly` command](../../guide/cli.md#the-game-repository)).

Oly does not draw, play audio or do physics. You draw with whatever you want (Canvas 2D, SDL, a test recorder) inside a `Render` system.

## How a game is assembled

You write the schema and the gameplay. The build turns them into one program.

On the web, `oly build` makes one WASM module and one JS bundle:

```
<game>.oly ──oly-schema──► schema.generated.hpp (C++) + web/schema.generated.ts (TS)
native/*.cpp  (your C++ systems, optional) ──┐
engine (ecs + app + input + capi + web_host) ┴──emcc──► build/web/<target>.mjs + .wasm
web/src/*.ts  (your TS systems and the shell) ──tsc + esbuild──► build/web/main.js
```

At start-up, `loadRuntime` checks that the WASM was built from the same schema as the TS code. If you change the schema and forget to rebuild, you get a `SchemaMismatchError` instead of corrupted data.

On the desktop, the build makes one executable. There is no registry, no `.mjs`, no TypeScript and no C API: your `main` creates the `World` and the `Engine`, opens a `Window` and hands the loop to `run`. The schema and the gameplay are compiled into it, and the shell is the SDL code in `desktop/main.cpp`.

In both, `<game>.oly` is the single source of the data: the build generates the code of each language from it ([Schema](schema.md)). The files `oly new` writes for each environment are listed in [The game template](../../reference/game-template.md).

## Drawing

The engine has no draw hook. A game draws in a `Render` system, as on the web: the host prepares the surface before the frame and shows it after the `Render` phase. The native host clears its renderer to black before the frame and presents it after; the web shell clears and draws on its canvas through a `Drawer`.

Keep the gameplay in files that do not touch the drawing library, and the drawing in the shell. The gameplay then stays the same if you add another shell, and a test can run it with no window and no page ([Test a desktop game](../../how-to/test-a-desktop-game.md), [Test a web game](../../how-to/test-a-web-game.md)).

## API

- C++: [Native host](../../reference/native-host.md) and [The game template](../../reference/game-template.md)
- TypeScript: [TypeScript client](../../reference/typescript/README.md)

## Next

[Schema](schema.md): how a `.oly` file declares the data of the game.
