import assert from "node:assert/strict";
import { test } from "node:test";
import { pathToFileURL } from "node:url";

import { loadRuntime } from "@oly/core";
import type { OlyModuleFactory } from "@oly/core";

import schema, { Player, Position } from "../schema.generated.js";
import { startGameplay } from "../src/gameplay.js";
import type { Drawer } from "../src/gameplay.js";

const StepsPerSecond = 60;
const PlayerSpeed = 200;
const Tolerance = 1e-3;

interface Square {
  readonly x: number;
  readonly y: number;
  readonly size: number;
}

/** Keeps the calls made since the last `clear`, which is what one frame drew. */
class RecordingDrawer implements Drawer {
  readonly squares: Square[] = [];

  clear(): void {
    this.squares.length = 0;
  }

  square(x: number, y: number, size: number): void {
    this.squares.push({ x, y, size });
  }
}

function gameFactory(): OlyModuleFactory {
  const modulePath = process.env.OLY_GAME_MODULE;
  if (!modulePath) {
    throw new Error("set OLY_GAME_MODULE to the path of the game's .mjs (ctest already does this)");
  }

  return async () => {
    const loaded: { default: OlyModuleFactory } = await import(pathToFileURL(modulePath).href);

    return loaded.default();
  };
}

const runtime = await loadRuntime(gameFactory(), schema);

test("holding KeyD for one second moves the player right by the player speed and draws one square", () => {
  const engine = runtime.createEngine({ fixedHz: StepsPerSecond });
  const drawer = new RecordingDrawer();

  try {
    startGameplay(engine, drawer);
    engine.frame(0);
    engine.actions.buttonDown("KeyD");

    for (let step = 1; step <= StepsPerSecond; ++step) {
      engine.frame(step / StepsPerSecond);
    }

    const player = engine.world.query({ with: [Player] }).single().entity;
    const position = engine.world.get(player, Position);
    assert.ok(Math.abs(position.x - (320 + PlayerSpeed)) <= Tolerance, `x is ${position.x}`);
    assert.ok(Math.abs(position.y - 180) <= Tolerance, `y is ${position.y}`);
    assert.equal(drawer.squares.length, 1);
  } finally {
    engine.dispose();
  }
});
