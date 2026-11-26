#include "oly/app/system.hpp"
#include "oly/app/system_descriptor.hpp"
#include "systems.hpp"

namespace {
  // Pixels per second; an initial value, not measured.
  constexpr float PlayerSpeed = 200.0f;
}

void movePlayer(oly::app::Query<Position, oly::ecs::With<Player>> players, oly::app::Res<const oly::input::ActionMap> actions, const oly::app::StepInfo& step) {
  const oly::input::Vector2 axis = actions->axis2d(actions::move);
  const float distance = PlayerSpeed * step.fixedDeltaTime;

  players.each([&](Position& position) {
    position.x += axis.x * distance;
    position.y += axis.y * distance;
  });
}

OLY_SYSTEM(movePlayer);
