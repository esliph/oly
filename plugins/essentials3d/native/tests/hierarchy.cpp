#include <doctest/doctest.h>

#include <numbers>
#include <string>
#include <vector>

#include "components.hpp"
#include "fixture.hpp"
#include "oly/app/state_context.hpp"
#include "oly/ecs/command_buffer.hpp"
#include "oly/ecs/diagnostics.hpp"

using oly::ecs::Entity;
using oly::essentials3d::detail::HasChildren;

#define CHECK_HIERARCHY(fixture)                                                                    \
  do {                                                                                              \
    std::string hierarchyWhy_;                                                                      \
    CHECK_MESSAGE((fixture).plugin.checkHierarchy((fixture).world, &hierarchyWhy_), hierarchyWhy_); \
    CHECK_INVARIANTS((fixture).world);                                                              \
  } while (0)

namespace {
  struct Family {
    Entity grandparent;
    Entity parent;
    Entity child;
  };

  void spawnScopedParent(oly::app::StateContext& context) {
    *static_cast<Entity*>(context.userData()) = context.spawn(Position{0, 0, 0});
  }

  Family spawnFamily(Fixture& fixture) {
    Family family;
    family.grandparent = fixture.world.spawn(Position{0, 0, 0});
    family.parent = fixture.world.spawn(Position{1, 0, 0}, Parent{family.grandparent});
    family.child = fixture.world.spawn(Position{2, 0, 0}, Parent{family.parent});
    return family;
  }
}

TEST_CASE("re-parenting keeps the local values and moves the entity in the world") {
  Fixture fixture;
  const Entity first = fixture.world.spawn(Position{10, 0, 0});
  const Entity second = fixture.world.spawn(Position{100, 0, 0});
  const Entity child = fixture.world.spawn(Position{1, 2, 0}, Parent{first});
  fixture.step();
  CHECK(fixture.world.get<WorldTransform>(child).x == doctest::Approx(11));

  fixture.world.add(child, Parent{second});
  fixture.step();

  CHECK(fixture.world.get<Position>(child).x == doctest::Approx(1));
  CHECK(fixture.world.get<Position>(child).y == doctest::Approx(2));
  CHECK(fixture.world.get<WorldTransform>(child).x == doctest::Approx(101));
  CHECK_FALSE(fixture.world.has<HasChildren>(first));
  CHECK(fixture.world.has<HasChildren>(second));
  CHECK_HIERARCHY(fixture);
}

TEST_CASE("removing Parent makes the entity a root with its local values") {
  Fixture fixture;
  const Entity parent = fixture.world.spawn(Position{10, 0, 0});
  const Entity child = fixture.world.spawn(Position{1, 2, 0}, Parent{parent});
  fixture.step();

  fixture.world.remove<Parent>(child);
  fixture.step();

  CHECK(fixture.world.get<Position>(child).x == doctest::Approx(1));
  CHECK(fixture.world.get<WorldTransform>(child).x == doctest::Approx(1));
  CHECK(fixture.world.get<WorldTransform>(child).y == doctest::Approx(2));
  CHECK_HIERARCHY(fixture);
}

TEST_CASE("the last child leaving removes the parent's marker") {
  Fixture fixture;
  const Entity parent = fixture.world.spawn(Position{0, 0, 0});
  const Entity first = fixture.world.spawn(Position{0, 0, 0}, Parent{parent});
  const Entity second = fixture.world.spawn(Position{0, 0, 0}, Parent{parent});
  CHECK(fixture.world.has<HasChildren>(parent));

  fixture.world.remove<Parent>(first);
  CHECK(fixture.world.has<HasChildren>(parent));
  fixture.world.remove<Parent>(second);

  CHECK_FALSE(fixture.world.has<HasChildren>(parent));
  CHECK_HIERARCHY(fixture);
}

TEST_CASE("destroying a parent destroys its descendants (direct destroy)") {
  Fixture fixture;
  const Family family = spawnFamily(fixture);
  const Entity sibling = fixture.world.spawn(Position{0, 0, 0}, Parent{family.grandparent});

  fixture.world.destroy(family.grandparent);

  CHECK_FALSE(fixture.world.isAlive(family.parent));
  CHECK_FALSE(fixture.world.isAlive(family.child));
  CHECK_FALSE(fixture.world.isAlive(sibling));
  CHECK_HIERARCHY(fixture);
}

TEST_CASE("destroying a parent destroys its descendants (CommandBuffer)") {
  Fixture fixture;
  const Family family = spawnFamily(fixture);
  oly::ecs::CommandBuffer commands = fixture.world.commands();

  commands.destroy(family.parent);
  fixture.world.flush(commands);

  CHECK(fixture.world.isAlive(family.grandparent));
  CHECK_FALSE(fixture.world.isAlive(family.parent));
  CHECK_FALSE(fixture.world.isAlive(family.child));
  CHECK_FALSE(fixture.world.has<HasChildren>(family.grandparent));
  CHECK_HIERARCHY(fixture);
}

TEST_CASE("destroying a parent destroys its descendants (state scope cleanup)") {
  Fixture fixture;
  const auto playing = fixture.engine.declareState("playing");
  const auto menu = fixture.engine.declareState("menu");
  Entity parent = oly::ecs::NullEntity;
  fixture.engine.onEnter(playing, "spawnParent", spawnScopedParent, &parent);
  fixture.engine.requestState(playing);
  fixture.step();
  const Entity child = fixture.world.spawn(Position{1, 0, 0}, Parent{parent});
  const Entity grandchild = fixture.world.spawn(Position{1, 0, 0}, Parent{child});

  fixture.engine.requestState(menu);
  fixture.step();

  CHECK_FALSE(fixture.world.isAlive(parent));
  CHECK_FALSE(fixture.world.isAlive(child));
  CHECK_FALSE(fixture.world.isAlive(grandchild));
  CHECK_HIERARCHY(fixture);
}

TEST_CASE("a parent spawned in the same buffer as its child is valid") {
  Fixture fixture;
  oly::ecs::CommandBuffer commands = fixture.world.commands();
  const Entity parent = commands.spawn();
  commands.add(parent, Position{5, 0, 0});
  const Entity child = commands.spawn();
  commands.add(child, Position{1, 0, 0});
  commands.add(child, Parent{parent});

  fixture.world.flush(commands);
  fixture.step();

  CHECK(fixture.world.get<WorldTransform>(child).x == doctest::Approx(6));
  CHECK_HIERARCHY(fixture);
}

TEST_CASE("checkHierarchy reports a Parent written through get") {
  Fixture fixture;
  const Entity first = fixture.world.spawn(Position{0, 0, 0});
  const Entity second = fixture.world.spawn(Position{0, 0, 0});
  const Entity child = fixture.world.spawn(Position{0, 0, 0}, Parent{first});

  fixture.world.get<Parent>(child).entity = second;

  CHECK_FALSE(fixture.plugin.checkHierarchy(fixture.world));
}

TEST_CASE("after the Engine is destroyed, the World keeps working without the plugin's observers") {
  oly::ecs::World world;
  Entity parent;
  {
    oly::app::Engine engine(world);
    engine.installPlugin<oly::essentials3d::Plugin>();
    parent = world.spawn(Position{0, 0, 0});
    world.spawn(Position{0, 0, 0}, Parent{parent});
  }

  const Entity orphan = world.spawn(Position{0, 0, 0}, Parent{parent});
  world.destroy(parent);

  CHECK(world.isAlive(orphan));
  CHECK_INVARIANTS(world);
}

TEST_CASE("removing Parent from a child whose Parent was written through get leaves the index of the other parent intact") {
  Fixture fixture;
  const Entity first = fixture.world.spawn(Position{0, 0, 0});
  const Entity second = fixture.world.spawn(Position{0, 0, 0});
  const Entity wrongChild = fixture.world.spawn(Position{0, 0, 0}, Parent{first});
  const Entity rightChild = fixture.world.spawn(Position{0, 0, 0}, Parent{second});
  fixture.world.get<Parent>(wrongChild).entity = second;

  fixture.world.remove<Parent>(wrongChild);

  CHECK(fixture.world.has<HasChildren>(second));
  CHECK(fixture.world.get<Parent>(rightChild).entity == second);
}

TEST_CASE("removing children in any order keeps the index consistent") {
  Fixture fixture;
  const Entity parent = fixture.world.spawn(Position{0, 0, 0});
  std::vector<Entity> children;

  for (uint32_t index = 0; index < 8; ++index) {
    children.push_back(fixture.world.spawn(Position{static_cast<float>(index), 0, 0}, Parent{parent}));
  }

  for (const uint32_t index : {3u, 0u, 7u, 4u}) {
    fixture.world.remove<Parent>(children[index]);
    CHECK_HIERARCHY(fixture);
  }
  fixture.world.destroy(children[1]);
  CHECK_HIERARCHY(fixture);
  fixture.step();

  CHECK(fixture.world.get<WorldTransform>(children[2]).x == doctest::Approx(2));
  CHECK(fixture.world.get<WorldTransform>(children[5]).x == doctest::Approx(5));
  CHECK(fixture.world.get<WorldTransform>(children[6]).x == doctest::Approx(6));
}

TEST_CASE("re-parenting inside a CommandBuffer moves the child and fixes both parents' markers") {
  Fixture fixture;
  const Entity first = fixture.world.spawn(Position{10, 0, 0});
  const Entity second = fixture.world.spawn(Position{100, 0, 0});
  const Entity child = fixture.world.spawn(Position{1, 0, 0}, Parent{first});
  oly::ecs::CommandBuffer commands = fixture.world.commands();

  commands.add(child, Parent{second});
  fixture.world.flush(commands);
  fixture.step();

  CHECK_FALSE(fixture.world.has<HasChildren>(first));
  CHECK(fixture.world.has<HasChildren>(second));
  CHECK(fixture.world.get<WorldTransform>(child).x == doctest::Approx(101));
  CHECK_HIERARCHY(fixture);
}

TEST_CASE("two children added to the same parent in one flush are both indexed") {
  Fixture fixture;
  const Entity parent = fixture.world.spawn(Position{5, 0, 0});
  const Entity first = fixture.world.spawn(Position{1, 0, 0});
  const Entity second = fixture.world.spawn(Position{2, 0, 0});
  oly::ecs::CommandBuffer commands = fixture.world.commands();

  commands.add(first, Parent{parent});
  commands.add(second, Parent{parent});
  fixture.world.flush(commands);
  fixture.step();

  CHECK(fixture.world.get<WorldTransform>(first).x == doctest::Approx(6));
  CHECK(fixture.world.get<WorldTransform>(second).x == doctest::Approx(7));
  CHECK_HIERARCHY(fixture);
}
