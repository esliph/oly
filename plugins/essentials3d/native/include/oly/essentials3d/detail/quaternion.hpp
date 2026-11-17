#pragma once

namespace oly::essentials3d::detail {
  struct Quaternion {
    float x;
    float y;
    float z;
    float w;
  };

  struct Vector {
    float x;
    float y;
    float z;
  };

  // Hamilton product: the rotation b followed by the rotation a.
  constexpr Quaternion multiply(const Quaternion& a, const Quaternion& b) {
    return Quaternion{
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
  }

  // Rotates (x, y, z) by the unit quaternion q: v + 2w(u × v) + 2u × (u × v), with u the vector part of q.
  constexpr Vector rotate(const Quaternion& q, float x, float y, float z) {
    const float crossX = q.y * z - q.z * y;
    const float crossY = q.z * x - q.x * z;
    const float crossZ = q.x * y - q.y * x;

    return Vector{
      x + 2 * (q.w * crossX + q.y * crossZ - q.z * crossY),
      y + 2 * (q.w * crossY + q.z * crossX - q.x * crossZ),
      z + 2 * (q.w * crossZ + q.x * crossY - q.y * crossX),
    };
  }
}
