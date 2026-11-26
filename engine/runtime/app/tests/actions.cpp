#include <doctest/doctest.h>

#include <vector>

#include "components.hpp"
#include "oly/app/controls.hpp"
#include "oly/app/engine.hpp"
#include "oly/input/action_map.hpp"

using oly::app::Engine;
using oly::app::Events;
using oly::app::HandlerContext;
using oly::app::PauseRequest;
using oly::app::Res;
using oly::app::Update;
using oly::input::ActionMap;
using oly::input::ButtonAction;
using oly::input::ButtonState;
using oly::input::Key;

namespace {
  struct Ping {
    uint32_t value;
  };

  struct Recorder {
    ButtonAction action;
    std::vector<ButtonState> states;
    uint32_t normalRuns = 0;
    bool rebound = false;
  };

  void record(Res<Recorder> recorder, Res<const ActionMap> actionMap) {
    recorder->states.push_back(actionMap->button(recorder->action));
  }

  void recordInHandler(HandlerContext& context) {
    auto& recorder = *static_cast<Recorder*>(context.userData());
    recorder.states.push_back(context.actions().button(recorder.action));
  }

  void emitPing(Events<Ping> pings) {
    pings.emit(Ping{1});
  }

  void resumeOnToggle(Res<Recorder> recorder, Res<const ActionMap> actionMap, Res<PauseRequest> pauseRequest) {
    if (actionMap->button(recorder->action).pressed) {
      pauseRequest->resume();
    }
  }

  void countNormal(Res<Recorder> recorder) {
    ++recorder->normalRuns;
  }

  void rebindOnce(Res<Recorder> recorder, Res<ActionMap> actionMap) {
    recorder->states.push_back(actionMap->button(recorder->action));

    if (!recorder->rebound) {
      recorder->rebound = true;
      actionMap->bind(recorder->action, Key::Q);
    }
  }

  struct Fixture {
    oly::ecs::World world;
    Engine engine{world};
    Recorder& recorder = world.insertResource(Recorder());
    double now = 0.0;
    bool started = false;

    Fixture() {
      recorder.action = engine.actions().declareButton("jump");
      engine.actions().bind(recorder.action, Key::Space);
    }

    void advance(double seconds) {
      if (!started) {
        engine.frame(0.0);
        started = true;
      }

      now += seconds;
      engine.frame(now);
    }

    void step() {
      advance(1.0 / 60);
    }
  };

  constexpr ButtonState pressedHeld{true, true, false};
  constexpr ButtonState heldOnly{false, true, false};
  constexpr ButtonState releasedOnly{false, false, true};
  constexpr ButtonState idle{false, false, false};
}

TEST_CASE("one capture per step, also when paused") {
  Fixture fixture;
  fixture.engine.addSystem<&record>(Update{}, "whilePaused").whilePaused();
  fixture.engine.pause();

  fixture.engine.actions().buttonDown(Key::Space);
  fixture.step();
  fixture.step();

  REQUIRE(fixture.recorder.states.size() == 2);
  CHECK(fixture.recorder.states[0] == pressedHeld);
  CHECK(fixture.recorder.states[1] == heldOnly);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("edges only in the first step of the frame") {
  Fixture fixture;
  fixture.engine.addSystem<&record>(Update{}, "u");

  fixture.engine.actions().buttonDown(Key::Space);
  fixture.engine.actions().buttonUp(Key::Space);
  fixture.advance(3.0 / 60 + 0.001);

  REQUIRE(fixture.recorder.states.size() == 3);
  CHECK(fixture.recorder.states[0] == ButtonState{true, false, true});
  CHECK(fixture.recorder.states[1] == idle);
  CHECK(fixture.recorder.states[2] == idle);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("frame without a step keeps the edge for the next step") {
  Fixture fixture;
  fixture.engine.addSystem<&record>(Update{}, "u");

  fixture.engine.actions().buttonDown(Key::Space);
  fixture.engine.actions().buttonUp(Key::Space);
  fixture.advance(0.005);
  CHECK(fixture.recorder.states.empty());

  fixture.advance(0.0125);

  REQUIRE(fixture.recorder.states.size() == 1);
  CHECK(fixture.recorder.states[0] == ButtonState{true, false, true});
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("leaving the pause through an action in a whilePaused system") {
  Fixture fixture;
  fixture.engine.addSystem<&resumeOnToggle>(Update{}, "toggle").whilePaused();
  fixture.engine.addSystem<&countNormal>(Update{}, "normal").after("toggle");
  fixture.engine.pause();

  fixture.engine.actions().buttonDown(Key::Space);
  fixture.step();
  CHECK_FALSE(fixture.engine.paused());
  CHECK(fixture.recorder.normalRuns == 0);

  fixture.engine.actions().buttonUp(Key::Space);
  fixture.step();
  CHECK(fixture.recorder.normalRuns == 1);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("setHidden(true) releases the buttons") {
  Fixture fixture;
  fixture.engine.addSystem<&record>(Update{}, "u");

  fixture.engine.actions().buttonDown(Key::Space);
  fixture.step();

  fixture.engine.setHidden(true);
  fixture.engine.setHidden(false);
  fixture.engine.frame(fixture.now);
  fixture.step();

  REQUIRE(fixture.recorder.states.size() == 2);
  CHECK(fixture.recorder.states[1] == releasedOnly);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("system rebinds and the change takes effect on the next capture") {
  Fixture fixture;
  fixture.engine.addSystem<&rebindOnce>(Update{}, "rebind");
  fixture.engine.actions().clearBindings(fixture.recorder.action.id);

  fixture.engine.actions().buttonDown(Key::Q);
  fixture.step();
  fixture.step();

  REQUIRE(fixture.recorder.states.size() == 2);
  CHECK(fixture.recorder.states[0] == idle);
  CHECK(fixture.recorder.states[1] == heldOnly);
  CHECK_INVARIANTS(fixture.world);
}

TEST_CASE("handler reads the actions through the same context") {
  Fixture fixture;
  fixture.engine.declareEvent<Ping>("Ping");
  fixture.engine.addSystem<&emitPing>(Update{}, "emit");
  fixture.engine.on<Ping>("handler", recordInHandler, &fixture.recorder);

  fixture.engine.actions().buttonDown(Key::Space);
  fixture.step();

  REQUIRE(fixture.recorder.states.size() == 1);
  CHECK(fixture.recorder.states[0].pressed);
  CHECK_INVARIANTS(fixture.world);
}
