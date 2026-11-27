#include "oly/ecs/resource.hpp"

#include <deque>

namespace oly::ecs {
  namespace {
    // Function with a local static: avoids initialization order problems, as the component registry does.
    // std::deque keeps stable references to each name.
    std::deque<std::string_view>& registry() {
      static std::deque<std::string_view> names;
      return names;
    }
  }

  std::string_view resourceName(ResourceId id) {
    return registry()[id];
  }

  uint32_t registeredResourceCount() noexcept {
    return static_cast<uint32_t>(registry().size());
  }

  ResourceId detail::registerResource(std::string_view name) {
    registry().push_back(name);
    return static_cast<ResourceId>(registry().size() - 1);
  }

  bool detail::ResourceStore::adopt(ResourceId id, void* object, void (*destroy)(void* object) noexcept) {
    if (find(id) != nullptr) {
      return false;
    }

    if (id >= slots_.size()) {
      slots_.resize(size_t(id) + 1);
    }

    insertionOrder_.push_back(id);
    slots_[id] = ResourceSlot{object, destroy};
    return true;
  }

  detail::ResourceStore::~ResourceStore() {
    for (auto it = insertionOrder_.rbegin(); it != insertionOrder_.rend(); ++it) {
      const ResourceSlot& slot = slots_[*it];
      slot.destroy(slot.object);
    }
  }
}
