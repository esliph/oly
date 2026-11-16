#include "oly/app/plugin.hpp"

namespace oly::app::detail {
  PluginKey nextPluginKey() noexcept {
    static PluginKey next = 0;
    return next++;
  }
}
