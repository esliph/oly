#include "oly/app/system_descriptor.hpp"
#include "oly/app/system_parameters.hpp"

namespace {
  struct Position {
    float x = 0;
  };

  void writesThroughConst(oly::app::Query<const Position> positions) {
    positions.each([](auto& position) { position.x = 1; });
  }
}

template const oly::app::SystemDescriptor& oly::app::systemDescriptor<&writesThroughConst>();
