# Changelog

Each version of the engine has one section. While Oly is `0.x`, a minor version can break compatibility: read `### Breaking` and `### Migration` before you upgrade ([Upgrade the engine](docs/how-to/upgrade-the-engine.md)).

## Unreleased

### Breaking

None.

### Migration

- Native: a game may derive `oly::native_host::Game`, write its content in `onStart` and start with `runGame<MyGame>(argumentCount, arguments)` in `main`. `runGame` calls `setUpGame`, reads `--frames <n>` and turns an exception into exit code 1 ([Native host](docs/reference/native-host.md#the-game)). A game that keeps its own `main` works as before.
- Web: a game may derive `Game` from `@oly/web-host` and start with `runGame(new MyGame())`. `runGame` finds the canvas, loads the runtime, calls `onStart`, attaches the input and starts the loop ([Web host](docs/reference/typescript/web-host.md#game)). A game that calls `loadRuntime`, `attachBrowserInput` and `runInBrowser` itself works as before.

## 0.1.0

First release.

### Breaking

None.

### Migration

None.
