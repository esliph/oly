#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "components.hpp"
#include "log.hpp"
#include "oly/app/controls.hpp"
#include "oly/app/engine.hpp"

using oly::app::Engine;
using oly::app::PauseRequest;
using oly::app::Render;
using oly::app::Res;
using oly::app::StepInfo;
using oly::app::SystemInfo;
using oly::app::Update;
using oly::app::UpdateAfter;

namespace {
  void pauseSelf(Res<Log> log, const SystemInfo& self, const StepInfo& step, Res<PauseRequest> pauseRequest) {
    recordName(log, self, step);
    pauseRequest->pause();
  }

  void resumeSelf(Res<Log> log, const SystemInfo& self, const StepInfo& step, Res<PauseRequest> pauseRequest) {
    recordName(log, self, step);
    pauseRequest->resume();
  }

  struct Fixture {
    oly::ecs::World world;
    Engine engine{world};
    Log& log = world.insertResource(Log());
    double now = 0.0;

    void step() {
      now += 1.0 / 60;
      engine.frame(now);
    }
  };
}

TEST_CASE("paused: only whilePaused runs, the clock freezes, render continues") {
  Fixture fixture;
  fixture.engine.addSystem<&recordName>(Update{}, "u");
  fixture.engine.addSystem<&recordName>(Update{}, "p").after("u").whilePaused();
  fixture.engine.addSystem<&recordName>(Render{}, "r");
  fixture.engine.frame(0.0);
  fixture.engine.pause();

  fixture.step();
  fixture.step();

  CHECK(fixture.log.calls == std::vector<std::string>{"p", "r", "p", "r"});
  CHECK(fixture.engine.time() == 0.0);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("resume runs the gameplay systems again instead of the whilePaused ones") {
  Fixture fixture;
  fixture.engine.addSystem<&recordName>(Update{}, "u");
  fixture.engine.addSystem<&recordName>(Update{}, "p").after("u").whilePaused();
  fixture.engine.addSystem<&recordName>(Render{}, "r");
  fixture.engine.frame(0.0);
  fixture.engine.pause();
  fixture.step();
  fixture.log.calls.clear();

  fixture.engine.resume();
  fixture.step();

  CHECK(fixture.log.calls == std::vector<std::string>{"u", "r"});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("pause requested by a system does not interrupt the current step") {
  Fixture fixture;
  fixture.engine.addSystem<&pauseSelf>(Update{}, "pauser");
  fixture.engine.addSystem<&recordName>(UpdateAfter{}, "a");
  fixture.engine.frame(0.0);

  fixture.step();

  CHECK(fixture.log.calls == std::vector<std::string>{"pauser", "a"});
  CHECK(fixture.engine.paused());
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("pause requested by a system takes effect in the next step") {
  Fixture fixture;
  fixture.engine.addSystem<&pauseSelf>(Update{}, "pauser");
  fixture.engine.addSystem<&recordName>(UpdateAfter{}, "a");
  fixture.engine.frame(0.0);
  fixture.step();
  fixture.log.calls.clear();

  fixture.step();

  CHECK(fixture.log.calls.empty());
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("whilePaused system resumes through the PauseRequest") {
  Fixture fixture;
  fixture.engine.addSystem<&resumeSelf>(Update{}, "p").whilePaused();
  fixture.engine.addSystem<&recordName>(Update{}, "u").after("p");
  fixture.engine.frame(0.0);
  fixture.engine.pause();

  fixture.step();
  fixture.step();

  CHECK(fixture.log.calls == std::vector<std::string>{"p", "u"});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("hidden tab ignores frames") {
  Fixture fixture;
  fixture.engine.addSystem<&recordName>(Update{}, "u");
  fixture.engine.addSystem<&recordName>(Render{}, "r");
  fixture.engine.frame(0.0);
  fixture.engine.setHidden(true);

  fixture.engine.frame(1.0);

  CHECK(fixture.log.calls.empty());
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("first frame after the hidden tab only records the time") {
  Fixture fixture;
  fixture.engine.addSystem<&recordName>(Update{}, "u");
  fixture.engine.addSystem<&recordName>(Render{}, "r");
  fixture.engine.frame(0.0);
  fixture.engine.setHidden(true);
  fixture.engine.frame(1.0);
  fixture.engine.setHidden(false);

  fixture.engine.frame(2.0);
  fixture.engine.frame(2.0 + 1.0 / 60);

  CHECK(fixture.log.calls == std::vector<std::string>{"u", "r"});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("hidden tab does not undo the game pause") {
  Fixture fixture;
  fixture.engine.pause();

  fixture.engine.setHidden(true);
  fixture.engine.setHidden(false);

  CHECK(fixture.engine.paused());
}
