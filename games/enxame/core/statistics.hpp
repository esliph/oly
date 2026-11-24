#pragma once

#include <cstdint>
#include <vector>

namespace enxame {
  // Nearest rank: sorts values and returns values[ceil(percent / 100 * n) - 1]. values is not empty and percent is in (0, 100].
  uint64_t percentile(std::vector<uint64_t>& values, double percent);
}
