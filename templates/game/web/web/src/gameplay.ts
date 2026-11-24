import { Phase } from "@oly/core";
import type { Engine } from "@oly/core";

import { Player, Position, move } from "../schema.generated.js";

const PlayerSize = 16;

export interface Drawer {
  clear(): void;
  square(x: number, y: number, size: number): void;
}

export function startGameplay(engine: Engine, drawer: Drawer): void {
  engine.actions.bind(move, { up: "KeyW", down: "KeyS", left: "KeyA", right: "KeyD" });
  engine.world.spawn([Player, Position], { Position: { x: 320, y: 180 } });

  engine.addNativeSystem(Phase.Update, "movePlayer");

  engine.addSystem(Phase.Render, "draw", (context) => {
    drawer.clear();
    context.query({ read: [Position], with: [Player] }).eachBlock((block) => {
      const position = block.read(Position);

      for (let row = 0; row < block.count; ++row) {
        drawer.square(position.x.get(row), position.y.get(row), PlayerSize);
      }
    });
  });
}
