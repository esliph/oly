#include <doctest/doctest.h>

#include <span>
#include <string>
#include <vector>

#include "components.hpp"
#include "log.hpp"
#include "oly/app/engine.hpp"

using oly::app::Engine;
using oly::app::FlushTiming;
using oly::app::OrderBand;
using oly::app::Phase;
using oly::app::SystemTiming;

namespace {
  struct Fixture {
    oly::ecs::World world;
    Engine engine{world};
    Log log;
    double now = 0.0;

    void step() {
      now += 1.0 / 60;
      engine.frame(now);
    }
  };

  std::vector<std::string> namesOf(std::span<const SystemTiming> timings) {
    std::vector<std::string> names;

    for (const SystemTiming& timing : timings) {
      names.emplace_back(timing.name);
    }

    return names;
  }

  // Short names fit in std::string's inline buffer, so they move with the entries when the system vectors grow.
  std::vector<std::string> addNumberedSystems(Fixture& fixture, int count) {
    std::vector<std::string> names;

    for (int index = 0; index < count; ++index) {
      names.push_back("s" + std::to_string(index));
      fixture.engine.addSystem(Phase::Update, names.back(), recordName, &fixture.log);
    }

    return names;
  }
}

TEST_CASE("timing off: a frame records no system and no flush") {
  Fixture fixture;
  fixture.engine.addSystem(Phase::Update, "update", recordName, &fixture.log);
  fixture.engine.frame(0.0);

  fixture.step();

  CHECK(fixture.engine.systemTimings().empty());
  CHECK(fixture.engine.flushTimings().empty());
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("timing on: one entry per system, in dispatch order, with its id and phase") {
  Fixture fixture;
  const auto last = fixture.engine.addSystem(Phase::Update, "last", recordName, &fixture.log, OrderBand::Last);
  const auto update = fixture.engine.addSystem(Phase::Update, "update", recordName, &fixture.log);
  const auto before = fixture.engine.addSystem(Phase::UpdateBefore, "before", recordName, &fixture.log);
  const auto render = fixture.engine.addSystem(Phase::Render, "render", recordName, &fixture.log);
  fixture.engine.setTimingEnabled(true);
  fixture.engine.frame(0.0);

  fixture.step();

  const std::span<const SystemTiming> timings = fixture.engine.systemTimings();
  REQUIRE(timings.size() == 4);
  CHECK(namesOf(timings) == std::vector<std::string>{"before", "update", "last", "render"});
  CHECK(timings[0].system == before);
  CHECK(timings[0].phase == fixture.engine.phaseId<oly::app::UpdateBefore>());
  CHECK(timings[1].system == update);
  CHECK(timings[2].system == last);
  CHECK(timings[2].phase == fixture.engine.phaseId<oly::app::Update>());
  CHECK(timings[3].system == render);
  CHECK(timings[3].phase == fixture.engine.phaseId<oly::app::Render>());
  CHECK(timings[0].ran);
  CHECK(timings[1].ran);
  CHECK(timings[2].ran);
  CHECK(timings[3].ran);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a system skipped by the pause or by its states has ran false and 0 ns") {
  Fixture fixture;
  const auto playing = fixture.engine.declareState("playing");
  fixture.engine.addSystem(Phase::Update, "gameplay", recordName, &fixture.log);
  const auto paused = fixture.engine.addSystem(Phase::Update, "paused", recordName, &fixture.log);
  fixture.engine.setWhilePaused(paused, true);
  const auto restricted = fixture.engine.addSystem(Phase::Update, "restricted", recordName, &fixture.log);
  fixture.engine.setWhilePaused(restricted, true);
  fixture.engine.setSystemStates(restricted, {playing});
  fixture.engine.setTimingEnabled(true);
  fixture.engine.frame(0.0);
  fixture.engine.pause();

  fixture.step();

  const std::span<const SystemTiming> timings = fixture.engine.systemTimings();
  REQUIRE(timings.size() == 3);
  CHECK_FALSE(timings[0].ran);
  CHECK(timings[0].nanoseconds == 0);
  CHECK(timings[1].ran);
  CHECK_FALSE(timings[2].ran);
  CHECK(timings[2].nanoseconds == 0);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("each gameplay phase and Render has its flush entry, in phase order") {
  Fixture fixture;
  fixture.engine.setTimingEnabled(true);
  fixture.engine.frame(0.0);

  fixture.step();

  const std::span<const FlushTiming> flushes = fixture.engine.flushTimings();
  REQUIRE(flushes.size() == 4);
  CHECK(flushes[0].phase == fixture.engine.phaseId<oly::app::UpdateBefore>());
  CHECK(flushes[1].phase == fixture.engine.phaseId<oly::app::Update>());
  CHECK(flushes[2].phase == fixture.engine.phaseId<oly::app::UpdateAfter>());
  CHECK(flushes[3].phase == fixture.engine.phaseId<oly::app::Render>());
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a frame with no fixed step keeps the record of the last step") {
  Fixture fixture;
  fixture.engine.addSystem(Phase::Update, "update", recordName, &fixture.log);
  fixture.engine.setTimingEnabled(true);
  fixture.engine.frame(0.0);
  fixture.step();

  fixture.engine.frame(fixture.now);

  CHECK(fixture.log.count("update") == 1);
  CHECK(fixture.engine.systemTimings()[0].ran);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("systems added while timing is on are recorded with their names") {
  Fixture fixture;
  fixture.engine.setTimingEnabled(true);
  const std::vector<std::string> expected = addNumberedSystems(fixture, 40);
  fixture.engine.frame(0.0);

  fixture.step();

  CHECK(namesOf(fixture.engine.systemTimings()) == expected);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("turning timing off empties the record") {
  Fixture fixture;
  fixture.engine.addSystem(Phase::Update, "update", recordName, &fixture.log);
  fixture.engine.setTimingEnabled(true);
  fixture.engine.frame(0.0);
  fixture.step();

  fixture.engine.setTimingEnabled(false);

  CHECK_FALSE(fixture.engine.timingEnabled());
  CHECK(fixture.engine.systemTimings().empty());
  CHECK(fixture.engine.flushTimings().empty());
  CHECK_INVARIANTS(fixture.world);
}
