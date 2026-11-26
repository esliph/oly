#include "scenario.hpp"

#include <array>
#include <cstdio>
#include <string>
#include <utility>

#include "oly/core/assert.hpp"
#include "oly/input/physical_button.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"
#include "workloads/s1_horde.hpp"
#include "workloads/s2_factory.hpp"
#include "workloads/s3_status.hpp"
#include "workloads/s5_replay.hpp"
#include "workloads/s6_hierarchy.hpp"
#include "workloads/s8_waves.hpp"

namespace enxame {
  namespace {
    // Playable has no name, so no command line reaches it, and it shares S5's world.
    constexpr std::array<ScenarioInfo, 9> Table{{
      {"", workloads::S5Sizes, workloads::S5Seed, workloads::S5Run, ""},
      {"S1", workloads::S1Sizes, workloads::S1Seed, workloads::S1Run, ""},
      {"S2", workloads::S2Sizes, workloads::S2Seed, workloads::S2Run, ""},
      {"S3", workloads::S3Sizes, workloads::S3Seed, workloads::S3Run, ""},
      {"S4", {}, 0, {0, 0}, "needs stable ids"},
      {"S5", workloads::S5Sizes, workloads::S5Seed, workloads::S5Run, ""},
      {"S6", workloads::S6Sizes, workloads::S6Seed, workloads::S6Run, ""},
      {"S7", {}, 0, {0, 0}, "needs assets"},
      {"S8", workloads::S8Sizes, workloads::S8Seed, workloads::S8Run, ""},
    }};

    constexpr std::array<ScenarioId, 8> Listed{ScenarioId::S1, ScenarioId::S2, ScenarioId::S3, ScenarioId::S4, ScenarioId::S5, ScenarioId::S6, ScenarioId::S7, ScenarioId::S8};

    std::string sizesText(std::span<const uint32_t> sizes) {
      std::string text;

      for (const uint32_t size : sizes) {
        text += text.empty() ? "" : ", ";
        text += std::to_string(size);
      }

      return text;
    }

    void spawnHorde(oly::ecs::World& world, uint32_t size) {
      const TileMap& map = world.resource<TileMap>();
      Random& random = world.resource<Random>();

      for (uint32_t index = 0; index < size; ++index) {
        TilePosition tile{0, 0};

        do {
          tile = {static_cast<int32_t>(random.nextBelow(map.width())), static_cast<int32_t>(random.nextBelow(map.height()))};
        } while (map.blocked(tile) || (tile.x == map.base().x && tile.y == map.base().y));

        const float jitterX = (random.nextFloat() - 0.5f) * map.tileSize();
        const float jitterY = (random.nextFloat() - 0.5f) * map.tileSize();
        world.spawn(Enemy{}, Position{map.centerX(tile) + jitterX, map.centerY(tile) + jitterY});
      }
    }

    void insertHordeResources(oly::ecs::World& world, const ScenarioInfo& info, const HordeParameters& horde) {
      Random& random = world.insertResource(Random(info.seed));
      const TileMap& map = world.insertResource(TileMap::generate(horde.mapWidth, horde.mapHeight, horde.tileSize, horde.obstacleRate, random));

      world.insertResource(FlowField()).build(map);
      world.insertResource(Camera{map.centerX(map.base()), map.centerY(map.base()), 1.0f});
      world.insertResource(SpatialHash(0.0f, 0.0f, static_cast<float>(horde.mapWidth) * horde.tileSize, static_cast<float>(horde.mapHeight) * horde.tileSize, horde.tileSize));
      world.insertResource(HordeParameters(horde));
      world.insertResource(GatheredEnemies());
    }

    void addHordeSystems(oly::app::Engine& engine) {
      engine.addSystem<&updateFlowField>(oly::app::UpdateBefore{}, "updateFlowField");
      engine.addSystem<&rebuildSpatialHash>(oly::app::Update{}, "rebuildSpatialHash");
      engine.addSystem<&moveHorde>(oly::app::Update{}, "moveHorde").after<&rebuildSpatialHash>();
    }

    void setUpHorde(const ScenarioInfo& info, const HordeParameters& horde, uint32_t size, oly::app::Engine& engine) {
      insertHordeResources(engine.world(), info, horde);
      addHordeSystems(engine);
      spawnHorde(engine.world(), size);
    }

    // S3 is S1's horde plus the status toggles of toggleStatus.
    void setUpStatusHorde(uint32_t size, oly::app::Engine& engine) {
      insertHordeResources(engine.world(), scenarioInfo(ScenarioId::S3), workloads::S3Horde);
      engine.world().insertResource(StatusToggles{workloads::S3ToggleProbability, 0});
      addHordeSystems(engine);
      engine.addSystem<&toggleStatus>(oly::app::UpdateAfter{}, "toggleStatus");
      spawnHorde(engine.world(), size);
    }

    // Vehicle i drives a circle around the i-th point of a grid; its four turrets are children at local (+-offset, +-offset).
    void setUpHierarchy(uint32_t size, oly::app::Engine& engine) {
      oly::ecs::World& world = engine.world();
      const VehicleParameters& vehicles = workloads::S6Vehicles;

      for (uint32_t index = 0; index < size; ++index) {
        const float centerX = static_cast<float>(index % vehicles.columns) * vehicles.spacing;
        const float centerY = static_cast<float>(index / vehicles.columns) * vehicles.spacing;
        const oly::ecs::Entity vehicle = world.spawn(Vehicle{}, Position{centerX + vehicles.radius, centerY}, Rotation{0.0f}, VehicleMotion{centerX, centerY, vehicles.radius, 0.0f, vehicles.angularSpeed});

        for (const float signX : {-1.0f, 1.0f}) {
          for (const float signY : {-1.0f, 1.0f}) {
            world.spawn(VehicleTurret{}, Position{signX * vehicles.turretOffset, signY * vehicles.turretOffset}, Parent{vehicle});
          }
        }
      }

      const uint32_t rows = (size + vehicles.columns - 1) / vehicles.columns;
      world.insertResource(Camera{static_cast<float>(vehicles.columns - 1) * vehicles.spacing / 2.0f, static_cast<float>(rows - 1) * vehicles.spacing / 2.0f, 0.2f});
      engine.addSystem<&moveVehicles>(oly::app::Update{}, "moveVehicles");
    }

    void setUpWaves(uint32_t size, oly::app::Engine& engine) {
      oly::ecs::World& world = engine.world();

      world.insertResource(Random(scenarioInfo(ScenarioId::S8).seed));
      world.insertResource(Camera{0.0f, 0.0f, 0.3f});
      world.insertResource(WaveParameters(workloads::S8Waves));
      world.insertResource(WaveClock{size, 0, 0});
      engine.addSystem<&runWaves>(oly::app::UpdateBefore{}, "runWaves");
    }

    // Belt b runs along y = beltSpacing * b from x = 0; its assembler sits one tile after the belt's end.
    void setUpFactory(uint32_t size, oly::app::Engine& engine) {
      oly::ecs::World& world = engine.world();
      const FactoryParameters& factory = workloads::S2Factory;
      const float beltLength = static_cast<float>(factory.beltTiles) * factory.tileSize;
      const uint32_t itemsPerBelt = size / factory.assemblers;
      Belts belts;

      for (uint32_t belt = 0; belt < factory.assemblers; ++belt) {
        const float y = static_cast<float>(belt) * factory.beltSpacing;
        belts.assemblerOfBelt.push_back(world.spawn(Assembler{belt, 0, 0.0f}, Position{beltLength + factory.tileSize, y}));

        for (uint32_t item = 0; item < itemsPerBelt; ++item) {
          const float offset = beltLength / static_cast<float>(itemsPerBelt) * static_cast<float>(item);
          world.spawn(BeltItem{belt, offset}, Position{offset, y});
        }
      }

      world.insertResource(std::move(belts));
      world.insertResource(FactoryParameters(factory));
      world.insertResource(Camera{(beltLength + factory.tileSize) / 2.0f, static_cast<float>(factory.assemblers - 1) * factory.beltSpacing / 2.0f, 1.0f});
      engine.addSystem<&moveBeltItems>(oly::app::Update{}, "moveBeltItems");
      engine.addSystem<&runAssemblers>(oly::app::Update{}, "runAssemblers").after<&moveBeltItems>();
    }

    void setUpPlayableWorld(uint32_t size, oly::app::Engine& engine) {
      insertHordeResources(engine.world(), scenarioInfo(ScenarioId::S5), workloads::S5Horde);
      engine.world().insertResource(CombatParameters(workloads::S5Combat));
      engine.world().insertResource(HitEnemies());

      // placeTurret goes before updateFlowField, so the field sees the new tile in the same step.
      engine.addSystem<&placeTurret>(oly::app::UpdateBefore{}, "placeTurret").before<&updateFlowField>();
      addHordeSystems(engine);
      engine.addSystem<&fireTurrets>(oly::app::UpdateAfter{}, "fireTurrets");
      engine.addSystem<&moveProjectiles>(oly::app::UpdateAfter{}, "moveProjectiles").after<&fireTurrets>();
      spawnHorde(engine.world(), size);
    }
  }

  std::optional<ScenarioId> parseScenario(std::string_view name) {
    for (const ScenarioId scenario : Listed) {
      if (scenarioInfo(scenario).name == name) {
        return scenario;
      }
    }

    return std::nullopt;
  }

  const ScenarioInfo& scenarioInfo(ScenarioId scenario) {
    return Table[static_cast<size_t>(scenario)];
  }

  std::span<const ScenarioId> listedScenarios() {
    return Listed;
  }

  bool checkScenario(ScenarioId scenario, std::optional<uint32_t> size) {
    const ScenarioInfo& info = scenarioInfo(scenario);

    if (!info.blockedReason.empty()) {
      std::fprintf(stderr, "enxame: %.*s is blocked: %.*s\n", static_cast<int>(info.name.size()), info.name.data(), static_cast<int>(info.blockedReason.size()), info.blockedReason.data());

      return false;
    }

    if (size) {
      bool frozen = false;

      for (const uint32_t candidate : info.sizes) {
        frozen = frozen || candidate == *size;
      }

      if (!frozen) {
        std::fprintf(stderr, "enxame: size %u is not one of %.*s's: %s\n", *size, static_cast<int>(info.name.size()), info.name.data(), sizesText(info.sizes).c_str());

        return false;
      }
    }

    return true;
  }

  void setUpScenario(ScenarioId scenario, uint32_t size, oly::app::Engine& engine) {
    OLY_ASSERT(scenarioInfo(scenario).blockedReason.empty(), "setUpScenario: scenario %u is blocked", static_cast<unsigned>(scenario));

    engine.actions().bind(actions::place, oly::input::MouseButton::Left);

    switch (scenario) {
      case ScenarioId::S1:
        setUpHorde(scenarioInfo(scenario), workloads::S1Horde, size, engine);
        break;
      case ScenarioId::S2:
        setUpFactory(size, engine);
        break;
      case ScenarioId::S3:
        setUpStatusHorde(size, engine);
        break;
      case ScenarioId::S6:
        setUpHierarchy(size, engine);
        break;
      case ScenarioId::S8:
        setUpWaves(size, engine);
        break;
      default:
        setUpPlayableWorld(size, engine);
        break;
    }
  }
}
