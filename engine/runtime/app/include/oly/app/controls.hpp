#pragma once

#include "oly/app/state.hpp"

namespace oly::app {
  class Engine;

  // The Engine's own controls, reachable from a system as a resource: `Res<StateRequest>` and `Res<PauseRequest>`. Each Engine owns
  // one of each, so a system reaches the controls of the Engine that runs it.
  class StateRequest {
   private:
    Engine* engine_;

   public:
    explicit StateRequest(Engine& engine) noexcept
      : engine_(&engine) {}

    // The last request before stage 5 of the step wins, as Engine::requestState.
    void request(StateId state);
  };

  class PauseRequest {
   private:
    bool requested_ = false;

   public:
    // Requests in the middle of a step take effect from the next fixed step on.
    void pause() noexcept {
      requested_ = true;
    }
    void resume() noexcept {
      requested_ = false;
    }
    bool requested() const noexcept {
      return requested_;
    }
  };
}
