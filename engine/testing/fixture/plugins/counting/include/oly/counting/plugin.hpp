#pragma once

#include "oly/app/plugin.hpp"
#include "oly/app/system.hpp"

namespace oly::counting {
  // The set of the systems that change Counter, so a system of another plugin orders itself against all of them.
  struct Counting {};

  // The fixed-step phase of the fixture plugins, after app::Update: their systems stay out of Update, where an exclusive script
  // system of the web fixture would conflict with them.
  struct CountingPhase {};

  // Test plugin: a CountingPhase system in the Counting set adds 1 to every Counter.
  class Plugin : public app::Plugin {
   private:
    app::SystemId system_ = 0;

   public:
    void install(app::Engine& engine) override;

    // SystemIds are dense in registration order, so they show the install order.
    app::SystemId system() const noexcept {
      return system_;
    }
  };
}
