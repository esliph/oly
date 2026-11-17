#pragma once

#include <cmath>

#include "oly/essentials2d/schema.generated.hpp"

namespace oly::essentials2d {
  inline constexpr WorldTransform IdentityWorld{0, 0, 0, 1, 1};

  // Translate-rotate-scale with no shear: the parent's scale applies to the local position before the parent's rotation.
  inline WorldTransform compose(const WorldTransform& parent, const Position& position, float angle, float scaleX, float scaleY) {
    const float cosine = std::cos(parent.angle);
    const float sine = std::sin(parent.angle);
    const float scaledX = parent.scaleX * position.x;
    const float scaledY = parent.scaleY * position.y;

    return WorldTransform{
      parent.x + cosine * scaledX - sine * scaledY,
      parent.y + sine * scaledX + cosine * scaledY,
      parent.angle + angle,
      parent.scaleX * scaleX,
      parent.scaleY * scaleY,
    };
  }
}
