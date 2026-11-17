#pragma once

#include "oly/app/plugin.hpp"
#include "oly/app/system.hpp"

namespace oly::counting {
  // Test plugin: a Default-band Update system adds 1 to every Counter.
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
