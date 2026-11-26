#include "systems.hpp"

namespace enxame {
  namespace {
    template <class Modifier>
    void addModifierByChance(oly::app::Commands& commands, Random& random, float probability, oly::ecs::Entity entity) {
      if (random.chance(probability)) {
        commands.add(entity, Modifier{});
      }
    }

    void spawnWave(oly::app::Commands& commands, Random& random, const WaveClock& clock, const WaveParameters& waves) {
      const float side = waves.areaSide;
      const float probability = waves.modifierProbability;

      for (uint32_t index = 0; index < clock.waveSize; ++index) {
        const float x = (random.nextFloat() - 0.5f) * side;
        const float y = (random.nextFloat() - 0.5f) * side;
        const oly::ecs::Entity enemy = commands.spawn(Enemy{}, Position{x, y});

        addModifierByChance<Armored>(commands, random, probability, enemy);
        addModifierByChance<Swift>(commands, random, probability, enemy);
        addModifierByChance<Regenerating>(commands, random, probability, enemy);
        addModifierByChance<Shielded>(commands, random, probability, enemy);
        addModifierByChance<Explosive>(commands, random, probability, enemy);
        addModifierByChance<Splitting>(commands, random, probability, enemy);
        addModifierByChance<Cloaked>(commands, random, probability, enemy);
        addModifierByChance<Enraged>(commands, random, probability, enemy);
      }
    }
  }

  // The modifier tags have no behaviour: S8 only counts the archetypes they make.
  void runWaves(oly::app::Query<const Position, oly::ecs::With<Enemy>> enemies, oly::app::Res<Random> random, oly::app::Res<WaveClock> clock, oly::app::Res<const WaveParameters> waves, oly::app::Commands commands) {
    if (clock->stepInWave == 0) {
      spawnWave(commands, *random, *clock, *waves);
    } else if (clock->stepInWave == waves->stepsPerWave - 1) {
      enemies.each([&](oly::ecs::Entity entity, const Position&) {
        commands.destroy(entity);
      });
      clock->wavesCompleted += 1;
    }

    clock->stepInWave = (clock->stepInWave + 1) % waves->stepsPerWave;
  }
}
