#pragma once

#include <cstdint>
#include <optional>
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

  // The state every system reaches through its userData: without resources (phase 5) a system has no other way to
  // share the map, the flow field or the hash.
  struct Simulation {
    Random random;
    Camera camera;
    std::optional<TileMap> map;
    FlowField flowField;
    std::optional<SpatialHash> hash;
    HordeParameters horde{};
    CombatParameters combat{};
    FactoryParameters factory{};
    VehicleParameters vehicles{};
    WaveParameters waves{};
    float toggleProbability = 0.0f;
    uint32_t statusTogglesLastStep = 0;
    uint32_t waveSize = 0; // enemies per wave
    uint32_t stepInWave = 0;
    uint32_t wavesCompleted = 0;
    std::vector<oly::ecs::Entity> assemblerOfBelt; // [belt]: an item reaches its assembler with get
    std::vector<oly::ecs::Entity> gatheredEntities;
    std::vector<float> gatheredXs;
    std::vector<float> gatheredYs;
    std::vector<oly::ecs::Entity> hitEnemies; // cleared at the start of moveProjectiles

    explicit Simulation(uint64_t seed)
      : random(seed) {}
  };
}
