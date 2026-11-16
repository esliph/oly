#pragma once

#include <span>
#include <vector>

#include "oly/ecs/component.hpp"
#include "oly/ecs/detail/archetype.hpp"

namespace oly::ecs::detail {
  // Constructs a value-initialized component in an unconstructed slot.
  using ValueInitializer = void (*)(void* slot) noexcept;

  // Cold data: read when an edge, an archetype or a flush memo is built, never per cached transition.
  class ImplicationTable {
   private:
    std::vector<std::vector<ComponentId>> implied_; // [implying]: direct implications
    std::vector<std::vector<ComponentId>> impliers_; // [implied]: direct impliers
    std::vector<ValueInitializer> initializers_; // [implied]
    bool empty_ = true;

   public:
    void add(ComponentId implying, ComponentId implied, ValueInitializer initializer);

    bool empty() const noexcept {
      return empty_;
    }
    std::span<const ComponentId> implicationsOf(ComponentId implying) const noexcept;
    std::span<const ComponentId> impliersOf(ComponentId implied) const noexcept;
    ValueInitializer initializerOf(ComponentId implied) const noexcept;

    // `changed` is `source` after the raw operations (both sorted). Removes, transitively, each implied component
    // none of whose impliers is left in `changed` while one was in `source` (R-IM-3), then adds the closure of
    // what is left (R-IM-2). Returns `changed` untouched when empty().
    Signature resolve(const Signature& source, Signature changed) const;

   private:
    bool leavesWithImpliers(const Signature& source, const Signature& changed, ComponentId componentId) const noexcept;
  };
}
