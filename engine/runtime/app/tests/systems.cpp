#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "components.hpp"
#include "log.hpp"
#include "oly/app/engine.hpp"

using oly::app::Engine;
using oly::app::OrderBand;
using oly::app::Phase;
using oly::app::SystemContext;

OLY_SYSTEM(appTestNamed) {
  recordName(context);
}

namespace {
  struct Shared {
    oly::ecs::Entity entity;
    bool aliveInSameSystem = true;
    bool aliveInNextPhase = false;
    float x = 0;
  };

  void spawnInUpdate(SystemContext& context) {
    Shared& shared = *static_cast<Shared*>(context.userData());
    shared.entity = context.spawn(Position{1, 2});
    shared.aliveInSameSystem = context.isAlive(shared.entity);
  }

  void observeInUpdateAfter(SystemContext& context) {
    Shared& shared = *static_cast<Shared*>(context.userData());
    shared.aliveInNextPhase = context.isAlive(shared.entity);
    shared.x = context.get<Position>(shared.entity).x;
  }

  void spawnInRender(SystemContext& context) {
    static_cast<Shared*>(context.userData())->entity = context.spawn();
  }

  void addAndDestroy(SystemContext& context) {
    Shared& shared = *static_cast<Shared*>(context.userData());
    context.add(shared.entity, Velocity{});
    context.destroy(shared.entity);
  }

  struct BandProbe {
    oly::ecs::Entity entity;
    bool aliveInLastBand = true;
    bool aliveInNextPhase = false;
  };

  void spawnInDefaultBand(SystemContext& context) {
    static_cast<BandProbe*>(context.userData())->entity = context.spawn(Position{1, 2});
  }

  void observeInLastBand(SystemContext& context) {
    BandProbe& probe = *static_cast<BandProbe*>(context.userData());
    probe.aliveInLastBand = context.isAlive(probe.entity);
  }

  void observeAfterLastBand(SystemContext& context) {
    BandProbe& probe = *static_cast<BandProbe*>(context.userData());
    probe.aliveInNextPhase = context.isAlive(probe.entity);
  }

  struct Clock {
    oly::ecs::World world;
    Engine engine{world};
    Log log;
    double now = 0.0;

    void step() {
      now += 1.0 / 60;
      engine.frame(now);
    }
  };
}

TEST_CASE("phases in order and, within a phase, registration order") {
  oly::ecs::World world;
  Engine engine(world);
  Log log;
  engine.addSystem(Phase::Render, "r", recordName, &log);
  engine.addSystem(Phase::Update, "u1", recordName, &log);
  engine.addSystem(Phase::UpdateBefore, "b", recordName, &log);
  engine.addSystem(Phase::UpdateAfter, "a", recordName, &log);
  engine.addSystem(Phase::Update, "u2", recordName, &log);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(log.calls == std::vector<std::string>{"b", "u1", "u2", "a", "r"});
  CHECK_INVARIANTS(world);
}

TEST_CASE("OLY_SYSTEM registers by name") {
  CHECK(oly::app::findNamedSystem("appTestNamed") != nullptr);
  CHECK(oly::app::findNamedSystem("doesNotExist") == nullptr);
}

TEST_CASE("deferred spawn: alive only in the next phase") {
  oly::ecs::World world;
  Engine engine(world);
  Shared shared;
  engine.addSystem(Phase::Update, "spawn", spawnInUpdate, &shared);
  engine.addSystem(Phase::UpdateAfter, "observe", observeInUpdateAfter, &shared);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(!shared.aliveInSameSystem);
  CHECK(shared.aliveInNextPhase);
  CHECK(shared.x == 1);
  CHECK_INVARIANTS(world);
}

TEST_CASE("render commands applied at the end of the frame") {
  oly::ecs::World world;
  Engine engine(world);
  Shared shared;
  engine.addSystem(Phase::Render, "spawn", spawnInRender, &shared);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(world.isAlive(shared.entity));
  CHECK_INVARIANTS(world);
}

TEST_CASE("destroy and add deferred in the same phase resolve like the CommandBuffer") {
  oly::ecs::World world;
  Engine engine(world);
  Shared shared;
  shared.entity = world.spawn(Position{});
  engine.addSystem(Phase::Update, "addAndDestroy", addAndDestroy, &shared);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(!world.isAlive(shared.entity));
  CHECK_INVARIANTS(world);
}

TEST_CASE("a Last system runs after every Default system of its phase, whatever the registration order") {
  oly::ecs::World world;
  Engine engine(world);
  Log log;
  engine.addSystem(Phase::Update, "last1", recordName, &log, OrderBand::Last);
  engine.addSystem(Phase::Update, "a", recordName, &log);
  engine.addSystem(Phase::Update, "b", recordName, &log);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(log.calls == std::vector<std::string>{"a", "b", "last1"});
  CHECK_INVARIANTS(world);
}

TEST_CASE("Last systems keep their registration order") {
  oly::ecs::World world;
  Engine engine(world);
  Log log;
  engine.addSystem(Phase::Update, "last2", recordName, &log, OrderBand::Last);
  engine.addSystem(Phase::Update, "a", recordName, &log);
  engine.addSystem(Phase::Update, "last1", recordName, &log, OrderBand::Last);
  engine.addSystem(Phase::UpdateAfter, "after", recordName, &log);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(log.calls == std::vector<std::string>{"a", "last2", "last1", "after"});
  CHECK_INVARIANTS(world);
}

TEST_CASE("a phase with Last systems flushes once, after the Last band") {
  oly::ecs::World world;
  Engine engine(world);
  BandProbe probe;
  engine.addSystem(Phase::Update, "observeLast", observeInLastBand, &probe, OrderBand::Last);
  engine.addSystem(Phase::Update, "spawn", spawnInDefaultBand, &probe);
  engine.addSystem(Phase::UpdateAfter, "observeAfter", observeAfterLastBand, &probe);
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(!probe.aliveInLastBand);
  CHECK(probe.aliveInNextPhase);
  CHECK_INVARIANTS(world);
}

TEST_CASE("setWhilePaused and setSystemStates apply to a Last system") {
  Clock clock;
  const auto menu = clock.engine.declareState("menu");
  clock.engine.addSystem(Phase::Update, "plain", recordName, &clock.log, OrderBand::Last);
  const auto whilePaused = clock.engine.addSystem(Phase::Update, "whilePaused", recordName, &clock.log, OrderBand::Last);
  clock.engine.setWhilePaused(whilePaused, true);
  const auto inMenu = clock.engine.addSystem(Phase::Update, "inMenu", recordName, &clock.log, OrderBand::Last);
  clock.engine.setSystemStates(inMenu, {menu});
  clock.engine.frame(0.0);
  clock.engine.requestState(menu);

  clock.step();
  clock.engine.pause();
  clock.step();
  clock.engine.resume();
  clock.step();

  CHECK(clock.log.calls == std::vector<std::string>{"plain", "whilePaused", "plain", "inMenu"});
  CHECK_INVARIANTS(clock.world);
}
