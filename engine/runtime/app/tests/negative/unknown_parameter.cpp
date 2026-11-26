#include "oly/app/system_descriptor.hpp"

namespace {
  void takesInteger(int) {}
}

template const oly::app::SystemDescriptor& oly::app::systemDescriptor<&takesInteger>();
