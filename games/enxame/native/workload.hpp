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

  struct VehicleParameters {
    uint32_t columns; // vehicle centres sit on a grid this wide
    float spacing;
    float radius; // of the circle each vehicle drives
    float angularSpeed; // radians per second
    float turretOffset; // each turret sits at local (+-offset, +-offset)
  };

  struct WaveParameters {
    uint32_t stepsPerWave;
    float modifierProbability; // each of the eight modifier tags, per enemy
    float areaSide; // the spawn area is a square of this side centred on the origin
  };

  struct CombatParameters {
    float reloadSeconds;
    float range;
    float projectileSpeed; // world units per second
    float projectileLife; // seconds
    float hitRadius;
  };
}
