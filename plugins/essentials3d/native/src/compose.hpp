#pragma once

#include "oly/essentials3d/detail/quaternion.hpp"
#include "oly/essentials3d/schema.generated.hpp"

namespace oly::essentials3d {
  inline constexpr WorldTransform IdentityWorld{0, 0, 0, 0, 0, 0, 1, 1, 1, 1};
  inline constexpr Rotation IdentityRotation{0, 0, 0, 1};
  inline constexpr Scale UnitScale{1, 1, 1};

  // Translate-rotate-scale with no shear: the parent's scale applies to the local position before the parent's rotation.
  inline WorldTransform compose(const WorldTransform& parent, const Position& position, const Rotation& rotation, const Scale& scale) {
    const detail::Quaternion parentRotation{parent.rotationX, parent.rotationY, parent.rotationZ, parent.rotationW};
    const detail::Quaternion rotated = detail::multiply(parentRotation, detail::Quaternion{rotation.x, rotation.y, rotation.z, rotation.w});
    const detail::Vector offset = detail::rotate(parentRotation, parent.scaleX * position.x, parent.scaleY * position.y, parent.scaleZ * position.z);

    return WorldTransform{
      parent.x + offset.x,
      parent.y + offset.y,
      parent.z + offset.z,
      rotated.x,
      rotated.y,
      rotated.z,
      rotated.w,
      parent.scaleX * scale.x,
      parent.scaleY * scale.y,
      parent.scaleZ * scale.z,
    };
  }
}
