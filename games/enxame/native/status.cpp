#include "systems.hpp"

namespace enxame {
  namespace {
    template <class Status>
    void toggle(oly::app::Lookup<const Status> statuses, oly::app::Commands& commands, oly::ecs::Entity entity) {
      if (statuses.has(entity)) {
        commands.remove<Status>(entity);
      } else {
        commands.add(entity, Status{});
      }
    }
  }

  // An entity toggles at most once per step, so the toggles of a step equal its archetype moves.
  void toggleStatus(oly::app::Query<const Position, oly::ecs::With<Enemy>> enemies, oly::app::Lookup<const Burning> burning, oly::app::Lookup<const Slowed> slowed, oly::app::Lookup<const Stunned> stunned, oly::app::Res<Random> random, oly::app::Res<StatusToggles> toggles, oly::app::Commands commands) {
    toggles->lastStep = 0;

    enemies.each([&](oly::ecs::Entity entity, const Position&) {
      if (!random->chance(toggles->probability)) {
        return;
      }

      switch (random->nextBelow(3)) {
        case 0:
          toggle(burning, commands, entity);
          break;
        case 1:
          toggle(slowed, commands, entity);
          break;
        default:
          toggle(stunned, commands, entity);
          break;
      }

      toggles->lastStep += 1;
    });
  }
}
