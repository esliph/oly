#include <doctest/doctest.h>

#include <span>

#include "oly/app/engine.hpp"
#include "oly/app/game.hpp"
#include "oly/counting/plugin.hpp"
#include "oly/doubling/plugin.hpp"
#include "schema.generated.hpp"

using oly::app::Engine;
using oly::app::Phase;
using oly::app::SystemContext;

namespace {
  void addTenToCounters(SystemContext& context) {
    context.query<Counter>().each([](Counter& counter) { counter.value += 10; });
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

TEST_CASE("the Last-band system of a plugin sees the step's Default-band changes") {
  oly::ecs::World world;
  Engine engine(world);
  oly::app::setUpGame(engine);
  engine.addSystem(Phase::Update, "addTen", addTenToCounters);
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
