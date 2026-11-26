#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "oly/app/controls.hpp"
#include "oly/app/detail/event_queues.hpp"
#include "oly/app/detail/schedule_graph.hpp"
#include "oly/app/event.hpp"
#include "oly/app/handler.hpp"
#include "oly/app/order.hpp"
#include "oly/app/phase.hpp"
#include "oly/app/plugin.hpp"
#include "oly/app/scheduler.hpp"
#include "oly/app/state.hpp"
#include "oly/app/system.hpp"
#include "oly/app/system_descriptor.hpp"
#include "oly/app/system_parameters.hpp"
#include "oly/app/timing.hpp"
#include "oly/core/assert.hpp"
#include "oly/core/type_name.hpp"
#include "oly/ecs/world.hpp"
#include "oly/input/action_map.hpp"

namespace oly::app {
  struct EngineConfig {
    double fixedHz = 60.0;
    uint32_t maxStepsPerFrame = 5;
    uint32_t maxEventRounds = 16;
  };

  class Engine {
   private:
    // Floating-point error in the accumulator must neither lose nor duplicate a step.
    static constexpr double StepTolerance = 1e-9;

    friend class PhaseBuilder;
    friend class SetBuilder;
    friend class SystemBuilder;

    enum class KeyKind : uint8_t { Phase,
                                   Set };

    struct PhaseEntry {
      ScheduleKey key;
      std::string name;
      ScheduleKind schedule;
      bool declared; // false while the phase is only named by a constraint or a system
    };

    struct SystemEntry {
      SystemFunction function; // the legacy bridge only
      void* userData; // the legacy bridge only
      std::string name;
      SystemId id;
      ScheduleKey phaseKey;
      OrderBand band; // the legacy bridge only
      const SystemDescriptor* descriptor; // nullptr for the legacy bridge
      detail::SystemSlots slots;
      SystemInfo info; // filled when registration closes, once the names stop moving
      std::vector<ScheduleKey> sets;
      bool whilePaused;
      StateMask states;
    };

    struct HandlerEntry {
      HandlerFunction function;
      void* userData;
      std::string name;
      HandlerId id;
      StateMask states;
    };

    ecs::World& world_;
    EngineConfig config_;
    double fixedDeltaTime_;

    std::vector<PhaseEntry> phases_; // [PhaseId], declaration order, the built-in phases first
    std::vector<std::pair<ScheduleKey, ScheduleKey>> phaseConstraints_; // (first, second)
    std::vector<std::pair<ScheduleKey, KeyKind>> keyKinds_;
    std::vector<SystemEntry> systems_; // [SystemId]
    std::vector<detail::OrderConstraint> constraints_;
    bool registrationClosed_ = false; // set by the first frame call, which builds the schedule below
    std::vector<uint32_t> fixedStepPhases_; // run order
    std::vector<uint32_t> framePhases_;
    std::vector<std::vector<SystemId>> dispatch_; // [PhaseId], dispatch order
    ecs::CommandBuffer commands_;

    detail::EventQueues queues_;
    std::vector<std::vector<HandlerEntry>> handlers_; // [EventTypeId]
    std::vector<std::pair<EventTypeId, uint32_t>> handlerLocations_; // [HandlerId]: (type, position in the type)
    uint32_t eventRoundsThisStep_ = 0;

    input::ActionMap actions_;
    Scheduler scheduler_{*this};
    StateRequest stateRequest_{*this};
    PauseRequest pauseRequest_;
    detail::EngineControls controls_{}; // the Engine's own resources, looked up before the World's

    struct StateHookEntry {
      StateHookFunction function;
      void* userData;
      std::string name;
      StateHookId id;
    };

    struct StateEntry {
      std::string name;
      std::vector<StateHookEntry> enterHooks;
      std::vector<StateHookEntry> exitHooks;
    };

    std::vector<StateEntry> states_; // [StateId]
    StateHookId stateHookCount_ = 0;
    StateId state_ = NoState;
    StateId requestedState_ = NoState;
    ecs::QueryById* scopedQuery_ = nullptr; // created on the first cleanup: creating it in the constructor would change the World's query count
    std::vector<ecs::Entity> scopedEntities_;

    double accumulator_ = 0.0;
    double lastTime_ = 0.0;
    bool hasLastTime_ = false;
    uint64_t steps_ = 0;
    double interpolation_ = 0.0;

    std::vector<std::pair<PluginKey, std::unique_ptr<Plugin>>> plugins_; // install order

    // Slot i is the i-th system in dispatch order; each runPhase overwrites the slots of its phase in place.
    std::vector<SystemTiming> systemTimings_;
    std::vector<uint32_t> firstTimingSlot_; // [PhaseId]
    std::vector<uint32_t> flushSlot_; // [PhaseId]
    std::vector<FlushTiming> flushTimings_;
    bool timingEnabled_ = false;

    bool pausedThisStep_ = false;
    bool inFrame_ = false;
    bool started_ = false; // set by the first frame call
    bool hidden_ = false;

   public:
    // fixedHz, maxStepsPerFrame and maxEventRounds must be positive (OLY_ASSERT). Two Engines over the same World
    // share the scope tag: a state change in one destroys the scoped entities of the other.
    explicit Engine(ecs::World& world, EngineConfig config = {});
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    // Destroys the plugins in reverse install order, while the World and the registrations still exist.
    ~Engine();

    ecs::World& world() noexcept {
      return world_;
    }

    // Constructs P, keeps it and runs its install once. Only before the first frame call, and once per type
    // (OLY_ASSERT). Plugins installed earlier are reachable from install through plugin<Q>().
    template <class P>
    P& installPlugin() {
      static_assert(std::is_base_of_v<Plugin, P>, "installPlugin: the plugin must derive from oly::app::Plugin");
      static_assert(std::is_default_constructible_v<P>, "installPlugin: the plugin must be default constructible");
      [[maybe_unused]] constexpr std::string_view typeName = oly::core::typeName<P>();
      OLY_ASSERT(!inFrame_ && !started_, "installPlugin: plugin \"%.*s\" installed after the first frame call", static_cast<int>(typeName.size()), typeName.data());
      OLY_ASSERT(findPlugin(pluginKey<P>()) == nullptr, "installPlugin: plugin \"%.*s\" already installed", static_cast<int>(typeName.size()), typeName.data());

      return static_cast<P&>(addPlugin(pluginKey<P>(), std::make_unique<P>()));
    }
    // The plugin must be installed (OLY_ASSERT).
    template <class P>
    P& plugin() {
      Plugin* found = findPlugin(pluginKey<P>());
      [[maybe_unused]] constexpr std::string_view typeName = oly::core::typeName<P>();
      OLY_ASSERT(found != nullptr, "plugin: plugin \"%.*s\" not installed", static_cast<int>(typeName.size()), typeName.data());

      return static_cast<P&>(*found);
    }

    // Cannot be called during frame (OLY_ASSERT). Within a phase, the Default band runs before the Last band, and
    // within a band systems run in registration order. The phase applies its commands once, after the Last band.
    // The legacy form: registers an exclusive system in a built-in phase.
    SystemId addSystem(Phase phase, std::string_view name, SystemFunction function, void* userData = nullptr, OrderBand band = OrderBand::Default);
    // In the fixed-step phases, a whilePaused system runs only in paused steps and the others only in unpaused ones.
    // A frame system does not accept whilePaused (OLY_ASSERT).
    void setWhilePaused(SystemId system, bool whilePaused);

    // Built in: UpdateBefore -> Update -> UpdateAfter in the fixed step, Render in the frame. Registration closes on the first
    // frame call: a later addSystem, addPhase, set, phase or constraint is a contract error (OLY_ASSERT), and the schedule is
    // built then (order and conflicts are checked, every parameter is resolved once).
    template <class P, class Schedule>
    PhaseBuilder addPhase(Schedule) {
      static_assert(std::is_same_v<Schedule, FixedStep> || std::is_same_v<Schedule, Frame>, "addPhase: the schedule is FixedStep or Frame");

      return declarePhase(scheduleKey<P>(), std::is_same_v<Schedule, FixedStep> ? ScheduleKind::FixedStep : ScheduleKind::Frame);
    }
    // Constraints on a phase declared now or later by a plugin.
    template <class P>
    PhaseBuilder phase() {
      return referPhase(scheduleKey<P>());
    }
    template <class S>
    SetBuilder set() {
      return declareSet(scheduleKey<S>());
    }
    // Function is a function pointer whose parameters declare what the system reads and writes. The name is unique per Engine
    // and breaks ties in the dispatch order.
    template <auto Function, class P>
    SystemBuilder addSystem(P, std::string_view name) {
      return addSystem(scheduleKey<P>(), name, systemDescriptor<Function>());
    }
    SystemBuilder addSystem(ScheduleKey phase, std::string_view name, const SystemDescriptor& descriptor);
    void orderSystems(SystemId first, SystemId second);
    // The phase must be declared (OLY_ASSERT).
    template <class P>
    PhaseId phaseId() const {
      return declaredPhaseId(scheduleKey<P>());
    }
    std::string_view phaseName(PhaseId phase) const {
      return phases_[phase].name;
    }

    // Declaration order is the dispatch order of the types. Cannot be called during frame or repeated for the
    // same type (OLY_ASSERT). The name appears in the error messages.
    template <class T>
    EventTypeId declareEvent(std::string_view name = oly::core::typeName<T>()) {
      static_assert(std::is_trivially_copyable_v<T>, "declareEvent: the event must be trivially copyable");
      static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "declareEvent: event alignment larger than the queue's");

      return declareEventType(eventKey<T>(), name, static_cast<uint32_t>(sizeof(T)));
    }
    // InvalidEventType if the type was not declared in this Engine.
    EventTypeId eventTypeOf(EventKey key) const noexcept {
      return queues_.typeOf(key);
    }
    uint32_t eventTypeCount() const noexcept {
      return queues_.typeCount();
    }

    // The type must be declared and the call cannot happen during frame (OLY_ASSERT). Handlers of the same type run in
    // registration order, once per round, with the whole batch.
    template <class T>
    HandlerId on(std::string_view name, HandlerFunction function, void* userData = nullptr) {
      const EventTypeId type = eventTypeOf(eventKey<T>());
      [[maybe_unused]] constexpr std::string_view typeName = oly::core::typeName<T>();
      OLY_ASSERT(type != InvalidEventType, "on: handler \"%.*s\" for event \"%.*s\" not declared", static_cast<int>(name.size()), name.data(), static_cast<int>(typeName.size()), typeName.data());

      return onById(type, name, function, userData);
    }
    HandlerId onById(EventTypeId type, std::string_view name, HandlerFunction function, void* userData = nullptr);

    // Valid inside and outside frame; outside, the event is dispatched in the next fixed step.
    template <class T>
    void emit(const T& event) {
      const EventTypeId type = eventTypeOf(eventKey<T>());
      [[maybe_unused]] constexpr std::string_view typeName = oly::core::typeName<T>();
      OLY_ASSERT(type != InvalidEventType, "emit: event \"%.*s\" not declared", static_cast<int>(typeName.size()), typeName.data());

      emitById(type, &event);
    }
    void emitById(EventTypeId type, const void* event);

    // nowSeconds cannot go backwards (OLY_ASSERT). Runs up to maxStepsPerFrame fixed steps and then the Render phase
    // once; the whole steps that did not fit are discarded and only the fraction stays accumulated. The first frame only
    // records the time. If the previous frame was interrupted by an exception, this one discards the pending commands, events
    // and state request (including those emitted after the exception) and also only records the time; scheduled tasks stay.
    void frame(double nowSeconds);
    // While hidden, frame does nothing; on showing, the next frame only records the time. Does not touch the game pause.
    // Hiding releases all buttons (the player may release them while the tab is out of focus).
    void setHidden(bool hidden);

    // Declarations are locked during frame; bindings and host input are not.
    input::ActionMap& actions() noexcept {
      return actions_;
    }
    const input::ActionMap& actions() const noexcept {
      return actions_;
    }

    // Flat states: declaration order gives the StateId (dense, at most MaxStates). Cannot be called during
    // frame, repeat a name or exceed the maximum (OLY_ASSERT).
    StateId declareState(std::string_view name);
    uint32_t stateCount() const noexcept {
      return static_cast<uint32_t>(states_.size());
    }
    // Valid inside and outside frame; the last request before stage 5 wins and the change happens there, paused or not.
    // A request made in a state hook takes effect in the next step.
    void requestState(StateId state);
    // NoState until the first change. Only changes in stage 5.
    StateId state() const noexcept {
      return state_;
    }
    // On a change, the onExit hooks of the leaving state run, then the scope cleanup and the onEnter hooks of the entering one, each group in
    // registration order. A hook may emit events, schedule tasks and make direct structural changes, but not
    // register anything. Hooks cannot be registered during frame (OLY_ASSERT). StateHookId counts enter and exit together.
    StateHookId onEnter(StateId state, std::string_view name, StateHookFunction function, void* userData = nullptr);
    StateHookId onExit(StateId state, std::string_view name, StateHookFunction function, void* userData = nullptr);
    // Restricts the system or handler to the given states, in all phases, including render; an empty list or mask
    // removes the restriction. With no active state, restricted ones do not run. These cannot be called during frame
    // (OLY_ASSERT).
    void setSystemStates(SystemId system, std::initializer_list<StateId> states);
    void setSystemStateMask(SystemId system, StateMask mask);
    void setHandlerStates(HandlerId handler, std::initializer_list<StateId> states);
    void setHandlerStateMask(HandlerId handler, StateMask mask);

    // Valid inside and outside frame; tasks survive exception recovery.
    Scheduler& schedule() noexcept {
      return scheduler_;
    }
    const Scheduler& schedule() const noexcept {
      return scheduler_;
    }

    // Requests in the middle of a step take effect from the next fixed step on.
    void pause() noexcept {
      pauseRequest_.pause();
    }
    void resume() noexcept {
      pauseRequest_.resume();
    }
    bool paused() const noexcept {
      return pauseRequest_.requested();
    }

    // Inside a system, it is the start of the current step; it does not advance in paused steps.
    double time() const noexcept {
      return static_cast<double>(steps_) * fixedDeltaTime_;
    }
    double fixedDeltaTime() const noexcept {
      return fixedDeltaTime_;
    }
    double interpolation() const noexcept {
      return interpolation_;
    }
    bool inFrame() const noexcept {
      return inFrame_;
    }

    // Off by default. Cannot be called during frame (OLY_ASSERT). On, each fixed step and each Render records the time of
    // every system that runs and of the flush that ends each phase; off, the record is empty and a frame pays one branch
    // per phase. Event handlers and scheduled tasks are not timed. The clock is std::chrono::steady_clock.
    void setTimingEnabled(bool enabled);
    bool timingEnabled() const noexcept {
      return timingEnabled_;
    }
    // One entry per registered system, in dispatch order (the fixed-step phases, then the frame phases), each with ran false
    // and 0 ns until its phase runs. Each runPhase overwrites the entries of its phase, so after a frame the fixed-step entries
    // hold the last fixed step and the frame entries the last frame; a frame with no fixed step keeps the last step's.
    // Empty while timing is off and until registration closes. The span, and the names in it, stay valid until the next
    // setTimingEnabled.
    std::span<const SystemTiming> systemTimings() const noexcept {
      return systemTimings_;
    }
    // One entry per phase, in the same order, while timing is on and registration is closed; empty otherwise. Same validity as
    // systemTimings.
    std::span<const FlushTiming> flushTimings() const noexcept {
      return flushTimings_;
    }

   private:
    EventTypeId declareEventType(EventKey key, std::string_view name, uint32_t size);
    SystemEntry& systemEntry(SystemId system);
    Plugin* findPlugin(PluginKey key) noexcept;
    Plugin& addPlugin(PluginKey key, std::unique_ptr<Plugin> plugin);
    void restartClock();
    void recoverFromInterruptedFrame();
    void runFixedStep();
    void runPhase(uint32_t phase);
    template <bool Timed>
    void runSystems(uint32_t phase);
    void runSystem(SystemEntry& entry, const StepInfo& step);
    void rebuildTimingSlots();

    void requireOpenRegistration(const char* operation, std::string_view name) const;
    void claimKey(const char* operation, ScheduleKey key, KeyKind kind);
    uint32_t ensurePhaseEntry(const char* operation, ScheduleKey key);
    PhaseBuilder declarePhase(ScheduleKey key, ScheduleKind schedule);
    PhaseBuilder referPhase(ScheduleKey key);
    SetBuilder declareSet(ScheduleKey key);
    PhaseId declaredPhaseId(ScheduleKey key) const;
    SystemId addSystemEntry(ScheduleKey phaseKey, std::string_view name, SystemEntry entry);
    void closeRegistration();
    void chainLegacySystems(std::vector<detail::OrderConstraint>& constraints) const;
    void dispatchEvents();
    void runDueSchedule();

    void checkState(const char* operation, StateId state) const;
    void checkStateMask(const char* operation, StateMask mask) const;
    StateMask maskOf(const char* operation, std::initializer_list<StateId> states) const;
    StateHookId addStateHook(const char* operation, bool entering, StateId state, std::string_view name, StateHookFunction function, void* userData);
    void runStateHooks(const std::vector<StateHookEntry>& hooks, StateId from, StateId to, bool entering);
    void clearStateScope();

    bool runsInState(StateMask states) const noexcept {
      return states == 0 || (state_ != NoState && ((states >> state_) & 1u) != 0);
    }

    // Stages of the fixed step defined in the spec (2026-11-09-game-engine-design.md §4.3).
    void captureActions() {
      actions_.capture();
    }
    void applyStateTransition();
  };
}
