#include <span>

#include "compose.hpp"
#include "oly/essentials2d/plugin.hpp"

namespace oly::essentials2d {
  void Plugin::propagateTransforms(app::SystemContext& context) {
    Plugin& plugin = *static_cast<Plugin*>(context.userData());
    plugin.propagateRoots(context);
    plugin.propagateChildren(context);
  }

  void Plugin::propagateRoots(app::SystemContext& context) {
    context.query<const Position, WorldTransform>(ecs::Without<Parent>{}).eachBlock([](std::span<const Position> positions, std::span<WorldTransform> worlds) {
      for (size_t row = 0; row < positions.size(); ++row) {
        worlds[row] = WorldTransform{positions[row].x, positions[row].y, 0.0f, 1.0f, 1.0f};
      }
    });
    context.query<const Rotation, WorldTransform>(ecs::Without<Parent>{}).eachBlock([](std::span<const Rotation> rotations, std::span<WorldTransform> worlds) {
      for (size_t row = 0; row < rotations.size(); ++row) {
        worlds[row].angle = rotations[row].angle;
      }
    });
    context.query<const Scale, WorldTransform>(ecs::Without<Parent>{}).eachBlock([](std::span<const Scale> scales, std::span<WorldTransform> worlds) {
      for (size_t row = 0; row < scales.size(); ++row) {
        worlds[row].scaleX = scales[row].x;
        worlds[row].scaleY = scales[row].y;
      }
    });
  }

  void Plugin::propagateChildren(app::SystemContext&) {
    ecs::World& world = *world_;

    if (orderDirty_) {
      rebuildOrder(world);
    }

    for (const ecs::Entity child : order_) {
      WorldTransform* destination = world.tryGet<WorldTransform>(child);

      if (destination == nullptr) {
        continue;
      }

      const WorldTransform* parentWorld = world.tryGet<WorldTransform>(world.get<Parent>(child).entity);
      const Rotation* rotation = world.tryGet<Rotation>(child);
      const Scale* scale = world.tryGet<Scale>(child);

      *destination = compose(parentWorld != nullptr ? *parentWorld : IdentityWorld, world.get<Position>(child), rotation != nullptr ? rotation->angle : 0.0f,
                             scale != nullptr ? scale->x : 1.0f, scale != nullptr ? scale->y : 1.0f);
    }
  }

  // Breadth-first from the parents that are roots themselves, so a parent always precedes its children.
  void Plugin::rebuildOrder(ecs::World& world) {
    order_.clear();

    for (const auto& [key, children] : children_) {
      const ecs::Entity parent{static_cast<uint32_t>(key >> 32), static_cast<uint32_t>(key)};

      if (!world.has<Parent>(parent)) {
        order_.insert(order_.end(), children.begin(), children.end());
      }
    }

    for (size_t next = 0; next < order_.size(); ++next) {
      const auto found = children_.find(keyOf(order_[next]));

      if (found != children_.end()) {
        order_.insert(order_.end(), found->second.begin(), found->second.end());
      }
    }

    orderDirty_ = false;
  }
}
