#include "oly/native_host/game.hpp"

#include <SDL3/SDL.h>
#include <doctest/doctest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "oly/app/engine.hpp"
#include "oly/app/game.hpp"
#include "oly/app/plugin.hpp"

using oly::app::Engine;
using oly::app::Update;
using oly::native_host::Game;
using oly::native_host::runGame;
using oly::native_host::Window;
using oly::native_host::WindowConfig;

namespace {
  struct Probe {
    static inline int constructed = 0;
    static inline int setUpCalls = 0;
    static inline int setUpCallsAtStart = 0;
    static inline int startCalls = 0;
    static inline int updateSteps = 0;
    static inline std::string titleAtStart;
    static inline bool engineDestroyed = false;
    static inline bool engineDestroyedWhenGameDestroyed = false;
    static inline bool gameDestroyed = false;

    static void reset() {
      constructed = 0;
      setUpCalls = 0;
      setUpCallsAtStart = 0;
      startCalls = 0;
      updateSteps = 0;
      titleAtStart.clear();
      engineDestroyed = false;
      engineDestroyedWhenGameDestroyed = false;
      gameDestroyed = false;
    }
  };

  class ProbePlugin : public oly::app::Plugin {
   public:
    ~ProbePlugin() override {
      Probe::engineDestroyed = true;
    }

    void install(Engine&) override {}
  };

  void countStep() {
    ++Probe::updateSteps;
  }

  class ProbeGame : public Game {
   public:
    ProbeGame()
      : Game({"probe", 64, 64}, {.fixedHz = 30.0}) {
      ++Probe::constructed;
    }

    ~ProbeGame() override {
      Probe::engineDestroyedWhenGameDestroyed = Probe::engineDestroyed;
      Probe::gameDestroyed = true;
    }

    void onStart(Engine& engine, Window& window) override {
      ++Probe::startCalls;
      Probe::setUpCallsAtStart = Probe::setUpCalls;
      Probe::titleAtStart = SDL_GetWindowTitle(window.window());
      engine.installPlugin<ProbePlugin>();
      engine.addSystem<&countStep>(Update{}, "countStep");
    }
  };

  class ThrowingConstructorGame : public Game {
   public:
    ThrowingConstructorGame()
      : Game({"throwing", 64, 64}) {
      throw std::runtime_error("constructor failed");
    }

    void onStart(Engine&, Window&) override {}
  };

  class ThrowingStartGame : public Game {
   public:
    ThrowingStartGame()
      : Game({"throwing", 64, 64}) {}

    ~ThrowingStartGame() override {
      Probe::gameDestroyed = true;
    }

    void onStart(Engine&, Window&) override {
      throw std::runtime_error("start failed");
    }
  };

  template <class G>
  int runWith(std::vector<std::string> arguments) {
    std::vector<char*> pointers;

    for (std::string& argument : arguments) {
      pointers.push_back(argument.data());
    }

    return runGame<G>(static_cast<int>(pointers.size()), pointers.data());
  }
}

namespace oly::app {
  void setUpGame(Engine&) {
    ++Probe::setUpCalls;
  }
}

TEST_CASE("runGame with --frames 3 starts the game once after setUpGame and steps at the game's fixedHz") {
  Probe::reset();

  CHECK(runWith<ProbeGame>({"probe", "--frames", "3"}) == 0);
  CHECK(Probe::constructed == 1);
  CHECK(Probe::setUpCallsAtStart == 1);
  CHECK(Probe::startCalls == 1);
  CHECK(Probe::titleAtStart == "probe");
  CHECK(Probe::updateSteps == 2); // 3 frames, the first only records the time; 1.0 / 60 would give 1 step at 30 Hz
}

TEST_CASE("runGame destroys the game after the Engine") {
  Probe::reset();

  runWith<ProbeGame>({"probe", "--frames", "1"});

  CHECK(Probe::engineDestroyedWhenGameDestroyed);
}

TEST_CASE("a wrong command line returns 64 and constructs nothing") {
  Probe::reset();

  std::vector<std::string> arguments;
  SUBCASE("unknown option") {
    arguments = {"probe", "--bogus"};
  }
  SUBCASE("missing value") {
    arguments = {"probe", "--frames"};
  }
  SUBCASE("zero") {
    arguments = {"probe", "--frames", "0"};
  }
  SUBCASE("negative") {
    arguments = {"probe", "--frames", "-1"};
  }
  SUBCASE("trailing text") {
    arguments = {"probe", "--frames", "5x"};
  }
  SUBCASE("overflow") {
    arguments = {"probe", "--frames", "18446744073709551616"};
  }
  SUBCASE("extra argument") {
    arguments = {"probe", "--frames", "3", "extra"};
  }

  CHECK(runWith<ProbeGame>(arguments) == 64);
  CHECK(Probe::constructed == 0);
}

TEST_CASE("an exception from the game's constructor returns 1") {
  Probe::reset();

  CHECK(runWith<ThrowingConstructorGame>({"probe", "--frames", "1"}) == 1);
}

TEST_CASE("an exception from the game's constructor with no arguments at all returns 1") {
  Probe::reset();

  CHECK(runGame<ThrowingConstructorGame>(0, nullptr) == 1);
}

TEST_CASE("an exception from onStart returns 1 and still destroys the game") {
  Probe::reset();

  CHECK(runWith<ThrowingStartGame>({"probe", "--frames", "1"}) == 1);
  CHECK(Probe::gameDestroyed);
}
