#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

#include "components.hpp"
#include "oly/ecs/resource.hpp"
#include "oly/ecs/world.hpp"

namespace {
  struct Score {
    int value;
  };

  struct Missing {
    int value;
  };

  template <int Index>
  struct Filler {
    int value;
  };

  struct Destruction {
    std::vector<std::string>* order;
    const char* name;

    Destruction(std::vector<std::string>* orderValue, const char* nameValue)
      : order(orderValue),
        name(nameValue) {}

    Destruction(Destruction&& other) noexcept
      : order(std::exchange(other.order, nullptr)),
        name(other.name) {}

    ~Destruction() {
      if (order) {
        order->push_back(name);
      }
    }
  };

  struct First : Destruction {
    using Destruction::Destruction;
  };

  struct Second : Destruction {
    using Destruction::Destruction;
  };

  template <int... Indices>
  void insertFillers(oly::ecs::World& world, std::integer_sequence<int, Indices...>) {
    (world.insertResource(Filler<Indices>{Indices}), ...);
  }
}

TEST_CASE("an inserted resource is reachable by type and by id") {
  oly::ecs::World world;
  Score& inserted = world.insertResource(Score{7});

  Score& reached = world.resource<Score>();

  CHECK(&reached == &inserted);
  CHECK(reached.value == 7);
  CHECK(world.tryResource<const Score>() == &inserted);
  CHECK(world.tryResourceById(oly::ecs::resourceId<Score>()) == &inserted);
  CHECK_INVARIANTS(world);
}

TEST_CASE("a resource keeps its address when other resources are inserted") {
  oly::ecs::World world;
  Score& inserted = world.insertResource(Score{7});

  insertFillers(world, std::make_integer_sequence<int, 20>{});

  CHECK(&world.resource<Score>() == &inserted);
  CHECK(world.resource<Filler<19>>().value == 19);
  CHECK_INVARIANTS(world);
}

TEST_CASE("a missing resource is nullptr") {
  oly::ecs::World world;
  world.insertResource(Score{7});

  CHECK(world.tryResource<Missing>() == nullptr);
  CHECK(world.tryResourceById(oly::ecs::resourceId<Missing>()) == nullptr);
  CHECK(world.tryResourceById(oly::ecs::registeredResourceCount() + 1000) == nullptr);
  CHECK_INVARIANTS(world);
}

TEST_CASE("resources are destroyed in reverse insertion order") {
  std::vector<std::string> order;

  {
    oly::ecs::World world;
    world.insertResource(First{&order, "first"});
    world.insertResource(Second{&order, "second"});
  }

  CHECK(order == std::vector<std::string>{"second", "first"});
}

TEST_CASE("resourceId ignores const and reference") {
  CHECK(oly::ecs::resourceId<const Score&>() == oly::ecs::resourceId<Score>());
}

TEST_CASE("resourceName is the type name") {
  const std::string_view name = oly::ecs::resourceName(oly::ecs::resourceId<Score>());

  CHECK(name.find("Score") != std::string_view::npos);
}
