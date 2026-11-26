#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "oly/app/access.hpp"
#include "oly/app/phase.hpp"
#include "oly/app/system.hpp"
#include "oly/app/system_descriptor.hpp"

namespace oly::app::detail {
  enum class TargetKind : uint8_t { Set,
                                    Function,
                                    Name,
                                    System };

  // key: a ScheduleKey, a SystemFunctionKey or a SystemId, by kind. name: the set's type name or the system's name, for messages.
  struct OrderTarget {
    TargetKind kind;
    uint32_t key;
    std::string name;
  };

  struct GraphPhase {
    ScheduleKey key;
    std::string name;
    ScheduleKind schedule;
    bool declared;
  };

  struct GraphSystem {
    std::string name;
    ScheduleKey phase;
    SystemFunctionKey function;
    std::vector<ScheduleKey> sets;
    Access access;
  };

  // first() runs before second().
  struct OrderConstraint {
    OrderTarget first;
    OrderTarget second;
  };

  struct ScheduleInput {
    std::vector<GraphPhase> phases;
    std::vector<std::pair<ScheduleKey, ScheduleKey>> phaseConstraints; // (first, second)
    std::vector<GraphSystem> systems; // index = SystemId
    std::vector<OrderConstraint> constraints;
  };

  struct SchedulePlan {
    std::vector<uint32_t> fixedStepPhases; // indices into ScheduleInput::phases, in run order
    std::vector<uint32_t> framePhases;
    std::vector<std::vector<SystemId>> systemsByPhase; // [phase index], dispatch order
    std::vector<std::string> errors; // without the "frame: " prefix
  };

  // Pure: no Engine, no World. The order depends on the constraints and the names only, never on the order of registration.
  // With errors, the plan still holds a complete order (the systems of a cycle go last, by name), so a build without asserts runs.
  SchedulePlan buildSchedule(const ScheduleInput& input);
}
