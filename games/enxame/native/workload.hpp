#pragma once

#include <cstdint>

namespace enxame {
  struct RunLength {
    uint32_t warmup;
    uint32_t steps;
  };

  struct HordeParameters {
    uint32_t mapWidth;
    uint32_t mapHeight;
    float tileSize;
    float obstacleRate;
    float enemySpeed; // world units per second
    float separationRadius;
    float separationStrength; // world units per second at distance 0
  };

  struct FactoryParameters {
    uint32_t assemblers; // one belt each
    uint32_t beltTiles;
    float tileSize;
    float itemSpeed; // world units per second
    float craftSeconds;
    uint32_t assemblerCapacity;
    float beltSpacing; // distance between two belts, along y
  };

  struct CombatParameters {
    float reloadSeconds;
    float range;
    float projectileSpeed; // world units per second
    float projectileLife; // seconds
    float hitRadius;
  };
}
