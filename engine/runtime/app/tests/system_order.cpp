#include <doctest/doctest.h>

#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "components.hpp"
#include "oly/app/controls.hpp"
#include "oly/app/detail/schedule_graph.hpp"
#include "oly/app/engine.hpp"
#include "oly/app/order.hpp"
#include "oly/app/system_descriptor.hpp"
#include "oly/app/system_parameters.hpp"
#include "oly/app/task_context.hpp"
#include "oly/core/type_name.hpp"
#include "oly/ecs/component.hpp"
#include "oly/ecs/resource.hpp"
#include "oly/input/action_map.hpp"

using oly::app::Access;
using oly::app::Commands;
using oly::app::Engine;
using oly::app::FixedStep;
using oly::app::Frame;
using oly::app::PauseRequest;
using oly::app::Query;
using oly::app::Render;
using oly::app::Res;
using oly::app::ScheduleKind;
using oly::app::StateRequest;
using oly::app::StepInfo;
using oly::app::SystemInfo;
using oly::app::Tasks;
using oly::app::Update;
using oly::app::UpdateAfter;
using oly::app::UpdateBefore;
using oly::app::detail::GraphPhase;
using oly::app::detail::GraphSystem;
using oly::app::detail::OrderConstraint;
using oly::app::detail::OrderTarget;
using oly::app::detail::ScheduleInput;
using oly::app::detail::SchedulePlan;
using oly::app::detail::TargetKind;

namespace {
  struct Position {
    float x = 0, y = 0;
  };

  struct Physics {};

  struct Late {};

  struct Spatial {};

  struct Score {
    int value = 0;
  };

  struct Order {
    int value = 0;
  };

  struct Counter {
    int calls = 0;
  };

  std::vector<std::string> trace;
  std::vector<bool> pauses;
  std::vector<size_t> counts;
  std::vector<bool> pressedStates;
  int taskRuns = 0;
  oly::input::ButtonAction jumpAction;

  constexpr uint32_t NoFunction = std::numeric_limits<uint32_t>::max();

  // Pure-graph helpers.
  template <class P>
  GraphPhase pluginPhase(ScheduleKind schedule) {
    return GraphPhase{oly::app::scheduleKey<P>(), std::string(oly::core::typeName<P>()), schedule, true};
  }

  ScheduleInput builtInPhases() {
    ScheduleInput input;
    input.phases = {pluginPhase<UpdateBefore>(ScheduleKind::FixedStep), pluginPhase<Update>(ScheduleKind::FixedStep), pluginPhase<UpdateAfter>(ScheduleKind::FixedStep), pluginPhase<Render>(ScheduleKind::Frame)};
    input.phaseConstraints = {{oly::app::scheduleKey<UpdateBefore>(), oly::app::scheduleKey<Update>()}, {oly::app::scheduleKey<Update>(), oly::app::scheduleKey<UpdateAfter>()}};

    return input;
  }

  template <class P>
  GraphSystem systemIn(std::string name, Access access = {}) {
    return GraphSystem{std::move(name), oly::app::scheduleKey<P>(), NoFunction, {}, std::move(access)};
  }

  template <class T>
  Access writes() {
    Access access;
    access.writeComponent(oly::ecs::componentId<T>());

    return access;
  }

  template <class T>
  Access reads() {
    Access access;
    access.readComponent(oly::ecs::componentId<T>());

    return access;
  }

  template <class T>
  Access writesResource() {
    Access access;
    access.writeResource(oly::ecs::resourceId<T>());

    return access;
  }

  template <class T>
  Access readsResource() {
    Access access;
    access.readResource(oly::ecs::resourceId<T>());

    return access;
  }

  Access exclusive() {
    Access access;
    access.exclusive = true;

    return access;
  }

  template <class T>
  std::string componentName() {
    return std::string(oly::ecs::typeInfo(oly::ecs::componentId<T>()).name);
  }

  std::string conflictOn(const char* kind, const std::string& name, const char* first, const char* second, const char* firstVerb, const char* secondVerb) {
    return std::string("systems \"") + first + "\" and \"" + second + "\" of phase \"oly::app::Update\" have no order and both access " + kind + " \"" + name + "\" (" + first + " " + firstVerb + ", " + second + " " + secondVerb + ")";
  }

  OrderTarget nameTarget(std::string name) {
    return OrderTarget{TargetKind::Name, 0, std::move(name)};
  }

  OrderConstraint before(OrderTarget first, OrderTarget second) {
    return OrderConstraint{std::move(first), std::move(second)};
  }

  template <class P>
  std::vector<std::string> namesIn(const SchedulePlan& plan, const ScheduleInput& input) {
    std::vector<std::string> names;

    for (size_t index = 0; index < input.phases.size(); ++index) {
      if (input.phases[index].key != oly::app::scheduleKey<P>()) {
        continue;
      }

      for (const oly::app::SystemId system : plan.systemsByPhase[index]) {
        names.push_back(input.systems[system].name);
      }
    }

    return names;
  }

  std::vector<std::string> phaseNames(std::span<const oly::app::FlushTiming> flushes) {
    std::vector<std::string> names;

    for (const oly::app::FlushTiming& flush : flushes) {
      names.emplace_back(flush.phaseName);
    }

    return names;
  }

  // Engine-level systems: each one records its registered name.
  void record(const SystemInfo& self) {
    trace.emplace_back(self.name);
  }

  void recordWithStep(const SystemInfo& self, const StepInfo& step) {
    trace.emplace_back(self.name);
    pauses.push_back(step.paused);
  }

  void pauseAndRecord(const SystemInfo& self, const StepInfo& step, Res<PauseRequest> pause) {
    trace.emplace_back(self.name);
    pauses.push_back(step.paused);
    pause->pause();
  }

  void pauseOnly(Res<PauseRequest> pause) {
    pause->pause();
  }

  void readScore(Res<const Score> score) {
    counts.push_back(static_cast<size_t>(score->value));
  }

  void throwOnce(const SystemInfo& self, Res<Counter> counter) {
    trace.emplace_back(self.name);

    if (counter->calls++ == 0) {
      throw std::runtime_error("system failure");
    }
  }

  void countEntities(const SystemInfo& self, Query<const Position> positions) {
    trace.emplace_back(self.name);

    size_t count = 0;
    positions.each([&](const Position&) { ++count; });
    counts.push_back(count);
  }

  void spawnFirst(Commands commands) {
    commands.spawn(Order{1});
  }

  void spawnSecond(Commands commands) {
    commands.spawn(Order{2});
  }

  void requestFirstState(Res<StateRequest> request) {
    request->request(0);
  }

  void readJump(Res<const oly::input::ActionMap> actions) {
    pressedStates.push_back(actions->button(jumpAction).pressed);
  }

  void countTask(oly::app::TaskContext&) {
    ++taskRuns;
  }

  void scheduleOnce(Tasks tasks, Res<Counter> counter) {
    if (counter->calls++ == 0) {
      tasks.after(0.0, countTask);
    }
  }

  struct PhysicsPlugin : oly::app::Plugin {
    void install(Engine& engine) override {
      engine.addPhase<Physics>(FixedStep{}).after<Update>();
    }
  };

  struct LatePlugin : oly::app::Plugin {
    void install(Engine& engine) override {
      engine.addPhase<Late>(FixedStep{}).after<Physics>().before<UpdateAfter>();
    }
  };

  struct Fixture {
    oly::ecs::World world;
    Engine engine{world};
    double now = 0.0;

    Fixture() {
      trace.clear();
      pauses.clear();
      counts.clear();
      pressedStates.clear();
      taskRuns = 0;
    }

    void step() {
      now += 1.0 / 60;
      engine.frame(now);
    }
  };
}

TEST_CASE("systems with no constraint run in name order") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("charlie"), systemIn<Update>("alpha"), systemIn<Update>("bravo")};

  const SchedulePlan plan = buildSchedule(input);

  CHECK(plan.errors.empty());
  CHECK(namesIn<Update>(plan, input) == std::vector<std::string>{"alpha", "bravo", "charlie"});
}

TEST_CASE("a constraint beats the name order") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("alpha"), systemIn<Update>("charlie")};
  input.constraints = {before(nameTarget("charlie"), nameTarget("alpha"))};

  const SchedulePlan plan = buildSchedule(input);

  CHECK(plan.errors.empty());
  CHECK(namesIn<Update>(plan, input) == std::vector<std::string>{"charlie", "alpha"});
}

TEST_CASE("a cycle of systems is an error naming the cycle") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("a"), systemIn<Update>("b")};
  input.constraints = {before(nameTarget("a"), nameTarget("b")), before(nameTarget("b"), nameTarget("a"))};

  const SchedulePlan plan = buildSchedule(input);

  const std::string phaseName(oly::core::typeName<Update>());
  CHECK(plan.errors == std::vector<std::string>{"cycle of system constraints in phase \"" + phaseName + "\": a -> b -> a"});
}

TEST_CASE("a cycle is reported when a system behind it sorts first") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("a"), systemIn<Update>("x"), systemIn<Update>("y")};
  input.constraints = {before(nameTarget("x"), nameTarget("y")), before(nameTarget("y"), nameTarget("x")), before(nameTarget("x"), nameTarget("a"))};

  const SchedulePlan plan = buildSchedule(input);

  const std::string phaseName(oly::core::typeName<Update>());
  CHECK(plan.errors == std::vector<std::string>{"cycle of system constraints in phase \"" + phaseName + "\": x -> y -> x"});
}

TEST_CASE("a phase cycle is reported when a phase behind it is declared first") {
  ScheduleInput input;
  input.phases = {pluginPhase<UpdateAfter>(ScheduleKind::FixedStep), pluginPhase<UpdateBefore>(ScheduleKind::FixedStep), pluginPhase<Update>(ScheduleKind::FixedStep)};
  input.phaseConstraints = {{oly::app::scheduleKey<UpdateBefore>(), oly::app::scheduleKey<Update>()}, {oly::app::scheduleKey<Update>(), oly::app::scheduleKey<UpdateBefore>()}, {oly::app::scheduleKey<Update>(), oly::app::scheduleKey<UpdateAfter>()}};

  const SchedulePlan plan = buildSchedule(input);

  REQUIRE(!plan.errors.empty());
  CHECK(plan.errors[0].find("cycle of phase constraints") != std::string::npos);
}

TEST_CASE("phases follow their constraints") {
  ScheduleInput input = builtInPhases();
  input.phases.push_back(pluginPhase<Physics>(ScheduleKind::FixedStep));
  input.phaseConstraints.emplace_back(oly::app::scheduleKey<Update>(), oly::app::scheduleKey<Physics>());
  input.phaseConstraints.emplace_back(oly::app::scheduleKey<Physics>(), oly::app::scheduleKey<UpdateAfter>());

  const SchedulePlan plan = buildSchedule(input);

  std::vector<std::string> order;

  for (const uint32_t index : plan.fixedStepPhases) {
    order.push_back(input.phases[index].name);
  }

  CHECK(plan.errors.empty());
  CHECK(order == std::vector<std::string>{std::string(oly::core::typeName<UpdateBefore>()), std::string(oly::core::typeName<Update>()), std::string(oly::core::typeName<Physics>()), std::string(oly::core::typeName<UpdateAfter>())});
}

TEST_CASE("two phases with no order are an error") {
  ScheduleInput input = builtInPhases();
  input.phases.push_back(pluginPhase<Physics>(ScheduleKind::FixedStep));
  input.phases.push_back(pluginPhase<Late>(ScheduleKind::FixedStep));
  input.phaseConstraints.emplace_back(oly::app::scheduleKey<Update>(), oly::app::scheduleKey<Physics>());
  input.phaseConstraints.emplace_back(oly::app::scheduleKey<Update>(), oly::app::scheduleKey<Late>());
  input.phaseConstraints.emplace_back(oly::app::scheduleKey<Physics>(), oly::app::scheduleKey<UpdateAfter>());
  input.phaseConstraints.emplace_back(oly::app::scheduleKey<Late>(), oly::app::scheduleKey<UpdateAfter>());

  const SchedulePlan plan = buildSchedule(input);

  REQUIRE(plan.errors.size() == 1);
  CHECK(plan.errors[0].find("have no order between them") != std::string::npos);
  CHECK(plan.errors[0].find("of the fixed step") != std::string::npos);
}

TEST_CASE("a phase cycle is an error") {
  ScheduleInput input = builtInPhases();
  input.phaseConstraints.emplace_back(oly::app::scheduleKey<UpdateAfter>(), oly::app::scheduleKey<UpdateBefore>());

  const SchedulePlan plan = buildSchedule(input);

  REQUIRE(!plan.errors.empty());
  CHECK(plan.errors[0].find("cycle of phase constraints") != std::string::npos);
}

TEST_CASE("a constraint naming an undeclared phase is an error") {
  ScheduleInput input = builtInPhases();
  input.phases.push_back(GraphPhase{oly::app::scheduleKey<Physics>(), std::string(oly::core::typeName<Physics>()), ScheduleKind::FixedStep, false});
  input.phaseConstraints.emplace_back(oly::app::scheduleKey<Update>(), oly::app::scheduleKey<Physics>());

  const SchedulePlan plan = buildSchedule(input);

  CHECK(plan.errors == std::vector<std::string>{"phase constraint names \"" + std::string(oly::core::typeName<Physics>()) + "\", which is not declared"});
}

TEST_CASE("phases of two schedules cannot be ordered against each other") {
  ScheduleInput input = builtInPhases();
  input.phaseConstraints.emplace_back(oly::app::scheduleKey<Update>(), oly::app::scheduleKey<Render>());

  const SchedulePlan plan = buildSchedule(input);

  REQUIRE(plan.errors.size() == 1);
  CHECK(plan.errors[0].find("are in different schedules") != std::string::npos);
}

TEST_CASE("the registration order does not change the dispatch order") {
  ScheduleInput forward = builtInPhases();
  forward.systems = {systemIn<Update>("delta"), systemIn<Update>("alpha"), systemIn<Update>("charlie"), systemIn<Update>("bravo")};
  forward.constraints = {before(nameTarget("delta"), nameTarget("alpha")), before(nameTarget("charlie"), nameTarget("bravo"))};

  ScheduleInput reversed = forward;
  reversed.systems = {systemIn<Update>("bravo"), systemIn<Update>("charlie"), systemIn<Update>("alpha"), systemIn<Update>("delta")};

  CHECK(namesIn<Update>(buildSchedule(forward), forward) == namesIn<Update>(buildSchedule(reversed), reversed));
}

TEST_CASE("a plugin phase runs between the phases it is ordered against") {
  Fixture fixture;
  fixture.engine.addPhase<Physics>(FixedStep{}).after<Update>().before<UpdateAfter>();
  fixture.engine.addSystem<&record>(UpdateAfter{}, "ua");
  fixture.engine.addSystem<&record>(Physics{}, "p");
  fixture.engine.addSystem<&record>(Update{}, "u");
  fixture.engine.addSystem<&record>(UpdateBefore{}, "ub");

  fixture.engine.frame(0.0);
  fixture.step();

  CHECK(trace == std::vector<std::string>{"ub", "u", "p", "ua"});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("systems of a set run after the set they follow") {
  Fixture fixture;
  fixture.engine.addSystem<&record>(Update{}, "zeta").inSet<Spatial>();
  fixture.engine.addSystem<&record>(Update{}, "alpha").after<Spatial>();

  fixture.engine.frame(0.0);
  fixture.step();

  CHECK(trace == std::vector<std::string>{"zeta", "alpha"});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a function constraint orders every registration of the function") {
  Fixture fixture;
  fixture.engine.addSystem<&record>(Update{}, "a1");
  fixture.engine.addSystem<&record>(Update{}, "a2");
  fixture.engine.addSystem<&countEntities>(Update{}, "a0").after<&record>();

  fixture.engine.frame(0.0);
  fixture.step();

  CHECK(trace == std::vector<std::string>{"a1", "a2", "a0"});

  Fixture named;
  named.engine.addSystem<&record>(Update{}, "a1");
  named.engine.addSystem<&record>(Update{}, "a2");
  named.engine.addSystem<&countEntities>(Update{}, "a0").after("a1");
  trace.clear();

  named.engine.frame(0.0);
  named.step();

  CHECK(trace == std::vector<std::string>{"a1", "a0", "a2"});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("phase order does not depend on which plugin declares first") {
  oly::ecs::World firstWorld;
  Engine first(firstWorld);
  first.installPlugin<PhysicsPlugin>();
  first.installPlugin<LatePlugin>();
  first.setTimingEnabled(true);
  first.frame(0.0);

  oly::ecs::World secondWorld;
  Engine second(secondWorld);
  second.installPlugin<LatePlugin>();
  second.installPlugin<PhysicsPlugin>();
  second.setTimingEnabled(true);
  second.frame(0.0);

  const std::vector<std::string> expected = phaseNames(first.flushTimings());
  CHECK(expected.size() == 6);
  CHECK(phaseNames(second.flushTimings()) == expected);
  CHECK_INVARIANTS(firstWorld);
  CHECK_INVARIANTS(secondWorld);
}

TEST_CASE("engine controls resolve to the system's own Engine") {
  Fixture fixture;
  Engine other(fixture.world);
  fixture.world.insertResource(Score{7});
  fixture.engine.addSystem<&pauseOnly>(Update{}, "pauser");
  fixture.engine.addSystem<&readScore>(Update{}, "readerA");
  other.addSystem<&readScore>(Update{}, "readerB");

  fixture.engine.frame(0.0);
  other.frame(0.0);
  fixture.step();
  other.frame(fixture.now);

  CHECK(fixture.engine.paused());
  CHECK_FALSE(other.paused());
  CHECK(counts == std::vector<size_t>{7, 7});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a frame after an exception keeps the closed schedule") {
  Fixture fixture;
  fixture.world.insertResource(Counter{});
  fixture.world.spawn(Position{1, 2});
  fixture.engine.addSystem<&throwOnce>(Update{}, "a");
  fixture.engine.addSystem<&countEntities>(Update{}, "b").after("a");
  fixture.engine.addSystem<&record>(Update{}, "c").after("b");

  fixture.engine.frame(0.0);
  CHECK_THROWS(fixture.step());
  fixture.step();
  trace.clear();
  counts.clear();

  fixture.step();

  CHECK(trace == std::vector<std::string>{"a", "b", "c"});
  CHECK(counts == std::vector<size_t>{1});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("a set orders its members in each phase") {
  Fixture fixture;
  fixture.engine.addSystem<&record>(Update{}, "m1").inSet<Spatial>();
  fixture.engine.addSystem<&record>(UpdateAfter{}, "m2").inSet<Spatial>();
  fixture.engine.addSystem<&countEntities>(UpdateAfter{}, "a_g");
  fixture.engine.set<Spatial>().before<&countEntities>();

  fixture.engine.frame(0.0);
  fixture.step();

  CHECK(trace == std::vector<std::string>{"m1", "m2", "a_g"});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("spawns of two unordered systems apply in dispatch order") {
  Fixture fixture;
  fixture.engine.addSystem<&spawnSecond>(Update{}, "b");
  fixture.engine.addSystem<&spawnFirst>(Update{}, "a");

  fixture.engine.frame(0.0);
  fixture.step();

  std::vector<int> values;
  fixture.world.query<const Order>().each([&](const Order& order) { values.push_back(order.value); });
  CHECK(values == std::vector<int>{1, 2});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("StepInfo holds the clock, the state and the pause read at the phase start") {
  Fixture fixture;
  fixture.engine.addSystem<&pauseAndRecord>(Update{}, "a");
  fixture.engine.addSystem<&recordWithStep>(Update{}, "b");
  fixture.engine.addSystem<&recordWithStep>(Update{}, "c").whilePaused();

  fixture.engine.frame(0.0);
  fixture.step();
  fixture.step();

  CHECK(trace == std::vector<std::string>{"a", "b", "c"});
  CHECK(pauses == std::vector<bool>{false, false, true});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("Res<StateRequest> requests a state that changes in stage 5") {
  Fixture fixture;
  fixture.engine.declareState("play");
  fixture.engine.addSystem<&requestFirstState>(Update{}, "request");

  fixture.engine.frame(0.0);
  fixture.step();

  CHECK(fixture.engine.state() == 0);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("Res<const ActionMap> reads the step's actions") {
  Fixture fixture;
  jumpAction = fixture.engine.actions().declareButton("jump");
  fixture.engine.actions().bind(jumpAction, oly::input::Key::Space);
  fixture.engine.addSystem<&readJump>(Update{}, "readJump");

  fixture.engine.frame(0.0);
  fixture.engine.actions().buttonDown(oly::input::Key::Space);
  fixture.step();

  CHECK(pressedStates == std::vector<bool>{true});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("Tasks schedule a task on the game clock") {
  Fixture fixture;
  fixture.world.insertResource(Counter{});
  fixture.engine.addSystem<&scheduleOnce>(Update{}, "schedule");

  fixture.engine.frame(0.0);
  fixture.step();
  fixture.step();

  CHECK(taskRuns == 1);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("two unordered systems that write one component are refused") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("moveA", writes<Position>()), systemIn<Update>("moveB", writes<Position>())};

  const SchedulePlan plan = buildSchedule(input);

  CHECK(plan.errors == std::vector<std::string>{conflictOn("component", componentName<Position>(), "moveA", "moveB", "writes", "writes")});
}

TEST_CASE("a read and a write are refused") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("reader", reads<Position>()), systemIn<Update>("writer", writes<Position>())};

  const SchedulePlan plan = buildSchedule(input);

  CHECK(plan.errors == std::vector<std::string>{conflictOn("component", componentName<Position>(), "reader", "writer", "reads", "writes")});
}

TEST_CASE("two reads are accepted") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("a", reads<Position>()), systemIn<Update>("b", reads<Position>())};

  CHECK(buildSchedule(input).errors.empty());
}

TEST_CASE("a constraint accepts the pair, directly or through a chain") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("a", writes<Position>()), systemIn<Update>("b"), systemIn<Update>("c", writes<Position>())};
  input.constraints = {before(nameTarget("a"), nameTarget("b")), before(nameTarget("b"), nameTarget("c"))};

  CHECK(buildSchedule(input).errors.empty());
}

TEST_CASE("a resource write and read are refused") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("reader", readsResource<Score>()), systemIn<Update>("writer", writesResource<Score>())};

  const SchedulePlan plan = buildSchedule(input);

  CHECK(plan.errors == std::vector<std::string>{conflictOn("resource", std::string(oly::core::typeName<Score>()), "reader", "writer", "reads", "writes")});
}

TEST_CASE("an exclusive system conflicts with every unordered system of its phase") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("alone", exclusive()), systemIn<Update>("other")};

  const SchedulePlan plan = buildSchedule(input);

  CHECK(plan.errors == std::vector<std::string>{"systems \"alone\" and \"other\" of phase \"oly::app::Update\" have no order and \"alone\" is exclusive"});
}

TEST_CASE("every conflict of a phase is reported") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("a", writes<Position>()), systemIn<Update>("b", writes<Position>()), systemIn<Update>("c", writes<Position>())};

  const SchedulePlan plan = buildSchedule(input);

  CHECK(plan.errors == std::vector<std::string>{conflictOn("component", componentName<Position>(), "a", "b", "writes", "writes"), conflictOn("component", componentName<Position>(), "a", "c", "writes", "writes"), conflictOn("component", componentName<Position>(), "b", "c", "writes", "writes")});
}

TEST_CASE("a system restricted to a state still conflicts") {
  // Restrictions live on the Engine side and never reach the graph: the input carries the access only.
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("menuOnly", writes<Position>()), systemIn<Update>("playOnly", writes<Position>())};

  CHECK(buildSchedule(input).errors.size() == 1);
}

TEST_CASE("systems of two phases never conflict") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("a", writes<Position>()), systemIn<UpdateAfter>("b", writes<Position>())};

  CHECK(buildSchedule(input).errors.empty());
}

TEST_CASE("constraints naming a missing set, name or function are errors") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("a")};
  input.constraints = {before(OrderTarget{TargetKind::Set, oly::app::scheduleKey<Spatial>(), std::string(oly::core::typeName<Spatial>())}, nameTarget("a")), before(nameTarget("a"), nameTarget("typo")), before(OrderTarget{TargetKind::Function, 12345, ""}, nameTarget("a"))};

  const SchedulePlan plan = buildSchedule(input);

  CHECK(plan.errors == std::vector<std::string>{"constraint names set \"" + std::string(oly::core::typeName<Spatial>()) + "\", which has no member", "constraint names system \"typo\", which is not registered", "constraint names a function that is never registered"});
}

TEST_CASE("a cross-phase constraint that agrees is accepted and one that contradicts is an error") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<UpdateBefore>("early"), systemIn<UpdateAfter>("late")};
  input.constraints = {before(nameTarget("early"), nameTarget("late"))};

  CHECK(buildSchedule(input).errors.empty());

  input.constraints = {before(nameTarget("late"), nameTarget("early"))};

  CHECK(buildSchedule(input).errors == std::vector<std::string>{"system \"late\" (phase \"oly::app::UpdateAfter\") must run before \"early\" (phase \"oly::app::UpdateBefore\"), but phase \"oly::app::UpdateBefore\" runs before \"oly::app::UpdateAfter\""});
}

TEST_CASE("a type cannot be both a phase and a set") {
  ScheduleInput input = builtInPhases();
  input.systems = {systemIn<Update>("a")};
  input.systems[0].sets = {oly::app::scheduleKey<Update>()};

  CHECK(buildSchedule(input).errors == std::vector<std::string>{"\"oly::app::Update\" is both a phase and a set"});
}
