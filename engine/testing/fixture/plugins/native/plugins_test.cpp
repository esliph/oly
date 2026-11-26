#include <doctest/doctest.h>

#include <span>
#include <string>
#include <vector>

#include "oly/app/engine.hpp"
#include "oly/app/game.hpp"
#include "oly/app/system_parameters.hpp"
#include "oly/counting/plugin.hpp"
#include "oly/doubling/plugin.hpp"
#include "oly/ecs/diagnostics.hpp"
#include "schema.generated.hpp"

using oly::app::Engine;
using oly::app::Query;
using oly::app::Update;

namespace {
  void addTenToCounters(Query<Counter> counters) {
    counters.each([](Counter& counter) { counter.value += 10; });
  }

  template <class First, class Second>
  std::vector<std::string> dispatchOrder() {
    oly::ecs::World world;
    Engine engine(world);
    engine.installPlugin<First>();
    engine.installPlugin<Second>();
    engine.setTimingEnabled(true);
    engine.frame(0.0);

    engine.frame(1.0 / 60);

    std::string why;
    CHECK_MESSAGE(oly::ecs::checkInvariants(world, &why), why);

    std::vector<std::string> names;

    for (const oly::app::SystemTiming& timing : engine.systemTimings()) {
      names.emplace_back(timing.name);
    }

    return names;
  }
}

TEST_CASE("setUpGame installs the dependency before the plugin that needs it") {
  oly::ecs::World world;
  Engine engine(world);

  oly::app::setUpGame(engine);

  CHECK(engine.plugin<oly::counting::Plugin>().system() < engine.plugin<oly::doubling::Plugin>().system());
}

TEST_CASE("plugin events are declared before the game's") {
  oly::ecs::World world;
  Engine engine(world);

  oly::app::setUpGame(engine);

  CHECK(engine.eventTypeOf(oly::app::eventKey<Counted>()) == 0);
  CHECK(engine.eventTypeOf(oly::app::eventKey<Hit>()) == 1);
}

TEST_CASE("the dispatch order does not depend on the install order") {
  const std::vector<std::string> countingFirst = dispatchOrder<oly::counting::Plugin, oly::doubling::Plugin>();

  const std::vector<std::string> doublingFirst = dispatchOrder<oly::doubling::Plugin, oly::counting::Plugin>();

  CHECK(countingFirst == std::vector<std::string>{"count", "writeDoubles"});
  CHECK(doublingFirst == countingFirst);
}

TEST_CASE("the doubling system sees the step's counts") {
  oly::ecs::World world;
  Engine engine(world);
  oly::app::setUpGame(engine);
  engine.addSystem<&addTenToCounters>(Update{}, "addTen").before<oly::counting::Counting>();
  const oly::ecs::Entity entity = world.spawn(Counter{0}, Doubled{0});
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(world.get<Counter>(entity).value == 11);
  CHECK(world.get<Doubled>(entity).value == 22);
}

TEST_CASE("the Engine times a plugin's systems like the game's") {
  oly::ecs::World world;
  Engine engine(world);
  oly::app::setUpGame(engine);
  engine.setTimingEnabled(true);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  const std::span<const oly::app::SystemTiming> timings = engine.systemTimings();
  REQUIRE(timings.size() == 2);
  CHECK(timings[0].name == "count");
  CHECK(timings[0].ran);
  CHECK(timings[1].name == "writeDoubles");
  CHECK(timings[1].ran);
}
