#include <algorithm>
#include <cmath>

#include "systems.hpp"

namespace enxame {
  void placeTurret(oly::app::Res<TileMap> map, oly::app::Res<const Camera> camera, oly::app::Res<const oly::input::ActionMap> actionMap, oly::app::Commands commands) {
    if (!actionMap->button(actions::place).pressed) {
      return;
    }

    const oly::input::PointerState pointer = actionMap->pointer(actions::cursor);

    if (!pointer.valid) {
      return;
    }

    const float worldX = camera->x + (pointer.x - LogicalWidth / 2.0f) / camera->zoom;
    const float worldY = camera->y + (pointer.y - LogicalHeight / 2.0f) / camera->zoom;
    const TilePosition tile = map->tileAt(worldX, worldY);

    if (map->block(tile)) {
      commands.spawn(Position{map->centerX(tile), map->centerY(tile)}, Turret{0.0f});
    }
  }

  void fireTurrets(oly::app::Query<Turret, const Position> turrets, oly::app::Res<const SpatialHash> hash, oly::app::Res<const CombatParameters> combat, oly::app::Commands commands, const oly::app::StepInfo& step) {
    const float deltaTime = step.fixedDeltaTime;

    turrets.each([&](Turret& turret, const Position& position) {
      turret.reload -= deltaTime;

      if (turret.reload > 0.0f) {
        return;
      }

      bool found = false;
      float nearestSquared = 0.0f;
      float targetX = 0.0f;
      float targetY = 0.0f;

      hash->forEachWithin(position.x, position.y, combat->range, [&](oly::ecs::Entity, float x, float y) {
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

      commands.spawn(Position{position.x, position.y}, Projectile{directionX * combat->projectileSpeed, directionY * combat->projectileSpeed, combat->projectileLife});
      turret.reload = combat->reloadSeconds;
    });
  }

  void moveProjectiles(oly::app::Query<Position, Projectile> projectiles, oly::app::Res<const SpatialHash> hash, oly::app::Res<const CombatParameters> combat, oly::app::Res<HitEnemies> hitEnemies, oly::app::Commands commands, const oly::app::StepInfo& step) {
    const float deltaTime = step.fixedDeltaTime;
    hitEnemies->entities.clear();

    projectiles.each([&](oly::ecs::Entity projectileEntity, Position& position, Projectile& projectile) {
      position.x += projectile.velocityX * deltaTime;
      position.y += projectile.velocityY * deltaTime;
      oly::ecs::Entity victim = oly::ecs::NullEntity;

      hash->forEachWithin(position.x, position.y, combat->hitRadius, [&](oly::ecs::Entity enemy, float, float) {
        const bool alreadyHit = std::find(hitEnemies->entities.begin(), hitEnemies->entities.end(), enemy) != hitEnemies->entities.end();

        if (victim == oly::ecs::NullEntity && !alreadyHit) {
          victim = enemy;
        }
      });

      if (victim != oly::ecs::NullEntity) {
        hitEnemies->entities.push_back(victim);
        commands.destroy(victim);
        commands.destroy(projectileEntity);

        return;
      }

      projectile.life -= deltaTime;

      if (projectile.life <= 0.0f) {
        commands.destroy(projectileEntity);
      }
    });
  }
}
