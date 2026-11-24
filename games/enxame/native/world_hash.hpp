#pragma once

#include <cstdint>

#include "oly/ecs/world.hpp"

namespace enxame {
  // A fingerprint of the playable world: every entity with a Position, sorted by (index, generation), with its
  // components in the order of enxame.oly. Two runs of the same input must give the same value.
  uint64_t worldHash(oly::ecs::World& world);
}
