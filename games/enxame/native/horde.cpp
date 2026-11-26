#include <cmath>

#include "systems.hpp"

namespace enxame {
  namespace {
    // A random free tile on the border of the map. Bounded: a map that blocks its whole border still gets an answer.
    TilePosition randomBorderTile(const TileMap& map, Random& random) {
      const uint32_t perimeter = 2 * (map.width() + map.height()) - 4;
      TilePosition tile{0, 0};

      for (uint32_t attempt = 0; attempt < 64; ++attempt) {
        const uint32_t slot = random.nextBelow(perimeter);

        if (slot < map.width()) {
          tile = {static_cast<int32_t>(slot), 0};
        } else if (slot < 2 * map.width()) {
          tile = {static_cast<int32_t>(slot - map.width()), static_cast<int32_t>(map.height()) - 1};
        } else if (slot < 2 * map.width() + map.height() - 2) {
          tile = {0, static_cast<int32_t>(slot - 2 * map.width()) + 1};
        } else {
          tile = {static_cast<int32_t>(map.width()) - 1, static_cast<int32_t>(slot - 2 * map.width() - map.height() + 2) + 1};
        }

        if (!map.blocked(tile)) {
          break;
        }
      }

      return tile;
    }
  }

  void updateFlowField(oly::app::Res<const TileMap> map, oly::app::Res<FlowField> flowField) {
    if (map->version() != flowField->builtVersion()) {
      flowField->build(*map);
    }
  }

  void rebuildSpatialHash(oly::app::Query<const Position, oly::ecs::With<Enemy>> enemies, oly::app::Res<SpatialHash> hash, oly::app::Res<GatheredEnemies> gathered) {
    gathered->entities.clear();
    gathered->xs.clear();
    gathered->ys.clear();

    enemies.each([&](oly::ecs::Entity entity, const Position& position) {
      gathered->entities.push_back(entity);
      gathered->xs.push_back(position.x);
      gathered->ys.push_back(position.y);
    });

    hash->rebuild(gathered->entities, gathered->xs, gathered->ys);
  }

  void moveHorde(oly::app::Query<Position, oly::ecs::With<Enemy>> enemies, oly::app::Res<const SpatialHash> hash, oly::app::Res<const FlowField> flowField, oly::app::Res<const TileMap> map, oly::app::Res<const HordeParameters> horde, oly::app::Res<Random> random, const oly::app::StepInfo& step) {
    const float deltaTime = step.fixedDeltaTime;
    const TilePosition baseTile = map->base();
    const float baseX = map->centerX(baseTile);
    const float baseY = map->centerY(baseTile);

    enemies.each([&](Position& position) {
      const float toBaseX = baseX - position.x;
      const float toBaseY = baseY - position.y;

      if (toBaseX * toBaseX + toBaseY * toBaseY < horde->tileSize * horde->tileSize) {
        const TilePosition tile = randomBorderTile(*map, *random);
        position.x = map->centerX(tile);
        position.y = map->centerY(tile);

        return;
      }

      const Direction direction = flowField->direction(map->tileAt(position.x, position.y));
      float directionX = static_cast<float>(direction.x);
      float directionY = static_cast<float>(direction.y);

      if (direction.x == 0 && direction.y == 0) {
        directionX = toBaseX;
        directionY = toBaseY;
      }

      const float directionLength = std::sqrt(directionX * directionX + directionY * directionY);
      float velocityX = directionX / directionLength * horde->enemySpeed;
      float velocityY = directionY / directionLength * horde->enemySpeed;

      hash->forEachWithin(position.x, position.y, horde->separationRadius, [&](oly::ecs::Entity, float otherX, float otherY) {
        const float deltaX = position.x - otherX;
        const float deltaY = position.y - otherY;
        const float distanceSquared = deltaX * deltaX + deltaY * deltaY;

        if (distanceSquared == 0.0f) {
          return;
        }

        const float distance = std::sqrt(distanceSquared);
        const float push = (1.0f - distance / horde->separationRadius) * horde->separationStrength / distance;
        velocityX += deltaX * push;
        velocityY += deltaY * push;
      });

      position.x += velocityX * deltaTime;
      position.y += velocityY * deltaTime;
    });
  }
}
