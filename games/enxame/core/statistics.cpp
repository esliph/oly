#include "statistics.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace enxame {
  uint64_t percentile(std::vector<uint64_t>& values, double percent) {
    std::sort(values.begin(), values.end());

    // percent * n first, then the division: 70 / 100 * 10 is 7.000000000000001 and would round the rank up.
    const double exactRank = percent * static_cast<double>(values.size()) / 100.0;
    const size_t rank = static_cast<size_t>(std::ceil(exactRank));

    return values[std::clamp<size_t>(rank, 1, values.size()) - 1];
  }
}
