#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "oly/ecs/entity.hpp"

namespace enxame {
  // A uniform grid rebuilt by counting sort. SoA entries sorted by cell: the separation loop reads the positions of
  // neighbours and never their entities.
  class SpatialHash {
   private:
    std::vector<uint32_t> cellStart_; // cells + 1
    std::vector<oly::ecs::Entity> entities_;
    std::vector<float> xs_;
    std::vector<float> ys_;
    std::vector<uint32_t> cellOfEntry_;
    std::vector<uint32_t> cursor_;
    float originX_;
    float originY_;
    float cellSize_;
    int32_t columns_;
    int32_t rows_;

   public:
    SpatialHash(float originX, float originY, float width, float height, float cellSize);

    // A point outside the area goes to the nearest edge cell.
    void rebuild(std::span<const oly::ecs::Entity> entities, std::span<const float> xs, std::span<const float> ys);

    uint32_t size() const noexcept {
      return static_cast<uint32_t>(entities_.size());
    }

    // Calls visit(entity, x, y) for every entry at distance <= radius, cell by cell in row order, then in rebuild order.
    template <class Visit>
    void forEachWithin(float x, float y, float radius, Visit&& visit) const {
      const int32_t firstColumn = columnOf(x - radius);
      const int32_t lastColumn = columnOf(x + radius);
      const int32_t firstRow = rowOf(y - radius);
      const int32_t lastRow = rowOf(y + radius);
      const float radiusSquared = radius * radius;

      for (int32_t row = firstRow; row <= lastRow; ++row) {
        for (int32_t column = firstColumn; column <= lastColumn; ++column) {
          const uint32_t cell = static_cast<uint32_t>(row * columns_ + column);

          for (uint32_t entry = cellStart_[cell]; entry < cellStart_[cell + 1]; ++entry) {
            const float deltaX = xs_[entry] - x;
            const float deltaY = ys_[entry] - y;

            if (deltaX * deltaX + deltaY * deltaY <= radiusSquared) {
              visit(entities_[entry], xs_[entry], ys_[entry]);
            }
          }
        }
      }
    }

   private:
    int32_t columnOf(float x) const noexcept {
      return std::clamp(static_cast<int32_t>(std::floor((x - originX_) / cellSize_)), 0, columns_ - 1);
    }

    int32_t rowOf(float y) const noexcept {
      return std::clamp(static_cast<int32_t>(std::floor((y - originY_) / cellSize_)), 0, rows_ - 1);
    }
  };
}
