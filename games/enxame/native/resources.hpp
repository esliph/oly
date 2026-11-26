#pragma once

#include <cstdint>
#include <vector>

#include "flow_field.hpp"
#include "oly/ecs/entity.hpp"
#include "random.hpp"
#include "spatial_hash.hpp"
#include "tile_map.hpp"
#include "workload.hpp"

namespace enxame {
  // The logical size of the window: pointer positions and the camera centre are in these units.
  inline constexpr int LogicalWidth = 640;
  inline constexpr int LogicalHeight = 360;

  struct Camera {
    float x = 0.0f;
    float y = 0.0f;
    float zoom = 1.0f;
  };

  struct GatheredEnemies {
    std::vector<oly::ecs::Entity> entities;
    std::vector<float> xs;
    std::vector<float> ys;
  };

  // Cleared at the start of moveProjectiles.
  struct HitEnemies {
    std::vector<oly::ecs::Entity> entities;
  };

  struct Belts {
    std::vector<oly::ecs::Entity> assemblerOfBelt; // [belt]: an item reaches its assembler with get
  };

  struct StatusToggles {
    float probability;
    uint32_t lastStep;
  };

  struct WaveClock {
    uint32_t waveSize; // enemies per wave
    uint32_t stepInWave;
    uint32_t wavesCompleted;
  };
}
