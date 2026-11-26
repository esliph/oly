#pragma once

#include "oly/app/engine.hpp"
#include "oly/ecs/world.hpp"
#include "oly/essentials2d/plugin.hpp"
#include "oly/essentials2d/schema.generated.hpp"

struct Fixture {
  oly::ecs::World world;
  oly::app::Engine engine{world};
  oly::essentials2d::Plugin& plugin = engine.installPlugin<oly::essentials2d::Plugin>();
  double now = 0.0;
  bool started = false;

  void step() {
    if (!started) {
      engine.frame(0.0);
      started = true;
    }

    now += 1.0 / 60;
    engine.frame(now);
  }
};
