#include "scenario.hpp"

#include <array>
#include <cstdio>
#include <string>

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

    void spawnHorde(Simulation& simulation, oly::app::Engine& engine, uint32_t size) {
      const TileMap& map = *simulation.map;

      for (uint32_t index = 0; index < size; ++index) {
        TilePosition tile{0, 0};

        do {
          tile = {static_cast<int32_t>(simulation.random.nextBelow(map.width())), static_cast<int32_t>(simulation.random.nextBelow(map.height()))};
        } while (map.blocked(tile) || (tile.x == map.base().x && tile.y == map.base().y));

        const float jitterX = (simulation.random.nextFloat() - 0.5f) * map.tileSize();
        const float jitterY = (simulation.random.nextFloat() - 0.5f) * map.tileSize();
        engine.world().spawn(Enemy{}, Position{map.centerX(tile) + jitterX, map.centerY(tile) + jitterY});
      }
    }

    std::unique_ptr<Simulation> makeHordeSimulation(const ScenarioInfo& info, const HordeParameters& horde) {
      std::unique_ptr<Simulation> simulation = std::make_unique<Simulation>(info.seed);
      simulation->horde = horde;
      simulation->map = TileMap::generate(horde.mapWidth, horde.mapHeight, horde.tileSize, horde.obstacleRate, simulation->random);
      simulation->flowField.build(*simulation->map);
      simulation->camera = {simulation->map->centerX(simulation->map->base()), simulation->map->centerY(simulation->map->base()), 1.0f};
      simulation->hash.emplace(0.0f, 0.0f, static_cast<float>(horde.mapWidth) * horde.tileSize, static_cast<float>(horde.mapHeight) * horde.tileSize, horde.tileSize);

      return simulation;
    }

    void addHordeSystems(Simulation& simulation, oly::app::Engine& engine) {
      engine.addSystem(oly::app::Phase::UpdateBefore, "updateFlowField", updateFlowField, &simulation);
      engine.addSystem(oly::app::Phase::Update, "rebuildSpatialHash", rebuildSpatialHash, &simulation);
      engine.addSystem(oly::app::Phase::Update, "moveHorde", moveHorde, &simulation);
    }

    std::unique_ptr<Simulation> setUpHorde(const ScenarioInfo& info, const HordeParameters& horde, uint32_t size, oly::app::Engine& engine) {
      std::unique_ptr<Simulation> simulation = makeHordeSimulation(info, horde);

      addHordeSystems(*simulation, engine);
      spawnHorde(*simulation, engine, size);

      return simulation;
    }

    // S3 is S1's horde plus the status toggles of toggleStatus.
    std::unique_ptr<Simulation> setUpStatusHorde(uint32_t size, oly::app::Engine& engine) {
      const ScenarioInfo& info = scenarioInfo(ScenarioId::S3);
      std::unique_ptr<Simulation> simulation = makeHordeSimulation(info, workloads::S3Horde);
      simulation->toggleProbability = workloads::S3ToggleProbability;

      addHordeSystems(*simulation, engine);
      engine.addSystem(oly::app::Phase::UpdateAfter, "toggleStatus", toggleStatus, simulation.get());
      spawnHorde(*simulation, engine, size);

      return simulation;
    }

    // Vehicle i drives a circle around the i-th point of a grid; its four turrets are children at local (+-offset, +-offset).
    std::unique_ptr<Simulation> setUpHierarchy(uint32_t size, oly::app::Engine& engine) {
      std::unique_ptr<Simulation> simulation = std::make_unique<Simulation>(scenarioInfo(ScenarioId::S6).seed);
      simulation->vehicles = workloads::S6Vehicles;
      const VehicleParameters& vehicles = simulation->vehicles;

      for (uint32_t index = 0; index < size; ++index) {
        const float centerX = static_cast<float>(index % vehicles.columns) * vehicles.spacing;
        const float centerY = static_cast<float>(index / vehicles.columns) * vehicles.spacing;
        const oly::ecs::Entity vehicle = engine.world().spawn(Vehicle{}, Position{centerX + vehicles.radius, centerY}, Rotation{0.0f}, VehicleMotion{centerX, centerY, vehicles.radius, 0.0f, vehicles.angularSpeed});

        for (const float signX : {-1.0f, 1.0f}) {
          for (const float signY : {-1.0f, 1.0f}) {
            engine.world().spawn(VehicleTurret{}, Position{signX * vehicles.turretOffset, signY * vehicles.turretOffset}, Parent{vehicle});
          }
        }
      }

      const uint32_t rows = (size + vehicles.columns - 1) / vehicles.columns;
      simulation->camera = {static_cast<float>(vehicles.columns - 1) * vehicles.spacing / 2.0f, static_cast<float>(rows - 1) * vehicles.spacing / 2.0f, 0.2f};
      engine.addSystem(oly::app::Phase::Update, "moveVehicles", moveVehicles, simulation.get());

      return simulation;
    }

    std::unique_ptr<Simulation> setUpWaves(uint32_t size, oly::app::Engine& engine) {
      std::unique_ptr<Simulation> simulation = std::make_unique<Simulation>(scenarioInfo(ScenarioId::S8).seed);
      simulation->waves = workloads::S8Waves;
      simulation->waveSize = size;
      simulation->camera = {0.0f, 0.0f, 0.3f};
      engine.addSystem(oly::app::Phase::UpdateBefore, "runWaves", runWaves, simulation.get());

      return simulation;
    }

    // Belt b runs along y = beltSpacing * b from x = 0; its assembler sits one tile after the belt's end.
    std::unique_ptr<Simulation> setUpFactory(uint32_t size, oly::app::Engine& engine) {
      const ScenarioInfo& info = scenarioInfo(ScenarioId::S2);
      std::unique_ptr<Simulation> simulation = std::make_unique<Simulation>(info.seed);
      simulation->factory = workloads::S2Factory;
      const FactoryParameters& factory = simulation->factory;
      const float beltLength = static_cast<float>(factory.beltTiles) * factory.tileSize;
      const uint32_t itemsPerBelt = size / factory.assemblers;

      for (uint32_t belt = 0; belt < factory.assemblers; ++belt) {
        const float y = static_cast<float>(belt) * factory.beltSpacing;
        simulation->assemblerOfBelt.push_back(engine.world().spawn(Assembler{belt, 0, 0.0f}, Position{beltLength + factory.tileSize, y}));

        for (uint32_t item = 0; item < itemsPerBelt; ++item) {
          const float offset = beltLength / static_cast<float>(itemsPerBelt) * static_cast<float>(item);
          engine.world().spawn(BeltItem{belt, offset}, Position{offset, y});
        }
      }

      simulation->camera = {(beltLength + factory.tileSize) / 2.0f, static_cast<float>(factory.assemblers - 1) * factory.beltSpacing / 2.0f, 1.0f};
      engine.addSystem(oly::app::Phase::Update, "moveBeltItems", moveBeltItems, simulation.get());
      engine.addSystem(oly::app::Phase::Update, "runAssemblers", runAssemblers, simulation.get());

      return simulation;
    }

    // placeTurret goes before updateFlowField, so the field sees the new tile in the same step.
    std::unique_ptr<Simulation> setUpPlayableWorld(uint32_t size, oly::app::Engine& engine) {
      std::unique_ptr<Simulation> simulation = makeHordeSimulation(scenarioInfo(ScenarioId::S5), workloads::S5Horde);
      simulation->combat = workloads::S5Combat;

      engine.addSystem(oly::app::Phase::UpdateBefore, "placeTurret", placeTurret, simulation.get());
      addHordeSystems(*simulation, engine);
      engine.addSystem(oly::app::Phase::UpdateAfter, "fireTurrets", fireTurrets, simulation.get());
      engine.addSystem(oly::app::Phase::UpdateAfter, "moveProjectiles", moveProjectiles, simulation.get());
      spawnHorde(*simulation, engine, size);

      return simulation;
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

  std::unique_ptr<Simulation> setUpScenario(ScenarioId scenario, uint32_t size, oly::app::Engine& engine) {
    OLY_ASSERT(scenarioInfo(scenario).blockedReason.empty(), "setUpScenario: scenario %u is blocked", static_cast<unsigned>(scenario));

    engine.actions().bind(actions::place, oly::input::MouseButton::Left);

    switch (scenario) {
      case ScenarioId::S1:
        return setUpHorde(scenarioInfo(scenario), workloads::S1Horde, size, engine);
      case ScenarioId::S2:
        return setUpFactory(size, engine);
      case ScenarioId::S3:
        return setUpStatusHorde(size, engine);
      case ScenarioId::S6:
        return setUpHierarchy(size, engine);
      case ScenarioId::S8:
        return setUpWaves(size, engine);
      default:
        return setUpPlayableWorld(size, engine);
    }
  }
}
