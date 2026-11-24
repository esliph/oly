import type { Engine } from "@oly/core";
import { Game } from "@oly/web-host";

import schema from "../schema.generated.js";
import { startGameplay } from "./gameplay.js";

const Width = 640;
const Height = 360;

export class MyGame extends Game {
  constructor() {
    super({ canvas: "game", module: "./@GAME_TARGET@.mjs", schema, width: Width, height: Height });
  }

  onStart(engine: Engine, canvas: HTMLCanvasElement): void {
    const context = canvas.getContext("2d");

    if (context === null) {
      throw new Error("the browser did not provide a 2D context for the canvas");
    }

    startGameplay(engine, {
      clear: () => context.clearRect(0, 0, canvas.width, canvas.height),
      square: (x, y, size) => {
        context.fillStyle = "#ffffff";
        context.fillRect(x - size / 2, y - size / 2, size, size);
      },
    });
  }
}
