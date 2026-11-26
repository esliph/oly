#include <algorithm>
#include <string_view>

#include "compose.hpp"
#include "oly/app/engine.hpp"
#include "oly/core/assert.hpp"
#include "oly/core/type_name.hpp"
#include "oly/essentials3d/plugin.hpp"

namespace oly::essentials3d {
  namespace {
    // A parent without a WorldTransform (a group with no Position) counts as the identity.
    void computeWorldTransform(ecs::World& world, ecs::Entity entity) {
      const Position* position = world.tryGet<Position>(entity);
      WorldTransform* destination = world.tryGet<WorldTransform>(entity);

      if (position == nullptr || destination == nullptr) {
        return;
      }

      const Rotation* rotation = world.tryGet<Rotation>(entity);
      const Scale* scale = world.tryGet<Scale>(entity);
      const Parent* parent = world.tryGet<Parent>(entity);
      const WorldTransform* parentWorld = parent != nullptr ? world.tryGet<WorldTransform>(parent->entity) : nullptr;

      *destination = compose(parentWorld != nullptr ? *parentWorld : IdentityWorld, *position, rotation != nullptr ? *rotation : IdentityRotation,
                             scale != nullptr ? *scale : UnitScale);
    }

    // Only the asserts call it.
    [[maybe_unused]] bool isSelfOrAncestor(ecs::World& world, ecs::Entity candidate, ecs::Entity from) {
      for (ecs::Entity current = from;;) {
        if (current == candidate) {
          return true;
        }

        const Parent* parent = world.tryGet<Parent>(current);

        if (parent == nullptr) {
          return false;
        }

        current = parent->entity;
      }
    }
  }

  void Plugin::onParentAdded(ecs::ObserverContext& context) {
    detail::Hierarchy& hierarchy = *static_cast<detail::Hierarchy*>(context.userData());
    ecs::World& world = context.world();
    const ecs::Entity entity = context.entity();
    const ecs::Entity parent = world.get<Parent>(entity).entity;
    [[maybe_unused]] constexpr std::string_view typeName = core::typeName<Parent>();

    OLY_ASSERT(world.isAlive(parent), "add e=%u:%u type=%.*s: parent e=%u:%u is dead", entity.index, entity.generation, static_cast<int>(typeName.size()),
               typeName.data(), parent.index, parent.generation);
    OLY_ASSERT(parent != entity, "add e=%u:%u type=%.*s: an entity cannot be its own parent", entity.index, entity.generation, static_cast<int>(typeName.size()),
               typeName.data());
    OLY_ASSERT(!isSelfOrAncestor(world, entity, parent), "add e=%u:%u type=%.*s: parent e=%u:%u is a descendant of the entity", entity.index,
               entity.generation, static_cast<int>(typeName.size()), typeName.data(), parent.index, parent.generation);

    std::vector<ecs::Entity>& siblings = hierarchy.children[detail::Hierarchy::keyOf(parent)];
    const bool firstChild = siblings.empty();
    hierarchy.slots[detail::Hierarchy::keyOf(entity)] = static_cast<uint32_t>(siblings.size());
    siblings.push_back(entity);

    if (firstChild) {
      context.commands().add(parent, detail::HasChildren{});
    }

    hierarchy.orderDirty = true;
    computeWorldTransform(world, entity);
  }

  void Plugin::onParentRemoved(ecs::ObserverContext& context) {
    detail::Hierarchy& hierarchy = *static_cast<detail::Hierarchy*>(context.userData());
    ecs::World& world = context.world();
    const ecs::Entity entity = context.entity();
    const ecs::Entity parent = world.get<Parent>(entity).entity;
    hierarchy.orderDirty = true;

    const auto slot = hierarchy.slots.find(detail::Hierarchy::keyOf(entity));
    const auto found = hierarchy.children.find(detail::Hierarchy::keyOf(parent));

    // A parent already erased by the cascade, or a Parent written through get (a contract violation), leaves nothing to fix.
    if (found == hierarchy.children.end() || slot == hierarchy.slots.end() || slot->second >= found->second.size() || found->second[slot->second] != entity) {
      if (slot != hierarchy.slots.end() && found == hierarchy.children.end()) {
        hierarchy.slots.erase(slot);
      }

      return;
    }

    std::vector<ecs::Entity>& siblings = found->second;
    const uint32_t index = slot->second;
    const ecs::Entity moved = siblings.back();
    siblings[index] = moved;
    hierarchy.slots[detail::Hierarchy::keyOf(moved)] = index;
    siblings.pop_back();
    hierarchy.slots.erase(detail::Hierarchy::keyOf(entity));

    if (siblings.empty()) {
      hierarchy.children.erase(found);

      if (world.isAlive(parent)) {
        context.commands().remove<detail::HasChildren>(parent);
      }
    }
  }

  void Plugin::onWorldTransformAdded(ecs::ObserverContext& context) {
    computeWorldTransform(context.world(), context.entity());
  }

  // The marker leaves either because the last child left (the entry is already gone) or because the parent is being
  // destroyed (children still listed).
  void Plugin::onHasChildrenRemoved(ecs::ObserverContext& context) {
    detail::Hierarchy& hierarchy = *static_cast<detail::Hierarchy*>(context.userData());
    const auto found = hierarchy.children.find(detail::Hierarchy::keyOf(context.entity()));

    if (found == hierarchy.children.end()) {
      return;
    }

    for (const ecs::Entity child : found->second) {
      context.commands().destroy(child);
    }

    hierarchy.children.erase(found);
    hierarchy.orderDirty = true;
  }
}
