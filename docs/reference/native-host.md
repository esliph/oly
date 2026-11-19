# Native host

[Reference](../README.md#reference) · Concept: [Environments and hosts](../explanation/concepts/environments-and-hosts.md)

The native host opens a window with SDL, forwards its input to the `Engine` and calls `Engine::frame` until the window closes. A game turns it on with `ENVIRONMENT native` and `DEPENDS ... native_host` in `oly_add_game` ([The game template](game-template.md#native)). Include `oly/native_host/window.hpp` and `oly/native_host/run.hpp`.

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

```cpp
oly::native_host::Window window({"<game>", 640, 360});
engine.addSystem(oly::app::Phase::Render, "draw", draw, window.renderer());
```

There is no draw hook. Draw in a `Render` system and reach the renderer through `userData`; include `<SDL3/SDL.h>` only in the file that draws. The host clears the renderer to black before each frame and presents it after the `Render` phase.

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
