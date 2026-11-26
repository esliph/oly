#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "components.hpp"
#include "log.hpp"
#include "oly/app/engine.hpp"

using oly::app::Engine;
using oly::app::EngineConfig;
using oly::app::Render;
using oly::app::Res;
using oly::app::StepInfo;
using oly::app::Update;
using oly::app::UpdateAfter;

TEST_CASE("first frame only records the time") {
  oly::ecs::World world;
  Engine engine(world);
  Log& log = world.insertResource(Log());
  engine.addSystem<&recordName>(Update{}, "u");
  engine.addSystem<&recordName>(Render{}, "r");

  engine.frame(10.0);
  engine.frame(10.0 + 1.0 / 60);

  CHECK(log.calls == std::vector<std::string>{"u", "r"});
  CHECK_INVARIANTS(world);
}

TEST_CASE("frame at exact multiples of 1/60 runs one step per frame") {
  oly::ecs::World world;
  Engine engine(world);
  Log& log = world.insertResource(Log());
  engine.addSystem<&recordName>(Update{}, "u");
  engine.addSystem<&recordName>(Render{}, "r");

  for (int frameIndex = 0; frameIndex <= 120; ++frameIndex) {
    engine.frame(frameIndex / 60.0);
  }

  CHECK(log.count("u") == 120);
  CHECK(log.count("r") == 120);
  CHECK_INVARIANTS(world);
}

TEST_CASE("fractions accumulate across frames") {
  oly::ecs::World world;
  Engine engine(world);
  Log& log = world.insertResource(Log());
  engine.addSystem<&recordName>(Update{}, "u");
  engine.addSystem<&recordName>(Render{}, "r");
  engine.frame(0.0);

  engine.frame(0.5 / 60);
  engine.frame(1.0 / 60);

  CHECK(log.calls == std::vector<std::string>{"r", "u", "r"});
  CHECK_INVARIANTS(world);
}

TEST_CASE("frame with a large jump runs at most maxStepsPerFrame steps") {
  oly::ecs::World world;
  Engine engine(world, EngineConfig{.fixedHz = 60.0, .maxStepsPerFrame = 5});
  Log& log = world.insertResource(Log());
  engine.addSystem<&recordName>(Update{}, "u");
  engine.frame(0.0);

  engine.frame(10.0);

  CHECK(log.count("u") == 5);
  CHECK_INVARIANTS(world);
}

TEST_CASE("steps beyond maxStepsPerFrame are discarded") {
  oly::ecs::World world;
  Engine engine(world, EngineConfig{.fixedHz = 60.0, .maxStepsPerFrame = 5});
  Log& log = world.insertResource(Log());
  engine.addSystem<&recordName>(Update{}, "u");
  engine.frame(0.0);
  engine.frame(10.0);

  engine.frame(10.0 + 1.0 / 60);

  CHECK(log.count("u") == 6);
  CHECK_INVARIANTS(world);
}

TEST_CASE("render receives the interpolation") {
  oly::ecs::World world;
  Engine engine(world);
  Log& log = world.insertResource(Log());
  engine.addSystem<&recordName>(Render{}, "r");
  engine.frame(0.0);

  engine.frame(1.5 / 60);

  REQUIRE(!log.interpolations.empty());
  CHECK(log.interpolations.back() == doctest::Approx(0.5).epsilon(1e-6));
  CHECK_INVARIANTS(world);
}

namespace {
  struct FixedDelta {
    float value = 0;
  };

  void checkFixedDelta(Res<FixedDelta> fixedDelta, const StepInfo& step) {
    fixedDelta->value = step.fixedDeltaTime;
  }
}

TEST_CASE("time is the start of the step and fixedDeltaTime comes from fixedHz") {
  oly::ecs::World world;
  Engine engine(world, EngineConfig{.fixedHz = 50.0});
  Log& log = world.insertResource(Log());
  FixedDelta& fixedDelta = world.insertResource(FixedDelta());
  engine.addSystem<&recordName>(Update{}, "u");
  engine.addSystem<&checkFixedDelta>(UpdateAfter{}, "d");
  engine.frame(0.0);

  engine.frame(0.06);

  REQUIRE(log.times.size() == 3);
  CHECK(log.times[0] == doctest::Approx(0.0));
  CHECK(log.times[1] == doctest::Approx(0.02));
  CHECK(log.times[2] == doctest::Approx(0.04));
  CHECK(fixedDelta.value == 0.02f);
  CHECK(engine.time() == doctest::Approx(0.06));
  CHECK_INVARIANTS(world);
}
