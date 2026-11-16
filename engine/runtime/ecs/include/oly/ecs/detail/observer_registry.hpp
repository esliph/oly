#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "oly/ecs/component.hpp"
#include "oly/ecs/detail/archetype.hpp"
#include "oly/ecs/observer.hpp"

namespace oly::ecs::detail {
  // Bits of Archetype::observed and of Edge::work (I-12).
  inline constexpr uint8_t ObservedOnAdd = 1;
  inline constexpr uint8_t ObservedOnRemove = 2;

  struct ObserverEntry {
    ObserverFunction function;
    void* userData;
    ObserverId id;
  };

  class ObserverRegistry {
   private:
    struct Registration {
      ObserverKind kind;
      ComponentId componentId;
      bool removed;
    };

    std::array<std::vector<std::vector<ObserverEntry>>, 2> entries_; // [kind][componentId], in registration order
    std::vector<uint8_t> masks_; // [componentId]
    std::vector<Registration> registrations_; // [ObserverId]

   public:
    ObserverId add(ObserverKind kind, ComponentId componentId, ObserverFunction function, void* userData);
    void remove(ObserverId observer); // OLY_ASSERT on an unknown or removed id

    uint8_t mask(ComponentId componentId) const noexcept {
      return componentId < masks_.size() ? masks_[componentId] : 0;
    }
    std::span<const ObserverEntry> observers(ObserverKind kind, ComponentId componentId) const noexcept;

    uint8_t signatureMask(const Signature& signature) const noexcept;
    // ObservedOnAdd for what `destination` gains, ObservedOnRemove for what `source` loses.
    uint8_t transitionMask(const Signature& source, const Signature& destination) const noexcept;

   private:
    void refreshMask(ComponentId componentId) noexcept;
  };

  inline uint8_t observedBit(ObserverKind kind) noexcept {
    return kind == ObserverKind::OnAdd ? ObservedOnAdd : ObservedOnRemove;
  }
}
