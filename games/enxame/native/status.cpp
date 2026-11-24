#include "oly/app/system.hpp"
#include "oly/ecs/query.hpp"
#include "schema.generated.hpp"
#include "simulation.hpp"
#include "systems.hpp"

namespace {
  template <class Status>
  void toggle(oly::app::SystemContext& context, oly::ecs::Entity entity) {
    if (context.has<Status>(entity)) {
      context.remove<Status>(entity);
    } else {
      context.add(entity, Status{});
    }
  }
}

// An entity toggles at most once per step, so the toggles of a step equal its archetype moves.
OLY_SYSTEM(toggleStatus) {
  enxame::Simulation& simulation = *static_cast<enxame::Simulation*>(context.userData());
  simulation.statusTogglesLastStep = 0;

  context.query<const Position>(oly::ecs::With<Enemy>{}).each([&](oly::ecs::Entity entity, const Position&) {
    if (!simulation.random.chance(simulation.toggleProbability)) {
      return;
    }

    switch (simulation.random.nextBelow(3)) {
      case 0:
        toggle<Burning>(context, entity);
        break;
      case 1:
        toggle<Slowed>(context, entity);
        break;
      default:
        toggle<Stunned>(context, entity);
        break;
    }

    simulation.statusTogglesLastStep += 1;
  });
}
