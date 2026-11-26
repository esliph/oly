#include "oly/doubling/plugin.hpp"

#include "oly/app/engine.hpp"
#include "oly/app/system_parameters.hpp"
#include "oly/counting/plugin.hpp"
#include "oly/counting/schema.generated.hpp"
#include "oly/doubling/schema.generated.hpp"

namespace oly::doubling {
  namespace {
    void writeDoubles(app::Query<const Counter, Doubled> counters) {
      counters.each([](const Counter& counter, Doubled& doubled) { doubled.value = 2 * counter.value; });
    }
  }

  void Plugin::install(app::Engine& engine) {
    system_ = engine.addSystem<&writeDoubles>(app::Update{}, "writeDoubles").after<counting::Counting>().id();
  }
}
