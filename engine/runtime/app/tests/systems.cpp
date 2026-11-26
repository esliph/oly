#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "components.hpp"
#include "log.hpp"
#include "oly/app/engine.hpp"

using oly::app::Commands;
using oly::app::Engine;
using oly::app::Lookup;
using oly::app::Render;
using oly::app::Res;
using oly::app::Update;
using oly::app::UpdateAfter;
using oly::app::UpdateBefore;

OLY_SYSTEM(appTestNamed) {}

namespace {
  struct Shared {
    oly::ecs::Entity entity;
    bool aliveInSameSystem = true;
    bool aliveInNextPhase = false;
    float x = 0;
  };

  void spawnInUpdate(Res<Shared> shared, Commands commands) {
    shared->entity = commands.spawn(Position{1, 2});
    shared->aliveInSameSystem = commands.isAlive(shared->entity);
  }

  void observeInUpdateAfter(Res<Shared> shared, Lookup<const Position> positions, Commands commands) {
    shared->aliveInNextPhase = commands.isAlive(shared->entity);
    shared->x = positions.get(shared->entity).x;
  }

  void spawnInRender(Res<Shared> shared, Commands commands) {
    shared->entity = commands.spawn();
  }

  void addAndDestroy(Res<Shared> shared, Commands commands) {
    commands.add(shared->entity, Velocity{});
    commands.destroy(shared->entity);
  }

  struct FlushProbe {
    oly::ecs::Entity entity;
    bool aliveInSamePhase = true;
    bool aliveInNextPhase = false;
  };

  void spawnInPhase(Res<FlushProbe> probe, Commands commands) {
    probe->entity = commands.spawn(Position{1, 2});
  }

  void observeInSamePhase(Res<FlushProbe> probe, Commands commands) {
    probe->aliveInSamePhase = commands.isAlive(probe->entity);
  }

  void observeInNextPhase(Res<FlushProbe> probe, Commands commands) {
    probe->aliveInNextPhase = commands.isAlive(probe->entity);
  }

  struct Clock {
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

TEST_CASE("phases in order and, within a phase, constraint order") {
  oly::ecs::World world;
  Engine engine(world);
  Log& log = world.insertResource(Log());
  engine.addSystem<&recordName>(Render{}, "r");
  engine.addSystem<&recordName>(Update{}, "u1");
  engine.addSystem<&recordName>(UpdateBefore{}, "b");
  engine.addSystem<&recordName>(UpdateAfter{}, "a");
  engine.addSystem<&recordName>(Update{}, "u2").after("u1");
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
  Shared& shared = world.insertResource(Shared());
  engine.addSystem<&spawnInUpdate>(Update{}, "spawn");
  engine.addSystem<&observeInUpdateAfter>(UpdateAfter{}, "observe");
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
  Shared& shared = world.insertResource(Shared());
  engine.addSystem<&spawnInRender>(Render{}, "spawn");
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(world.isAlive(shared.entity));
  CHECK_INVARIANTS(world);
}

TEST_CASE("destroy and add deferred in the same phase resolve like the CommandBuffer") {
  oly::ecs::World world;
  Engine engine(world);
  Shared& shared = world.insertResource(Shared());
  shared.entity = world.spawn(Position{});
  engine.addSystem<&addAndDestroy>(Update{}, "addAndDestroy");
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(!world.isAlive(shared.entity));
  CHECK_INVARIANTS(world);
}

TEST_CASE("a constrained system runs after the systems it names, whatever the registration order") {
  oly::ecs::World world;
  Engine engine(world);
  Log& log = world.insertResource(Log());
  engine.addSystem<&recordName>(Update{}, "last").after("a").after("b");
  engine.addSystem<&recordName>(Update{}, "a");
  engine.addSystem<&recordName>(Update{}, "b").after("a");
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(log.calls == std::vector<std::string>{"a", "b", "last"});
  CHECK_INVARIANTS(world);
}

TEST_CASE("a constraint overrides the name order of its phase") {
  oly::ecs::World world;
  Engine engine(world);
  Log& log = world.insertResource(Log());
  engine.addSystem<&recordName>(Update{}, "a").after("c");
  engine.addSystem<&recordName>(Update{}, "b").after("a");
  engine.addSystem<&recordName>(Update{}, "c");
  engine.addSystem<&recordName>(UpdateAfter{}, "after");
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(log.calls == std::vector<std::string>{"c", "a", "b", "after"});
  CHECK_INVARIANTS(world);
}

TEST_CASE("a phase flushes once, after every system of the phase") {
  oly::ecs::World world;
  Engine engine(world);
  FlushProbe& probe = world.insertResource(FlushProbe());
  engine.addSystem<&spawnInPhase>(Update{}, "spawn");
  engine.addSystem<&observeInSamePhase>(Update{}, "observeSame").after("spawn");
  engine.addSystem<&observeInNextPhase>(UpdateAfter{}, "observeNext");
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(!probe.aliveInSamePhase);
  CHECK(probe.aliveInNextPhase);
  CHECK_INVARIANTS(world);
}

TEST_CASE("whilePaused and inStates apply to systems ordered by constraints") {
  Clock clock;
  const auto menu = clock.engine.declareState("menu");
  clock.engine.addSystem<&recordName>(Update{}, "plain");
  clock.engine.addSystem<&recordName>(Update{}, "whilePaused").after("plain").whilePaused();
  clock.engine.addSystem<&recordName>(Update{}, "inMenu").after("whilePaused").inStates({menu});
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
