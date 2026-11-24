#pragma once

#include <cstdint>
#include <vector>

#include "tile_map.hpp"

namespace enxame {
  struct Direction {
    int8_t x;
    int8_t y;
  };

  // SoA: the horde reads only the directions, so they do not share cache lines with the distances.
  class FlowField {
   private:
    std::vector<uint32_t> distance_;
    std::vector<int8_t> directionX_;
    std::vector<int8_t> directionY_;
    std::vector<uint32_t> queue_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t builtVersion_ = 0;

   public:
    static constexpr uint32_t Unreachable = UINT32_MAX;

    // Breadth-first search from the base over 8 neighbours; a diagonal move needs both orthogonal tiles free (no corner
    // cutting). Records map.version() as builtVersion().
    void build(const TileMap& map);

    uint32_t distance(TilePosition tile) const noexcept; // Unreachable for blocked, enclosed or outside tiles

    // The neighbour with the smallest distance, first in the order N, S, W, E, NW, NE, SW, SE on a tie; {0, 0} at the
    // base and where distance is Unreachable.
    Direction direction(TilePosition tile) const noexcept;

    uint32_t builtVersion() const noexcept {
      return builtVersion_;
    }

   private:
    bool contains(TilePosition tile) const noexcept;
  };
}
