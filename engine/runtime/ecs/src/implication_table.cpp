#include "oly/ecs/detail/implication_table.hpp"

#include <algorithm>

namespace oly::ecs::detail {
  namespace {
    bool contains(const Signature& signature, ComponentId componentId) noexcept {
      return std::binary_search(signature.begin(), signature.end(), componentId);
    }

    std::span<const ComponentId> listAt(const std::vector<std::vector<ComponentId>>& lists, ComponentId componentId) noexcept {
      if (componentId >= lists.size()) {
        return {};
      }

      return lists[componentId];
    }

    void insertSorted(std::vector<ComponentId>& list, ComponentId componentId) {
      auto position = std::lower_bound(list.begin(), list.end(), componentId);
      if (position == list.end() || *position != componentId) {
        list.insert(position, componentId);
      }
    }
  }

  void ImplicationTable::add(ComponentId implying, ComponentId implied, ValueInitializer initializer) {
    const size_t needed = size_t(std::max(implying, implied)) + 1;
    if (needed > implied_.size()) {
      implied_.resize(needed);
      impliers_.resize(needed);
      initializers_.resize(needed, nullptr);
    }

    insertSorted(implied_[implying], implied);
    insertSorted(impliers_[implied], implying);
    initializers_[implied] = initializer;
    empty_ = false;
  }

  std::span<const ComponentId> ImplicationTable::implicationsOf(ComponentId implying) const noexcept {
    return listAt(implied_, implying);
  }

  std::span<const ComponentId> ImplicationTable::impliersOf(ComponentId implied) const noexcept {
    return listAt(impliers_, implied);
  }

  ValueInitializer ImplicationTable::initializerOf(ComponentId implied) const noexcept {
    return implied < initializers_.size() ? initializers_[implied] : nullptr;
  }

  Signature ImplicationTable::resolve(const Signature& source, Signature changed) const {
    if (empty_) {
      return changed;
    }

    for (bool removed = true; removed;) {
      removed = false;
      for (auto it = changed.begin(); it != changed.end();) {
        if (leavesWithImpliers(source, changed, *it)) {
          it = changed.erase(it);
          removed = true;
        } else {
          ++it;
        }
      }
    }

    std::vector<ComponentId> pending(changed.begin(), changed.end());
    while (!pending.empty()) {
      const ComponentId implying = pending.back();
      pending.pop_back();
      for (ComponentId implied : implicationsOf(implying)) {
        if (!contains(changed, implied)) {
          insertSorted(changed, implied);
          pending.push_back(implied);
        }
      }
    }

    return changed;
  }

  bool ImplicationTable::leavesWithImpliers(const Signature& source, const Signature& changed, ComponentId componentId) const noexcept {
    bool implierInSource = false;
    for (ComponentId implier : impliersOf(componentId)) {
      if (contains(changed, implier)) {
        return false;
      }
      implierInSource = implierInSource || contains(source, implier);
    }

    return implierInSource;
  }
}
