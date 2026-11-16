#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "components.hpp"
#include "oly/ecs/detail/implication_table.hpp"
#include "oly/ecs/observer.hpp"
#include "oly/ecs/world.hpp"

namespace {
  using Log = std::vector<std::string>;

  struct Mark {
    uint32_t value = 0;
  };
  struct Derived {
    float value = 5.f;
  };
  struct Chained {
    uint32_t value = 9;
  };
  struct Other {
    uint32_t value = 0;
  };

  void logMarkAdded(oly::ecs::ObserverContext& context) {
    static_cast<Log*>(context.userData())->push_back("add Mark");
  }

  void logMarkSeenByDerived(oly::ecs::ObserverContext& context) {
    const uint32_t mark = context.world().get<Mark>(context.entity()).value;
    static_cast<Log*>(context.userData())->push_back("Derived sees mark=" + std::to_string(mark));
  }
}

TEST_CASE("R-IM-2: add brings the implied component in the same transition, value-initialized") {
  oly::ecs::World world;
  world.imply<Mark, Derived>();
  auto entity = world.spawn();

  world.add(entity, Mark{1});
  CHECK_INVARIANTS(world);

  CHECK(world.get<Mark>(entity).value == 1);
  CHECK(world.get<Derived>(entity).value == 5.f);
  CHECK(oly::ecs::archetypeCount(world) == 2);
}

TEST_CASE("R-IM-2: spawn<Ts...> brings the closure") {
  oly::ecs::World world;
  world.imply<Mark, Derived>();

  auto entity = world.spawn(Mark{1}, A{2});
  CHECK_INVARIANTS(world);

  CHECK(world.get<Derived>(entity).value == 5.f);
  CHECK(world.get<A>(entity).value == 2);
}

TEST_CASE("R-IM-2: a flush brings the closure in one transition") {
  oly::ecs::World world;
  world.imply<Mark, Derived>();
  auto commandBuffer = world.commands();
  auto entity = commandBuffer.spawn();
  commandBuffer.add(entity, Mark{1});

  world.flush(commandBuffer);
  CHECK_INVARIANTS(world);

  CHECK(world.get<Mark>(entity).value == 1);
  CHECK(world.get<Derived>(entity).value == 5.f);
  CHECK(oly::ecs::archetypeCount(world) == 2);
}

TEST_CASE("R-IM-2: an implied component the entity already has keeps its value") {
  oly::ecs::World world;
  world.imply<Mark, Derived>();
  auto entity = world.spawn(Derived{7.f});

  world.add(entity, Mark{1});
  CHECK_INVARIANTS(world);

  CHECK(world.get<Derived>(entity).value == 7.f);
}

TEST_CASE("R-IM-2: an implied component the entity already has keeps its value in a flush") {
  oly::ecs::World world;
  world.imply<Mark, Derived>();
  auto entity = world.spawn(Derived{7.f});
  auto commandBuffer = world.commands();
  commandBuffer.add(entity, Mark{1});

  world.flush(commandBuffer);
  CHECK_INVARIANTS(world);

  CHECK(world.get<Derived>(entity).value == 7.f);
}

TEST_CASE("R-IM-2: implications are transitive") {
  oly::ecs::World world;
  world.imply<Mark, Derived>();
  world.imply<Derived, Chained>();
  auto entity = world.spawn();

  world.add(entity, Mark{1});
  CHECK_INVARIANTS(world);

  CHECK(world.get<Derived>(entity).value == 5.f);
  CHECK(world.get<Chained>(entity).value == 9);
}

TEST_CASE("R-IM-2: an implied component runs its on add observers after the implying one") {
  oly::ecs::World world;
  Log log;
  world.imply<Mark, Derived>();
  world.onAdd<Mark>(&logMarkAdded, &log);
  world.onAdd<Derived>(&logMarkSeenByDerived, &log);
  auto entity = world.spawn();

  world.add(entity, Mark{1});
  CHECK_INVARIANTS(world);

  CHECK(log == Log{"add Mark", "Derived sees mark=1"});
}

TEST_CASE("R-IM-2: a non-trivial implied component is constructed and destroyed once") {
  LifeCounter::alive = 0;
  {
    oly::ecs::World world;
    world.imply<Mark, LifeCounter>();
    auto entity = world.spawn(A{1});

    world.add(entity, Mark{1});
    CHECK_INVARIANTS(world);
    CHECK(LifeCounter::alive == 1);

    world.remove<Mark>(entity);
    CHECK_INVARIANTS(world);
    CHECK(LifeCounter::alive == 0);

    world.spawn(Mark{2});
    CHECK(LifeCounter::alive == 1);
  }
  CHECK(LifeCounter::alive == 0);
}

TEST_CASE("R-IM-3: removing the implying component removes what it implied") {
  oly::ecs::World world;
  world.imply<Mark, Derived>();
  world.imply<Derived, Chained>();
  auto entity = world.spawn(Mark{1}, A{2});

  world.remove<Mark>(entity);
  CHECK_INVARIANTS(world);

  CHECK_FALSE(world.has<Derived>(entity));
  CHECK_FALSE(world.has<Chained>(entity));
  CHECK(world.get<A>(entity).value == 2);
}

TEST_CASE("R-IM-3: a flush that removes the implying component removes what it implied") {
  oly::ecs::World world;
  world.imply<Mark, Derived>();
  auto entity = world.spawn(Mark{1}, A{2});
  auto commandBuffer = world.commands();
  commandBuffer.remove<Mark>(entity);

  world.flush(commandBuffer);
  CHECK_INVARIANTS(world);

  CHECK_FALSE(world.has<Derived>(entity));
}

TEST_CASE("R-IM-3: an implied component stays while another implier stays") {
  oly::ecs::World world;
  world.imply<Mark, Derived>();
  world.imply<Other, Derived>();
  auto entity = world.spawn(Mark{1}, Other{2});

  world.remove<Mark>(entity);
  CHECK_INVARIANTS(world);

  CHECK(world.has<Derived>(entity));
}

TEST_CASE("R-IM-3: an explicitly added implied component leaves with its last implier") {
  oly::ecs::World world;
  world.imply<Mark, Derived>();
  auto entity = world.spawn(Derived{7.f});
  world.add(entity, Mark{1});

  world.remove<Mark>(entity);
  CHECK_INVARIANTS(world);

  CHECK_FALSE(world.has<Derived>(entity));
}

TEST_CASE("R-IM-1: a cycle of implications closes to the same set") {
  oly::ecs::World world;
  world.imply<Mark, Derived>();
  world.imply<Derived, Mark>();

  auto fromMark = world.spawn(Mark{1});
  auto fromDerived = world.spawn(Derived{2.f});
  CHECK_INVARIANTS(world);
  world.remove<Mark>(fromMark);
  CHECK_INVARIANTS(world);

  CHECK(world.get<Mark>(fromDerived).value == 0);
  CHECK(world.get<Derived>(fromDerived).value == 2.f);
  CHECK_FALSE(world.has<Derived>(fromMark));
  CHECK(oly::ecs::archetypeCount(world) == 2);
}

TEST_CASE("with no implication registered, resolve returns the signature untouched") {
  oly::ecs::detail::ImplicationTable table;

  const oly::ecs::detail::Signature resolved = table.resolve({1, 2}, {2, 5});

  CHECK(table.empty());
  CHECK(resolved == oly::ecs::detail::Signature{2, 5});
}
