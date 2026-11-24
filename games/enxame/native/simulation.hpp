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
    std::vector<oly::ecs::Entity> gatheredEntities;
    std::vector<float> gatheredXs;
    std::vector<float> gatheredYs;

    explicit Simulation(uint64_t seed)
      : random(seed) {}
  };
}
