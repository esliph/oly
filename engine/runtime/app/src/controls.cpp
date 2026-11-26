#include "oly/app/controls.hpp"

#include "oly/app/engine.hpp"

namespace oly::app {
  void StateRequest::request(StateId state) {
    engine_->requestState(state);
  }
}
