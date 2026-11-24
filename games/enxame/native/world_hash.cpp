#include "world_hash.hpp"

#include <algorithm>
#include <type_traits>
#include <vector>

#include "hashing.hpp"
#include "schema.generated.hpp"

namespace enxame {
  namespace {
    // The components are plain floats: a size that is not the sum of the members means padding, whose bytes are not
    // defined and would make the hash differ between runs.
    static_assert(sizeof(Position) == 2 * sizeof(float));
    static_assert(sizeof(Turret) == sizeof(float));
    static_assert(sizeof(Projectile) == 3 * sizeof(float));

    template <class T>
    uint64_t foldBytes(uint64_t hash, const T& value) {
      static_assert(std::is_trivially_copyable_v<T>);

      return fnv1a(hash, &value, sizeof(T));
    }

    // A one-byte presence flag, then the component's bytes when the entity has it. A tag has no bytes.
    template <class T>
    uint64_t foldComponent(uint64_t hash, oly::ecs::World& world, oly::ecs::Entity entity) {
      const uint8_t present = world.has<T>(entity) ? 1 : 0;
      hash = fnv1a(hash, &present, sizeof(present));

      if constexpr (!std::is_empty_v<T>) {
        if (present) {
          hash = foldBytes(hash, world.get<T>(entity));
        }
      }

      return hash;
    }
  }

  uint64_t worldHash(oly::ecs::World& world) {
    std::vector<oly::ecs::Entity> entities;

    world.query<const Position>().each([&](oly::ecs::Entity entity, const Position&) {
      entities.push_back(entity);
    });

    std::sort(entities.begin(), entities.end(), [](oly::ecs::Entity left, oly::ecs::Entity right) {
      return left.index != right.index ? left.index < right.index : left.generation < right.generation;
    });

    uint64_t hash = FnvOffsetBasis;

    for (const oly::ecs::Entity entity : entities) {
      hash = fnv1a(hash, &entity.index, sizeof(entity.index));
      hash = fnv1a(hash, &entity.generation, sizeof(entity.generation));
      hash = foldBytes(hash, world.get<Position>(entity));
      hash = foldComponent<Enemy>(hash, world, entity);
      hash = foldComponent<Turret>(hash, world, entity);
      hash = foldComponent<Projectile>(hash, world, entity);
    }

    return hash;
  }
}
