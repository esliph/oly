#include <algorithm>

#include "systems.hpp"

namespace enxame {
  void moveBeltItems(oly::app::Query<BeltItem, Position> items, oly::app::Lookup<Assembler> assemblers, oly::app::Res<const Belts> belts, oly::app::Res<const FactoryParameters> factory, oly::app::Commands commands, const oly::app::StepInfo& step) {
    const float beltLength = static_cast<float>(factory->beltTiles) * factory->tileSize;
    const float distance = factory->itemSpeed * step.fixedDeltaTime;

    items.each([&](oly::ecs::Entity item, BeltItem& beltItem, Position& position) {
      beltItem.offset += distance;

      if (beltItem.offset >= beltLength) {
        Assembler& assembler = assemblers.get(belts->assemblerOfBelt[beltItem.belt]);

        if (assembler.stock < factory->assemblerCapacity) {
          assembler.stock += 1;
          commands.destroy(item);

          return;
        }

        beltItem.offset = beltLength;
      }

      position.x = beltItem.offset;
    });
  }

  void runAssemblers(oly::app::Query<Assembler> assemblers, oly::app::Res<const FactoryParameters> factory, oly::app::Commands commands, const oly::app::StepInfo& step) {
    const float deltaTime = step.fixedDeltaTime;

    assemblers.each([&](Assembler& assembler) {
      assembler.timer = std::max(assembler.timer - deltaTime, 0.0f);

      if (assembler.timer > 0.0f || assembler.stock == 0) {
        return;
      }

      assembler.stock -= 1;
      assembler.timer = factory->craftSeconds;
      commands.spawn(BeltItem{assembler.belt, 0.0f}, Position{0.0f, static_cast<float>(assembler.belt) * factory->beltSpacing});
    });
  }
}
