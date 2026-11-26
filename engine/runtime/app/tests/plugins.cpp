#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "components.hpp"
#include "log.hpp"
#include "oly/app/engine.hpp"
#include "oly/app/plugin.hpp"

using oly::app::Engine;
using oly::app::Plugin;
using oly::app::Res;
using oly::app::Update;

namespace {
  std::vector<std::string> g_events;

  struct InstallCounter : Plugin {
    int installs = 0;

    void install(Engine&) override {
      ++installs;
    }
  };

  struct FirstPlugin : Plugin {
    ~FirstPlugin() override {
      g_events.push_back("first");
    }

    void install(Engine&) override {}
  };

  struct SecondPlugin : Plugin {
    ~SecondPlugin() override {
      g_events.push_back("second");
    }

    void install(Engine&) override {}
  };

  struct Ticks {
    int count = 0;
  };

  struct TickingPlugin : Plugin {
    void install(Engine& engine) override {
      engine.world().insertResource(Ticks());
      engine.addSystem<&tick>(Update{}, "tick");
    }

    static void tick(Res<Ticks> ticks) {
      ++ticks->count;
    }
  };

  struct ObservingPlugin : Plugin {
    oly::ecs::World* world = nullptr;
    oly::ecs::ObserverId observer = 0;

    ~ObservingPlugin() override {
      world->removeObserver(observer);
    }

    void install(Engine& engine) override {
      world = &engine.world();
      observer = world->onAdd<Position>(onPositionAdded, this);
    }

    static void onPositionAdded(oly::ecs::ObserverContext&) {
      g_events.push_back("added");
    }
  };

  struct SchedulingPlugin : Plugin {
    int pendingTasks = 0;

    ~SchedulingPlugin() override {
      g_events.push_back("destroyed");
    }

    void install(Engine& engine) override {
      engine.schedule().every(1.0, tick, this, onDiscard);
      ++pendingTasks;
    }

    static void tick(oly::app::TaskContext&) {}

    static void onDiscard(void* payload) {
      SchedulingPlugin* plugin = *static_cast<SchedulingPlugin**>(payload);
      --plugin->pendingTasks;
      g_events.push_back("discarded");
    }
  };
}

TEST_CASE("installPlugin runs install once and plugin<P> returns the same instance") {
  oly::ecs::World world;
  Engine engine(world);

  InstallCounter& installed = engine.installPlugin<InstallCounter>();

  CHECK(installed.installs == 1);
  CHECK(&engine.plugin<InstallCounter>() == &installed);
}

TEST_CASE("plugins are destroyed in reverse install order") {
  g_events.clear();
  oly::ecs::World world;

  {
    Engine engine(world);
    engine.installPlugin<FirstPlugin>();
    engine.installPlugin<SecondPlugin>();
  }

  CHECK(g_events == std::vector<std::string>{"second", "first"});
}

TEST_CASE("a plugin keeps state reachable from its systems through a resource") {
  oly::ecs::World world;
  Engine engine(world);
  engine.installPlugin<TickingPlugin>();
  engine.frame(0.0);

  engine.frame(1.0 / 60);

  CHECK(world.resource<Ticks>().count == 1);
  CHECK_INVARIANTS(world);
}

TEST_CASE("after the Engine is destroyed, the World runs no observer of its plugins") {
  g_events.clear();
  oly::ecs::World world;
  {
    Engine engine(world);
    engine.installPlugin<ObservingPlugin>();
    world.spawn(Position{1, 2});
  }

  world.spawn(Position{3, 4});

  CHECK(g_events == std::vector<std::string>{"added"});
  CHECK_INVARIANTS(world);
}

TEST_CASE("a pending task of a plugin is discarded before the plugin is destroyed") {
  g_events.clear();
  oly::ecs::World world;
  {
    Engine engine(world);
    engine.installPlugin<SchedulingPlugin>();
  }

  CHECK(g_events == std::vector<std::string>{"discarded", "destroyed"});
}
