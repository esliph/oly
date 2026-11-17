#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "oly/app/plugin.hpp"
#include "oly/app/system.hpp"
#include "oly/ecs/observer.hpp"
#include "oly/ecs/world.hpp"

namespace oly::essentials2d {
  namespace detail {
    // Marks an entity that has children; C++ only, so it is not in the .oly.
    struct HasChildren {};
  }

  // Position, Rotation and Scale are local; WorldTransform is derived from them and from the Parent chain, and the
  // plugin writes it: once per fixed step in the Last band of Update, and at once when it is added. A missing Rotation
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
    // Keyed by (index << 32) | generation of the parent. It changes only inside observers, so it is off the hot path.
    std::unordered_map<uint64_t, std::vector<ecs::Entity>> children_;
    std::vector<ecs::Entity> order_; // every entity with a Parent, each after its own parent
    bool orderDirty_ = true;

   public:
    ~Plugin() override;

    void install(app::Engine& engine) override;

    // For tests: whether the children index and the HasChildren markers agree with the Parent components of the world.
    bool checkHierarchy(ecs::World& world, std::string* why = nullptr) const;

   private:
    static uint64_t keyOf(ecs::Entity parent) noexcept {
      return (uint64_t(parent.index) << 32) | parent.generation;
    }

    static void onParentAdded(ecs::ObserverContext& context);
    static void onParentRemoved(ecs::ObserverContext& context);
    static void onWorldTransformAdded(ecs::ObserverContext& context);
    static void onHasChildrenRemoved(ecs::ObserverContext& context);
    static void propagateTransforms(app::SystemContext& context);

    void rebuildOrder(ecs::World& world);
    void propagateRoots(app::SystemContext& context);
    void propagateChildren(app::SystemContext& context);
  };
}
