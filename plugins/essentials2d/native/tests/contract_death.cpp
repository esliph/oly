// Each case violates a hierarchy contract and must trigger OLY_ASSERT with the required substrings; it only exists with
// asserts enabled. Same protocol as the ecs contract_death.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "fixture.hpp"
#include "oly/core/assert.hpp"
#include "oly/ecs/command_buffer.hpp"

namespace {
  using oly::ecs::Entity;

  const std::vector<const char*>* g_required = nullptr;

  void handler(const char* message) noexcept {
    std::fprintf(stderr, "%s\n", message);

    if (g_required) {
      for (const char* needle : *g_required) {
        if (!std::strstr(message, needle)) {
          std::fprintf(stderr, "message missing the required substring: \"%s\"\n", needle);
          std::fflush(stderr);
          std::_Exit(2);
        }
      }
    }

    std::fflush(stderr);
    std::_Exit(0);
  }

  void parentDead() {
    Fixture fixture;
    const Entity parent = fixture.world.spawn(Position{0, 0});
    fixture.world.destroy(parent);

    fixture.world.spawn(Position{0, 0}, Parent{parent});
  }

  void parentSelf() {
    Fixture fixture;
    const Entity entity = fixture.world.spawn(Position{0, 0});

    fixture.world.add(entity, Parent{entity});
  }

  void parentCycle() {
    Fixture fixture;
    const Entity grandparent = fixture.world.spawn(Position{0, 0});
    const Entity parent = fixture.world.spawn(Position{0, 0}, Parent{grandparent});

    fixture.world.add(grandparent, Parent{parent});
  }

  void parentDestroyedInSameBuffer() {
    Fixture fixture;
    const Entity parent = fixture.world.spawn(Position{0, 0});
    const Entity child = fixture.world.spawn(Position{0, 0});
    oly::ecs::CommandBuffer commands = fixture.world.commands();

    commands.add(child, Parent{parent});
    commands.destroy(parent);
    fixture.world.flush(commands);
  }

  void parentDestroyedBeforeInSameBuffer() {
    Fixture fixture;
    const Entity parent = fixture.world.spawn(Position{0, 0});
    const Entity child = fixture.world.spawn(Position{0, 0});
    oly::ecs::CommandBuffer commands = fixture.world.commands();

    commands.destroy(parent);
    commands.add(child, Parent{parent});
    fixture.world.flush(commands);
  }

  void installWithLiveEngine() {
    Fixture fixture;
    oly::app::Engine second(fixture.world);

    second.installPlugin<oly::essentials2d::Plugin>();
  }

  void installAfterDestroyedEngine() {
    oly::ecs::World world;

    {
      oly::app::Engine first(world);
      first.installPlugin<oly::essentials2d::Plugin>();
      const Entity parent = world.spawn(Position{0, 0});
      world.spawn(Position{1, 0}, Parent{parent});
    }

    oly::app::Engine second(world);

    second.installPlugin<oly::essentials2d::Plugin>();
  }

  struct Case {
    const char* name;
    void (*run)();
    std::vector<const char*> required;
  };
}

int main(int argumentCount, char** arguments) {
  static const std::vector<Case> cases = {
    {"parent_dead", parentDead, {"Parent", "e=", "dead"}},
    {"parent_self", parentSelf, {"Parent", "own parent"}},
    {"parent_cycle", parentCycle, {"Parent", "descendant"}},
    {"parent_destroyed_in_same_buffer", parentDestroyedInSameBuffer, {"Parent", "dead"}},
    {"parent_destroyed_before_in_same_buffer", parentDestroyedBeforeInSameBuffer, {"Parent", "dead"}},
    {"install_with_live_engine", installWithLiveEngine, {"install", "essentials2d::Plugin", "once per World"}},
    {"install_after_destroyed_engine", installAfterDestroyedEngine, {"install", "essentials2d::Plugin", "once per World"}},
  };

  if (argumentCount != 2) {
    std::fprintf(stderr, "usage: contract_death <case>\n");
    return 64;
  }

  for (const Case& testCase : cases) {
    if (std::strcmp(testCase.name, arguments[1]) != 0) {
      continue;
    }

    g_required = &testCase.required;
    oly::core::setAssertHandler(&handler);
    testCase.run();
    std::fprintf(stderr, "assert did not fire\n");
    return 1;
  }

  std::fprintf(stderr, "unknown case: %s\n", arguments[1]);
  return 64;
}
