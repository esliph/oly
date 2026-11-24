#include "scenario.hpp"

#include <array>
#include <cstdio>
#include <string>

#include "oly/core/assert.hpp"
#include "oly/input/physical_button.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"
#include "workloads/s1_horde.hpp"
#include "workloads/s5_replay.hpp"

namespace enxame {
  namespace {
    // Playable has no name, so no command line reaches it, and it shares S5's world.
    constexpr std::array<ScenarioInfo, 9> Table{{
      {"", workloads::S5Sizes, workloads::S5Seed, workloads::S5Run, ""},
      {"S1", workloads::S1Sizes, workloads::S1Seed, workloads::S1Run, ""},
      {"", {}, 0, {0, 0}, ""},
      {"", {}, 0, {0, 0}, ""},
      {"S4", {}, 0, {0, 0}, "needs stable ids"},
      {"S5", workloads::S5Sizes, workloads::S5Seed, workloads::S5Run, ""},
      {"", {}, 0, {0, 0}, ""},
      {"S7", {}, 0, {0, 0}, "needs assets"},
      {"", {}, 0, {0, 0}, ""},
    }};

    constexpr std::array<ScenarioId, 4> Listed{ScenarioId::S1, ScenarioId::S4, ScenarioId::S5, ScenarioId::S7};

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
    OLY_ASSERT(scenario == ScenarioId::S1 || scenario == ScenarioId::S5 || scenario == ScenarioId::Playable, "setUpScenario: scenario %u is not implemented yet", static_cast<unsigned>(scenario));

    engine.actions().bind(actions::place, oly::input::MouseButton::Left);

    if (scenario == ScenarioId::S1) {
      return setUpHorde(scenarioInfo(scenario), workloads::S1Horde, size, engine);
    }

    return setUpPlayableWorld(size, engine);
  }
}
