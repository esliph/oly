#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "components.hpp"
#include "oly/ecs/observer.hpp"
#include "oly/ecs/world.hpp"

namespace {
  using Log = std::vector<std::string>;

  struct Health {
    static constexpr const char* label = "Health";
    uint64_t value;
  };
  struct Armor {
    static constexpr const char* label = "Armor";
    uint64_t value;
  };
  struct Frozen {
    static constexpr const char* label = "Frozen";
  };
  struct Link {
    oly::ecs::Entity next;
  };

  std::string line(const char* kind, const char* label, oly::ecs::Entity entity, const std::string& value) {
    return std::string(kind) + ":" + label + ":" + std::to_string(entity.index) + ":" + value;
  }

  template <class T>
  void record(const char* kind, oly::ecs::ObserverContext& context) {
    std::string value = "-";
    if constexpr (!std::is_empty_v<T>) {
      value = std::to_string(context.world().get<T>(context.entity()).value);
    }

    static_cast<Log*>(context.userData())->push_back(line(kind, T::label, context.entity(), value));
  }

  template <class T>
  void recordOnAdd(oly::ecs::ObserverContext& context) {
    record<T>("add", context);
  }

  template <class T>
  void recordOnRemove(oly::ecs::ObserverContext& context) {
    record<T>("remove", context);
  }

  void destroyNext(oly::ecs::ObserverContext& context) {
    context.commands().destroy(context.world().get<Link>(context.entity()).next);
  }

  void logAlpha(oly::ecs::ObserverContext& context) {
    static_cast<Log*>(context.userData())->push_back("alpha");
  }

  void logBravo(oly::ecs::ObserverContext& context) {
    static_cast<Log*>(context.userData())->push_back("bravo");
  }

  void logArmorOfEntity(oly::ecs::ObserverContext& context) {
    const uint64_t armor = context.world().get<Armor>(context.entity()).value;
    static_cast<Log*>(context.userData())->push_back("armor=" + std::to_string(armor));
  }
}

TEST_CASE("R-OB-1: on add reads the new value") {
  oly::ecs::World world;
  Log log;
  world.onAdd<Health>(&recordOnAdd<Health>, &log);
  auto entity = world.spawn();

  world.add(entity, Health{7});
  CHECK_INVARIANTS(world);

  CHECK(log == Log{line("add", "Health", entity, "7")});
}

TEST_CASE("R-OB-2: on remove reads the old value") {
  oly::ecs::World world;
  Log log;
  world.onRemove<Health>(&recordOnRemove<Health>, &log);
  auto entity = world.spawn(Health{7});

  world.remove<Health>(entity);
  CHECK_INVARIANTS(world);

  CHECK(log == Log{line("remove", "Health", entity, "7")});
  CHECK_FALSE(world.has<Health>(entity));
}

TEST_CASE("R-OB-3: destroy runs on remove for every observed component in ComponentId order") {
  oly::ecs::World world;
  Log log;
  world.onRemove<Health>(&recordOnRemove<Health>, &log);
  world.onRemove<Armor>(&recordOnRemove<Armor>, &log);
  auto entity = world.spawn(Health{1}, Armor{2}, A{3});

  world.destroy(entity);
  CHECK_INVARIANTS(world);

  const std::string health = line("remove", "Health", entity, "1"), armor = line("remove", "Armor", entity, "2");
  const bool healthFirst = oly::ecs::componentId<Health>() < oly::ecs::componentId<Armor>();
  CHECK(log == (healthFirst ? Log{health, armor} : Log{armor, health}));
  CHECK_FALSE(world.isAlive(entity));
}

TEST_CASE("R-OB-4: add of an existing component runs on remove with the old value, then on add with the new one") {
  oly::ecs::World world;
  Log log;
  world.onAdd<Health>(&recordOnAdd<Health>, &log);
  world.onRemove<Health>(&recordOnRemove<Health>, &log);
  auto entity = world.spawn();
  world.add(entity, Health{1});

  world.add(entity, Health{2});
  CHECK_INVARIANTS(world);

  CHECK(log == Log{line("add", "Health", entity, "1"), line("remove", "Health", entity, "1"), line("add", "Health", entity, "2")});
}

TEST_CASE("R-OB-4: add of an existing tag runs nothing") {
  oly::ecs::World world;
  Log log;
  world.onAdd<Frozen>(&recordOnAdd<Frozen>, &log);
  world.onRemove<Frozen>(&recordOnRemove<Frozen>, &log);
  auto entity = world.spawn(Frozen{});
  log.clear();

  world.add(entity, Frozen{});
  CHECK_INVARIANTS(world);

  CHECK(log.empty());
}

TEST_CASE("R-OB-5: a write through get is not observed") {
  oly::ecs::World world;
  Log log;
  world.onAdd<Health>(&recordOnAdd<Health>, &log);
  world.onRemove<Health>(&recordOnRemove<Health>, &log);
  auto entity = world.spawn(Health{1});
  log.clear();

  world.get<Health>(entity).value = 5;

  CHECK(log.empty());
}

TEST_CASE("spawn<Ts...> runs on add for each observed component after every value is in place") {
  oly::ecs::World world;
  Log log;
  world.onAdd<Health>(&logArmorOfEntity, &log);

  world.spawn(Health{1}, Armor{9});
  CHECK_INVARIANTS(world);

  CHECK(log == Log{"armor=9"});
}

TEST_CASE("R-OB-6: commands recorded by an observer are applied before the operation returns") {
  oly::ecs::World world;
  world.onRemove<Link>(&destroyNext);
  auto second = world.spawn(Health{1});
  auto first = world.spawn(Link{second});

  world.destroy(first);
  CHECK_INVARIANTS(world);

  CHECK_FALSE(world.isAlive(first));
  CHECK_FALSE(world.isAlive(second));
}

TEST_CASE("R-OB-6: a chain of recorded destroys runs until nothing is pending") {
  oly::ecs::World world;
  world.onRemove<Link>(&destroyNext);
  auto third = world.spawn(Health{3});
  auto second = world.spawn(Link{third});
  auto first = world.spawn(Link{second});

  world.destroy(first);
  CHECK_INVARIANTS(world);

  CHECK_FALSE(world.isAlive(first));
  CHECK_FALSE(world.isAlive(second));
  CHECK_FALSE(world.isAlive(third));
}

TEST_CASE("R-OB-6: a recorded command for an entity that died meanwhile is ignored") {
  oly::ecs::World world;
  world.onRemove<Link>(&destroyNext);
  auto target = world.spawn(Health{1});
  auto first = world.spawn(Link{target});
  auto commandBuffer = world.commands();
  commandBuffer.destroy(first);
  commandBuffer.destroy(target);

  world.flush(commandBuffer);
  CHECK_INVARIANTS(world);

  CHECK_FALSE(world.isAlive(first));
  CHECK_FALSE(world.isAlive(target));
}

TEST_CASE("R-OB-8: observers of the same component and kind run in registration order") {
  oly::ecs::World world;
  Log log;
  world.onAdd<Health>(&logBravo, &log);
  world.onAdd<Health>(&logAlpha, &log);

  world.spawn(Health{1});
  CHECK_INVARIANTS(world);

  CHECK(log == Log{"bravo", "alpha"});
}

TEST_CASE("removeObserver stops the observer and the flags follow") {
  oly::ecs::World world;
  Log log;
  const oly::ecs::ObserverId observer = world.onAdd<Health>(&recordOnAdd<Health>, &log);
  auto entity = world.spawn();
  world.add(entity, Health{1});
  world.remove<Health>(entity);

  world.removeObserver(observer);
  CHECK_INVARIANTS(world);
  world.add(entity, Health{2});
  CHECK_INVARIANTS(world);

  CHECK(log == Log{line("add", "Health", entity, "1")});
}

TEST_CASE("an observer registered after the edge was cached still runs") {
  oly::ecs::World world;
  Log log;
  auto entity = world.spawn();
  world.add(entity, Health{1});
  world.remove<Health>(entity);

  world.onAdd<Health>(&recordOnAdd<Health>, &log);
  CHECK_INVARIANTS(world);
  world.add(entity, Health{2});
  CHECK_INVARIANTS(world);

  CHECK(log == Log{line("add", "Health", entity, "2")});
}
