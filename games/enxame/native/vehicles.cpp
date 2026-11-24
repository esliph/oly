#include <cmath>

#include "oly/app/system.hpp"
#include "oly/ecs/query.hpp"
#include "schema.generated.hpp"
#include "simulation.hpp"
#include "systems.hpp"

// The turrets follow through propagateTransforms, which runs after this system in the same phase.
OLY_SYSTEM(moveVehicles) {
  const float deltaTime = context.fixedDeltaTime();

  context.query<Position, Rotation, VehicleMotion>().each([&](Position& position, Rotation& rotation, VehicleMotion& motion) {
    motion.angle += motion.angularSpeed * deltaTime;
    position.x = motion.centerX + motion.radius * std::cos(motion.angle);
    position.y = motion.centerY + motion.radius * std::sin(motion.angle);
    rotation.angle = motion.angle;
  });
}
