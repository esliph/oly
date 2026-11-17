#include "oly/doubling/plugin.hpp"

#include "oly/app/context.hpp"
#include "oly/app/engine.hpp"
#include "oly/counting/schema.generated.hpp"
#include "oly/doubling/schema.generated.hpp"

namespace oly::doubling {
  namespace {
    void writeDoubles(app::SystemContext& context) {
      context.query<const Counter, Doubled>().each([](const Counter& counter, Doubled& doubled) { doubled.value = 2 * counter.value; });
    }
  }

  void Plugin::install(app::Engine& engine) {
    system_ = engine.addSystem(app::Phase::Update, "writeDoubles", writeDoubles, nullptr, app::OrderBand::Last);
  }
}
