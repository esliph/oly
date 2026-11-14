# Drift

Desktop game in C++ only: the player steers a square at the bottom of the screen, rocks fall from the top, and touching one ends the run. It shows the engine without the C API or TypeScript, on the `native` environment.

What it uses of the engine: a schema (`drift.oly`) declared with `oly::app::declareSchema`, systems in three phases, an event (`PlayerHit`) with a handler, two states (`playing`, `gameOver`) with scope, a repeating task (the rock spawner), an `axis2d` and a `button` action, and drawing in a `Render` system through the native host's `Window`.

## Controls

- WASD or the arrow keys: move.
- Enter or Space: play again after a game over.
- Minimizing the window pauses the game.

## How to run

```sh
./spur desktop drift                     # builds and prints build/native-games-release/games/drift_game
PRESET=windows-release ./spur desktop drift   # cross-compiles build/windows-release/games/drift_game.exe
./spur native-games-test                 # includes drift.desktop: 600 frames with the dummy video driver
```

On Linux the executable opens an X11 or Wayland window, so it runs on the host, not in the `dev` container. The Windows `.exe` is static: it needs no DLL besides the ones Windows ships.

`drift_game --frames <n>` runs `n` frames of 1/60 s and exits, with no wait for the clock. `SDL_VIDEO_DRIVER=dummy` makes it headless.

## Layout

- `drift.oly`: the schema.
- `native/`: the gameplay, which never includes SDL (`gameplay.cpp`, constants in `drift.hpp`).
- `desktop/`: `main.cpp` (engine, window, `run`) and `render.cpp` (the `Render` system, with SDL).
