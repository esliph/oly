#include "oly/counting/plugin.hpp"

#include "oly/app/context.hpp"
#include "oly/app/engine.hpp"
#include "oly/counting/schema.generated.hpp"

namespace oly::counting {
  namespace {
    void count(app::SystemContext& context) {
      context.query<Counter>().each([](Counter& counter) { ++counter.value; });
    }
  }

  void Plugin::install(app::Engine& engine) {
    system_ = engine.addSystem(app::Phase::Update, "count", count);
  }
}
