#include "oly/ecs/detail/observer_registry.hpp"

#include <algorithm>

#include "oly/core/assert.hpp"

namespace oly::ecs::detail {
  ObserverId ObserverRegistry::add(ObserverKind kind, ComponentId componentId, ObserverFunction function, void* userData) {
    const auto observer = static_cast<ObserverId>(registrations_.size());
    registrations_.push_back({kind, componentId, false});

    std::vector<std::vector<ObserverEntry>>& lists = entries_[size_t(kind)];
    if (componentId >= lists.size()) {
      lists.resize(size_t(componentId) + 1);
    }
    lists[componentId].push_back({function, userData, observer});

    refreshMask(componentId);
    return observer;
  }

  void ObserverRegistry::remove(ObserverId observer) {
    OLY_ASSERT(observer < registrations_.size() && !registrations_[observer].removed, "removeObserver id=%u: unknown or already removed", observer);

    Registration& registration = registrations_[observer];
    registration.removed = true;

    std::vector<ObserverEntry>& list = entries_[size_t(registration.kind)][registration.componentId];
    list.erase(std::find_if(list.begin(), list.end(), [&](const ObserverEntry& entry) { return entry.id == observer; }));

    refreshMask(registration.componentId);
  }

  std::span<const ObserverEntry> ObserverRegistry::observers(ObserverKind kind, ComponentId componentId) const noexcept {
    const std::vector<std::vector<ObserverEntry>>& lists = entries_[size_t(kind)];
    if (componentId >= lists.size()) {
      return {};
    }

    return lists[componentId];
  }

  uint8_t ObserverRegistry::signatureMask(const Signature& signature) const noexcept {
    uint8_t observed = 0;
    for (ComponentId componentId : signature) {
      observed |= mask(componentId);
    }

    return observed;
  }

  uint8_t ObserverRegistry::transitionMask(const Signature& source, const Signature& destination) const noexcept {
    uint8_t observed = 0;
    for (ComponentId componentId : destination) {
      if (!std::binary_search(source.begin(), source.end(), componentId)) {
        observed |= mask(componentId) & ObservedOnAdd;
      }
    }
    for (ComponentId componentId : source) {
      if (!std::binary_search(destination.begin(), destination.end(), componentId)) {
        observed |= mask(componentId) & ObservedOnRemove;
      }
    }

    return observed;
  }

  void ObserverRegistry::refreshMask(ComponentId componentId) noexcept {
    if (componentId >= masks_.size()) {
      masks_.resize(size_t(componentId) + 1, 0);
    }

    uint8_t observed = 0;
    for (ObserverKind kind : {ObserverKind::OnAdd, ObserverKind::OnRemove}) {
      if (!observers(kind, componentId).empty()) {
        observed |= observedBit(kind);
      }
    }
    masks_[componentId] = observed;
  }
}
