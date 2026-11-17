#include <doctest/doctest.h>

#include <numbers>
#include <string>

#include "components.hpp"
#include "fixture.hpp"
#include "oly/app/context.hpp"
#include "oly/app/system.hpp"
#include "oly/ecs/diagnostics.hpp"

using oly::app::OrderBand;
using oly::app::Phase;
using oly::app::SystemContext;
using oly::ecs::Entity;

namespace {
  constexpr float HalfTurn = std::numbers::pi_v<float> / 2;

  void moveRightByOne(SystemContext& context) {
    context.query<Position>().each([](Position& position) { position.x += 1; });
  }

  struct Probe {
    Entity entity;
    float worldXSeen = -1;
    bool moved = false;
  };

  void readWorldThenMove(SystemContext& context) {
    Probe& probe = *static_cast<Probe*>(context.userData());
    probe.worldXSeen = context.get<WorldTransform>(probe.entity).x;

    if (!probe.moved) {
      context.get<Position>(probe.entity).x = 7;
      probe.moved = true;
    }
  }

  struct Spawner {
    Entity child;
    Entity parent;
    bool spawned = false;
  };

  void spawnChildOfParent(SystemContext& context) {
    Spawner& spawner = *static_cast<Spawner*>(context.userData());

    if (!spawner.spawned) {
      spawner.child = context.spawn(Position{1, 0}, Parent{spawner.parent});
      spawner.spawned = true;
    }
  }
}

TEST_CASE("an entity with Position gets WorldTransform in the same spawn") {
  Fixture fixture;
  const uint32_t archetypesBefore = oly::ecs::archetypeCount(fixture.world);

  const Entity entity = fixture.world.spawn(Position{3, 4});

  CHECK(fixture.world.has<WorldTransform>(entity));
  CHECK(oly::ecs::archetypeCount(fixture.world) == archetypesBefore + 1);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a root's WorldTransform copies its locals, with identity rotation and unit scale when missing") {
  Fixture fixture;
  const Entity plain = fixture.world.spawn(Position{3, 4});
  const Entity full = fixture.world.spawn(Position{1, 2}, Rotation{0.5f}, Scale{2, 3});

  fixture.step();

  const WorldTransform& plainWorld = fixture.world.get<WorldTransform>(plain);
  CHECK(plainWorld.x == doctest::Approx(3));
  CHECK(plainWorld.y == doctest::Approx(4));
  CHECK(plainWorld.angle == doctest::Approx(0));
  CHECK(plainWorld.scaleX == doctest::Approx(1));
  CHECK(plainWorld.scaleY == doctest::Approx(1));

  const WorldTransform& fullWorld = fixture.world.get<WorldTransform>(full);
  CHECK(fullWorld.x == doctest::Approx(1));
  CHECK(fullWorld.y == doctest::Approx(2));
  CHECK(fullWorld.angle == doctest::Approx(0.5));
  CHECK(fullWorld.scaleX == doctest::Approx(2));
  CHECK(fullWorld.scaleY == doctest::Approx(3));
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a child's WorldTransform composes translate-rotate-scale") {
  Fixture fixture;
  const Entity parent = fixture.world.spawn(Position{10, 0}, Rotation{HalfTurn}, Scale{2, 2});
  const Entity child = fixture.world.spawn(Position{1, 0}, Rotation{0.5f}, Scale{3, 1}, Parent{parent});

  fixture.step();

  const WorldTransform& world = fixture.world.get<WorldTransform>(child);
  CHECK(world.x == doctest::Approx(10));
  CHECK(world.y == doctest::Approx(2));
  CHECK(world.angle == doctest::Approx(HalfTurn + 0.5f));
  CHECK(world.scaleX == doctest::Approx(6));
  CHECK(world.scaleY == doctest::Approx(2));
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("propagation runs after the Default systems of update, so updateAfter reads the new world values") {
  Fixture fixture;
  fixture.engine.addSystem(Phase::Update, "moveRight", moveRightByOne, nullptr, OrderBand::Default);
  Probe probe;
  probe.entity = fixture.world.spawn(Position{0, 0});
  probe.moved = true;
  fixture.engine.addSystem(Phase::UpdateAfter, "read", readWorldThenMove, &probe);

  fixture.step();

  CHECK(probe.worldXSeen == doctest::Approx(1));
}

TEST_CASE("a Position change in updateAfter reaches WorldTransform in the next step") {
  Fixture fixture;
  Probe probe;
  probe.entity = fixture.world.spawn(Position{1, 0});
  fixture.engine.addSystem(Phase::UpdateAfter, "readAndMove", readWorldThenMove, &probe);

  fixture.step();
  CHECK(probe.worldXSeen == doctest::Approx(1));
  CHECK(fixture.world.get<WorldTransform>(probe.entity).x == doctest::Approx(1));

  fixture.step();
  CHECK(fixture.world.get<WorldTransform>(probe.entity).x == doctest::Approx(7));
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a child spawned by a CommandBuffer during update has its WorldTransform in updateAfter") {
  Fixture fixture;
  Spawner spawner;
  spawner.parent = fixture.world.spawn(Position{10, 5});
  fixture.engine.addSystem(Phase::Update, "spawnChild", spawnChildOfParent, &spawner);
  fixture.engine.addSystem(Phase::UpdateAfter, "read", [](SystemContext& context) {
    Spawner& spawned = *static_cast<Spawner*>(context.userData());
    const WorldTransform& world = context.get<WorldTransform>(spawned.child);
    CHECK(world.x == doctest::Approx(11));
    CHECK(world.y == doctest::Approx(5));
  }, &spawner);

  fixture.step();

  CHECK(spawner.spawned);
  std::string why;
  CHECK_MESSAGE(fixture.plugin.checkHierarchy(fixture.world, &why), why);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a parent without Position counts as the identity") {
  Fixture fixture;
  const Entity group = fixture.world.spawn();
  const Entity child = fixture.world.spawn(Position{4, 5}, Rotation{0.25f}, Scale{2, 2}, Parent{group});

  fixture.step();

  const WorldTransform& world = fixture.world.get<WorldTransform>(child);
  CHECK(world.x == doctest::Approx(4));
  CHECK(world.y == doctest::Approx(5));
  CHECK(world.angle == doctest::Approx(0.25));
  CHECK(world.scaleX == doctest::Approx(2));
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("non-uniform scale under rotation keeps no shear") {
  Fixture fixture;
  const Entity parent = fixture.world.spawn(Position{0, 0}, Rotation{HalfTurn}, Scale{4, 1});
  const Entity child = fixture.world.spawn(Position{0, 0}, Scale{1, 2}, Parent{parent});

  fixture.step();

  const WorldTransform& world = fixture.world.get<WorldTransform>(child);
  CHECK(world.scaleX == doctest::Approx(4));
  CHECK(world.scaleY == doctest::Approx(2));
  CHECK(world.angle == doctest::Approx(HalfTurn));
  CHECK_INVARIANTS(fixture.world);
}
