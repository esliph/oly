#pragma once

#include "oly/app/system.hpp"

// Registered by name through OLY_SYSTEM; declared here for the scenarios that add them with Engine::addSystem.
void updateFlowField(oly::app::SystemContext& context);
void rebuildSpatialHash(oly::app::SystemContext& context);
void moveHorde(oly::app::SystemContext& context);
