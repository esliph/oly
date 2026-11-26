#pragma once

#include "oly/app/plugin.hpp"

namespace oly::physics2d {
  // The phase of the physics, in the fixed step, after app::Update and before essentials2d::Propagation.
  struct Physics {};

  // Declares the Physics phase and has no system yet.
  class Plugin final : public app::Plugin {
   public:
    void install(app::Engine& engine) override;
  };
}
