# The web host in TypeScript

[TypeScript client](README.md) · Concept: [Environments and hosts](../../explanation/concepts/environments-and-hosts.md)

The package `@oly/web-host` is the web host: it loads the WASM module, drives the loop in a browser tab and delivers keyboard and mouse input to the engine. It has three functions, which a game calls once at start-up. The template calls all three ([The game template](../game-template.md)).

```ts
import { attachBrowserInput, moduleFactoryFromUrl, runInBrowser } from "@oly/web-host";
```

## `moduleFactoryFromUrl`

```ts
function moduleFactoryFromUrl(url: string | URL): OlyModuleFactory;
```

Returns the factory that `loadRuntime` takes. `url` is the `.mjs` file the build wrote for the game, relative to the page. The `.wasm` file is fetched from the same folder. Nothing loads until `loadRuntime` calls the factory.

```ts
const runtime = await loadRuntime(moduleFactoryFromUrl("./game.mjs"), schema);
```

The factory rejects with `Error` if the `.mjs` does not export the Emscripten factory as its default export: `<href>: the module does not export an Emscripten factory as default`. If the module does not link `oly::web_host`, `loadRuntime` throws `ContractError`.

## `runInBrowser`

```ts
function runInBrowser(target: FrameTarget): BrowserLoop;
```

Runs the loop of an `Engine` in the tab. Pass the engine as `target`. It calls `engine.frame(seconds)` on every `requestAnimationFrame`, with the time in seconds, and `engine.setHidden(hidden)` whenever the document's visibility changes. If the document is already hidden, it calls `setHidden(true)` at once.

The next `requestAnimationFrame` is requested before `frame` runs, so an exception in a system reaches the browser console and the loop goes on.

The returned `BrowserLoop` has one method, `stop()`. It cancels the pending frame and removes the visibility listener. Calling it again does nothing.

```ts
const loop = runInBrowser(engine);
// later
loop.stop();
```

To call `frame` yourself, for a test or a tool, see [Drive the loop yourself](../../how-to/drive-the-loop-yourself.md).

## `attachBrowserInput`

```ts
function attachBrowserInput(target: InputTarget, canvas: InputCanvas): BrowserInput;
```

Delivers input from the page to the engine. Pass the engine as `target` and the game's `<canvas>` element as `canvas`.

| Event | What the host does |
|---|---|
| `keydown`, `keyup` on the window | calls `buttonDown` or `buttonUp` with the key's W3C `code`. A `code` outside the button table is ignored |
| `keydown` of a bound button | calls `preventDefault`, so Space does not scroll the page. Unbound keys keep their browser behaviour |
| `pointerdown` on the canvas | presses the mouse buttons in `PointerEvent.buttons`. A press counts only on the canvas |
| `pointerup`, `pointermove`, `pointercancel` on the window | releases the mouse buttons that are no longer down, anywhere in the window |
| `pointerdown`, `pointermove` | calls `pointerMove` with the position in canvas pixels, not screen pixels; it follows CSS scaling. Both events are ignored while the canvas has no size. A button held while the pointer enters the canvas counts as pressed from then on |
| `contextmenu` on the canvas | calls `preventDefault` only when `MouseRight` is bound |
| `blur` on the window | calls `releaseAll`, so no button stays stuck |

The mouse buttons are `MouseLeft`, `MouseRight` and `MouseMiddle`. The returned `BrowserInput` has one method, `detach()`. It removes every listener. Calling it again does nothing.

```ts
const input = attachBrowserInput(engine, canvas);
// later
input.detach();
```

The actions the engine reads, and how to bind buttons to them: [Input in TypeScript](input.md).
