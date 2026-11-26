#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "oly/app/plugin.hpp"
#include "oly/ecs/observer.hpp"
#include "oly/ecs/world.hpp"

namespace oly::essentials2d {
  // The phase of the propagation, in the fixed step, after app::Update and before app::UpdateAfter.
  struct Propagation {};

  namespace detail {
    // Marks an entity that has children; C++ only, so it is not in the .oly.
    struct HasChildren {};

    // The state of the plugin, a World resource; the observers get its address as userData. It changes only inside observers,
    // so it is off the hot path.
    struct Hierarchy {
      // Keyed by (index << 32) | generation of the parent.
      std::unordered_map<uint64_t, std::vector<ecs::Entity>> children;
      // Position of each child in its parent's list, keyed like children by the child, so a child leaves in O(1).
      std::unordered_map<uint64_t, uint32_t> slots;
      std::vector<ecs::Entity> order; // every entity with a Parent, each after its own parent
      bool orderDirty = true;

      static uint64_t keyOf(ecs::Entity entity) noexcept {
        return (uint64_t(entity.index) << 32) | entity.generation;
      }
    };

    // Registers the propagation system in the Propagation phase.
    void addPropagation(app::Engine& engine);
  }

  // Position, Rotation and Scale are local; WorldTransform is derived from them and from the Parent chain, and the
  // plugin writes it: once per fixed step in the Propagation phase, after Update, and at once when it is added. A missing Rotation
  // is the identity and a missing Scale is 1. Re-parenting and un-parenting keep the local values. Destroying an entity
  // destroys its descendants, whichever way the destroy happens.
  //
  // Position implies WorldTransform on the World, and that cannot be undone: the destructor removes the observers but
  // the implication stays, so a World that outlives its Engine keeps giving Position a WorldTransform that nothing
  // updates.
  //
  // A Parent that points to a dead entity, to its own entity or to one of its descendants is a contract error (OLY_ASSERT).
  class Plugin final : public app::Plugin {
   private:
    static constexpr uint32_t ObserverCount = 4;

    ecs::World* world_ = nullptr;
    std::array<ecs::ObserverId, ObserverCount> observers_{};

   public:
    ~Plugin() override;

    void install(app::Engine& engine) override;

    // For tests: whether the children index and the HasChildren markers agree with the Parent components of the world.
    bool checkHierarchy(ecs::World& world, std::string* why = nullptr) const;

   private:
    static void onParentAdded(ecs::ObserverContext& context);
    static void onParentRemoved(ecs::ObserverContext& context);
    static void onWorldTransformAdded(ecs::ObserverContext& context);
    static void onHasChildrenRemoved(ecs::ObserverContext& context);
  };
}
