#include "spatial_hash.hpp"

#include <cstddef>

namespace enxame {
  SpatialHash::SpatialHash(float originX, float originY, float width, float height, float cellSize)
    : originX_(originX),
      originY_(originY),
      cellSize_(cellSize),
      columns_(std::max(1, static_cast<int32_t>(std::ceil(width / cellSize)))),
      rows_(std::max(1, static_cast<int32_t>(std::ceil(height / cellSize)))) {
    cellStart_.assign(static_cast<size_t>(columns_) * rows_ + 1, 0);
  }

  void SpatialHash::rebuild(std::span<const oly::ecs::Entity> entities, std::span<const float> xs, std::span<const float> ys) {
    const size_t count = entities.size();
    cellOfEntry_.resize(count);
    std::fill(cellStart_.begin(), cellStart_.end(), 0u);

    for (size_t index = 0; index < count; ++index) {
      const uint32_t cell = static_cast<uint32_t>(rowOf(ys[index]) * columns_ + columnOf(xs[index]));
      cellOfEntry_[index] = cell;
      ++cellStart_[cell + 1];
    }

    for (size_t cell = 1; cell < cellStart_.size(); ++cell) {
      cellStart_[cell] += cellStart_[cell - 1];
    }

    cursor_.assign(cellStart_.begin(), cellStart_.end() - 1);
    entities_.resize(count);
    xs_.resize(count);
    ys_.resize(count);

    for (size_t index = 0; index < count; ++index) {
      const uint32_t slot = cursor_[cellOfEntry_[index]]++;
      entities_[slot] = entities[index];
      xs_[slot] = xs[index];
      ys_[slot] = ys[index];
    }
  }
}
