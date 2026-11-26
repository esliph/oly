#include "oly/physics2d/plugin.hpp"

#include "oly/app/engine.hpp"
#include "oly/essentials2d/plugin.hpp"

namespace oly::physics2d {
  void Plugin::install(app::Engine& engine) {
    engine.addPhase<Physics>(app::FixedStep{}).after<app::Update>().before<essentials2d::Propagation>();
  }
}
