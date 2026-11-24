#pragma once

#include "oly/app/system.hpp"

// Registered by name through OLY_SYSTEM; declared here for a shell that adds it with Engine::addSystem.
void movePlayer(oly::app::SystemContext& context);
