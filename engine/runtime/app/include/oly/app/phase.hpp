#pragma once

#include <cstdint>
#include <string_view>

#include "oly/core/type_name.hpp"

namespace oly::app {
  enum class Phase : uint8_t { UpdateBefore,
                               Update,
                               UpdateAfter,
                               Render };

  inline constexpr uint32_t PhaseCount = static_cast<uint32_t>(Phase::Render) + 1;

  using PhaseId = uint32_t;

  enum class ScheduleKind : uint8_t { FixedStep,
                                      Frame };

  // Schedule tags: a phase runs in the fixed step or in the frame. The built-in phases are marker types like the ones a plugin adds.
  struct FixedStep {};
  struct Frame {};
  struct UpdateBefore {};
  struct Update {};
  struct UpdateAfter {};
  struct Render {};

  using ScheduleKey = uint32_t;

  namespace detail {
    ScheduleKey registerSchedule(std::string_view name);
    std::string_view scheduleName(ScheduleKey key);
  }

  // Dense per process, shared by phases and sets (a type is one or the other), and not for persisting.
  template <class T>
  ScheduleKey scheduleKey() {
    static const ScheduleKey key = detail::registerSchedule(core::typeName<T>());
    return key;
  }

  // Inside a phase, the Default band runs before the Last band; inside a band, registration order.
  enum class OrderBand : uint8_t { Default,
                                   Last };

  inline constexpr uint32_t OrderBandCount = static_cast<uint32_t>(OrderBand::Last) + 1;

  constexpr bool isGameplayPhase(Phase phase) noexcept {
    return phase != Phase::Render;
  }
}
