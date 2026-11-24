#pragma once

#include "oly/app/system.hpp"

// Registered by name through OLY_SYSTEM; declared here for the scenarios that add them with Engine::addSystem.
void updateFlowField(oly::app::SystemContext& context);
void rebuildSpatialHash(oly::app::SystemContext& context);
void moveHorde(oly::app::SystemContext& context);

// Window only: EnxameGame adds them, so no headless run has them.
void moveCamera(oly::app::SystemContext& context);
void drawWorld(oly::app::SystemContext& context);

// Playable world and S5.
void placeTurret(oly::app::SystemContext& context);
void fireTurrets(oly::app::SystemContext& context);
void moveProjectiles(oly::app::SystemContext& context);

// S2.
void moveBeltItems(oly::app::SystemContext& context);
void runAssemblers(oly::app::SystemContext& context);

// S3, S6 and S8.
void toggleStatus(oly::app::SystemContext& context);
void moveVehicles(oly::app::SystemContext& context);
void runWaves(oly::app::SystemContext& context);
