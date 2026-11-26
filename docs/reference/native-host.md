# Native host

[Reference](../README.md#reference) · Concept: [Environments and hosts](../explanation/concepts/environments-and-hosts.md)

The native host opens a window with SDL, forwards its input to the `Engine` and calls `Engine::frame` until the window closes. A game turns it on with `ENVIRONMENT native` and `DEPENDS ... native_host` in `oly_add_game` ([The game template](game-template.md#native)). Include `oly/native_host/game.hpp` to start a game, and `oly/native_host/window.hpp` and `oly/native_host/run.hpp` for the pieces it uses.

## The game

A game derives one class from `Game`, writes its content in `onStart` and starts with one `runGame` call. Include `oly/native_host/game.hpp`.

### `Game`

```cpp
explicit Game(WindowConfig window, oly::app::EngineConfig engine = {});
const WindowConfig& windowConfig() const noexcept;
const oly::app::EngineConfig& engineConfig() const noexcept;
virtual void onStart(oly::app::Engine& engine, Window& window) = 0;
```

The class holds the window configuration, the engine configuration and the game's own state. A `Game` cannot be copied. `runGame` calls `onStart` once, on an `Engine` that `setUpGame` already set up and with the window open, before the first frame. Bind actions, spawn entities and add systems there. A system that needs the game's state reads it as a resource (`Res<T>`), which `onStart` inserts with `engine.world().insertResource<T>()`.

```cpp
class MyGame : public oly::native_host::Game {
 public:
  MyGame() : Game({"<game>", 640, 360}) {}

  void onStart(oly::app::Engine& engine, oly::native_host::Window& window) override {
    engine.world().insertResource<Renderer>(window.renderer());
    engine.addSystem<&draw>(oly::app::Render{}, "draw");
  }
};
```

### `runGame`

```cpp
template <class G>
int runGame(int argumentCount, char** arguments);

int runGame(Game& game, RunOptions options = {});
```

`runGame<G>(argumentCount, arguments)` is the whole `main` of most games: `return oly::native_host::runGame<MyGame>(argumentCount, arguments);`. `G` must derive from `Game` and have a default constructor. It does this, in order:

1. Reads the command line. The only option is `--frames <n>`, where `n` is a whole number above 0.
2. Constructs `G`.
3. With `--frames`, sets `maxFrames = n` and `frameSeconds = 1 / fixedHz` of the game's `EngineConfig`, so one frame is one simulation step. The first frame only records the time, so `n` frames run `n - 1` steps.
4. Calls `runGame(game, options)` and returns what it returns.

| Exit code | When | stderr |
|---|---|---|
| `0` | `run` returned | nothing |
| `64` | an unknown argument, or a `--frames` value that is not a whole number above 0; nothing is constructed | `usage: <program> [--frames <n>]` |
| `1` | a `std::exception` escaped, the constructor's included | `<title>: <message>`, where `<title>` is the window title (the program name when the constructor threw) |

`runGame(Game&, RunOptions)` does not catch. It builds the `World` and the `Engine` with `game.engineConfig()`, calls `oly::app::setUpGame` (so a game never calls it), opens the `Window` with `game.windowConfig()`, calls `game.onStart(engine, window)` and returns `run(engine, window, options)`.

A game with its own `main` (to read options of its own, for example) declares the game first and then calls `runGame(game, options)`. The `Game` is then destroyed after the `Engine`, as below.

**Teardown order.** When `runGame(Game&)` returns or throws, the `Window` closes first. Then the `Engine` runs the `discard` of each pending task and destroys the plugins in reverse install order. Then the `World` goes. Only after that is the `Game` destroyed. So a task's `discard` and a plugin's destructor run while the game is alive, but after the window is gone: they must not use the renderer.

## The window

### `WindowConfig`

```cpp
struct WindowConfig {
  std::string title;
  int width;
  int height;
};
```

`width` and `height` are the **logical size**: the size your game draws in. The host scales it to the window and adds bars when the aspect ratio differs.

### `Window`

```cpp
explicit Window(const WindowConfig& config);
SDL_Window* window() const noexcept;
SDL_Renderer* renderer() const noexcept;
```

Opens a resizable window and an SDL renderer, and turns vsync on. The constructor throws `std::runtime_error` with SDL's message when SDL cannot start its video subsystem or create the window (no display, no video driver). A `Window` cannot be copied. Its destructor closes the renderer and the window.

`runGame` opens the window and passes it to `onStart`:

```cpp
void onStart(oly::app::Engine& engine, oly::native_host::Window& window) override {
  engine.world().insertResource<Renderer>(window.renderer());
  engine.addSystem<&draw>(oly::app::Render{}, "draw");
}
```

A game that drives the loop itself builds the `Window` directly: `oly::native_host::Window window({"<game>", 640, 360});`.

There is no draw hook. Draw in a `Render` system and reach the renderer through a resource that holds it (`Renderer` above is the game's own struct around the `SDL_Renderer*`); include `<SDL3/SDL.h>` only in the file that draws. The host clears the renderer to black before each frame and presents it after the `Render` phase.

## The loop

### `RunOptions`

```cpp
struct RunOptions {
  uint64_t maxFrames = 0;
  double frameSeconds = 0.0;
};
```

| Field | Meaning |
|---|---|
| `maxFrames` | stop after this many frames; `0` (default) runs until the window closes |
| `frameSeconds` | greater than 0: advance the clock by exactly this much per frame instead of reading it |

The first frame only records the time, so `maxFrames = n` runs `n - 1` simulated frames.

### `run`

```cpp
int run(oly::app::Engine& engine, Window& window, RunOptions options = {});
```

Drives the engine until the window is closed or `maxFrames` frames ran, and returns `0`, which `main` returns as the exit code. Each iteration applies the pending window events, clears the renderer, calls `engine.frame(now)` and presents.

```cpp
return oly::native_host::run(engine, window, {.maxFrames = 600, .frameSeconds = 1.0 / 60});
```

With `frameSeconds` set and `SDL_VIDEO_DRIVER=dummy`, the whole game runs with no display ([Test a desktop game](../how-to/test-a-desktop-game.md)).

## Keys

### `keyButtonOf`

```cpp
std::optional<oly::input::PhysicalButton> keyButtonOf(SDL_Scancode scancode) noexcept;
```

Include `oly/native_host/keys.hpp`. Returns the `oly::input::Key` button of an SDL scancode, or `std::nullopt` when `Key` does not declare it. `run` uses it to turn key events into buttons, and ignores the keys that return `std::nullopt`. A shell that reads the keyboard itself can use it with `engine.actions().buttonDown`.

## What the host does with the window

| Window event | What the engine sees |
|---|---|
| key press and release | the matching `oly::input::Key` button; key repeat is ignored; keys that `Key` does not declare are ignored |
| mouse move | the pointer, in logical coordinates |
| mouse button press | the pointer moves, and `MouseButton::Left`, `Right` or `Middle` goes down only if the click is **inside** the logical area (not in the bars) |
| mouse button release | the button goes up, wherever the pointer is, if it was pressed in the window |
| minimized, hidden or covered | `setHidden(true)`: the game stops and the clock restarts on return |
| restored or shown again | `setHidden(false)` |
| focus lost | every key and mouse button is released; the game keeps running |
| close request | `run` returns |

While the window is hidden, `run` waits for events instead of spinning.

## See also

- [Your first desktop game](../tutorials/first-desktop-game.md)
- [Engine, loop and systems in C++](cpp/engine-and-systems.md#the-loop): `Engine::frame` and `setHidden`.
- [Input in C++](cpp/input.md): the `Key` and `MouseButton` names.
