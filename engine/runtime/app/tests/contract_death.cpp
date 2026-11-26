// Each case violates an app contract and must trigger OLY_ASSERT with the required substrings; it is only compiled with
// asserts enabled. Same protocol as the ecs contract_death.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "oly/app/engine.hpp"
#include "oly/app/handler.hpp"
#include "oly/app/plugin.hpp"
#include "oly/app/state_context.hpp"
#include "oly/app/system_descriptor.hpp"
#include "oly/app/system_parameters.hpp"
#include "oly/app/task_context.hpp"
#include "oly/core/assert.hpp"
#include "oly/input/action_map.hpp"

namespace {
  using oly::app::Engine;
  using oly::app::HandlerContext;
  using oly::app::Res;
  using oly::app::SystemInfo;
  using oly::app::TaskContext;
  using oly::app::TaskHandle;
  using oly::app::Update;

  // A system reaches the Engine only through this non-owning handle: the cases below call the Engine during a frame.
  struct EngineHandle {
    Engine* engine;
  };

  void idle() {}

  struct Needed {};

  struct Unannounced {
    int value;
  };

  struct DeathFirst {};

  struct DeathSecond {};

  const std::vector<const char*>* g_required = nullptr;

  void handler(const char* message) noexcept {
    std::fprintf(stderr, "%s\n", message);

    if (g_required) {
      for (const char* needle : *g_required) {
        if (!std::strstr(message, needle)) {
          std::fprintf(stderr, "message missing the required substring: \"%s\"\n", needle);
          std::fflush(stderr);
          std::_Exit(2);
        }
      }
    }

    std::fflush(stderr);
    std::_Exit(0);
  }

  void addSystemFromSystem(Res<EngineHandle> handle) {
    handle->engine->addSystem<&idle>(Update{}, "other");
  }

  void addSystemDuringFrame() {
    oly::ecs::World world;
    Engine engine(world);
    world.insertResource(EngineHandle{&engine});
    engine.addSystem<&addSystemFromSystem>(Update{}, "adder");

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  void enableTimingFromSystem(Res<EngineHandle> handle) {
    handle->engine->setTimingEnabled(true);
  }

  void setTimingEnabledDuringFrame() {
    oly::ecs::World world;
    Engine engine(world);
    world.insertResource(EngineHandle{&engine});
    engine.addSystem<&enableTimingFromSystem>(Update{}, "enabler");

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  void idleWithResource(oly::app::Res<Needed>) {}

  void idleWithEvent(oly::app::Events<Unannounced>) {}

  void whilePausedFrameSystem() {
    oly::ecs::World world;
    Engine engine(world);
    const auto system = engine.addSystem<&idle>(oly::app::Render{}, "drawing");

    engine.setWhilePaused(system.id(), true);
  }

  void systemCycle() {
    oly::ecs::World world;
    Engine engine(world);
    engine.addSystem<&idle>(oly::app::Update{}, "a").before("b");
    engine.addSystem<&idle>(oly::app::Update{}, "b").before("a");

    engine.frame(0.0);
  }

  void phaseCycle() {
    oly::ecs::World world;
    Engine engine(world);
    engine.phase<oly::app::UpdateAfter>().before<oly::app::UpdateBefore>();

    engine.frame(0.0);
  }

  void unorderedPhases() {
    oly::ecs::World world;
    Engine engine(world);
    engine.addPhase<DeathFirst>(oly::app::FixedStep{}).after<oly::app::Update>();
    engine.addPhase<DeathSecond>(oly::app::FixedStep{}).after<oly::app::Update>();

    engine.frame(0.0);
  }

  void duplicateSystemName() {
    oly::ecs::World world;
    Engine engine(world);
    engine.addSystem<&idle>(oly::app::Update{}, "twice");
    engine.addSystem<&idle>(oly::app::Update{}, "twice");
  }

  void addSystemAfterFirstFrame() {
    oly::ecs::World world;
    Engine engine(world);
    engine.frame(0.0);

    engine.addSystem<&idle>(oly::app::Update{}, "late");
  }

  struct Health {
    int value;
  };

  struct Gold {
    int value;
  };

  void writeHealth(oly::app::Query<Health> query) {
    query.each([](Health& health) { health.value += 1; });
  }

  void readHealth(oly::app::Query<const Health> query) {
    query.each([](const Health&) {});
  }

  void writeGold(oly::app::Res<Gold> gold) {
    gold->value += 1;
  }

  void readGold(oly::app::Res<const Gold> gold) {
    (void)gold;
  }

  void writeWriteConflict() {
    oly::ecs::World world;
    Engine engine(world);
    engine.addSystem<&writeHealth>(oly::app::Update{}, "healA");
    engine.addSystem<&writeHealth>(oly::app::Update{}, "healB");

    engine.frame(0.0);
  }

  void readWriteConflict() {
    oly::ecs::World world;
    Engine engine(world);
    engine.addSystem<&writeHealth>(oly::app::Update{}, "heal");
    engine.addSystem<&readHealth>(oly::app::Update{}, "show");

    engine.frame(0.0);
  }

  void resourceConflict() {
    oly::ecs::World world;
    world.insertResource(Gold{0});
    Engine engine(world);
    engine.addSystem<&writeGold>(oly::app::Update{}, "earn");
    engine.addSystem<&readGold>(oly::app::Update{}, "show");

    engine.frame(0.0);
  }

  void missingConstraintTarget() {
    oly::ecs::World world;
    Engine engine(world);
    engine.addSystem<&idle>(oly::app::Update{}, "a").before("typo");

    engine.frame(0.0);
  }

  void contradictingCrossPhaseConstraint() {
    oly::ecs::World world;
    Engine engine(world);
    engine.addSystem<&idle>(oly::app::UpdateAfter{}, "late").before("early");
    engine.addSystem<&idle>(oly::app::UpdateBefore{}, "early");

    engine.frame(0.0);
  }

  void missingResource() {
    oly::ecs::World world;
    Engine engine(world);
    engine.addSystem<&idleWithResource>(oly::app::Update{}, "needy");

    engine.frame(0.0);
  }

  void undeclaredEvent() {
    oly::ecs::World world;
    Engine engine(world);
    engine.addSystem<&idleWithEvent>(oly::app::Update{}, "announcer");

    engine.frame(0.0);
  }

  void frameTimeBackwards() {
    oly::ecs::World world;
    Engine engine(world);

    engine.frame(1.0);
    engine.frame(0.5);
  }

  struct Ping {
    uint32_t value;
  };
  struct Echo {
    uint32_t depth;
  };

  void nothing(HandlerContext&) {}

  void emitUndeclaredEvent() {
    oly::ecs::World world;
    Engine engine(world);

    engine.emit(Ping{});
  }

  void onUndeclaredEvent() {
    oly::ecs::World world;
    Engine engine(world);

    engine.on<Ping>("h", nothing);
  }

  void declareEventTwice() {
    oly::ecs::World world;
    Engine engine(world);

    engine.declareEvent<Ping>();
    engine.declareEvent<Ping>();
  }

  void onFromSystem(Res<EngineHandle> handle) {
    handle->engine->on<Ping>("late", nothing);
  }

  void onDuringFrame() {
    oly::ecs::World world;
    Engine engine(world);
    world.insertResource(EngineHandle{&engine});
    engine.declareEvent<Ping>();
    engine.addSystem<&onFromSystem>(Update{}, "registrar");

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  void declareEventFromSystem(Res<EngineHandle> handle) {
    handle->engine->declareEvent<Echo>();
  }

  void declareEventDuringFrame() {
    oly::ecs::World world;
    Engine engine(world);
    world.insertResource(EngineHandle{&engine});
    engine.addSystem<&declareEventFromSystem>(Update{}, "declarer");

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  void declareActionFromSystem(Res<oly::input::ActionMap> actionMap) {
    actionMap->declareButton("late");
  }

  void declareActionDuringFrame() {
    oly::ecs::World world;
    Engine engine(world);
    engine.addSystem<&declareActionFromSystem>(Update{}, "declarer");

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  void emitEchoAgain(HandlerContext& context) {
    context.emit(Echo{0});
  }

  void eventRoundLimit() {
    oly::ecs::World world;
    Engine engine(world);
    engine.declareEvent<Echo>("Echo");
    engine.on<Echo>("loop", emitEchoAgain);
    engine.emit(Echo{0});

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  void readWrongType(HandlerContext& context) {
    context.events<Echo>();
  }

  void eventsWrongType() {
    oly::ecs::World world;
    Engine engine(world);
    engine.declareEvent<Ping>();
    engine.declareEvent<Echo>();
    engine.on<Ping>("wrong", readWrongType);
    engine.emit(Ping{});

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  struct Mark {
    int value;
  };

  struct Other {
    double value;
  };

  void nothingTask(TaskContext&) {}

  void readWrongPayload(TaskContext& context) {
    context.payload<Other>();
  }

  void scheduleNegativeDelay() {
    oly::ecs::World world;
    Engine engine(world);

    engine.schedule().after(-1.0, nothingTask);
  }

  void scheduleNotFiniteDelay() {
    oly::ecs::World world;
    Engine engine(world);

    engine.schedule().after(NAN, nothingTask);
  }

  void everyZeroInterval() {
    oly::ecs::World world;
    Engine engine(world);

    engine.schedule().every(0.0, nothingTask);
  }

  void scheduleNullFunction() {
    oly::ecs::World world;
    Engine engine(world);

    engine.schedule().after(1.0, nullptr);
  }

  void taskPayloadWrongType() {
    oly::ecs::World world;
    Engine engine(world);
    engine.schedule().after(0.0, readWrongPayload, Mark{1});

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  void cancelSlotOutOfRange() {
    oly::ecs::World world;
    Engine engine(world);

    engine.schedule().cancel(TaskHandle{7, 0});
  }

  void declareStateFromSystem(Res<EngineHandle> handle) {
    handle->engine->declareState("late");
  }

  void declareStateDuringFrame() {
    oly::ecs::World world;
    Engine engine(world);
    world.insertResource(EngineHandle{&engine});
    engine.addSystem<&declareStateFromSystem>(Update{}, "declarer");

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  void declareStateTwice() {
    oly::ecs::World world;
    Engine engine(world);

    engine.declareState("a");
    engine.declareState("a");
  }

  void tooManyStates() {
    oly::ecs::World world;
    Engine engine(world);

    for (uint32_t index = 0; index <= oly::app::MaxStates; ++index) {
      engine.declareState("state" + std::to_string(index));
    }
  }

  void requestStateOutOfRange() {
    oly::ecs::World world;
    Engine engine(world);
    engine.declareState("a");
    engine.declareState("b");

    engine.requestState(5);
  }

  void onEnterFromSystem(Res<EngineHandle> handle) {
    handle->engine->onEnter(0, "late", [](oly::app::StateContext&) {});
  }

  void onEnterDuringFrame() {
    oly::ecs::World world;
    Engine engine(world);
    world.insertResource(EngineHandle{&engine});
    engine.declareState("a");
    engine.addSystem<&onEnterFromSystem>(Update{}, "registrar");

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  void setSystemStatesFromSystem(Res<EngineHandle> handle, const SystemInfo& self) {
    handle->engine->setSystemStates(self.id, {0});
  }

  void systemStatesDuringFrame() {
    oly::ecs::World world;
    Engine engine(world);
    world.insertResource(EngineHandle{&engine});
    engine.declareState("a");
    engine.addSystem<&setSystemStatesFromSystem>(Update{}, "restrictor");

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  void persistFromTask(TaskContext& context) {
    context.persist(context.payload<oly::ecs::Entity>());
  }

  void persistDeadEntity() {
    oly::ecs::World world;
    Engine engine(world);
    const oly::ecs::Entity entity = world.spawn();
    world.destroy(entity);
    engine.schedule().after(0.0, persistFromTask, entity);

    engine.frame(0.0);
    engine.frame(1.0 / 60);
  }

  void systemStateMaskUndeclared() {
    oly::ecs::World world;
    Engine engine(world);
    engine.declareState("a");
    engine.declareState("b");
    const auto system = engine.addSystem<&idle>(Update{}, "s").id();

    engine.setSystemStateMask(system, 0b100);
  }

  void systemStatesUnknownSystem() {
    oly::ecs::World world;
    Engine engine(world);
    engine.declareState("a");

    engine.setSystemStateMask(7, 0b1);
  }

  void handlerStatesUnknownHandler() {
    oly::ecs::World world;
    Engine engine(world);
    engine.declareState("a");

    engine.setHandlerStateMask(7, 0b1);
  }

  struct ProbePlugin : oly::app::Plugin {
    void install(Engine&) override {}
  };

  struct MissingPlugin : oly::app::Plugin {
    void install(Engine&) override {}
  };

  void installPluginTwice() {
    oly::ecs::World world;
    Engine engine(world);
    engine.installPlugin<ProbePlugin>();

    engine.installPlugin<ProbePlugin>();
  }

  void installPluginAfterFirstFrame() {
    oly::ecs::World world;
    Engine engine(world);
    engine.frame(0.0);

    engine.installPlugin<ProbePlugin>();
  }

  void pluginNotInstalled() {
    oly::ecs::World world;
    Engine engine(world);
    engine.installPlugin<ProbePlugin>();

    engine.plugin<MissingPlugin>();
  }

  struct Case {
    const char* name;
    void (*run)();
    std::vector<const char*> required;
  };
}

int main(int argumentCount, char** arguments) {
  static const std::vector<Case> cases = {
    {"add_system_during_frame", addSystemDuringFrame, {"addSystem", "during frame"}},
    {"set_timing_enabled_during_frame", setTimingEnabledDuringFrame, {"setTimingEnabled", "during frame"}},
    {"while_paused_frame_system", whilePausedFrameSystem, {"setWhilePaused", "cannot be whilePaused"}},
    {"system_cycle", systemCycle, {"frame: cycle of system constraints", "a", "b"}},
    {"phase_cycle", phaseCycle, {"frame: cycle of phase constraints"}},
    {"unordered_phases", unorderedPhases, {"frame: ", "have no order between them"}},
    {"duplicate_system_name", duplicateSystemName, {"already registered", "twice"}},
    {"add_system_after_first_frame", addSystemAfterFirstFrame, {"addSystem", "registered after the first frame"}},
    {"write_write_conflict", writeWriteConflict, {"frame: systems", "have no order", "component", "writes"}},
    {"read_write_conflict", readWriteConflict, {"frame: systems", "have no order", "component", "reads"}},
    {"resource_conflict", resourceConflict, {"frame: systems", "have no order", "resource"}},
    {"missing_constraint_target", missingConstraintTarget, {"which is not registered"}},
    {"contradicting_cross_phase_constraint", contradictingCrossPhaseConstraint, {"runs before"}},
    {"missing_resource", missingResource, {"frame: system \"needy\" needs resource"}},
    {"undeclared_event", undeclaredEvent, {"frame: system \"announcer\" emits event"}},
    {"frame_time_backwards", frameTimeBackwards, {"frame", "time"}},
    {"emit_undeclared_event", emitUndeclaredEvent, {"emit", "not declared"}},
    {"on_undeclared_event", onUndeclaredEvent, {"on", "not declared"}},
    {"declare_event_twice", declareEventTwice, {"declareEvent", "already declared"}},
    {"on_during_frame", onDuringFrame, {"on", "during frame"}},
    {"declare_event_during_frame", declareEventDuringFrame, {"declareEvent", "during frame"}},
    {"declare_action_during_frame", declareActionDuringFrame, {"declare", "during frame"}},
    {"event_round_limit", eventRoundLimit, {"dispatchEvents", "rounds", "Echo"}},
    {"events_wrong_type", eventsWrongType, {"events", "type"}},
    {"schedule_negative_delay", scheduleNegativeDelay, {"schedule", "delay"}},
    {"schedule_not_finite_delay", scheduleNotFiniteDelay, {"schedule", "delay"}},
    {"every_zero_interval", everyZeroInterval, {"every", "interval"}},
    {"schedule_null_function", scheduleNullFunction, {"schedule", "null function"}},
    {"task_payload_wrong_type", taskPayloadWrongType, {"payload", "Other"}},
    {"cancel_slot_out_of_range", cancelSlotOutOfRange, {"cancel", "out of range"}},
    {"declare_state_during_frame", declareStateDuringFrame, {"declareState", "during frame"}},
    {"declare_state_twice", declareStateTwice, {"declareState", "already declared"}},
    {"too_many_states", tooManyStates, {"declareState", "maximum"}},
    {"request_state_out_of_range", requestStateOutOfRange, {"requestState", "out of range"}},
    {"on_enter_during_frame", onEnterDuringFrame, {"onEnter", "during frame"}},
    {"system_states_during_frame", systemStatesDuringFrame, {"setSystemStates", "during frame"}},
    {"system_state_mask_undeclared", systemStateMaskUndeclared, {"setSystemStates", "undeclared"}},
    {"system_states_unknown_system", systemStatesUnknownSystem, {"setSystemStates", "out of range"}},
    {"handler_states_unknown_handler", handlerStatesUnknownHandler, {"setHandlerStates", "out of range"}},
    {"persist_dead_entity", persistDeadEntity, {"remove", "StateScoped"}},
    {"install_plugin_twice", installPluginTwice, {"installPlugin", "ProbePlugin", "already installed"}},
    {"install_plugin_after_first_frame", installPluginAfterFirstFrame, {"installPlugin", "ProbePlugin", "frame"}},
    {"plugin_not_installed", pluginNotInstalled, {"plugin", "MissingPlugin", "not installed"}},
  };

  if (argumentCount != 2) {
    std::fprintf(stderr, "usage: app_contract_death <case>\n");
    return 64;
  }

  for (const Case& testCase : cases) {
    if (std::strcmp(testCase.name, arguments[1]) != 0) {
      continue;
    }

    g_required = &testCase.required;
    oly::core::setAssertHandler(&handler);
    testCase.run();
    std::fprintf(stderr, "assert did not fire\n");
    return 1;
  }

  std::fprintf(stderr, "unknown case: %s\n", arguments[1]);
  return 64;
}
