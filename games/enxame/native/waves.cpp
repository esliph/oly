#include "oly/app/system.hpp"
#include "oly/ecs/query.hpp"
#include "schema.generated.hpp"
#include "simulation.hpp"
#include "systems.hpp"

namespace {
  template <class Modifier>
  void addModifierByChance(oly::app::SystemContext& context, enxame::Random& random, float probability, oly::ecs::Entity entity) {
    if (random.chance(probability)) {
      context.add(entity, Modifier{});
    }
  }

  void spawnWave(oly::app::SystemContext& context, enxame::Simulation& simulation) {
    const float side = simulation.waves.areaSide;
    const float probability = simulation.waves.modifierProbability;

    for (uint32_t index = 0; index < simulation.waveSize; ++index) {
      const float x = (simulation.random.nextFloat() - 0.5f) * side;
      const float y = (simulation.random.nextFloat() - 0.5f) * side;
      const oly::ecs::Entity enemy = context.spawn(Enemy{}, Position{x, y});

      addModifierByChance<Armored>(context, simulation.random, probability, enemy);
      addModifierByChance<Swift>(context, simulation.random, probability, enemy);
      addModifierByChance<Regenerating>(context, simulation.random, probability, enemy);
      addModifierByChance<Shielded>(context, simulation.random, probability, enemy);
      addModifierByChance<Explosive>(context, simulation.random, probability, enemy);
      addModifierByChance<Splitting>(context, simulation.random, probability, enemy);
      addModifierByChance<Cloaked>(context, simulation.random, probability, enemy);
      addModifierByChance<Enraged>(context, simulation.random, probability, enemy);
    }
  }
}

// The modifier tags have no behaviour: S8 only counts the archetypes they make.
OLY_SYSTEM(runWaves) {
  enxame::Simulation& simulation = *static_cast<enxame::Simulation*>(context.userData());

  if (simulation.stepInWave == 0) {
    spawnWave(context, simulation);
  } else if (simulation.stepInWave == simulation.waves.stepsPerWave - 1) {
    context.query<const Position>(oly::ecs::With<Enemy>{}).each([&](oly::ecs::Entity entity, const Position&) {
      context.destroy(entity);
    });
    simulation.wavesCompleted += 1;
  }

  simulation.stepInWave = (simulation.stepInWave + 1) % simulation.waves.stepsPerWave;
}
