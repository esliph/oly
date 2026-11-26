#pragma once

#include "oly/app/system_parameters.hpp"
#include "oly/ecs/query.hpp"
#include "oly/input/action_map.hpp"
#include "resources.hpp"
#include "schema.generated.hpp"

namespace enxame {
  // Playable world and S5.
  void placeTurret(oly::app::Res<TileMap> map, oly::app::Res<const Camera> camera, oly::app::Res<const oly::input::ActionMap> actionMap, oly::app::Commands commands);
  void fireTurrets(oly::app::Query<Turret, const Position> turrets, oly::app::Res<const SpatialHash> hash, oly::app::Res<const CombatParameters> combat, oly::app::Commands commands, const oly::app::StepInfo& step);
  void moveProjectiles(oly::app::Query<Position, Projectile> projectiles, oly::app::Res<const SpatialHash> hash, oly::app::Res<const CombatParameters> combat, oly::app::Res<HitEnemies> hitEnemies, oly::app::Commands commands, const oly::app::StepInfo& step);

  // The horde: S1, S3 and the playable world.
  void updateFlowField(oly::app::Res<const TileMap> map, oly::app::Res<FlowField> flowField);
  void rebuildSpatialHash(oly::app::Query<const Position, oly::ecs::With<Enemy>> enemies, oly::app::Res<SpatialHash> hash, oly::app::Res<GatheredEnemies> gathered);
  void moveHorde(oly::app::Query<Position, oly::ecs::With<Enemy>> enemies, oly::app::Res<const SpatialHash> hash, oly::app::Res<const FlowField> flowField, oly::app::Res<const TileMap> map, oly::app::Res<const HordeParameters> horde, oly::app::Res<Random> random, const oly::app::StepInfo& step);

  // S2.
  void moveBeltItems(oly::app::Query<BeltItem, Position> items, oly::app::Lookup<Assembler> assemblers, oly::app::Res<const Belts> belts, oly::app::Res<const FactoryParameters> factory, oly::app::Commands commands, const oly::app::StepInfo& step);
  void runAssemblers(oly::app::Query<Assembler> assemblers, oly::app::Res<const FactoryParameters> factory, oly::app::Commands commands, const oly::app::StepInfo& step);

  // S3, S6 and S8.
  void toggleStatus(oly::app::Query<const Position, oly::ecs::With<Enemy>> enemies, oly::app::Lookup<const Burning> burning, oly::app::Lookup<const Slowed> slowed, oly::app::Lookup<const Stunned> stunned, oly::app::Res<Random> random, oly::app::Res<StatusToggles> toggles, oly::app::Commands commands);
  void moveVehicles(oly::app::Query<Position, Rotation, VehicleMotion> vehicles, const oly::app::StepInfo& step);
  void runWaves(oly::app::Query<const Position, oly::ecs::With<Enemy>> enemies, oly::app::Res<Random> random, oly::app::Res<WaveClock> clock, oly::app::Res<const WaveParameters> waves, oly::app::Commands commands);
}
