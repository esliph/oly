#pragma once

#include "oly/app/plugin.hpp"
#include "oly/app/system.hpp"

namespace oly::doubling {
  // Test plugin that needs counting: an Update system ordered after counting::Counting writes Doubled.value = 2 * Counter.value.
  class Plugin : public app::Plugin {
   private:
    app::SystemId system_ = 0;

   public:
    void install(app::Engine& engine) override;

    app::SystemId system() const noexcept {
      return system_;
    }
  };
}
