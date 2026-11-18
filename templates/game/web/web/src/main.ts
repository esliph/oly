import { loadRuntime } from "@oly/core";
import { attachBrowserInput, moduleFactoryFromUrl, runInBrowser } from "@oly/web-host";

import schema from "../schema.generated.js";
import { installGame } from "./game.js";

const Width = 640;
const Height = 360;

async function boot(): Promise<void> {
  const canvas = document.getElementById("game");
  if (!(canvas instanceof HTMLCanvasElement)) {
    throw new Error('canvas with id "game" not found on the page');
  }

  canvas.width = Width;
  canvas.height = Height;
  const context = canvas.getContext("2d");
  if (context === null) {
    throw new Error("the browser did not provide a 2D context for the canvas");
  }

  const runtime = await loadRuntime(moduleFactoryFromUrl("./@GAME_TARGET@.mjs"), schema);
  const engine = runtime.createEngine({ fixedHz: 60 });
  installGame(engine, {
    clear: () => context.clearRect(0, 0, Width, Height),
    square: (x, y, size) => {
      context.fillStyle = "#ffffff";
      context.fillRect(x - size / 2, y - size / 2, size, size);
    },
  });
  attachBrowserInput(engine, canvas);
  runInBrowser(engine);
}

boot().catch((error: unknown) => {
  console.error(error);
  document.body.textContent = `Error starting the game: ${error instanceof Error ? error.message : String(error)}`;
});
