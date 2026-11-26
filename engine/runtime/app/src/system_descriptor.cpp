#include "oly/app/system_descriptor.hpp"

namespace oly::app {
  SystemFunctionKey detail::nextSystemFunctionKey() noexcept {
    static SystemFunctionKey next = 0;
    return next++;
  }
}
