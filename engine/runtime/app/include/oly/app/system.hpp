#pragma once

#include <cstdint>
#include <string_view>

#include "oly/app/context.hpp"
#include "oly/app/phase.hpp"
#include "oly/ecs/world.hpp"

namespace oly::app {
  class Engine;

  using SystemId = uint32_t;
  struct SystemDescriptor;

  // nullptr if the name does not exist.
  const SystemDescriptor* findNamedSystem(std::string_view name) noexcept;

  struct NamedSystemRegistration {
    NamedSystemRegistration(std::string_view name, const SystemDescriptor& descriptor);
  };
}

#define OLY_SYSTEM(name) static const oly::app::NamedSystemRegistration name##Registration(#name, oly::app::systemDescriptor<&name>())
