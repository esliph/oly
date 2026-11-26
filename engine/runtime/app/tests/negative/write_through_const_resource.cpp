#include "oly/app/system_descriptor.hpp"
#include "oly/app/system_parameters.hpp"

namespace {
  struct Score {
    int value = 0;
  };

  void writesThroughConst(oly::app::Res<const Score> score) {
    score->value = 1;
  }
}

template const oly::app::SystemDescriptor& oly::app::systemDescriptor<&writesThroughConst>();
