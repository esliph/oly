#include <cmath>

#include "systems.hpp"

namespace enxame {
  // The turrets follow through propagateTransforms, which runs in the Propagation phase, after Update.
  void moveVehicles(oly::app::Query<Position, Rotation, VehicleMotion> vehicles, const oly::app::StepInfo& step) {
    const float deltaTime = step.fixedDeltaTime;

    vehicles.each([&](Position& position, Rotation& rotation, VehicleMotion& motion) {
      motion.angle += motion.angularSpeed * deltaTime;
      position.x = motion.centerX + motion.radius * std::cos(motion.angle);
      position.y = motion.centerY + motion.radius * std::sin(motion.angle);
      rotation.angle = motion.angle;
    });
  }
}
