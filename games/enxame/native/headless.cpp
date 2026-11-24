#include "headless.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include "oly/app/engine.hpp"
#include "oly/app/game.hpp"
#include "oly/ecs/world.hpp"
#include "scenario.hpp"
#include "statistics.hpp"

namespace enxame {
  namespace {
    struct FileCloser {
      void operator()(std::FILE* file) const {
        std::fclose(file);
      }
    };

    using File = std::unique_ptr<std::FILE, FileCloser>;

    const char* phaseName(oly::app::Phase phase) {
      switch (phase) {
        case oly::app::Phase::UpdateBefore:
          return "UpdateBefore";
        case oly::app::Phase::Update:
          return "Update";
        case oly::app::Phase::UpdateAfter:
          return "UpdateAfter";
        case oly::app::Phase::Render:
          return "Render";
      }

      return "";
    }

    uint64_t nanosecondsSince(std::chrono::steady_clock::time_point start) {
      return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
    }

    uint64_t medianOf(std::vector<uint64_t> values) {
      return values.empty() ? 0 : percentile(values, 50.0);
    }

    // What one run of a scenario measured: wall time per step, and per system and per flush when timing is on.
    struct Measurements {
      std::vector<uint64_t> stepNanoseconds;
      std::vector<std::vector<uint64_t>> systemNanoseconds; // [system slot]: only the steps where it ran
      std::vector<std::vector<uint64_t>> flushNanoseconds; // [phase]
    };

    void record(const oly::app::Engine& engine, Measurements& measurements) {
      const auto systems = engine.systemTimings();
      measurements.systemNanoseconds.resize(systems.size());

      for (size_t slot = 0; slot < systems.size(); ++slot) {
        if (systems[slot].ran) {
          measurements.systemNanoseconds[slot].push_back(systems[slot].nanoseconds);
        }
      }

      const auto flushes = engine.flushTimings();
      measurements.flushNanoseconds.resize(flushes.size());

      for (size_t slot = 0; slot < flushes.size(); ++slot) {
        measurements.flushNanoseconds[slot].push_back(flushes[slot].nanoseconds);
      }
    }

    void writeJson(std::FILE* file, const LaunchOptions& options, const ScenarioInfo& info, RunLength run, const oly::app::Engine& engine, Measurements& measurements) {
      std::fprintf(file, "{\n  \"commit\": \"%s\", \"scenario\": \"%.*s\", \"size\": %u, \"seed\": %llu,\n", options.commit.c_str(), static_cast<int>(info.name.size()), info.name.data(), *options.size, static_cast<unsigned long long>(info.seed));
      std::fprintf(file, "  \"warmup\": %u, \"steps\": %u, \"stepsOverridden\": %s, \"timing\": %s,\n", run.warmup, run.steps, options.steps ? "true" : "false", options.timing ? "true" : "false");
      std::fprintf(file, "  \"step\": { \"p50\": %llu, \"p99\": %llu, \"max\": %llu },\n", static_cast<unsigned long long>(percentile(measurements.stepNanoseconds, 50.0)), static_cast<unsigned long long>(percentile(measurements.stepNanoseconds, 99.0)), static_cast<unsigned long long>(percentile(measurements.stepNanoseconds, 100.0)));
      std::fprintf(file, "  \"systems\": [");

      const auto systems = engine.systemTimings();

      for (size_t slot = 0; slot < systems.size(); ++slot) {
        const std::vector<uint64_t>& values = measurements.systemNanoseconds[slot];
        std::fprintf(file, "%s\n    { \"name\": \"%.*s\", \"phase\": \"%s\", \"ran\": %zu, \"median\": %llu }", slot == 0 ? "" : ",", static_cast<int>(systems[slot].name.size()), systems[slot].name.data(), phaseName(systems[slot].phase), values.size(), static_cast<unsigned long long>(medianOf(values)));
      }

      std::fprintf(file, "%s],\n  \"flushes\": [", systems.empty() ? "" : "\n  ");

      const auto flushes = engine.flushTimings();

      for (size_t slot = 0; slot < flushes.size(); ++slot) {
        std::fprintf(file, "%s\n    { \"phase\": \"%s\", \"median\": %llu }", slot == 0 ? "" : ",", phaseName(flushes[slot].phase), static_cast<unsigned long long>(medianOf(measurements.flushNanoseconds[slot])));
      }

      std::fprintf(file, "%s],\n  \"measures\": {}\n}\n", flushes.empty() ? "" : "\n  ");
    }
  }

  int runHeadless(const LaunchOptions& options) {
    if (!checkScenario(*options.scenario, options.size)) {
      return 64;
    }

    File file(std::fopen(options.out.c_str(), "w"));

    if (!file) {
      std::fprintf(stderr, "enxame: cannot write %s\n", options.out.c_str());

      return 1;
    }

    const ScenarioInfo& info = scenarioInfo(*options.scenario);
    const RunLength run = options.steps ? RunLength{0, *options.steps} : info.run;
    oly::ecs::World world;
    oly::app::Engine engine(world);
    oly::app::setUpGame(engine);
    const std::unique_ptr<Simulation> simulation = setUpScenario(*options.scenario, *options.size, engine);
    engine.setTimingEnabled(options.timing);
    Measurements measurements;
    engine.frame(0.0);

    for (uint32_t frame = 1; frame <= run.warmup + run.steps; ++frame) {
      const auto start = std::chrono::steady_clock::now();
      engine.frame(static_cast<double>(frame) / 60.0);
      const uint64_t elapsed = nanosecondsSince(start);

      if (frame > run.warmup) {
        measurements.stepNanoseconds.push_back(elapsed);

        if (options.timing) {
          record(engine, measurements);
        }
      }
    }

    writeJson(file.get(), options, info, run, engine, measurements);
    std::printf("wrote %s\n", options.out.c_str());

    return 0;
  }
}
