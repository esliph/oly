#pragma once

#include "oly/app/system_parameters.hpp"
#include "oly/ecs/query.hpp"
#include "oly/input/action_map.hpp"
#include "schema.generated.hpp"

// Registered by name through OLY_SYSTEM; declared here for a shell that adds it with Engine::addSystem.
void movePlayer(oly::app::Query<Position, oly::ecs::With<Player>> players, oly::app::Res<const oly::input::ActionMap> actions, const oly::app::StepInfo& step);
