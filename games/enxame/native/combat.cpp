#include <algorithm>
#include <cmath>

#include "oly/app/system.hpp"
#include "oly/ecs/query.hpp"
#include "schema.generated.hpp"
#include "simulation.hpp"
#include "systems.hpp"

OLY_SYSTEM(placeTurret) {
  enxame::Simulation& simulation = *static_cast<enxame::Simulation*>(context.userData());

  if (!context.actions().button(actions::place).pressed) {
    return;
  }

  const oly::input::PointerState pointer = context.actions().pointer(actions::cursor);

  if (!pointer.valid) {
    return;
  }

  const enxame::Camera& camera = simulation.camera;
  const float worldX = camera.x + (pointer.x - enxame::LogicalWidth / 2.0f) / camera.zoom;
  const float worldY = camera.y + (pointer.y - enxame::LogicalHeight / 2.0f) / camera.zoom;
  const enxame::TilePosition tile = simulation.map->tileAt(worldX, worldY);

  if (simulation.map->block(tile)) {
    context.spawn(Position{simulation.map->centerX(tile), simulation.map->centerY(tile)}, Turret{0.0f});
  }
}

OLY_SYSTEM(fireTurrets) {
  enxame::Simulation& simulation = *static_cast<enxame::Simulation*>(context.userData());
  const enxame::CombatParameters& combat = simulation.combat;
  const float deltaTime = context.fixedDeltaTime();

  context.query<Turret, const Position>().each([&](Turret& turret, const Position& position) {
    turret.reload -= deltaTime;

    if (turret.reload > 0.0f) {
      return;
    }

    bool found = false;
    float nearestSquared = 0.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;

    simulation.hash->forEachWithin(position.x, position.y, combat.range, [&](oly::ecs::Entity, float x, float y) {
      const float distanceSquared = (x - position.x) * (x - position.x) + (y - position.y) * (y - position.y);

      if (!found || distanceSquared < nearestSquared) {
        found = true;
        nearestSquared = distanceSquared;
        targetX = x;
        targetY = y;
      }
    });

    if (!found) {
      turret.reload = 0.0f;

      return;
    }

    const float distance = std::sqrt(nearestSquared);
    const float directionX = distance == 0.0f ? 1.0f : (targetX - position.x) / distance;
    const float directionY = distance == 0.0f ? 0.0f : (targetY - position.y) / distance;

    context.spawn(Position{position.x, position.y}, Projectile{directionX * combat.projectileSpeed, directionY * combat.projectileSpeed, combat.projectileLife});
    turret.reload = combat.reloadSeconds;
  });
}

OLY_SYSTEM(moveProjectiles) {
  enxame::Simulation& simulation = *static_cast<enxame::Simulation*>(context.userData());
  const enxame::CombatParameters& combat = simulation.combat;
  const float deltaTime = context.fixedDeltaTime();
  simulation.hitEnemies.clear();

  context.query<Position, Projectile>().each([&](oly::ecs::Entity projectileEntity, Position& position, Projectile& projectile) {
    position.x += projectile.velocityX * deltaTime;
    position.y += projectile.velocityY * deltaTime;
    oly::ecs::Entity victim = oly::ecs::NullEntity;

    simulation.hash->forEachWithin(position.x, position.y, combat.hitRadius, [&](oly::ecs::Entity enemy, float, float) {
      const bool alreadyHit = std::find(simulation.hitEnemies.begin(), simulation.hitEnemies.end(), enemy) != simulation.hitEnemies.end();

      if (victim == oly::ecs::NullEntity && !alreadyHit) {
        victim = enemy;
      }
    });

    if (victim != oly::ecs::NullEntity) {
      simulation.hitEnemies.push_back(victim);
      context.destroy(victim);
      context.destroy(projectileEntity);

      return;
    }

    projectile.life -= deltaTime;

    if (projectile.life <= 0.0f) {
      context.destroy(projectileEntity);
    }
  });
}
