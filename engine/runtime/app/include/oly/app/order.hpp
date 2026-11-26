#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

#include "oly/app/detail/schedule_graph.hpp"
#include "oly/app/phase.hpp"
#include "oly/app/state.hpp"
#include "oly/app/system.hpp"
#include "oly/app/system_descriptor.hpp"
#include "oly/core/type_name.hpp"

namespace oly::app {
  class Engine;

  namespace detail {
    template <class S>
    OrderTarget setTarget() {
      return OrderTarget{TargetKind::Set, scheduleKey<S>(), std::string(core::typeName<S>())};
    }

    template <auto Function>
    OrderTarget functionTarget() {
      return OrderTarget{TargetKind::Function, systemFunctionKey<Function>(), {}};
    }

    inline OrderTarget nameTarget(std::string_view name) {
      return OrderTarget{TargetKind::Name, 0, std::string(name)};
    }
  }

  // Each call adds a constraint: the first side runs before the second. Constraints are checked when registration closes.
  class PhaseBuilder {
   private:
    Engine* engine_;
    ScheduleKey phase_;

    void constrain(ScheduleKey first, ScheduleKey second);

   public:
    PhaseBuilder(Engine& engine, ScheduleKey phase) noexcept
      : engine_(&engine),
        phase_(phase) {}

    template <class Q>
    PhaseBuilder& before() {
      constrain(phase_, scheduleKey<Q>());
      return *this;
    }
    template <class Q>
    PhaseBuilder& after() {
      constrain(scheduleKey<Q>(), phase_);
      return *this;
    }
  };

  class SetBuilder {
   private:
    Engine* engine_;
    ScheduleKey set_;

    void constrain(bool selfFirst, detail::OrderTarget other);

   public:
    SetBuilder(Engine& engine, ScheduleKey set) noexcept
      : engine_(&engine),
        set_(set) {}

    template <class S>
    SetBuilder& before() {
      constrain(true, detail::setTarget<S>());
      return *this;
    }
    template <auto Function>
    SetBuilder& before() {
      constrain(true, detail::functionTarget<Function>());
      return *this;
    }
    SetBuilder& before(std::string_view systemName) {
      constrain(true, detail::nameTarget(systemName));
      return *this;
    }
    template <class S>
    SetBuilder& after() {
      constrain(false, detail::setTarget<S>());
      return *this;
    }
    template <auto Function>
    SetBuilder& after() {
      constrain(false, detail::functionTarget<Function>());
      return *this;
    }
    SetBuilder& after(std::string_view systemName) {
      constrain(false, detail::nameTarget(systemName));
      return *this;
    }
  };

  class SystemBuilder {
   private:
    Engine* engine_;
    SystemId system_;

    void constrain(bool selfFirst, detail::OrderTarget other);
    void joinSet(ScheduleKey set, std::string_view setName);

   public:
    SystemBuilder(Engine& engine, SystemId system) noexcept
      : engine_(&engine),
        system_(system) {}

    template <class S>
    SystemBuilder& inSet() {
      joinSet(scheduleKey<S>(), core::typeName<S>());
      return *this;
    }
    template <class S>
    SystemBuilder& before() {
      constrain(true, detail::setTarget<S>());
      return *this;
    }
    template <auto Function>
    SystemBuilder& before() {
      constrain(true, detail::functionTarget<Function>());
      return *this;
    }
    SystemBuilder& before(std::string_view systemName) {
      constrain(true, detail::nameTarget(systemName));
      return *this;
    }
    template <class S>
    SystemBuilder& after() {
      constrain(false, detail::setTarget<S>());
      return *this;
    }
    template <auto Function>
    SystemBuilder& after() {
      constrain(false, detail::functionTarget<Function>());
      return *this;
    }
    SystemBuilder& after(std::string_view systemName) {
      constrain(false, detail::nameTarget(systemName));
      return *this;
    }

    // In the fixed-step phases, a whilePaused system runs only in paused steps and the others only in unpaused ones. A
    // frame system cannot be whilePaused (OLY_ASSERT).
    SystemBuilder& whilePaused(bool whilePaused = true);
    // Restricts the system to the given states; an empty list removes the restriction.
    SystemBuilder& inStates(std::initializer_list<StateId> states);

    SystemId id() const noexcept {
      return system_;
    }
  };
}
