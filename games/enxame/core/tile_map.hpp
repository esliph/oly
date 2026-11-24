#pragma once

#include <cstdint>
#include <vector>

#include "random.hpp"

namespace enxame {
  // y grows downwards, index = y * width + x; tile (x, y) covers [x, x + 1) * tileSize.
  struct TilePosition {
    int32_t x;
    int32_t y;
  };

  class TileMap {
   private:
    std::vector<uint8_t> blocked_;
    uint32_t width_;
    uint32_t height_;
    float tileSize_;
    TilePosition base_;
    uint32_t version_ = 0;

   public:
    TileMap(uint32_t width, uint32_t height, float tileSize, TilePosition base);

    // The base is the centre tile; it and its 8 neighbours are never blocked. Each other tile is blocked with
    // probability obstacleRate, drawn once per tile in index order.
    static TileMap generate(uint32_t width, uint32_t height, float tileSize, float obstacleRate, Random& random);

    uint32_t width() const noexcept {
      return width_;
    }

    uint32_t height() const noexcept {
      return height_;
    }

    float tileSize() const noexcept {
      return tileSize_;
    }

    TilePosition base() const noexcept {
      return base_;
    }

    uint32_t version() const noexcept {
      return version_;
    }

    bool inside(TilePosition tile) const noexcept;
    bool blocked(TilePosition tile) const noexcept; // false outside the map

    // false, and no change, when the tile is outside, already blocked or the base. Otherwise blocks it and bumps version().
    bool block(TilePosition tile);

    TilePosition tileAt(float x, float y) const noexcept; // floor of x / tileSize, y / tileSize; may be outside
    float centerX(TilePosition tile) const noexcept;
    float centerY(TilePosition tile) const noexcept;
  };
}
