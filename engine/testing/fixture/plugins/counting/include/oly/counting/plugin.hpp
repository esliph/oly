#pragma once

#include "oly/app/plugin.hpp"
#include "oly/app/system.hpp"

namespace oly::counting {
  // The set of the systems that change Counter, so a system of another plugin orders itself against all of them.
  struct Counting {};

  // Test plugin: an Update system in the Counting set adds 1 to every Counter.
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
