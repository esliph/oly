import { ContractError, loadRuntime } from "@oly/core";
import type { Engine, EngineConfig, Schema } from "@oly/core";

import { attachBrowserInput } from "./browser_input.js";
import type { BrowserInput } from "./browser_input.js";
import { runInBrowser } from "./browser_loop.js";
import type { BrowserLoop, FrameTarget } from "./browser_loop.js";
import { moduleFactoryFromUrl } from "./module_factory.js";

export interface GameOptions {
  /** The canvas, or the id of a canvas in the page. */
  readonly canvas: string | HTMLCanvasElement;
  /** URL of the game's WASM module (`<game>.mjs`). */
  readonly module: string | URL;
  readonly schema: Schema;
  /** When given, set the canvas size in pixels; otherwise the canvas keeps its own. */
  readonly width?: number;
  readonly height?: number;
  readonly engine?: EngineConfig;
}

/** The game itself: its options and its own state. */
export abstract class Game {
  readonly options: GameOptions;

  protected constructor(options: GameOptions) {
    this.options = options;
  }

  /** Called once, on a set-up engine, before the first frame. Synchronous: `runGame` does not await it. */
  abstract onStart(engine: Engine, canvas: HTMLCanvasElement): void;
}

export interface BrowserGame {
  readonly engine: Engine;
  /** Stops the loop, detaches the input and disposes the engine; calling it again does nothing. */
  stop(): void;
}

/** What runGame reaches in the browser; tests pass a fake. Not documented in web-host.md, like BrowserEnvironment. */
export interface GameEnvironment {
  /** The canvas with this id, or null when the id finds nothing or finds an element that is not a canvas. */
  canvasById(id: string): HTMLCanvasElement | null;
  showError(message: string): void;
  loadRuntime(module: string | URL, schema: Schema): Promise<{ createEngine(config?: EngineConfig): Engine }>;
  attachBrowserInput(engine: Engine, canvas: HTMLCanvasElement): BrowserInput;
  runInBrowser(target: FrameTarget): BrowserLoop;
}

// Reaches the browser globals only when called, so importing the package in Node stays safe.
function globalGameEnvironment(): GameEnvironment {
  return {
    canvasById: (id) => {
      const element = globalThis.document.getElementById(id);

      return element instanceof HTMLCanvasElement ? element : null;
    },
    showError: (message) => {
      globalThis.document.body.textContent = message;
    },
    loadRuntime: (module, schema) => loadRuntime(moduleFactoryFromUrl(module), schema),
    attachBrowserInput: (engine, canvas) => attachBrowserInput(engine, canvas),
    runInBrowser: (target) => runInBrowser(target),
  };
}

function findCanvas(canvas: string | HTMLCanvasElement, environment: GameEnvironment): HTMLCanvasElement {
  if (typeof canvas !== "string") {
    return canvas;
  }

  const found = environment.canvasById(canvas);
  if (found === null) {
    throw new Error(`canvas with id "${canvas}" not found on the page`);
  }

  return found;
}

/**
 * Finds the canvas, loads the runtime, creates the engine, calls `game.onStart`, attaches the input and starts the
 * loop, in this order. When a step throws or rejects, it undoes what already happened, writes
 * `Error starting the game: <message>` into the page and rejects with the same value.
 */
export async function runGame(game: Game, environment: GameEnvironment = globalGameEnvironment()): Promise<BrowserGame> {
  let engine: Engine | undefined;
  let input: BrowserInput | undefined;
  let loop: BrowserLoop | undefined;

  try {
    const { options } = game;
    const canvas = findCanvas(options.canvas, environment);
    if (options.width !== undefined) {
      canvas.width = options.width;
    }
    if (options.height !== undefined) {
      canvas.height = options.height;
    }

    const runtime = await environment.loadRuntime(options.module, options.schema);
    const startedEngine = runtime.createEngine(options.engine);
    engine = startedEngine;

    game.onStart(startedEngine, canvas);

    const attachedInput = environment.attachBrowserInput(startedEngine, canvas);
    input = attachedInput;

    // The host loop calls this wrapper, not the engine: the flag is how `stop` knows it runs inside a frame (a system,
    // a handler, a hook or a task), without any change to Engine.
    let inFrame = false;
    const startedLoop = environment.runInBrowser({
      frame(nowSeconds) {
        inFrame = true;
        try {
          startedEngine.frame(nowSeconds);
        } finally {
          inFrame = false;
        }
      },
      setHidden: (hidden) => startedEngine.setHidden(hidden),
    });
    loop = startedLoop;

    let stopped = false;

    return {
      engine: startedEngine,
      stop(): void {
        if (inFrame) {
          throw new ContractError("stop: game in frame; stop after the frame");
        }
        if (stopped) {
          return;
        }

        stopped = true;
        startedLoop.stop();
        attachedInput.detach();
        startedEngine.dispose();
      },
    };
  } catch (error) {
    loop?.stop();
    input?.detach();
    engine?.dispose();
    environment.showError(`Error starting the game: ${error instanceof Error ? error.message : String(error)}`);

    throw error;
  }
}
