#pragma once

#include <cstdint>

namespace enxame {
  // How many times the global operator new ran in this process. allocation_counter.cpp replaces the operator; only the
  // game executable links it, never the unit tests.
  uint64_t allocationCount() noexcept;
}
