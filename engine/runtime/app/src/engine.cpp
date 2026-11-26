#include "oly/app/engine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>

#include "oly/app/task_context.hpp"
#include "oly/core/assert.hpp"
#include "oly/ecs/resource.hpp"

namespace oly::app {
  namespace {
    uint64_t elapsedNanoseconds(std::chrono::steady_clock::time_point start) {
      return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
    }

    constexpr uint32_t MissingIndex = 0xFFFFFFFFu;
  }

  Engine::Engine(ecs::World& world, EngineConfig config)
    : world_(world),
      config_(config),
      fixedDeltaTime_(1.0 / config.fixedHz),
      commands_(world.commands()) {
    OLY_ASSERT(config.fixedHz > 0.0, "Engine: fixedHz=%f must be positive", config.fixedHz);
    OLY_ASSERT(config.maxStepsPerFrame > 0, "Engine: maxStepsPerFrame must be positive");
    OLY_ASSERT(config.maxEventRounds > 0, "Engine: maxEventRounds must be positive");

    declarePhase(scheduleKey<UpdateBefore>(), ScheduleKind::FixedStep);
    declarePhase(scheduleKey<Update>(), ScheduleKind::FixedStep);
    declarePhase(scheduleKey<UpdateAfter>(), ScheduleKind::FixedStep);
    declarePhase(scheduleKey<Render>(), ScheduleKind::Frame);
    phaseConstraints_.emplace_back(scheduleKey<UpdateBefore>(), scheduleKey<Update>());
    phaseConstraints_.emplace_back(scheduleKey<Update>(), scheduleKey<UpdateAfter>());

    controls_.entries = {{{ecs::resourceId<StateRequest>(), &stateRequest_},
                          {ecs::resourceId<PauseRequest>(), &pauseRequest_},
                          {ecs::resourceId<input::ActionMap>(), &actions_},
                          {ecs::resourceId<Scheduler>(), &scheduler_}}};
  }

  Engine::~Engine() {
    // A task's discard may reach a plugin through its payload, so it runs while every plugin is still alive.
    scheduler_.queue_.discardAll();

    while (!plugins_.empty()) {
      plugins_.pop_back();
    }
  }

  Plugin* Engine::findPlugin(PluginKey key) noexcept {
    for (const auto& [installedKey, plugin] : plugins_) {
      if (installedKey == key) {
        return plugin.get();
      }
    }

    return nullptr;
  }

  Plugin& Engine::addPlugin(PluginKey key, std::unique_ptr<Plugin> plugin) {
    Plugin& installed = *plugin;
    plugins_.emplace_back(key, std::move(plugin));
    installed.install(*this);

    return installed;
  }

  void Engine::requireOpenRegistration(const char* operation, std::string_view name) const {
    OLY_ASSERT(!registrationClosed_, "%s: \"%.*s\" registered after the first frame", operation, static_cast<int>(name.size()), name.data());
  }

  void Engine::claimKey(const char* operation, ScheduleKey key, KeyKind kind) {
    for (const auto& [claimedKey, claimedKind] : keyKinds_) {
      if (claimedKey == key) {
        const std::string_view name = detail::scheduleName(key);
        OLY_ASSERT(claimedKind == kind, "%s: type \"%.*s\" is used as both a phase and a set", operation, static_cast<int>(name.size()), name.data());

        return;
      }
    }

    keyKinds_.emplace_back(key, kind);
  }

  uint32_t Engine::ensurePhaseEntry(const char* operation, ScheduleKey key) {
    claimKey(operation, key, KeyKind::Phase);

    for (uint32_t index = 0; index < phases_.size(); ++index) {
      if (phases_[index].key == key) {
        return index;
      }
    }

    phases_.push_back(PhaseEntry{key, std::string(detail::scheduleName(key)), ScheduleKind::FixedStep, false});

    return static_cast<uint32_t>(phases_.size() - 1);
  }

  PhaseBuilder Engine::declarePhase(ScheduleKey key, ScheduleKind schedule) {
    const std::string_view name = detail::scheduleName(key);
    requireOpenRegistration("addPhase", name);

    PhaseEntry& entry = phases_[ensurePhaseEntry("addPhase", key)];
    OLY_ASSERT(!entry.declared, "addPhase: phase \"%.*s\" already declared", static_cast<int>(name.size()), name.data());

    entry.declared = true;
    entry.schedule = schedule;

    return PhaseBuilder(*this, key);
  }

  PhaseBuilder Engine::referPhase(ScheduleKey key) {
    requireOpenRegistration("phase", detail::scheduleName(key));
    ensurePhaseEntry("phase", key);

    return PhaseBuilder(*this, key);
  }

  SetBuilder Engine::declareSet(ScheduleKey key) {
    requireOpenRegistration("set", detail::scheduleName(key));
    claimKey("set", key, KeyKind::Set);

    return SetBuilder(*this, key);
  }

  PhaseId Engine::declaredPhaseId(ScheduleKey key) const {
    for (uint32_t index = 0; index < phases_.size(); ++index) {
      if (phases_[index].key == key) {
        OLY_ASSERT(phases_[index].declared, "phaseId: phase \"%s\" not declared", phases_[index].name.c_str());

        return index;
      }
    }

    const std::string_view name = detail::scheduleName(key);
    OLY_ASSERT(false, "phaseId: phase \"%.*s\" not declared", static_cast<int>(name.size()), name.data());

    return MissingIndex;
  }

  SystemId Engine::addSystemEntry(ScheduleKey phaseKey, std::string_view name, SystemEntry entry) {
    OLY_ASSERT(!inFrame_, "addSystem: system \"%.*s\" registered during frame", static_cast<int>(name.size()), name.data());
    requireOpenRegistration("addSystem", name);

    for ([[maybe_unused]] const SystemEntry& registered : systems_) {
      OLY_ASSERT(registered.name != name, "addSystem: system \"%.*s\" already registered", static_cast<int>(name.size()), name.data());
    }

    ensurePhaseEntry("addSystem", phaseKey);
    entry.id = static_cast<SystemId>(systems_.size());
    entry.name = std::string(name);
    entry.phaseKey = phaseKey;
    systems_.push_back(std::move(entry));

    return systems_.back().id;
  }

  SystemBuilder Engine::addSystem(ScheduleKey phase, std::string_view name, const SystemDescriptor& descriptor) {
    SystemEntry entry{};
    entry.descriptor = &descriptor;

    return SystemBuilder(*this, addSystemEntry(phase, name, std::move(entry)));
  }

  void Engine::orderSystems(SystemId first, SystemId second) {
    OLY_ASSERT(first < systems_.size() && second < systems_.size(), "orderSystems: system=%u or %u out of range (%zu systems)", first, second, systems_.size());
    requireOpenRegistration("orderSystems", systems_[first].name);

    constraints_.push_back(detail::OrderConstraint{detail::OrderTarget{detail::TargetKind::System, first, systems_[first].name}, detail::OrderTarget{detail::TargetKind::System, second, systems_[second].name}});
  }

  void Engine::setWhilePaused(SystemId system, bool whilePaused) {
    SystemEntry& entry = systemEntry(system);

    for ([[maybe_unused]] const PhaseEntry& phase : phases_) {
      OLY_ASSERT(phase.key != entry.phaseKey || phase.schedule == ScheduleKind::FixedStep, "setWhilePaused: frame system \"%s\" cannot be whilePaused", entry.name.c_str());
    }

    entry.whilePaused = whilePaused;
  }

  Engine::SystemEntry& Engine::systemEntry(SystemId system) {
    return systems_[system];
  }

  void PhaseBuilder::constrain(ScheduleKey first, ScheduleKey second) {
    engine_->requireOpenRegistration("phase", detail::scheduleName(phase_));
    engine_->ensurePhaseEntry("phase", first);
    engine_->ensurePhaseEntry("phase", second);
    engine_->phaseConstraints_.emplace_back(first, second);
  }

  void SetBuilder::constrain(bool selfFirst, detail::OrderTarget other) {
    engine_->requireOpenRegistration("set", detail::scheduleName(set_));

    if (other.kind == detail::TargetKind::Set) {
      engine_->claimKey("set", other.key, Engine::KeyKind::Set);
    }

    detail::OrderTarget self{detail::TargetKind::Set, set_, std::string(detail::scheduleName(set_))};
    engine_->constraints_.push_back(selfFirst ? detail::OrderConstraint{std::move(self), std::move(other)} : detail::OrderConstraint{std::move(other), std::move(self)});
  }

  void SystemBuilder::constrain(bool selfFirst, detail::OrderTarget other) {
    engine_->requireOpenRegistration("addSystem", engine_->systems_[system_].name);

    if (other.kind == detail::TargetKind::Set) {
      engine_->claimKey("addSystem", other.key, Engine::KeyKind::Set);
    }

    detail::OrderTarget self{detail::TargetKind::System, system_, engine_->systems_[system_].name};
    engine_->constraints_.push_back(selfFirst ? detail::OrderConstraint{std::move(self), std::move(other)} : detail::OrderConstraint{std::move(other), std::move(self)});
  }

  void SystemBuilder::joinSet(ScheduleKey set, std::string_view setName) {
    engine_->requireOpenRegistration("inSet", setName);
    engine_->claimKey("inSet", set, Engine::KeyKind::Set);

    std::vector<ScheduleKey>& sets = engine_->systems_[system_].sets;

    if (std::find(sets.begin(), sets.end(), set) == sets.end()) {
      sets.push_back(set);
    }
  }

  SystemBuilder& SystemBuilder::whilePaused(bool whilePaused) {
    engine_->setWhilePaused(system_, whilePaused);

    return *this;
  }

  SystemBuilder& SystemBuilder::inStates(std::initializer_list<StateId> states) {
    engine_->setSystemStates(system_, states);

    return *this;
  }

  void Engine::closeRegistration() {
    detail::ScheduleInput input;

    for (const PhaseEntry& phase : phases_) {
      input.phases.push_back(detail::GraphPhase{phase.key, phase.name, phase.schedule, phase.declared});
    }

    input.phaseConstraints = phaseConstraints_;
    input.constraints = constraints_;

    for (SystemEntry& entry : systems_) {
      const uint32_t phaseIndex = ensurePhaseEntry("frame", entry.phaseKey);
      entry.info = SystemInfo{entry.id, entry.name, phaseIndex, phases_[phaseIndex].schedule};

      detail::GraphSystem system{entry.name, entry.phaseKey, entry.descriptor->key, entry.sets, {}};

      entry.descriptor->declare(system.access);

      input.systems.push_back(std::move(system));
    }

    const detail::SchedulePlan plan = detail::buildSchedule(input);
    std::vector<std::string> errors = plan.errors;

    for (SystemEntry& entry : systems_) {
      detail::ResolveContext context{world_, controls_, queues_, scheduler_, entry.name, errors};
      entry.descriptor->resolve(context, entry.slots);
    }

    registrationClosed_ = true;
    fixedStepPhases_ = plan.fixedStepPhases;
    framePhases_ = plan.framePhases;
    dispatch_ = plan.systemsByPhase;

    if (timingEnabled_) {
      rebuildTimingSlots();
    }

    if (!errors.empty()) {
      std::string text;

      for (const std::string& error : errors) {
        text += text.empty() ? "" : "\n";
        text += "frame: " + error;
      }

      OLY_ASSERT(false, "%s", text.c_str());
    }
  }

  EventTypeId Engine::declareEventType(EventKey key, std::string_view name, uint32_t size) {
    OLY_ASSERT(!inFrame_, "declareEvent: event \"%.*s\" declared during frame", static_cast<int>(name.size()), name.data());
    OLY_ASSERT(queues_.typeOf(key) == InvalidEventType, "declareEvent: event \"%.*s\" already declared", static_cast<int>(name.size()), name.data());

    const EventTypeId type = queues_.declare(key, name, size);
    handlers_.emplace_back();

    return type;
  }

  HandlerId Engine::onById(EventTypeId type, std::string_view name, HandlerFunction function, void* userData) {
    OLY_ASSERT(!inFrame_, "on: handler \"%.*s\" registered during frame", static_cast<int>(name.size()), name.data());
    OLY_ASSERT(type < queues_.typeCount(), "on: handler \"%.*s\" for event type %u not declared", static_cast<int>(name.size()), name.data(), type);

    const HandlerId id = static_cast<HandlerId>(handlerLocations_.size());
    handlerLocations_.emplace_back(type, static_cast<uint32_t>(handlers_[type].size()));
    handlers_[type].push_back(HandlerEntry{function, userData, std::string(name), id, 0});

    return id;
  }

  void Engine::emitById(EventTypeId type, const void* event) {
    OLY_ASSERT(type < queues_.typeCount(), "emit: event type %u not declared", type);

    queues_.push(type, event);
  }

  void Engine::setHidden(bool hidden) {
    hidden_ = hidden;

    if (hidden) {
      actions_.releaseAll();
    }

    restartClock();
  }

  void Engine::restartClock() {
    accumulator_ = 0.0;
    hasLastTime_ = false;
  }

  // An exception from a system (e.g. JS unwinding the WASM without destructors) leaves inFrame_ set.
  void Engine::recoverFromInterruptedFrame() {
    commands_ = world_.commands();
    queues_.clear();
    requestedState_ = NoState;
    scheduler_.queue_.recoverFromInterruptedRun();
    inFrame_ = false;
    actions_.setDeclarationsLocked(false);
    restartClock();
  }

  void Engine::frame(double nowSeconds) {
    if (inFrame_) {
      recoverFromInterruptedFrame();
    }

    started_ = true;

    if (!registrationClosed_) {
      closeRegistration();
    }

    if (hidden_) {
      return;
    }

    if (!hasLastTime_) {
      lastTime_ = nowSeconds;
      hasLastTime_ = true;
      return;
    }

    OLY_ASSERT(nowSeconds >= lastTime_, "frame: time %f less than the previous %f", nowSeconds, lastTime_);

    inFrame_ = true;
    actions_.setDeclarationsLocked(true);
    accumulator_ += nowSeconds - lastTime_;
    lastTime_ = nowSeconds;

    const double threshold = fixedDeltaTime_ * (1.0 - StepTolerance);
    uint32_t stepsRun = 0;

    while (accumulator_ >= threshold && stepsRun < config_.maxStepsPerFrame) {
      runFixedStep();
      accumulator_ -= fixedDeltaTime_;

      if (accumulator_ < 0.0) {
        accumulator_ = 0.0;
      }

      ++stepsRun;
    }

    // Discarding the steps that did not fit avoids the lag spiral (spec game-engine-design §4.3).
    if (accumulator_ >= threshold) {
      accumulator_ = std::fmod(accumulator_, fixedDeltaTime_);

      if (accumulator_ >= threshold) {
        accumulator_ = 0.0;
      }
    }

    interpolation_ = accumulator_ / fixedDeltaTime_;
    for (const uint32_t phase : framePhases_) {
      runPhase(phase);
    }

    inFrame_ = false;
    actions_.setDeclarationsLocked(false);
  }

  void Engine::runFixedStep() {
    // Fixed at the start: pause/resume requested by a system only take effect in the next step.
    pausedThisStep_ = pauseRequest_.requested();
    eventRoundsThisStep_ = 0;

    captureActions();

    if (!pausedThisStep_) {
      runDueSchedule();
    }

    for (const uint32_t phase : fixedStepPhases_) {
      runPhase(phase);
    }

    dispatchEvents();
    applyStateTransition();

    // The clock only advances after dispatch: handlers see the same time() as the step's systems.
    if (!pausedThisStep_) {
      ++steps_;
    }
  }

  void Engine::dispatchEvents() {
    while (queues_.beginRound()) {
      if (eventRoundsThisStep_ == config_.maxEventRounds) {
        OLY_ASSERT(false, "dispatchEvents: limit of %u event rounds exceeded; pending: %s", config_.maxEventRounds, queues_.pendingNames().c_str());

        // Without asserts, discards what is left so the step continues in a defined state.
        queues_.clear();
        return;
      }

      ++eventRoundsThisStep_;

      for (EventTypeId type = 0; type < queues_.typeCount(); ++type) {
        const uint32_t count = queues_.batchCount(type);

        if (count == 0) {
          continue;
        }

        const std::byte* data = queues_.batchData(type);

        for (HandlerEntry& entry : handlers_[type]) {
          if (!runsInState(entry.states)) {
            continue;
          }

          HandlerContext context(*this, world_, type, entry.id, entry.name, entry.userData, data, count);
          entry.function(context);
        }
      }

      queues_.endRound();
    }
  }

  void Engine::runDueSchedule() {
    detail::TaskQueue& queue = scheduler_.queue_;
    const double limit = time() + fixedDeltaTime_ * StepTolerance;

    queue.beginRun();

    while (std::optional<detail::DueTask> due = queue.popDue(limit)) {
      TaskContext context(*this, world_, due->handle, due->lastRun, due->payload);
      due->function(context);
    }

    queue.endRun();
  }

  void Engine::runSystem(SystemEntry& entry, const StepInfo& step) {
    detail::RunContext context{world_, commands_, queues_, step, entry.info, state_ != NoState};
    entry.descriptor->run(entry.slots, context);
  }

  template <bool Timed>
  void Engine::runSystems(uint32_t phase) {
    const bool pausable = phases_[phase].schedule == ScheduleKind::FixedStep;
    const StepInfo step{time(), static_cast<float>(fixedDeltaTime_), static_cast<float>(interpolation_), state_, pauseRequest_.requested()};
    uint32_t slot = Timed ? firstTimingSlot_[phase] : 0;

    for (const SystemId id : dispatch_[phase]) {
      SystemEntry& entry = systems_[id];
      SystemTiming* timing = nullptr;

      if constexpr (Timed) {
        timing = &systemTimings_[slot++];
        timing->ran = false;
        timing->nanoseconds = 0;
      }

      if (pausable && entry.whilePaused != pausedThisStep_) {
        continue;
      }

      if (!runsInState(entry.states)) {
        continue;
      }

      if constexpr (Timed) {
        const auto start = std::chrono::steady_clock::now();
        runSystem(entry, step);
        timing->ran = true;
        timing->nanoseconds = elapsedNanoseconds(start);
      } else {
        runSystem(entry, step);
      }
    }

    if constexpr (Timed) {
      const auto start = std::chrono::steady_clock::now();
      world_.flush(commands_);
      flushTimings_[flushSlot_[phase]].nanoseconds = elapsedNanoseconds(start);
    } else {
      world_.flush(commands_);
    }
  }

  void Engine::runPhase(uint32_t phase) {
    if (timingEnabled_) {
      runSystems<true>(phase);
    } else {
      runSystems<false>(phase);
    }
  }

  void Engine::setTimingEnabled(bool enabled) {
    OLY_ASSERT(!inFrame_, "setTimingEnabled: called during frame");

    timingEnabled_ = enabled;

    if (enabled) {
      rebuildTimingSlots();
    } else {
      systemTimings_.clear();
      flushTimings_.clear();
    }
  }

  void Engine::rebuildTimingSlots() {
    systemTimings_.clear();
    flushTimings_.clear();
    firstTimingSlot_.assign(phases_.size(), 0);
    flushSlot_.assign(phases_.size(), 0);

    if (!registrationClosed_) {
      return;
    }

    for (const std::vector<uint32_t>* order : {&fixedStepPhases_, &framePhases_}) {
      for (const uint32_t phase : *order) {
        firstTimingSlot_[phase] = static_cast<uint32_t>(systemTimings_.size());

        for (const SystemId id : dispatch_[phase]) {
          systemTimings_.push_back(SystemTiming{id, systems_[id].name, phase, phases_[phase].name, false, 0});
        }

        flushSlot_[phase] = static_cast<uint32_t>(flushTimings_.size());
        flushTimings_.push_back(FlushTiming{phase, phases_[phase].name, 0});
      }
    }
  }
}
