#include <span>

#include "compose.hpp"
#include "oly/app/engine.hpp"
#include "oly/app/system_parameters.hpp"
#include "oly/essentials3d/plugin.hpp"

namespace oly::essentials3d {
  namespace {
    // Breadth-first from the parents that are roots themselves, so a parent always precedes its children.
    void rebuildOrder(detail::Hierarchy& hierarchy, app::Lookup<const Parent> parents) {
      hierarchy.order.clear();

      for (const auto& [key, children] : hierarchy.children) {
        const ecs::Entity parent{static_cast<uint32_t>(key >> 32), static_cast<uint32_t>(key)};

        if (!parents.has(parent)) {
          hierarchy.order.insert(hierarchy.order.end(), children.begin(), children.end());
        }
      }

      for (size_t next = 0; next < hierarchy.order.size(); ++next) {
        const auto found = hierarchy.children.find(detail::Hierarchy::keyOf(hierarchy.order[next]));

        if (found != hierarchy.children.end()) {
          hierarchy.order.insert(hierarchy.order.end(), found->second.begin(), found->second.end());
        }
      }

      hierarchy.orderDirty = false;
    }

    void propagateTransforms(app::Query<const Position, WorldTransform, ecs::Without<Parent>> positions, app::Query<const Rotation, WorldTransform, ecs::Without<Parent>> rotations, app::Query<const Scale, WorldTransform, ecs::Without<Parent>> scales, app::Res<detail::Hierarchy> hierarchy, app::Lookup<WorldTransform> worlds, app::Lookup<const Position> localPositions, app::Lookup<const Rotation> localRotations, app::Lookup<const Scale> localScales, app::Lookup<const Parent> parents) {
      positions.eachBlock([](std::span<const Position> positionRows, std::span<WorldTransform> worldRows) {
        for (size_t row = 0; row < positionRows.size(); ++row) {
          worldRows[row] = WorldTransform{positionRows[row].x, positionRows[row].y, positionRows[row].z, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f};
        }
      });
      rotations.eachBlock([](std::span<const Rotation> rotationRows, std::span<WorldTransform> worldRows) {
        for (size_t row = 0; row < rotationRows.size(); ++row) {
          worldRows[row].rotationX = rotationRows[row].x;
          worldRows[row].rotationY = rotationRows[row].y;
          worldRows[row].rotationZ = rotationRows[row].z;
          worldRows[row].rotationW = rotationRows[row].w;
        }
      });
      scales.eachBlock([](std::span<const Scale> scaleRows, std::span<WorldTransform> worldRows) {
        for (size_t row = 0; row < scaleRows.size(); ++row) {
          worldRows[row].scaleX = scaleRows[row].x;
          worldRows[row].scaleY = scaleRows[row].y;
          worldRows[row].scaleZ = scaleRows[row].z;
        }
      });

      if (hierarchy->orderDirty) {
        rebuildOrder(*hierarchy, parents);
      }

      for (const ecs::Entity child : hierarchy->order) {
        WorldTransform* destination = worlds.tryGet(child);
        const Position* position = localPositions.tryGet(child);

        if (destination == nullptr || position == nullptr) {
          continue;
        }

        const WorldTransform* parentWorld = worlds.tryGet(parents.get(child).entity);
        const Rotation* rotation = localRotations.tryGet(child);
        const Scale* scale = localScales.tryGet(child);

        *destination = compose(parentWorld != nullptr ? *parentWorld : IdentityWorld, *position, rotation != nullptr ? *rotation : IdentityRotation,
                               scale != nullptr ? *scale : UnitScale);
      }
    }
  }

  void detail::addPropagation(app::Engine& engine) {
    engine.addSystem<&propagateTransforms>(Propagation{}, "propagateTransforms");
  }
}
