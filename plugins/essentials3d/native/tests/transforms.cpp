#include <doctest/doctest.h>

#include <cmath>
#include <numbers>
#include <string>

#include "components.hpp"
#include "fixture.hpp"
#include "oly/ecs/diagnostics.hpp"

using oly::ecs::Entity;

namespace {
  const float QuarterSine = std::sin(std::numbers::pi_v<float> / 4);
  const float QuarterCosine = std::cos(std::numbers::pi_v<float> / 4);

  // 90 degrees about z.
  Rotation quarterTurnAboutZ() {
    return Rotation{0, 0, QuarterSine, QuarterCosine};
  }
}

TEST_CASE("an entity with Position gets WorldTransform in the same spawn") {
  Fixture fixture;
  const uint32_t archetypesBefore = oly::ecs::archetypeCount(fixture.world);

  const Entity entity = fixture.world.spawn(Position{3, 4, 5});

  CHECK(fixture.world.has<WorldTransform>(entity));
  CHECK(oly::ecs::archetypeCount(fixture.world) == archetypesBefore + 1);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a root's WorldTransform copies its locals, with identity rotation and unit scale when missing") {
  Fixture fixture;
  const Entity plain = fixture.world.spawn(Position{3, 4, 5});
  const Entity full = fixture.world.spawn(Position{1, 2, 3}, quarterTurnAboutZ(), Scale{2, 3, 4});

  fixture.step();

  const WorldTransform& plainWorld = fixture.world.get<WorldTransform>(plain);
  CHECK(plainWorld.x == doctest::Approx(3));
  CHECK(plainWorld.y == doctest::Approx(4));
  CHECK(plainWorld.z == doctest::Approx(5));
  CHECK(plainWorld.rotationX == doctest::Approx(0));
  CHECK(plainWorld.rotationY == doctest::Approx(0));
  CHECK(plainWorld.rotationZ == doctest::Approx(0));
  CHECK(plainWorld.rotationW == doctest::Approx(1));
  CHECK(plainWorld.scaleX == doctest::Approx(1));
  CHECK(plainWorld.scaleY == doctest::Approx(1));
  CHECK(plainWorld.scaleZ == doctest::Approx(1));

  const WorldTransform& fullWorld = fixture.world.get<WorldTransform>(full);
  CHECK(fullWorld.z == doctest::Approx(3));
  CHECK(fullWorld.rotationZ == doctest::Approx(QuarterSine));
  CHECK(fullWorld.rotationW == doctest::Approx(QuarterCosine));
  CHECK(fullWorld.scaleX == doctest::Approx(2));
  CHECK(fullWorld.scaleY == doctest::Approx(3));
  CHECK(fullWorld.scaleZ == doctest::Approx(4));
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("quaternion composition: two 90° turns about z equal one 180° turn") {
  Fixture fixture;
  const Entity parent = fixture.world.spawn(Position{0, 0, 0}, quarterTurnAboutZ());
  const Entity child = fixture.world.spawn(Position{0, 0, 0}, quarterTurnAboutZ(), Parent{parent});

  fixture.step();

  const WorldTransform& world = fixture.world.get<WorldTransform>(child);
  CHECK(world.rotationX == doctest::Approx(0).epsilon(1e-5));
  CHECK(world.rotationY == doctest::Approx(0).epsilon(1e-5));
  CHECK(world.rotationZ == doctest::Approx(1));
  CHECK(world.rotationW == doctest::Approx(0).epsilon(1e-5));
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a child's position is rotated by the parent's quaternion") {
  Fixture fixture;
  const Entity parent = fixture.world.spawn(Position{10, 0, 0}, quarterTurnAboutZ(), Scale{2, 2, 2});
  const Entity child = fixture.world.spawn(Position{1, 0, 0}, Scale{3, 1, 1}, Parent{parent});

  fixture.step();

  const WorldTransform& world = fixture.world.get<WorldTransform>(child);
  CHECK(world.x == doctest::Approx(10));
  CHECK(world.y == doctest::Approx(2));
  CHECK(world.z == doctest::Approx(0));
  CHECK(world.scaleX == doctest::Approx(6));
  CHECK(world.scaleY == doctest::Approx(2));
  CHECK(world.scaleZ == doctest::Approx(2));
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a missing Rotation is the identity quaternion") {
  Fixture fixture;
  const Entity parent = fixture.world.spawn(Position{1, 2, 3});
  const Entity child = fixture.world.spawn(Position{10, 20, 30}, Parent{parent});

  fixture.step();

  const WorldTransform& world = fixture.world.get<WorldTransform>(child);
  CHECK(world.x == doctest::Approx(11));
  CHECK(world.y == doctest::Approx(22));
  CHECK(world.z == doctest::Approx(33));
  CHECK(world.rotationW == doctest::Approx(1));
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a child with WorldTransform and no Position is skipped by propagation") {
  Fixture fixture;
  const Entity parent = fixture.world.spawn(Position{1, 2, 3});
  const Entity child = fixture.world.spawn(WorldTransform{}, Parent{parent});

  fixture.step();

  CHECK(fixture.world.isAlive(child));
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("the propagation phase runs after Update and before UpdateAfter") {
  Fixture fixture;
  fixture.engine.setTimingEnabled(true);

  fixture.start();

  CHECK(phaseNames(fixture.engine.flushTimings()) == std::vector<std::string>{"oly::app::UpdateBefore", "oly::app::Update", "oly::essentials3d::Propagation", "oly::app::UpdateAfter", "oly::app::Render"});
  CHECK_INVARIANTS(fixture.world);
}
