#include "oly/app/system.hpp"

#include <string>
#include <utility>
#include <vector>

#include "oly/app/engine.hpp"

namespace oly::app {
  namespace {
    // Local static, not global: OLY_SYSTEM registers during the static initialization of other files, whose
    // order relative to this one is undefined.
    std::vector<std::pair<std::string, const SystemDescriptor*>>& namedSystems() {
      static std::vector<std::pair<std::string, const SystemDescriptor*>> systems;
      return systems;
    }
  }

  const SystemDescriptor* findNamedSystem(std::string_view name) noexcept {
    for (const auto& [registeredName, descriptor] : namedSystems()) {
      if (registeredName == name) {
        return descriptor;
      }
    }

    return nullptr;
  }

  NamedSystemRegistration::NamedSystemRegistration(std::string_view name, const SystemDescriptor& descriptor) {
    namedSystems().emplace_back(std::string(name), &descriptor);
  }
}
