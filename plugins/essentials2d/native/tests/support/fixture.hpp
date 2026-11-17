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

  Fixture() {
    engine.frame(0.0);
  }

  void step() {
    now += 1.0 / 60;
    engine.frame(now);
  }
};
