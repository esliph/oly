#include <cmath>

#include "oly/app/system.hpp"
#include "oly/ecs/query.hpp"
#include "schema.generated.hpp"
#include "simulation.hpp"
#include "systems.hpp"

namespace {
  // A random free tile on the border of the map. Bounded: a map that blocks its whole border still gets an answer.
  enxame::TilePosition randomBorderTile(enxame::Simulation& simulation) {
    const enxame::TileMap& map = *simulation.map;
    const uint32_t perimeter = 2 * (map.width() + map.height()) - 4;
    enxame::TilePosition tile{0, 0};

    for (uint32_t attempt = 0; attempt < 64; ++attempt) {
      const uint32_t slot = simulation.random.nextBelow(perimeter);

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

OLY_SYSTEM(updateFlowField) {
  enxame::Simulation& simulation = *static_cast<enxame::Simulation*>(context.userData());

  if (simulation.map->version() != simulation.flowField.builtVersion()) {
    simulation.flowField.build(*simulation.map);
  }
}

OLY_SYSTEM(rebuildSpatialHash) {
  enxame::Simulation& simulation = *static_cast<enxame::Simulation*>(context.userData());
  simulation.gatheredEntities.clear();
  simulation.gatheredXs.clear();
  simulation.gatheredYs.clear();

  context.query<const Position>(oly::ecs::With<Enemy>{}).each([&](oly::ecs::Entity entity, const Position& position) {
    simulation.gatheredEntities.push_back(entity);
    simulation.gatheredXs.push_back(position.x);
    simulation.gatheredYs.push_back(position.y);
  });

  simulation.hash->rebuild(simulation.gatheredEntities, simulation.gatheredXs, simulation.gatheredYs);
}

OLY_SYSTEM(moveHorde) {
  enxame::Simulation& simulation = *static_cast<enxame::Simulation*>(context.userData());
  const enxame::TileMap& map = *simulation.map;
  const enxame::HordeParameters& horde = simulation.horde;
  const float deltaTime = context.fixedDeltaTime();
  const enxame::TilePosition baseTile = map.base();
  const float baseX = map.centerX(baseTile);
  const float baseY = map.centerY(baseTile);

  context.query<Position>(oly::ecs::With<Enemy>{}).each([&](Position& position) {
    const float toBaseX = baseX - position.x;
    const float toBaseY = baseY - position.y;

    if (toBaseX * toBaseX + toBaseY * toBaseY < horde.tileSize * horde.tileSize) {
      const enxame::TilePosition tile = randomBorderTile(simulation);
      position.x = map.centerX(tile);
      position.y = map.centerY(tile);

      return;
    }

    const enxame::Direction direction = simulation.flowField.direction(map.tileAt(position.x, position.y));
    float directionX = static_cast<float>(direction.x);
    float directionY = static_cast<float>(direction.y);

    if (direction.x == 0 && direction.y == 0) {
      directionX = toBaseX;
      directionY = toBaseY;
    }

    const float directionLength = std::sqrt(directionX * directionX + directionY * directionY);
    float velocityX = directionX / directionLength * horde.enemySpeed;
    float velocityY = directionY / directionLength * horde.enemySpeed;

    simulation.hash->forEachWithin(position.x, position.y, horde.separationRadius, [&](oly::ecs::Entity, float otherX, float otherY) {
      const float deltaX = position.x - otherX;
      const float deltaY = position.y - otherY;
      const float distanceSquared = deltaX * deltaX + deltaY * deltaY;

      if (distanceSquared == 0.0f) {
        return;
      }

      const float distance = std::sqrt(distanceSquared);
      const float push = (1.0f - distance / horde.separationRadius) * horde.separationStrength / distance;
      velocityX += deltaX * push;
      velocityY += deltaY * push;
    });

    position.x += velocityX * deltaTime;
    position.y += velocityY * deltaTime;
  });
}
