import assert from "node:assert/strict";
import { test } from "node:test";

import { ContractError } from "@oly/core";
import type { Engine, EngineConfig, Schema } from "@oly/core";

import { Game, runGame } from "../dist/index.js";
import type { BrowserInput, BrowserLoop, FrameTarget, GameEnvironment, GameOptions } from "../dist/index.js";

class FakeEngine {
  onFrame: (() => void) | null = null;

  constructor(private readonly log: string[]) {}

  frame(_nowSeconds: number): void {
    this.onFrame?.();
  }

  setHidden(_hidden: boolean): void {}

  dispose(): void {
    this.log.push("dispose");
  }
}

class FakeLoop implements BrowserLoop {
  constructor(
    private readonly log: string[],
    readonly target: FrameTarget,
  ) {}

  stop(): void {
    this.log.push("loopStop");
  }

  tick(nowSeconds: number): void {
    this.target.frame(nowSeconds);
  }
}

class FakeEnvironment implements GameEnvironment {
  readonly log: string[] = [];
  readonly canvases = new Map<string, HTMLCanvasElement | null>();
  readonly engine = new FakeEngine(this.log);
  canvasByIdCalls = 0;
  loadCalls = 0;
  pageText = "";
  loop: FakeLoop | null = null;
  runInBrowserError: unknown = null;

  canvasById(id: string): HTMLCanvasElement | null {
    ++this.canvasByIdCalls;

    return this.canvases.get(id) ?? null;
  }

  showError(message: string): void {
    this.pageText = message;
  }

  async loadRuntime(_module: string | URL, _schema: Schema): Promise<{ createEngine(config?: EngineConfig): Engine }> {
    ++this.loadCalls;
    this.log.push("loadRuntime");

    return {
      createEngine: () => {
        this.log.push("createEngine");

        return this.engine as unknown as Engine;
      },
    };
  }

  attachBrowserInput(_engine: Engine, _canvas: HTMLCanvasElement): BrowserInput {
    this.log.push("attachBrowserInput");

    return { detach: () => this.log.push("detach") };
  }

  runInBrowser(target: FrameTarget): BrowserLoop {
    this.log.push("runInBrowser");
    if (this.runInBrowserError !== null) {
      throw this.runInBrowserError;
    }

    this.loop = new FakeLoop(this.log, target);

    return this.loop;
  }
}

class TestGame extends Game {
  onStartCallback: (engine: Engine, canvas: HTMLCanvasElement) => void = () => {};

  constructor(
    private readonly log: string[],
    options: Partial<GameOptions> = {},
  ) {
    super({ canvas: "game", module: "./game.mjs", schema: {} as unknown as Schema, ...options });
  }

  onStart(engine: Engine, canvas: HTMLCanvasElement): void {
    this.log.push("onStart");
    this.onStartCallback(engine, canvas);
  }
}

function fakeCanvas(): HTMLCanvasElement {
  return { width: 0, height: 0 } as unknown as HTMLCanvasElement;
}

function environmentWithCanvas(): { environment: FakeEnvironment; canvas: HTMLCanvasElement } {
  const environment = new FakeEnvironment();
  const canvas = fakeCanvas();
  environment.canvases.set("game", canvas);

  return { environment, canvas };
}

test("runGame starts the game before it attaches the input and starts the loop", async () => {
  const { environment } = environmentWithCanvas();

  await runGame(new TestGame(environment.log), environment);

  assert.deepEqual(environment.log, ["loadRuntime", "createEngine", "onStart", "attachBrowserInput", "runInBrowser"]);
});

test("runGame finds the canvas by id and sets the size given in the options", async () => {
  const { environment, canvas } = environmentWithCanvas();
  const game = new TestGame(environment.log, { width: 640, height: 360 });
  let received: HTMLCanvasElement | null = null;
  game.onStartCallback = (_engine, startCanvas) => {
    received = startCanvas;
  };

  await runGame(game, environment);

  assert.equal(received, canvas);
  assert.equal(canvas.width, 640);
  assert.equal(canvas.height, 360);
});

test("runGame uses a canvas given as an element without looking it up", async () => {
  const environment = new FakeEnvironment();
  const canvas = fakeCanvas();
  const game = new TestGame(environment.log, { canvas });
  let received: HTMLCanvasElement | null = null;
  game.onStartCallback = (_engine, startCanvas) => {
    received = startCanvas;
  };

  await runGame(game, environment);

  assert.equal(environment.canvasByIdCalls, 0);
  assert.equal(received, canvas);
});

test("an id that finds no canvas rejects naming the id, writes the page and loads nothing", async () => {
  const environment = new FakeEnvironment();
  environment.canvases.set("game", null);

  await assert.rejects(runGame(new TestGame(environment.log), environment), /canvas with id "game" not found on the page/);

  assert.equal(environment.pageText, 'Error starting the game: canvas with id "game" not found on the page');
  assert.equal(environment.loadCalls, 0);
});

test("an onStart that throws disposes the engine, writes the page and rejects with the same error", async () => {
  const { environment } = environmentWithCanvas();
  const error = new Error("boom");
  const game = new TestGame(environment.log);
  game.onStartCallback = () => {
    throw error;
  };

  await assert.rejects(runGame(game, environment), (thrown) => thrown === error);

  assert.equal(environment.log.at(-1), "dispose");
  assert.ok(!environment.log.includes("attachBrowserInput"));
  assert.equal(environment.pageText, "Error starting the game: boom");
});

test("a thrown value that is not an Error reaches the page through String", async () => {
  const { environment } = environmentWithCanvas();
  const game = new TestGame(environment.log);
  game.onStartCallback = () => {
    throw "text";
  };

  await assert.rejects(runGame(game, environment), (thrown) => thrown === "text");

  assert.equal(environment.pageText, "Error starting the game: text");
});

test("a runInBrowser that throws detaches the input, then disposes the engine", async () => {
  const { environment } = environmentWithCanvas();
  environment.runInBrowserError = new Error("no loop");

  await assert.rejects(runGame(new TestGame(environment.log), environment), /no loop/);

  assert.deepEqual(environment.log.slice(-3), ["runInBrowser", "detach", "dispose"]);
});

test("stop stops the loop, detaches the input and disposes the engine, once", async () => {
  const { environment } = environmentWithCanvas();
  const browserGame = await runGame(new TestGame(environment.log), environment);
  const startLength = environment.log.length;

  browserGame.stop();
  browserGame.stop();

  assert.deepEqual(environment.log.slice(startLength), ["loopStop", "detach", "dispose"]);
});

test("stop during a frame throws ContractError and leaves the game running", async () => {
  const { environment } = environmentWithCanvas();
  const browserGame = await runGame(new TestGame(environment.log), environment);
  environment.engine.onFrame = () => browserGame.stop();

  assert.throws(
    () => environment.loop!.tick(1000),
    (error) => error instanceof ContractError && /^stop:/.test(error.message),
  );

  assert.ok(!environment.log.includes("loopStop"));
  assert.ok(!environment.log.includes("detach"));
  assert.ok(!environment.log.includes("dispose"));
});

test("stop works after a frame that threw", async () => {
  const { environment } = environmentWithCanvas();
  const browserGame = await runGame(new TestGame(environment.log), environment);
  environment.engine.onFrame = () => {
    throw new Error("system");
  };

  assert.throws(() => environment.loop!.tick(1000), /system/);
  const startLength = environment.log.length;

  browserGame.stop();

  assert.deepEqual(environment.log.slice(startLength), ["loopStop", "detach", "dispose"]);
});
