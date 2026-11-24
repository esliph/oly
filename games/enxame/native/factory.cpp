#include <algorithm>

#include "oly/app/system.hpp"
#include "oly/ecs/query.hpp"
#include "schema.generated.hpp"
#include "simulation.hpp"
#include "systems.hpp"

OLY_SYSTEM(moveBeltItems) {
  enxame::Simulation& simulation = *static_cast<enxame::Simulation*>(context.userData());
  const enxame::FactoryParameters& factory = simulation.factory;
  const float beltLength = static_cast<float>(factory.beltTiles) * factory.tileSize;
  const float distance = factory.itemSpeed * context.fixedDeltaTime();

  context.query<BeltItem, Position>().each([&](oly::ecs::Entity item, BeltItem& beltItem, Position& position) {
    beltItem.offset += distance;

    if (beltItem.offset >= beltLength) {
      Assembler& assembler = context.get<Assembler>(simulation.assemblerOfBelt[beltItem.belt]);

      if (assembler.stock < factory.assemblerCapacity) {
        assembler.stock += 1;
        context.destroy(item);

        return;
      }

      beltItem.offset = beltLength;
    }

    position.x = beltItem.offset;
  });
}

OLY_SYSTEM(runAssemblers) {
  enxame::Simulation& simulation = *static_cast<enxame::Simulation*>(context.userData());
  const enxame::FactoryParameters& factory = simulation.factory;
  const float deltaTime = context.fixedDeltaTime();

  context.query<Assembler>().each([&](Assembler& assembler) {
    assembler.timer = std::max(assembler.timer - deltaTime, 0.0f);

    if (assembler.timer > 0.0f || assembler.stock == 0) {
      return;
    }

    assembler.stock -= 1;
    assembler.timer = factory.craftSeconds;
    context.spawn(BeltItem{assembler.belt, 0.0f}, Position{0.0f, static_cast<float>(assembler.belt) * factory.beltSpacing});
  });
}
