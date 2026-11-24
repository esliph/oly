#include "oly/app/system.hpp"
#include "oly/ecs/query.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"

namespace {
  // Pixels per second; an initial value, not measured.
  constexpr float PlayerSpeed = 200.0f;
}

OLY_SYSTEM(movePlayer) {
  const oly::input::Vector2 axis = context.actions().axis2d(actions::move);
  const float step = PlayerSpeed * context.fixedDeltaTime();

  context.query<Position>(oly::ecs::With<Player>{}).each([&](Position& position) {
    position.x += axis.x * step;
    position.y += axis.y * step;
  });
}
