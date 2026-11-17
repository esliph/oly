#include <benchmark/benchmark.h>

#include <cstdint>

#include "oly/app/engine.hpp"
#include "oly/ecs/world.hpp"
#include "oly/essentials2d/plugin.hpp"
#include "oly/essentials2d/schema.generated.hpp"

namespace {
  constexpr uint32_t ChildrenPerParent = 8;

  // One frame is one fixed step; the plugin's system is the only one registered.
  struct Scene {
    oly::ecs::World world;
    oly::app::Engine engine{world};
    double now = 0.0;

    Scene() {
      engine.installPlugin<oly::essentials2d::Plugin>();
      engine.frame(0.0);
    }

    void step() {
      now += engine.fixedDeltaTime();
      engine.frame(now);
    }
  };
}

static void BM_PropagateRoots(benchmark::State& state) {
  const auto entityCount = static_cast<uint32_t>(state.range(0));
  Scene scene;

  for (uint32_t index = 0; index < entityCount; ++index) {
    const auto offset = static_cast<float>(index);
    scene.world.spawn(Position{offset, offset}, Rotation{0.1f}, Scale{2.0f, 2.0f});
  }

  for (auto _ : state) {
    scene.step();
  }

  state.counters["per_entity"] = benchmark::Counter(static_cast<double>(entityCount), benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert);
}

static void BM_PropagateChildren(benchmark::State& state) {
  const auto entityCount = static_cast<uint32_t>(state.range(0));
  Scene scene;

  for (uint32_t index = 0; index < entityCount / (ChildrenPerParent + 1); ++index) {
    const oly::ecs::Entity parent = scene.world.spawn(Position{static_cast<float>(index), 0.0f}, Rotation{0.1f}, Scale{2.0f, 2.0f});

    for (uint32_t child = 0; child < ChildrenPerParent; ++child) {
      scene.world.spawn(Position{1.0f, static_cast<float>(child)}, Rotation{0.2f}, Scale{1.0f, 1.0f}, Parent{parent});
    }
  }

  for (auto _ : state) {
    scene.step();
  }

  state.counters["per_entity"] = benchmark::Counter(static_cast<double>(entityCount), benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert);
}

BENCHMARK(BM_PropagateRoots)->Arg(1000)->Arg(100000);
BENCHMARK(BM_PropagateChildren)->Arg(1000)->Arg(100000);

BENCHMARK_MAIN();
