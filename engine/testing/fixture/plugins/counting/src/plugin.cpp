#include "oly/counting/plugin.hpp"

#include "oly/app/engine.hpp"
#include "oly/app/system_parameters.hpp"
#include "oly/counting/schema.generated.hpp"

namespace oly::counting {
  namespace {
    void count(app::Query<Counter> counters) {
      counters.each([](Counter& counter) { ++counter.value; });
    }
  }

  void Plugin::install(app::Engine& engine) {
    engine.addPhase<CountingPhase>(app::FixedStep{}).after<app::Update>().before<app::UpdateAfter>();
    system_ = engine.addSystem<&count>(CountingPhase{}, "count").inSet<Counting>().id();
  }
}
