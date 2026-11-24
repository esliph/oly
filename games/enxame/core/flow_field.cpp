#include "flow_field.hpp"

#include <array>

namespace enxame {
  namespace {
    // N, S, W, E first, then the diagonals: the tie order of FlowField::direction.
    constexpr std::array<Direction, 8> Neighbours{{{0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}}};

    bool canStep(const TileMap& map, TilePosition from, Direction step) {
      const TilePosition target{from.x + step.x, from.y + step.y};

      if (!map.inside(target) || map.blocked(target)) {
        return false;
      }

      if (step.x != 0 && step.y != 0) {
        return !map.blocked({from.x + step.x, from.y}) && !map.blocked({from.x, from.y + step.y});
      }

      return true;
    }
  }

  void FlowField::build(const TileMap& map) {
    width_ = map.width();
    height_ = map.height();
    builtVersion_ = map.version();
    const size_t tileCount = static_cast<size_t>(width_) * height_;
    distance_.assign(tileCount, Unreachable);
    directionX_.assign(tileCount, 0);
    directionY_.assign(tileCount, 0);
    queue_.clear();

    const TilePosition base = map.base();
    const uint32_t baseIndex = static_cast<uint32_t>(base.y) * width_ + static_cast<uint32_t>(base.x);
    distance_[baseIndex] = 0;
    queue_.push_back(baseIndex);

    for (size_t head = 0; head < queue_.size(); ++head) {
      const uint32_t index = queue_[head];
      const TilePosition from{static_cast<int32_t>(index % width_), static_cast<int32_t>(index / width_)};

      for (const Direction step : Neighbours) {
        if (!canStep(map, from, step)) {
          continue;
        }

        const uint32_t next = static_cast<uint32_t>(from.y + step.y) * width_ + static_cast<uint32_t>(from.x + step.x);

        if (distance_[next] == Unreachable) {
          distance_[next] = distance_[index] + 1;
          queue_.push_back(next);
        }
      }
    }

    for (uint32_t index = 0; index < tileCount; ++index) {
      if (distance_[index] == Unreachable || distance_[index] == 0) {
        continue;
      }

      const TilePosition from{static_cast<int32_t>(index % width_), static_cast<int32_t>(index / width_)};
      uint32_t best = Unreachable;

      for (const Direction step : Neighbours) {
        if (!canStep(map, from, step)) {
          continue;
        }

        const uint32_t next = distance_[static_cast<uint32_t>(from.y + step.y) * width_ + static_cast<uint32_t>(from.x + step.x)];

        if (next < best) {
          best = next;
          directionX_[index] = step.x;
          directionY_[index] = step.y;
        }
      }
    }
  }

  uint32_t FlowField::distance(TilePosition tile) const noexcept {
    return contains(tile) ? distance_[static_cast<size_t>(tile.y) * width_ + tile.x] : Unreachable;
  }

  Direction FlowField::direction(TilePosition tile) const noexcept {
    if (!contains(tile)) {
      return {0, 0};
    }

    const size_t index = static_cast<size_t>(tile.y) * width_ + tile.x;

    return {directionX_[index], directionY_[index]};
  }

  bool FlowField::contains(TilePosition tile) const noexcept {
    return tile.x >= 0 && tile.y >= 0 && tile.x < static_cast<int32_t>(width_) && tile.y < static_cast<int32_t>(height_);
  }
}
