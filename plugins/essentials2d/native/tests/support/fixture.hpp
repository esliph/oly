#pragma once

#include <span>
#include <string>
#include <vector>

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

  // The first frame closes the registration, so it comes after the test registers its systems.
  void start() {
    if (!started) {
      engine.frame(0.0);
      started = true;
    }
  }

  void step() {
    start();
    now += 1.0 / 60;
    engine.frame(now);
  }
};

inline std::vector<std::string> phaseNames(std::span<const oly::app::FlushTiming> flushes) {
  std::vector<std::string> names;

  for (const oly::app::FlushTiming& flush : flushes) {
    names.emplace_back(flush.phaseName);
  }

  return names;
}
