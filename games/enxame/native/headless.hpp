#pragma once

#include "command_line.hpp"

namespace enxame {
  // 0, 64 (blocked scenario, size not frozen) or 1 (cannot write the output).
  int runHeadless(const LaunchOptions& options);
}
