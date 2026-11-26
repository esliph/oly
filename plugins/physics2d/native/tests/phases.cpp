#include <doctest/doctest.h>

#include <span>
#include <string>
#include <vector>

#include "components.hpp"
#include "oly/app/engine.hpp"
#include "oly/ecs/diagnostics.hpp"
#include "oly/ecs/world.hpp"
#include "oly/essentials2d/plugin.hpp"
#include "oly/physics2d/plugin.hpp"

namespace {
  std::vector<std::string> phaseNames(std::span<const oly::app::FlushTiming> flushes) {
    std::vector<std::string> names;

    for (const oly::app::FlushTiming& flush : flushes) {
      names.emplace_back(flush.phaseName);
    }

    return names;
  }

  const std::vector<std::string> expectedPhases{"oly::app::UpdateBefore", "oly::app::Update", "oly::physics2d::Physics", "oly::essentials2d::Propagation", "oly::app::UpdateAfter", "oly::app::Render"};
}

TEST_CASE("Physics runs after Update and before the propagation") {
  oly::ecs::World world;
  oly::app::Engine engine(world);
  engine.installPlugin<oly::essentials2d::Plugin>();
  engine.installPlugin<oly::physics2d::Plugin>();
  engine.setTimingEnabled(true);

  engine.frame(0.0);

  CHECK(phaseNames(engine.flushTimings()) == expectedPhases);
  CHECK_INVARIANTS(world);
}

TEST_CASE("install order does not change the phase order") {
  oly::ecs::World world;
  oly::app::Engine engine(world);
  engine.installPlugin<oly::physics2d::Plugin>();
  engine.installPlugin<oly::essentials2d::Plugin>();
  engine.setTimingEnabled(true);

  engine.frame(0.0);

  CHECK(phaseNames(engine.flushTimings()) == expectedPhases);
  CHECK_INVARIANTS(world);
}
