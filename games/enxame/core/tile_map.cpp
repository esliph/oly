#include "tile_map.hpp"

#include <cmath>
#include <cstdlib>

namespace enxame {
  TileMap::TileMap(uint32_t width, uint32_t height, float tileSize, TilePosition base)
    : blocked_(static_cast<size_t>(width) * height, 0),
      width_(width),
      height_(height),
      tileSize_(tileSize),
      base_(base) {}

  TileMap TileMap::generate(uint32_t width, uint32_t height, float tileSize, float obstacleRate, Random& random) {
    TileMap map(width, height, tileSize, {static_cast<int32_t>(width / 2), static_cast<int32_t>(height / 2)});

    for (int32_t y = 0; y < static_cast<int32_t>(height); ++y) {
      for (int32_t x = 0; x < static_cast<int32_t>(width); ++x) {
        const bool nearBase = std::abs(x - map.base_.x) <= 1 && std::abs(y - map.base_.y) <= 1;

        if (!nearBase && random.chance(obstacleRate)) {
          map.blocked_[static_cast<size_t>(y) * width + x] = 1;
        }
      }
    }

    return map;
  }

  bool TileMap::inside(TilePosition tile) const noexcept {
    return tile.x >= 0 && tile.y >= 0 && tile.x < static_cast<int32_t>(width_) && tile.y < static_cast<int32_t>(height_);
  }

  bool TileMap::blocked(TilePosition tile) const noexcept {
    return inside(tile) && blocked_[static_cast<size_t>(tile.y) * width_ + tile.x] != 0;
  }

  bool TileMap::block(TilePosition tile) {
    if (!inside(tile) || blocked(tile) || (tile.x == base_.x && tile.y == base_.y)) {
      return false;
    }

    blocked_[static_cast<size_t>(tile.y) * width_ + tile.x] = 1;
    ++version_;

    return true;
  }

  TilePosition TileMap::tileAt(float x, float y) const noexcept {
    return {static_cast<int32_t>(std::floor(x / tileSize_)), static_cast<int32_t>(std::floor(y / tileSize_))};
  }

  float TileMap::centerX(TilePosition tile) const noexcept {
    return (static_cast<float>(tile.x) + 0.5f) * tileSize_;
  }

  float TileMap::centerY(TilePosition tile) const noexcept {
    return (static_cast<float>(tile.y) + 0.5f) * tileSize_;
  }
}
