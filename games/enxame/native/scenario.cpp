#include "scenario.hpp"

#include <array>
#include <cstdio>
#include <string>

#include "oly/core/assert.hpp"
#include "oly/input/physical_button.hpp"
#include "schema.generated.hpp"
#include "systems.hpp"
#include "workloads/s1_horde.hpp"

namespace enxame {
  namespace {
    constexpr std::array<ScenarioInfo, 9> Table{{
      {"", {}, 0, {0, 0}, ""},
      {"S1", workloads::S1Sizes, workloads::S1Seed, workloads::S1Run, ""},
      {"", {}, 0, {0, 0}, ""},
      {"", {}, 0, {0, 0}, ""},
      {"S4", {}, 0, {0, 0}, "needs stable ids"},
      {"", {}, 0, {0, 0}, ""},
      {"", {}, 0, {0, 0}, ""},
      {"S7", {}, 0, {0, 0}, "needs assets"},
      {"", {}, 0, {0, 0}, ""},
    }};

    constexpr std::array<ScenarioId, 3> Listed{ScenarioId::S1, ScenarioId::S4, ScenarioId::S7};

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

    std::unique_ptr<Simulation> setUpHorde(const ScenarioInfo& info, const HordeParameters& horde, uint32_t size, oly::app::Engine& engine) {
      std::unique_ptr<Simulation> simulation = std::make_unique<Simulation>(info.seed);
      simulation->horde = horde;
      simulation->map = TileMap::generate(horde.mapWidth, horde.mapHeight, horde.tileSize, horde.obstacleRate, simulation->random);
      simulation->flowField.build(*simulation->map);
      simulation->hash.emplace(0.0f, 0.0f, static_cast<float>(horde.mapWidth) * horde.tileSize, static_cast<float>(horde.mapHeight) * horde.tileSize, horde.tileSize);

      engine.addSystem(oly::app::Phase::UpdateBefore, "updateFlowField", updateFlowField, simulation.get());
      engine.addSystem(oly::app::Phase::Update, "rebuildSpatialHash", rebuildSpatialHash, simulation.get());
      engine.addSystem(oly::app::Phase::Update, "moveHorde", moveHorde, simulation.get());
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
    OLY_ASSERT(scenario == ScenarioId::S1, "setUpScenario: scenario %u is not implemented yet", static_cast<unsigned>(scenario));

    engine.actions().bind(actions::place, oly::input::MouseButton::Left);

    return setUpHorde(scenarioInfo(scenario), workloads::S1Horde, size, engine);
  }
}
