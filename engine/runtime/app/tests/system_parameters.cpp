#include "oly/app/system_parameters.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

#include "components.hpp"
#include "oly/app/access.hpp"
#include "oly/app/detail/event_queues.hpp"
#include "oly/app/engine.hpp"
#include "oly/app/system_descriptor.hpp"

using oly::app::Access;
using oly::app::Commands;
using oly::app::Events;
using oly::app::Lookup;
using oly::app::Query;
using oly::app::Res;
using oly::app::StepInfo;
using oly::app::SystemInfo;
using oly::app::Tasks;

namespace {
  struct Position {
    float x = 0, y = 0;
  };

  struct Velocity {
    float x = 0, y = 0;
  };

  struct Health {
    int value = 0;
  };

  struct Enemy {};

  struct Score {
    int value = 0;
  };

  struct Config {
    int value = 0;
  };

  struct Seen {
    size_t entities = 0;
    int score = 0;
    double time = 0;
  };

  struct Ping {
    int value = 0;
  };

  std::vector<uint32_t> sorted(std::initializer_list<uint32_t> ids) {
    std::vector<uint32_t> result(ids);
    std::sort(result.begin(), result.end());

    return result;
  }

  void sample(Query<Position, const Velocity, oly::ecs::With<Enemy>>, Lookup<const Health>, Res<Score>, Res<const Config>, Commands, Events<Ping>, Tasks, const StepInfo&, const SystemInfo&) {}

  void exclusive(oly::ecs::World&) {}

  void readThenWrite(Query<const Position>, Lookup<Position>) {}

  void filtered(Query<const Position, oly::ecs::With<Enemy>>) {}

  void first() {}

  void second() {}

  void observe(Query<const Position> positions, Res<const Score> score, Res<Seen> seen, const StepInfo& step) {
    size_t count = 0;
    positions.each([&](const Position&) { ++count; });

    seen->entities = count;
    seen->score = score->value;
    seen->time = step.time;
  }

  void spawning(Commands commands) {
    commands.spawn(Position{1, 2});
  }

  void pinging(Events<Ping> events) {
    events.emit(Ping{3});
  }

  void needy(Res<Score>, Events<Ping>) {}

  struct Harness {
    oly::ecs::World world;
    oly::app::Engine engine{world};
    oly::app::detail::EventQueues queues;
    oly::app::detail::EngineControls controls{};
    std::vector<std::string> errors;
    oly::ecs::CommandBuffer commands = world.commands();
    StepInfo step{2.5, 0.01f, 0.0f, oly::app::NoState, false};
    SystemInfo system{0, "sample", 0, oly::app::ScheduleKind::FixedStep};
    oly::app::detail::SystemSlots slots{};

    template <auto Function>
    void resolve(std::string_view name) {
      oly::app::detail::ResolveContext context{world, controls, queues, engine.schedule(), name, errors};
      oly::app::systemDescriptor<Function>().resolve(context, slots);
    }

    template <auto Function>
    void run(bool spawnsScoped) {
      oly::app::detail::RunContext context{world, commands, queues, step, system, spawnsScoped};
      oly::app::systemDescriptor<Function>().run(slots, context);
    }
  };
}

TEST_CASE("a system declares the access of its parameters") {
  Access access;

  oly::app::systemDescriptor<&sample>().declare(access);

  CHECK(access.componentWrites == std::vector{oly::ecs::componentId<Position>()});
  CHECK(access.componentReads == sorted({oly::ecs::componentId<Velocity>(), oly::ecs::componentId<Health>()}));
  CHECK(access.resourceWrites == std::vector{oly::ecs::resourceId<Score>()});
  CHECK(access.resourceReads == std::vector{oly::ecs::resourceId<Config>()});
  CHECK_FALSE(access.exclusive);
}

TEST_CASE("a World parameter makes the system exclusive") {
  Access access;

  oly::app::systemDescriptor<&exclusive>().declare(access);

  CHECK(access.exclusive);
}

TEST_CASE("a write of a type supersedes its read") {
  Access access;

  oly::app::systemDescriptor<&readThenWrite>().declare(access);

  CHECK(access.componentWrites == std::vector{oly::ecs::componentId<Position>()});
  CHECK(access.componentReads.empty());
}

TEST_CASE("a filter declares nothing") {
  Access access;

  oly::app::systemDescriptor<&filtered>().declare(access);

  const auto enemy = oly::ecs::componentId<Enemy>();
  CHECK(std::count(access.componentReads.begin(), access.componentReads.end(), enemy) == 0);
  CHECK(std::count(access.componentWrites.begin(), access.componentWrites.end(), enemy) == 0);
}

TEST_CASE("two functions have two keys and one function one key") {
  CHECK(oly::app::systemFunctionKey<&first>() != oly::app::systemFunctionKey<&second>());
  CHECK(oly::app::systemFunctionKey<&first>() == oly::app::systemFunctionKey<&first>());
  CHECK(oly::app::systemDescriptor<&first>().key == oly::app::systemFunctionKey<&first>());
  CHECK(&oly::app::systemDescriptor<&first>() == &oly::app::systemDescriptor<&first>());
}

TEST_CASE("run hands the system the entities of its query, the inserted resource and the step") {
  Harness harness;
  harness.world.spawn(Position{1, 1});
  harness.world.spawn(Position{2, 2});
  harness.world.insertResource(Score{42});
  Seen& seen = harness.world.insertResource(Seen{});

  harness.resolve<&observe>("observe");
  harness.run<&observe>(false);

  CHECK(harness.errors.empty());
  CHECK(seen.entities == 2);
  CHECK(seen.score == 42);
  CHECK(seen.time == 2.5);
  CHECK_INVARIANTS(harness.world);
}

TEST_CASE("Commands defer until the flush and spawn scoped while a state is active") {
  for (const bool scoped : {false, true}) {
    Harness harness;
    harness.resolve<&spawning>("spawning");

    harness.run<&spawning>(scoped);

    CHECK(harness.world.query<const Position>().matchedCount() == 0);

    harness.world.flush(harness.commands);

    const auto [entity, position] = harness.world.query<Position>().single();
    CHECK(position.y == 2);
    CHECK(harness.world.has<oly::app::detail::StateScoped>(entity) == scoped);
    CHECK_INVARIANTS(harness.world);
  }
}

TEST_CASE("Events push to the queue of their type") {
  Harness harness;
  const oly::app::EventTypeId type = harness.queues.declare(oly::app::eventKey<Ping>(), "Ping", sizeof(Ping));
  harness.resolve<&pinging>("pinging");

  harness.run<&pinging>(false);

  CHECK(harness.queues.beginRound());
  CHECK(harness.queues.batchCount(type) == 1);
  CHECK_INVARIANTS(harness.world);
}

TEST_CASE("a missing resource and an undeclared event are reported with the system's name") {
  Harness harness;

  harness.resolve<&needy>("needy");

  const std::string resourceName(oly::ecs::resourceName(oly::ecs::resourceId<Score>()));
  const std::string eventName(oly::core::typeName<Ping>());
  CHECK(harness.errors == std::vector<std::string>{"system \"needy\" needs resource \"" + resourceName + "\", which is not inserted", "system \"needy\" emits event \"" + eventName + "\", which is not declared"});
  CHECK_INVARIANTS(harness.world);
}
